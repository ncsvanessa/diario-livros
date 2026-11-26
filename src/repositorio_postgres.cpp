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
CREATE TABLE IF NOT EXISTS usuarios (
    id         integer     GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    login      text        NOT NULL UNIQUE,
    senha_hash text        NOT NULL,
    criado_em  timestamptz NOT NULL DEFAULT now()
);
-- Cada usuário numera os próprios livros a partir de 1: a chave é (usuario_id, id).
CREATE TABLE IF NOT EXISTS livros (
    usuario_id      integer  NOT NULL REFERENCES usuarios (id) ON DELETE CASCADE,
    id              integer  NOT NULL,
    titulo          text     NOT NULL,
    autor           text     NOT NULL,
    capa_url        text     NOT NULL DEFAULT '',
    data_lancamento date,
    data_leitura    date,
    nota            smallint NOT NULL CHECK (nota BETWEEN 0 AND 5),
    registrado_em   text     NOT NULL DEFAULT '',
    PRIMARY KEY (usuario_id, id)
);
-- Bancos criados antes da coluna registrado_em: CREATE TABLE IF NOT EXISTS não a adiciona.
ALTER TABLE livros ADD COLUMN IF NOT EXISTS registrado_em text NOT NULL DEFAULT '';
-- Próximo id de livro de cada usuário, para que ids removidos não sejam reaproveitados.
CREATE TABLE IF NOT EXISTS diario_meta (
    usuario_id integer PRIMARY KEY REFERENCES usuarios (id) ON DELETE CASCADE,
    proximo_id integer NOT NULL
);
)SQL";

// Erro do servidor PostgreSQL com o código SQLSTATE (ex.: 23505 = violação de unicidade).
class ErroSql : public std::runtime_error {
public:
    ErroSql(const std::string& msg, std::string sqlstate)
        : std::runtime_error(msg), sqlstate(std::move(sqlstate)) {}
    std::string sqlstate;
};

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
        const char* sqlstate = PQresultErrorField(r.get(), PG_DIAG_SQLSTATE);
        throw ErroSql(std::string("erro no PostgreSQL: ") + PQerrorMessage(c), sqlstate ? sqlstate : "");
    }
    return r;
}

std::optional<Usuario> primeiroUsuario(const PGresult* r) {
    if (PQntuples(r) == 0) return std::nullopt;
    return Usuario{std::stoi(PQgetvalue(r, 0, 0)), PQgetvalue(r, 0, 1), PQgetvalue(r, 0, 2)};
}

}  // namespace

RepositorioPostgres::RepositorioPostgres(std::string url) : url_(std::move(url)) {
    auto c = conectar(url_);
    executar(c.get(), kEsquema);

    // CREATE TABLE IF NOT EXISTS não altera uma tabela existente: um banco da versão
    // anterior (um único diário, sem usuários) continuaria com o esquema velho. Melhor
    // parar com uma mensagem clara do que falhar depois em cada consulta.
    auto colunas = executar(c.get(), R"SQL(
        SELECT 1 FROM information_schema.columns
        WHERE table_schema = current_schema() AND table_name = 'livros' AND column_name = 'usuario_id')SQL");
    if (PQntuples(colunas.get()) == 0) {
        throw std::runtime_error(
            "o banco tem o esquema antigo, sem usuários. Para começar do zero, execute "
            "'DROP TABLE livros, diario_meta;' e reinicie o servidor");
    }
}

int RepositorioPostgres::criarUsuario(const std::string& login, const std::string& senhaHash) {
    auto c = conectar(url_);
    try {
        // A unicidade é garantida pelo banco (UNIQUE), sem corrida entre dois cadastros simultâneos.
        auto r = executar(c.get(), "INSERT INTO usuarios (login, senha_hash) VALUES ($1, $2) RETURNING id",
                          {login, senhaHash});
        return std::stoi(PQgetvalue(r.get(), 0, 0));
    } catch (const ErroSql& e) {
        if (e.sqlstate == "23505") throw LoginEmUso{};
        throw;
    }
}

std::optional<Usuario> RepositorioPostgres::buscarUsuarioPorLogin(const std::string& login) {
    auto c = conectar(url_);
    auto r = executar(c.get(), "SELECT id, login, senha_hash FROM usuarios WHERE login = $1", {login});
    return primeiroUsuario(r.get());
}

std::optional<Usuario> RepositorioPostgres::buscarUsuarioPorId(int id) {
    auto c = conectar(url_);
    auto r = executar(c.get(), "SELECT id, login, senha_hash FROM usuarios WHERE id = $1", {std::to_string(id)});
    return primeiroUsuario(r.get());
}

Diario RepositorioPostgres::carregarDiario(int usuarioId) {
    auto c = conectar(url_);
    const auto usuario = std::to_string(usuarioId);

    // ORDER BY id reproduz a ordem de inserção, pois ids só crescem.
    auto livros = executar(c.get(), R"SQL(
        SELECT id, titulo, autor, capa_url,
               coalesce(to_char(data_lancamento, 'YYYY-MM-DD'), ''),
               coalesce(to_char(data_leitura, 'YYYY-MM-DD'), ''),
               nota, registrado_em
        FROM livros WHERE usuario_id = $1 ORDER BY id)SQL",
                           {usuario});
    auto meta = executar(c.get(), "SELECT proximo_id FROM diario_meta WHERE usuario_id = $1", {usuario});

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
                               {"nota", std::stoi(PQgetvalue(r, i, 6))},
                               {"registradoEm", PQgetvalue(r, i, 7)}});
    }
    return Diario::deJson(j);
}

void RepositorioPostgres::salvarDiario(int usuarioId, const Diario& diario) {
    auto c = conectar(url_);
    const auto usuario = std::to_string(usuarioId);
    const auto j = diario.paraJson();

    // Se algo lançar antes do COMMIT, a conexão é fechada e o PostgreSQL desfaz a transação.
    executar(c.get(), "BEGIN");
    executar(c.get(), "DELETE FROM livros WHERE usuario_id = $1", {usuario});
    for (const auto& l : j["livros"]) {
        executar(c.get(),
                 "INSERT INTO livros (usuario_id, id, titulo, autor, capa_url, data_lancamento, data_leitura, nota, "
                 "registrado_em) "
                 "VALUES ($1, $2, $3, $4, $5, NULLIF($6, '')::date, NULLIF($7, '')::date, $8, $9)",
                 {usuario, std::to_string(l["id"].get<int>()), l["titulo"].get<std::string>(),
                  l["autor"].get<std::string>(), l["capaUrl"].get<std::string>(),
                  l["dataLancamento"].get<std::string>(), l["dataLeitura"].get<std::string>(),
                  std::to_string(l["nota"].get<int>()), l["registradoEm"].get<std::string>()});
    }
    executar(c.get(),
             "INSERT INTO diario_meta (usuario_id, proximo_id) VALUES ($1, $2) "
             "ON CONFLICT (usuario_id) DO UPDATE SET proximo_id = EXCLUDED.proximo_id",
             {usuario, std::to_string(j["proximoId"].get<int>())});
    executar(c.get(), "COMMIT");
}

}  // namespace diario
