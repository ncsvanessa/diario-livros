// Testes de integração do RepositorioPostgres. Exigem um PostgreSQL acessível em
// DIARIO_TEST_DATABASE_URL (no CI, um service container).
//
// Cada teste cria usuários com login aleatório, então não depende do estado deixado por
// outros testes nem precisa apagar o banco.

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <libpq-fe.h>

#include "diario/aleatorio.hpp"
#include "diario/repositorio_postgres.hpp"

using diario::Diario;
using diario::Livro;
using diario::LoginEmUso;
using diario::RepositorioPostgres;

namespace {

Livro livro(const std::string& titulo, const std::string& autor, int nota) {
    Livro l;
    l.titulo = titulo;
    l.autor = autor;
    l.nota = nota;
    return l;
}

// Login único por teste: 12 dígitos hexadecimais aleatórios (48 bits).
std::string loginAleatorio() {
    std::string login = "t";
    for (const auto b : diario::bytesAleatorios(12)) login += "0123456789abcdef"[b & 0x0f];
    return login;
}

class RepositorioPostgresTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* url = std::getenv("DIARIO_TEST_DATABASE_URL");
        // Falha em vez de pular: um teste pulado em silêncio no CI esconderia a falta do banco.
        ASSERT_NE(url, nullptr) << "defina DIARIO_TEST_DATABASE_URL para rodar estes testes";
        url_ = url;
        repo_ = std::make_unique<RepositorioPostgres>(url_);
    }

    int novoUsuario() { return repo_->criarUsuario(loginAleatorio(), "hash"); }

    std::string url_;
    std::unique_ptr<RepositorioPostgres> repo_;
};

}  // namespace

// ---- Contas -------------------------------------------------------------------

TEST_F(RepositorioPostgresTest, UsuarioCriadoPodeSerBuscadoPeloLogin) {
    const auto login = loginAleatorio();

    const int id = repo_->criarUsuario(login, "hash-guardado");
    const auto usuario = repo_->buscarUsuarioPorLogin(login);

    ASSERT_TRUE(usuario.has_value());
    EXPECT_EQ(usuario->id, id);
    EXPECT_EQ(usuario->senhaHash, "hash-guardado");
}

TEST_F(RepositorioPostgresTest, UsuarioCriadoPodeSerBuscadoPeloId) {
    const auto login = loginAleatorio();

    const int id = repo_->criarUsuario(login, "hash");

    EXPECT_EQ(repo_->buscarUsuarioPorId(id)->login, login);
}

TEST_F(RepositorioPostgresTest, BuscarLoginInexistenteDevolveVazio) {
    EXPECT_FALSE(repo_->buscarUsuarioPorLogin(loginAleatorio()).has_value());
}

TEST_F(RepositorioPostgresTest, CriarLoginRepetidoLancaLoginEmUso) {
    const auto login = loginAleatorio();
    repo_->criarUsuario(login, "hash");

    EXPECT_THROW(repo_->criarUsuario(login, "outro-hash"), LoginEmUso);
}

// ---- Diários ------------------------------------------------------------------

TEST_F(RepositorioPostgresTest, DiarioDeUsuarioNovoEhVazio) {
    EXPECT_EQ(repo_->carregarDiario(novoUsuario()).tamanho(), 0u);
}

TEST_F(RepositorioPostgresTest, CarregarDevolveTodosOsCamposSalvos) {
    const int usuario = novoUsuario();
    Livro l = livro("Vidas Secas", "Graciliano Ramos", 4);
    l.capaUrl = "https://exemplo.com/vidas-secas.jpg";
    l.dataLancamento = "1938-01-01";
    l.dataLeitura = "2026-10-20";
    Diario original;
    original.adicionar(l);

    repo_->salvarDiario(usuario, original);
    const Livro lido = repo_->carregarDiario(usuario).listar().at(0);

    EXPECT_EQ(lido.titulo, "Vidas Secas");
    EXPECT_EQ(lido.autor, "Graciliano Ramos");
    EXPECT_EQ(lido.capaUrl, "https://exemplo.com/vidas-secas.jpg");
    EXPECT_EQ(lido.dataLancamento, "1938-01-01");
    EXPECT_EQ(lido.dataLeitura, "2026-10-20");
    EXPECT_EQ(lido.nota, 4);
}

TEST_F(RepositorioPostgresTest, DatasVaziasContinuamVaziasAposCarregar) {
    const int usuario = novoUsuario();
    Diario original;
    original.adicionar(livro("Sem datas", "Autor", 3));

    repo_->salvarDiario(usuario, original);
    const Livro lido = repo_->carregarDiario(usuario).listar().at(0);

    EXPECT_EQ(lido.dataLancamento, "");
    EXPECT_EQ(lido.dataLeitura, "");
}

