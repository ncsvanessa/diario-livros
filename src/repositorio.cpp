#include "diario/repositorio.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace diario {

using nlohmann::json;

// Formato do arquivo:
// {
//   "versao": 2,
//   "proximoUsuarioId": 3,
//   "usuarios": [{"id": 1, "login": "ana", "senhaHash": "argon2id$..."}],
//   "diarios": {"1": {"proximoId": 2, "livros": [...]}},
//   "seguindo": {"1": [2, 3]}
// }
// "seguindo" é opcional: arquivos gravados antes de existir seguir não o têm.

namespace {

std::optional<Usuario> usuarioDe(const json& u) {
    return Usuario{u.at("id").get<int>(), u.at("login").get<std::string>(), u.at("senhaHash").get<std::string>()};
}

}  // namespace

RepositorioArquivo::RepositorioArquivo(std::string caminho) : caminho_(std::move(caminho)) {}

json RepositorioArquivo::ler() const {
    std::ifstream in(caminho_, std::ios::binary);
    if (!in) {
        return json{{"versao", 2}, {"proximoUsuarioId", 1}, {"usuarios", json::array()}, {"diarios", json::object()}};
    }
    // Erros do arquivo viram std::runtime_error (falha de armazenamento), para não serem
    // confundidos com JSON inválido enviado na requisição.
    json dados;
    try {
        dados = json::parse(in);
    } catch (const json::exception&) {
        throw std::runtime_error("o arquivo " + caminho_ + " não é um JSON válido");
    }
    if (dados.is_object() && dados.contains("livros") && !dados.contains("versao")) {
        // Versões anteriores guardavam um único diário, sem contas.
        throw std::runtime_error("o arquivo " + caminho_ +
                                 " está no formato antigo, sem usuários; renomeie ou apague o arquivo");
    }
    if (!dados.is_object() || !dados.contains("usuarios") || !dados.contains("diarios") ||
        !dados.contains("proximoUsuarioId")) {
        throw std::runtime_error("o arquivo " + caminho_ + " não está no formato esperado");
    }
    return dados;
}

void RepositorioArquivo::gravar(const json& dados) const {
    std::ofstream out(caminho_, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("não foi possível abrir " + caminho_ + " para escrita");
    out << dados.dump(2);
}

int RepositorioArquivo::criarUsuario(const std::string& login, const std::string& senhaHash) {
    auto dados = ler();
    for (const auto& u : dados["usuarios"]) {
        if (u.at("login") == login) throw LoginEmUso{};
    }
    const int id = dados["proximoUsuarioId"].get<int>();
    dados["usuarios"].push_back({{"id", id}, {"login", login}, {"senhaHash", senhaHash}});
    dados["proximoUsuarioId"] = id + 1;
    gravar(dados);
    return id;
}

std::optional<Usuario> RepositorioArquivo::buscarUsuarioPorLogin(const std::string& login) {
    // Guarda o JSON numa variável: em `for (... : ler()["usuarios"])` o temporário
    // devolvido por ler() seria destruído antes do laço (referência pendente).
    const auto dados = ler();
    for (const auto& u : dados.at("usuarios")) {
        if (u.at("login") == login) return usuarioDe(u);
    }
    return std::nullopt;
}

std::optional<Usuario> RepositorioArquivo::buscarUsuarioPorId(int id) {
    const auto dados = ler();
    for (const auto& u : dados.at("usuarios")) {
        if (u.at("id") == id) return usuarioDe(u);
    }
    return std::nullopt;
}

std::vector<Usuario> RepositorioArquivo::buscarUsuarios(const std::string& trecho, std::size_t limite) {
    std::vector<Usuario> encontrados;
    if (trecho.empty()) return encontrados;
    const auto dados = ler();
    for (const auto& u : dados.at("usuarios")) {
        if (u.at("login").get<std::string>().find(trecho) != std::string::npos) encontrados.push_back(*usuarioDe(u));
    }
    std::sort(encontrados.begin(), encontrados.end(),
              [](const Usuario& a, const Usuario& b) { return a.login < b.login; });
    if (encontrados.size() > limite) encontrados.resize(limite);
    return encontrados;
}

namespace {

std::vector<int> idsSeguidos(const json& dados, int usuarioId) {
    const auto chave = std::to_string(usuarioId);
    if (!dados.contains("seguindo") || !dados.at("seguindo").contains(chave)) return {};
    return dados.at("seguindo").at(chave).get<std::vector<int>>();
}

}  // namespace

void RepositorioArquivo::seguir(int seguidorId, int seguidoId) {
    auto dados = ler();
    auto ids = idsSeguidos(dados, seguidorId);
    if (std::find(ids.begin(), ids.end(), seguidoId) != ids.end()) return;
    ids.push_back(seguidoId);
    std::sort(ids.begin(), ids.end());
    dados["seguindo"][std::to_string(seguidorId)] = ids;
    gravar(dados);
}

void RepositorioArquivo::deixarDeSeguir(int seguidorId, int seguidoId) {
    auto dados = ler();
    auto ids = idsSeguidos(dados, seguidorId);
    const auto fim = std::remove(ids.begin(), ids.end(), seguidoId);
    if (fim == ids.end()) return;
    ids.erase(fim, ids.end());
    dados["seguindo"][std::to_string(seguidorId)] = ids;
    gravar(dados);
}

std::vector<int> RepositorioArquivo::seguidos(int usuarioId) { return idsSeguidos(ler(), usuarioId); }

std::vector<int> RepositorioArquivo::seguidores(int usuarioId) {
    const auto dados = ler();
    std::vector<int> ids;
    if (!dados.contains("seguindo")) return ids;
    for (const auto& [chave, seguidos] : dados.at("seguindo").items()) {
        for (const auto& id : seguidos) {
            if (id.get<int>() == usuarioId) ids.push_back(std::stoi(chave));
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

Diario RepositorioArquivo::carregarDiario(int usuarioId) {
    const auto dados = ler();
    const auto chave = std::to_string(usuarioId);
    if (!dados.at("diarios").contains(chave)) return Diario{};
    return Diario::deJson(dados.at("diarios").at(chave));
}

void RepositorioArquivo::salvarDiario(int usuarioId, const Diario& diario) {
    auto dados = ler();
    dados["diarios"][std::to_string(usuarioId)] = diario.paraJson();
    gravar(dados);
}

}  // namespace diario
