// Servidor do Diário de Livros.
// Uso: diario_server [--porta 8080] [--host 127.0.0.1] [--dados diario.json] [--web <pasta>]
//
// Persistência: se a variável de ambiente DATABASE_URL estiver definida, usa PostgreSQL
// (exige build com DIARIO_WITH_POSTGRES=ON); senão, o arquivo indicado em --dados.
// A URL vem do ambiente, e não de um argumento, porque contém a senha do banco.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <httplib.h>

#include "diario/api.hpp"
#include "diario/repositorio.hpp"
#ifdef DIARIO_COM_POSTGRES
#include "diario/repositorio_postgres.hpp"
#endif

namespace {

struct Config {
    int porta = 8080;
    // Padrão só localhost; em produção (container) usa-se --host 0.0.0.0.
    std::string host = "127.0.0.1";
    std::string arquivoDados = "diario.json";
    std::string pastaWeb = DIARIO_WEB_DIR;
};

Config lerArgs(int argc, char** argv) {
    Config c;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string chave = argv[i];
        const std::string valor = argv[i + 1];
        if (chave == "--porta") c.porta = std::atoi(valor.c_str());
        else if (chave == "--host") c.host = valor;
        else if (chave == "--dados") c.arquivoDados = valor;
        else if (chave == "--web") c.pastaWeb = valor;
    }
    return c;
}

std::unique_ptr<diario::Repositorio> criarRepositorio(const Config& cfg) {
    const char* url = std::getenv("DATABASE_URL");
    if (url == nullptr || *url == '\0') {
        return std::make_unique<diario::RepositorioArquivo>(cfg.arquivoDados);
    }
#ifdef DIARIO_COM_POSTGRES
    return std::make_unique<diario::RepositorioPostgres>(url);
#else
    throw std::runtime_error("DATABASE_URL definida, mas o servidor foi compilado sem DIARIO_WITH_POSTGRES");
#endif
}

diario::Requisicao converter(const httplib::Request& req) {
    diario::Requisicao r;
    // emplace mantém o primeiro valor de um parâmetro repetido, como get_param_value.
    for (const auto& [nome, valor] : req.params) r.parametros.emplace(nome, valor);
    r.corpo = req.body;
    return r;
}

void responder(httplib::Response& res, const diario::Resposta& r) {
    res.status = r.status;
    if (!r.corpo.is_null()) res.set_content(r.corpo.dump(), "application/json; charset=utf-8");
}

}  // namespace

int main(int argc, char** argv) {
    const Config cfg = lerArgs(argc, argv);

    std::unique_ptr<diario::Repositorio> repo;
    std::unique_ptr<diario::Api> api;
    try {
        repo = criarRepositorio(cfg);
        api = std::make_unique<diario::Api>(*repo);
    } catch (const std::exception& e) {
        std::cerr << "Erro ao carregar os dados: " << e.what() << '\n';
        return 1;
    }

    httplib::Server srv;

    // Capas podem vir como data:image em base64 (upload local).
    srv.set_payload_max_length(8 * 1024 * 1024);

    if (!srv.set_mount_point("/", cfg.pastaWeb)) {
        std::cerr << "Pasta da interface nao encontrada: " << cfg.pastaWeb << '\n';
        return 1;
    }

    // As regras de cada rota ficam em diario::Api (testada por unidade); aqui só a ligação HTTP.
    srv.Get("/api/livros", [&](const httplib::Request& req, httplib::Response& res) {
        responder(res, api->listarLivros(converter(req)));
    });

    srv.Get("/api/estatisticas", [&](const httplib::Request& req, httplib::Response& res) {
        responder(res, api->estatisticas(converter(req)));
    });

    srv.Post("/api/livros", [&](const httplib::Request& req, httplib::Response& res) {
        responder(res, api->criarLivro(converter(req)));
    });

    // Ids com no máximo 9 dígitos cabem em int; um id maior não casa com a rota
    // e recebe 404, em vez de estourar std::stoi (std::out_of_range -> erro 500).
    srv.Put(R"(/api/livros/(\d{1,9}))", [&](const httplib::Request& req, httplib::Response& res) {
        responder(res, api->atualizarLivro(converter(req), std::stoi(req.matches[1])));
    });

    srv.Delete(R"(/api/livros/(\d{1,9}))", [&](const httplib::Request& req, httplib::Response& res) {
        responder(res, api->removerLivro(converter(req), std::stoi(req.matches[1])));
    });

    // Não imprime a DATABASE_URL: ela contém a senha e iria para os logs da plataforma.
    const bool usaBanco = std::getenv("DATABASE_URL") != nullptr && *std::getenv("DATABASE_URL") != '\0';
    std::cout << "Diário de Livros em http://" << cfg.host << ':' << cfg.porta << '\n'
              << "Dados: " << (usaBanco ? "PostgreSQL (DATABASE_URL)" : cfg.arquivoDados) << std::endl;
    if (!srv.listen(cfg.host, cfg.porta)) {
        std::cerr << "Não foi possível abrir a porta " << cfg.porta << '\n';
        return 1;
    }
    return 0;
}
