#pragma once

#include <map>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "diario/diario.hpp"
#include "diario/repositorio.hpp"

namespace diario {

// Requisição HTTP já reduzida ao que a API usa, sem depender da biblioteca de servidor.
struct Requisicao {
    std::map<std::string, std::string> parametros;  // query string (?q=...&ordem=...)
    std::string corpo;
};

struct Resposta {
    int status = 200;
    nlohmann::json corpo;  // null: resposta sem corpo (ex.: 204)
};

// Regras das rotas /api/*: interpreta a requisição, chama o núcleo e o repositório e
// decide status e corpo. O main.cpp só traduz entre o cpp-httplib e estes tipos, o que
// permite testar as rotas sem subir um servidor.
//
// Thread-safe: o cpp-httplib atende requisições em várias threads.
class Api {
public:
    // Carrega o diário salvo; propaga a exceção do repositório se falhar.
    explicit Api(Repositorio& repo);

    Resposta listarLivros(const Requisicao& req);
    Resposta estatisticas(const Requisicao& req);
    Resposta criarLivro(const Requisicao& req);
    Resposta atualizarLivro(const Requisicao& req, int id);
    Resposta removerLivro(const Requisicao& req, int id);

private:
    Repositorio& repo_;
    Diario diario_;
    std::mutex mtx_;
};

}  // namespace diario
