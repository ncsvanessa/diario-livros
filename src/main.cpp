// Servidor do Diário de Livros.
// Uso: diario_server [--porta 8080] [--host 127.0.0.1] [--dados diario.json] [--web <pasta>]
//
// Persistência: se a variável de ambiente DATABASE_URL estiver definida, usa PostgreSQL
// (exige build com DIARIO_WITH_POSTGRES=ON); senão, o arquivo indicado em --dados.
// A URL vem do ambiente, e não de um argumento, porque contém a senha do banco.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include <httplib.h>

#include "diario/diario.hpp"
#include "diario/repositorio.hpp"
#ifdef DIARIO_COM_POSTGRES
#include "diario/repositorio_postgres.hpp"
#endif

using nlohmann::json;

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

void responderJson(httplib::Response& res, const json& corpo, int status = 200) {
    res.status = status;
    res.set_content(corpo.dump(), "application/json; charset=utf-8");
}

void responderErro(httplib::Response& res, int status, const std::string& msg) {
    responderJson(res, json{{"erro", msg}}, status);
}

}  // namespace

int main(int argc, char** argv) {
    const Config cfg = lerArgs(argc, argv);

    std::unique_ptr<diario::Repositorio> repo;
    diario::Diario dl;
    try {
        repo = criarRepositorio(cfg);
        dl = repo->carregar();
    } catch (const std::exception& e) {
        std::cerr << "Erro ao carregar os dados: " << e.what() << '\n';
        return 1;
    }

    // httplib atende requisições em várias threads; o diário não é thread-safe.
    std::mutex mtx;
    httplib::Server srv;

    // Capas podem vir como data:image em base64 (upload local).
    srv.set_payload_max_length(8 * 1024 * 1024);

    if (!srv.set_mount_point("/", cfg.pastaWeb)) {
        std::cerr << "Pasta da interface nao encontrada: " << cfg.pastaWeb << '\n';
        return 1;
    }

    srv.Get("/api/livros", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        const auto termo = req.get_param_value("q");
        const auto ordem = req.get_param_value("ordem");
        auto livros = ordem == "nota" ? dl.ordenadosPorNota() : dl.listar();
        if (!termo.empty()) {
            const auto filtrados = dl.pesquisar(termo);
            std::vector<diario::Livro> out;
            for (const auto& l : livros) {
                for (const auto& f : filtrados) {
                    if (f.id == l.id) { out.push_back(l); break; }
                }
            }
            livros = std::move(out);
        }
        responderJson(res, livros);
    });

    srv.Get("/api/estatisticas", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        responderJson(res, json{{"total", dl.tamanho()}, {"media", dl.mediaNotas()}});
    });

    srv.Post("/api/livros", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto livro = json::parse(req.body).get<diario::Livro>();
            std::lock_guard<std::mutex> lock(mtx);
            const int id = dl.adicionar(std::move(livro));
            repo->salvar(dl);
            responderJson(res, *dl.buscar(id), 201);
        } catch (const std::invalid_argument& e) {
            responderErro(res, 400, e.what());
        } catch (const json::exception&) {
            responderErro(res, 400, "JSON inválido");
        }
    });

    srv.Put(R"(/api/livros/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        const int id = std::stoi(req.matches[1]);
        try {
            auto livro = json::parse(req.body).get<diario::Livro>();
            std::lock_guard<std::mutex> lock(mtx);
            if (!dl.atualizar(id, std::move(livro))) {
                responderErro(res, 404, "livro não encontrado");
                return;
            }
            repo->salvar(dl);
            responderJson(res, *dl.buscar(id));
        } catch (const std::invalid_argument& e) {
            responderErro(res, 400, e.what());
        } catch (const json::exception&) {
            responderErro(res, 400, "JSON inválido");
        }
    });

    srv.Delete(R"(/api/livros/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        const int id = std::stoi(req.matches[1]);
        std::lock_guard<std::mutex> lock(mtx);
        if (!dl.remover(id)) {
            responderErro(res, 404, "livro não encontrado");
            return;
        }
        repo->salvar(dl);
        res.status = 204;
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
