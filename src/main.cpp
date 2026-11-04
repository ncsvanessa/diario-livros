// Servidor local do Diário de Livros.
// Uso: diario_server [--porta 8080] [--dados diario.json] [--web <pasta>]

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

#include <httplib.h>

#include "diario/diario.hpp"

using nlohmann::json;

namespace {

struct Config {
    int porta = 8080;
    std::string arquivoDados = "diario.json";
    std::string pastaWeb = DIARIO_WEB_DIR;
};

Config lerArgs(int argc, char** argv) {
    Config c;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string chave = argv[i];
        const std::string valor = argv[i + 1];
        if (chave == "--porta") c.porta = std::atoi(valor.c_str());
        else if (chave == "--dados") c.arquivoDados = valor;
        else if (chave == "--web") c.pastaWeb = valor;
    }
    return c;
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

    diario::Diario dl;
    try {
        dl = diario::Diario::carregar(cfg.arquivoDados);
    } catch (const std::exception& e) {
        std::cerr << "Erro ao ler " << cfg.arquivoDados << ": " << e.what() << '\n';
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
            dl.salvar(cfg.arquivoDados);
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
            dl.salvar(cfg.arquivoDados);
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
        dl.salvar(cfg.arquivoDados);
        res.status = 204;
    });

    std::cout << "Diário de Livros em http://localhost:" << cfg.porta << '\n'
              << "Dados: " << cfg.arquivoDados << '\n';
    // Só localhost: não expõe o diário na rede.
    if (!srv.listen("127.0.0.1", cfg.porta)) {
        std::cerr << "Não foi possível abrir a porta " << cfg.porta << '\n';
        return 1;
    }
    return 0;
}
