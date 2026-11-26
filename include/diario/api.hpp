#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "diario/diario.hpp"
#include "diario/repositorio.hpp"
#include "diario/sessao.hpp"

namespace diario {

// Requisição HTTP já reduzida ao que a API usa, sem depender da biblioteca de servidor.
struct Requisicao {
    std::map<std::string, std::string> parametros;  // query string (?q=...&ordem=...)
    std::string corpo;
    std::string cookie;  // valor bruto do cabeçalho Cookie
};

struct Resposta {
    int status = 200;
    nlohmann::json corpo;              // null: resposta sem corpo (ex.: 204)
    std::vector<std::string> cookies;  // valores de Set-Cookie
};

struct OpcoesApi {
    std::string segredoSessao;  // ao menos 32 caracteres (ver AssinadorSessao)
    // Marca o cookie como Secure (só trafega em HTTPS). Ligado em produção; desligado em
    // localhost, onde não há HTTPS.
    bool cookieSeguro = false;
    // Fonte do instante atual; os testes injetam um relógio controlado para testar expiração.
    std::function<AssinadorSessao::Instante()> relogio = [] { return std::chrono::system_clock::now(); };
};

// Regras das rotas /api/*: interpreta a requisição, identifica o usuário pelo cookie de
// sessão, chama o núcleo e o repositório e decide status e corpo. O main.cpp só traduz
// entre o cpp-httplib e estes tipos, o que permite testar as rotas sem subir um servidor.
//
// Thread-safe: o cpp-httplib atende requisições em várias threads.
class Api {
public:
    static constexpr const char* kNomeCookie = "sessao";

    Api(Repositorio& repo, OpcoesApi opcoes);

    // Contas
    Resposta cadastrar(const Requisicao& req);  // cria a conta e já entra
    Resposta entrar(const Requisicao& req);
    Resposta sair(const Requisicao& req);
    Resposta usuarioAtual(const Requisicao& req);

    // Livros do usuário da sessão; 401 sem sessão válida.
    Resposta listarLivros(const Requisicao& req);
    Resposta estatisticas(const Requisicao& req);
    Resposta criarLivro(const Requisicao& req);
    Resposta atualizarLivro(const Requisicao& req, int id);
    Resposta removerLivro(const Requisicao& req, int id);

    // Pessoas (exigem sessão). Todo diário é visível para qualquer usuário logado.
    // ?q=<trecho do login>: até kLimiteBuscaUsuarios logins, em ordem alfabética.
    Resposta buscarUsuarios(const Requisicao& req);
    // Login, livros, estatísticas e contagem de seguidores de outro usuário; 404 se não existir.
    Resposta perfil(const Requisicao& req, const std::string& login);

    // Seguir e deixar de seguir `login`. Respondem {"euSigo", "seguidores"} já atualizados.
    // Repetir não é erro. 404 se o usuário não existir; 400 ao tentar seguir a si mesmo.
    Resposta seguir(const Requisicao& req, const std::string& login);
    Resposta deixarDeSeguir(const Requisicao& req, const std::string& login);

    // Livros de quem o usuário segue, do registrado mais recentemente para o mais antigo,
    // no máximo kLimiteFeed. Cada item: {"login": <dono>, "livro": {...}}.
    Resposta feed(const Requisicao& req);

    static constexpr std::size_t kLimiteBuscaUsuarios = 20;
    static constexpr std::size_t kLimiteFeed = 50;

private:
    std::optional<int> usuarioDaSessao(const Requisicao& req) const;
    std::string cookieDeSessao(int usuarioId) const;
    // Diário do usuário em memória, carregado do repositório no primeiro acesso.
    // Chamar com mtx_ travado.
    Diario& diarioDe(int usuarioId);
    // Parte comum de seguir e deixarDeSeguir.
    Resposta alterarSeguir(const Requisicao& req, const std::string& login, bool seguir);

    Repositorio& repo_;
    OpcoesApi opcoes_;
    AssinadorSessao assinador_;
    // Hash de uma senha aleatória: o login com usuário inexistente também calcula um
    // Argon2, para que o tempo de resposta não revele quais logins existem.
    std::string hashFicticio_;
    std::map<int, Diario> diarios_;
    std::mutex mtx_;
};

}  // namespace diario
