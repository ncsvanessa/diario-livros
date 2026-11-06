#include "diario/repositorio_postgres.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <libpq-fe.h>

namespace diario {

namespace {

using Conexao = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using Resultado = std::unique_ptr<PGresult, decltype(&PQclear)>;

constexpr const char* kEsquema = R"SQL(
-- Evita NOTICE "relation already exists" a cada inicialização.
SET client_min_messages TO warning;
CREATE TABLE IF NOT EXISTS livros (
    id              integer  PRIMARY KEY,
    titulo          text     NOT NULL,
    autor           text     NOT NULL,
    capa_url        text     NOT NULL DEFAULT '',
    data_lancamento date,
    data_leitura    date,
    nota            smallint NOT NULL CHECK (nota BETWEEN 0 AND 5)
);
-- Linha única com o próximo id, para que ids removidos não sejam reaproveitados.
CREATE TABLE IF NOT EXISTS diario_meta (
    unica      boolean PRIMARY KEY DEFAULT true CHECK (unica),
    proximo_id integer NOT NULL
);
)SQL";

Conexao conectar(const std::string& url) {
    Conexao c(PQconnectdb(url.c_str()), &PQfinish);
    if (!c || PQstatus(c.get()) != CONNECTION_OK) {
        const std::string msg = c ? PQerrorMessage(c.get()) : "sem memória";
        throw std::runtime_error("falha ao conectar ao PostgreSQL: " + msg);
    }
    PQsetClientEncoding(c.get(), "UTF8");
    return c;
}

Resultado executar(PGconn* c, const std::string& sql, const std::vector<std::string>& params = {}) {
    std::vector<const char*> valores;
    valores.reserve(params.size());
    for (const auto& p : params) valores.push_back(p.c_str());

    // Sempre parametrizado: nenhum dado do usuário é concatenado no SQL.
    Resultado r(params.empty()
                    ? PQexec(c, sql.c_str())
                    : PQexecParams(c, sql.c_str(), static_cast<int>(params.size()), nullptr,
                                   valores.data(), nullptr, nullptr, 0),
                &PQclear);
    const auto status = PQresultStatus(r.get());
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        throw std::runtime_error(std::string("erro no PostgreSQL: ") + PQerrorMessage(c));
    }
    return r;
}

}  // namespace

RepositorioPostgres::RepositorioPostgres(std::string url) : url_(std::move(url)) {
    auto c = conectar(url_);
    executar(c.get(), kEsquema);
}

Diario RepositorioPostgres::carregar() {
    auto c = conectar(url_);

    // ORDER BY id reproduz a ordem de inserção, pois ids só crescem.
    auto livros = executar(c.get(), R"SQL(
        SELECT id, titulo, autor, capa_url,
               coalesce(to_char(data_lancamento, 'YYYY-MM-DD'), ''),
               coalesce(to_char(data_leitura, 'YYYY-MM-DD'), ''),
               nota
        FROM livros ORDER BY id)SQL");
    auto meta = executar(c.get(), "SELECT proximo_id FROM diario_meta");

    // Reconstrói pelo mesmo caminho do arquivo JSON, reaproveitando a validação de deJson.
    nlohmann::json j;
    j["proximoId"] = PQntuples(meta.get()) > 0 ? std::stoi(PQgetvalue(meta.get(), 0, 0)) : 1;
    j["livros"] = nlohmann::json::array();
    for (int i = 0; i < PQntuples(livros.get()); ++i) {
        const auto* r = livros.get();
        j["livros"].push_back({{"id", std::stoi(PQgetvalue(r, i, 0))},
                               {"titulo", PQgetvalue(r, i, 1)},
                               {"autor", PQgetvalue(r, i, 2)},
                               {"capaUrl", PQgetvalue(r, i, 3)},
                               {"dataLancamento", PQgetvalue(r, i, 4)},
                               {"dataLeitura", PQgetvalue(r, i, 5)},
                               {"nota", std::stoi(PQgetvalue(r, i, 6))}});
    }
    return Diario::deJson(j);
}

void RepositorioPostgres::salvar(const Diario& diario) {
    auto c = conectar(url_);
    const auto j = diario.paraJson();

    // Se algo lançar antes do COMMIT, a conexão é fechada e o PostgreSQL desfaz a transação.
    executar(c.get(), "BEGIN");
    executar(c.get(), "DELETE FROM livros");
    for (const auto& l : j["livros"]) {
        executar(c.get(),
                 "INSERT INTO livros (id, titulo, autor, capa_url, data_lancamento, data_leitura, nota) "
                 "VALUES ($1, $2, $3, $4, NULLIF($5, '')::date, NULLIF($6, '')::date, $7)",
                 {std::to_string(l["id"].get<int>()), l["titulo"].get<std::string>(),
                  l["autor"].get<std::string>(), l["capaUrl"].get<std::string>(),
                  l["dataLancamento"].get<std::string>(), l["dataLeitura"].get<std::string>(),
                  std::to_string(l["nota"].get<int>())});
    }
    executar(c.get(),
             "INSERT INTO diario_meta (unica, proximo_id) VALUES (true, $1) "
             "ON CONFLICT (unica) DO UPDATE SET proximo_id = EXCLUDED.proximo_id",
             {std::to_string(j["proximoId"].get<int>())});
    executar(c.get(), "COMMIT");
}

}  // namespace diario
