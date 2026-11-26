#include "diario/api.hpp"

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace diario {

using nlohmann::json;

namespace {

Resposta erro(int status, const std::string& mensagem) { return {status, json{{"erro", mensagem}}}; }

// Falha de gravação (ex.: banco fora do ar) é problema do servidor, não do pedido.
Resposta falhaAoSalvar(const std::exception& e) {
    std::cerr << "Erro ao salvar: " << e.what() << '\n';
    return erro(503, "não foi possível salvar; tente novamente");
}

std::string parametro(const Requisicao& req, const std::string& nome) {
    const auto it = req.parametros.find(nome);
    return it == req.parametros.end() ? "" : it->second;
}

}  // namespace

Api::Api(Repositorio& repo) : repo_(repo), diario_(repo.carregar()) {}

Resposta Api::listarLivros(const Requisicao& req) {
    const auto termo = parametro(req, "q");
    const auto ordem = parametro(req, "ordem");
    const auto notaMin = parametro(req, "notaMin");
    // Aceita só um dígito; o intervalo 0 a 5 é validado pelo núcleo.
    if (!notaMin.empty() && (notaMin.size() != 1 || !std::isdigit(static_cast<unsigned char>(notaMin[0])))) {
        return erro(400, "notaMin deve ser um inteiro de 0 a 5");
    }

    std::lock_guard<std::mutex> lock(mtx_);
    auto livros = ordem == "nota" ? diario_.ordenadosPorNota() : diario_.listar();
    try {
        // Cada filtro devolve um subconjunto; mantém só os livros presentes nele,
        // preservando a ordenação escolhida.
        const auto manterSo = [&livros](const std::vector<Livro>& filtrados) {
            std::vector<Livro> out;
            for (const auto& l : livros) {
                for (const auto& f : filtrados) {
                    if (f.id == l.id) { out.push_back(l); break; }
                }
            }
            livros = std::move(out);
        };
        if (!termo.empty()) manterSo(diario_.pesquisar(termo));
        if (!notaMin.empty()) manterSo(diario_.comNotaMinima(notaMin[0] - '0'));
    } catch (const std::invalid_argument& e) {
        return erro(400, e.what());
    }
    return {200, livros};
}

Resposta Api::estatisticas(const Requisicao&) {
    std::lock_guard<std::mutex> lock(mtx_);
    return {200, json{{"total", diario_.tamanho()},
                      {"media", diario_.mediaNotas()},
                      {"autoresMaisLidos", diario_.autoresMaisLidos(3)}}};
}

Resposta Api::criarLivro(const Requisicao& req) {
    try {
        auto livro = json::parse(req.corpo).get<Livro>();
        std::lock_guard<std::mutex> lock(mtx_);
        // Altera uma cópia e só a adota depois de salvar: se a gravação falhar,
        // a memória continua igual ao que está persistido.
        auto novo = diario_;
        const int id = novo.adicionar(std::move(livro));
        repo_.salvar(novo);
        diario_ = std::move(novo);
        return {201, *diario_.buscar(id)};
    } catch (const std::invalid_argument& e) {
        return erro(400, e.what());
    } catch (const json::exception&) {
        return erro(400, "JSON inválido");
    } catch (const std::runtime_error& e) {
        return falhaAoSalvar(e);
    }
}

Resposta Api::atualizarLivro(const Requisicao& req, int id) {
    try {
        auto livro = json::parse(req.corpo).get<Livro>();
        std::lock_guard<std::mutex> lock(mtx_);
        auto novo = diario_;
        if (!novo.atualizar(id, std::move(livro))) return erro(404, "livro não encontrado");
        repo_.salvar(novo);
        diario_ = std::move(novo);
        return {200, *diario_.buscar(id)};
    } catch (const std::invalid_argument& e) {
        return erro(400, e.what());
    } catch (const json::exception&) {
        return erro(400, "JSON inválido");
    } catch (const std::runtime_error& e) {
        return falhaAoSalvar(e);
    }
}

Resposta Api::removerLivro(const Requisicao&, int id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto novo = diario_;
    if (!novo.remover(id)) return erro(404, "livro não encontrado");
    try {
        repo_.salvar(novo);
    } catch (const std::runtime_error& e) {
        return falhaAoSalvar(e);
    }
    diario_ = std::move(novo);
    return {204, nullptr};
}

}  // namespace diario