TEST_F(RepositorioPostgresTest, PreservaAcentosEmUtf8) {
    const int usuario = novoUsuario();
    Diario original;
    original.adicionar(livro("Grande Sertão: Veredas", "João Guimarães Rosa", 5));

    repo_->salvarDiario(usuario, original);

    EXPECT_EQ(repo_->carregarDiario(usuario).listar().at(0).titulo, "Grande Sertão: Veredas");
}

TEST_F(RepositorioPostgresTest, CarregarMantemOrdemDeInsercao) {
    const int usuario = novoUsuario();
    Diario original;
    original.adicionar(livro("B", "X", 1));
    original.adicionar(livro("A", "X", 5));

    repo_->salvarDiario(usuario, original);
    const auto livros = repo_->carregarDiario(usuario).listar();

    ASSERT_EQ(livros.size(), 2u);
    EXPECT_EQ(livros[0].titulo, "B");
    EXPECT_EQ(livros[1].titulo, "A");
}

TEST_F(RepositorioPostgresTest, LivroRemovidoNaoReapareceAposSalvar) {
    const int usuario = novoUsuario();
    Diario d;
    const int id = d.adicionar(livro("Temporário", "X", 1));
    repo_->salvarDiario(usuario, d);
    d.remover(id);

    repo_->salvarDiario(usuario, d);

    EXPECT_EQ(repo_->carregarDiario(usuario).tamanho(), 0u);
}

TEST_F(RepositorioPostgresTest, IdRemovidoNaoEReaproveitadoAposCarregar) {
    const int usuario = novoUsuario();
    Diario d;
    d.adicionar(livro("A", "X", 1));
    const int removido = d.adicionar(livro("B", "X", 1));
    d.remover(removido);
    repo_->salvarDiario(usuario, d);

    Diario lido = repo_->carregarDiario(usuario);

    EXPECT_EQ(lido.adicionar(livro("C", "X", 1)), removido + 1);
}

TEST_F(RepositorioPostgresTest, SalvarDiarioDeUmUsuarioNaoAfetaOutro) {
    const int ana = novoUsuario();
    const int bia = novoUsuario();
    Diario daBia;
    daBia.adicionar(livro("Livro da Bia", "X", 3));
    repo_->salvarDiario(bia, daBia);
    Diario daAna;
    daAna.adicionar(livro("Livro da Ana", "X", 3));  // mesmo id (1) que o livro da Bia

    repo_->salvarDiario(ana, daAna);

    EXPECT_EQ(repo_->carregarDiario(bia).listar().at(0).titulo, "Livro da Bia");
}

// ---- Erros --------------------------------------------------------------------

TEST(RepositorioPostgresConexao, UrlInacessivelLancaErroNaConstrucao) {
    // Porta 1 em localhost: conexão recusada imediatamente, sem esperar timeout.
    EXPECT_THROW(RepositorioPostgres("postgresql://usuario@127.0.0.1:1/banco?connect_timeout=2"),
                 std::runtime_error);
}

TEST(RepositorioPostgresEsquema, BancoComEsquemaAntigoLancaErroNaConstrucao) {
    const char* url = std::getenv("DIARIO_TEST_DATABASE_URL");
    ASSERT_NE(url, nullptr) << "defina DIARIO_TEST_DATABASE_URL para rodar estes testes";
    // Monta o esquema da versão anterior (sem usuario_id) num schema separado, para não
    // interferir nos outros testes.
    std::unique_ptr<PGconn, decltype(&PQfinish)> c(PQconnectdb(url), &PQfinish);
    ASSERT_EQ(PQstatus(c.get()), CONNECTION_OK);
    PQclear(PQexec(c.get(), R"SQL(
        SET client_min_messages TO warning;
        DROP SCHEMA IF EXISTS esquema_antigo CASCADE;
        CREATE SCHEMA esquema_antigo;
        CREATE TABLE esquema_antigo.livros (id integer PRIMARY KEY, titulo text NOT NULL);
        CREATE TABLE esquema_antigo.diario_meta (unica boolean PRIMARY KEY, proximo_id integer);)SQL"));
    const std::string separador = std::string(url).find('?') == std::string::npos ? "?" : "&";
    const std::string urlEsquemaAntigo = std::string(url) + separador + "options=-csearch_path%3Desquema_antigo";

    EXPECT_THROW(RepositorioPostgres{urlEsquemaAntigo}, std::runtime_error);

    PQclear(PQexec(c.get(), "DROP SCHEMA esquema_antigo CASCADE"));
}
