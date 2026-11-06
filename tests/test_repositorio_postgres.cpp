// Testes de integração do RepositorioPostgres. Exigem um PostgreSQL acessível em
// DIARIO_TEST_DATABASE_URL (no CI, um service container). O banco é zerado antes de cada teste.

#include <cstdlib>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "diario/repositorio_postgres.hpp"

using diario::Diario;
using diario::Livro;
using diario::RepositorioPostgres;

namespace {

Livro livro(const std::string& titulo, const std::string& autor, int nota) {
    Livro l;
    l.titulo = titulo;
    l.autor = autor;
    l.nota = nota;
    return l;
}

class RepositorioPostgresTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* url = std::getenv("DIARIO_TEST_DATABASE_URL");
        // Falha em vez de pular: um teste pulado em silêncio no CI esconderia a falta do banco.
        ASSERT_NE(url, nullptr) << "defina DIARIO_TEST_DATABASE_URL para rodar estes testes";
        url_ = url;
        RepositorioPostgres(url_).salvar(Diario{});  // estado inicial: diário vazio
    }

    std::string url_;
};

}  // namespace

TEST_F(RepositorioPostgresTest, CarregarBancoVazioRetornaDiarioVazio) {
    RepositorioPostgres repo(url_);

    const Diario d = repo.carregar();

    EXPECT_EQ(d.tamanho(), 0u);
}

TEST_F(RepositorioPostgresTest, CarregarDevolveTodosOsCamposSalvos) {
    RepositorioPostgres repo(url_);
    Livro l = livro("Vidas Secas", "Graciliano Ramos", 4);
    l.capaUrl = "https://exemplo.com/vidas-secas.jpg";
    l.dataLancamento = "1938-01-01";
    l.dataLeitura = "2026-10-20";
    Diario original;
    original.adicionar(l);

    repo.salvar(original);
    const Livro lido = repo.carregar().listar().at(0);

    EXPECT_EQ(lido.titulo, "Vidas Secas");
    EXPECT_EQ(lido.autor, "Graciliano Ramos");
    EXPECT_EQ(lido.capaUrl, "https://exemplo.com/vidas-secas.jpg");
    EXPECT_EQ(lido.dataLancamento, "1938-01-01");
    EXPECT_EQ(lido.dataLeitura, "2026-10-20");
    EXPECT_EQ(lido.nota, 4);
}

TEST_F(RepositorioPostgresTest, DatasVaziasContinuamVaziasAposCarregar) {
    RepositorioPostgres repo(url_);
    Diario original;
    original.adicionar(livro("Sem datas", "Autor", 3));

    repo.salvar(original);
    const Livro lido = repo.carregar().listar().at(0);

    EXPECT_EQ(lido.dataLancamento, "");
    EXPECT_EQ(lido.dataLeitura, "");
}

TEST_F(RepositorioPostgresTest, PreservaAcentosEmUtf8) {
    RepositorioPostgres repo(url_);
    Diario original;
    original.adicionar(livro("Grande Sertão: Veredas", "João Guimarães Rosa", 5));

    repo.salvar(original);

    EXPECT_EQ(repo.carregar().listar().at(0).titulo, "Grande Sertão: Veredas");
}

TEST_F(RepositorioPostgresTest, CarregarMantemOrdemDeInsercao) {
    RepositorioPostgres repo(url_);
    Diario original;
    original.adicionar(livro("B", "X", 1));
    original.adicionar(livro("A", "X", 5));

    repo.salvar(original);
    const auto livros = repo.carregar().listar();

    ASSERT_EQ(livros.size(), 2u);
    EXPECT_EQ(livros[0].titulo, "B");
    EXPECT_EQ(livros[1].titulo, "A");
}

TEST_F(RepositorioPostgresTest, LivroRemovidoNaoReapareceAposSalvar) {
    RepositorioPostgres repo(url_);
    Diario d;
    const int id = d.adicionar(livro("Temporário", "X", 1));
    repo.salvar(d);
    d.remover(id);

    repo.salvar(d);

    EXPECT_EQ(repo.carregar().tamanho(), 0u);
}

TEST_F(RepositorioPostgresTest, IdRemovidoNaoEReaproveitadoAposCarregar) {
    RepositorioPostgres repo(url_);
    Diario d;
    d.adicionar(livro("A", "X", 1));
    const int removido = d.adicionar(livro("B", "X", 1));
    d.remover(removido);
    repo.salvar(d);

    Diario lido = repo.carregar();

    EXPECT_EQ(lido.adicionar(livro("C", "X", 1)), removido + 1);
}

TEST(RepositorioPostgresConexao, UrlInacessivelLancaErroNaConstrucao) {
    // Porta 1 em localhost: conexão recusada imediatamente, sem esperar timeout.
    EXPECT_THROW(RepositorioPostgres("postgresql://usuario@127.0.0.1:1/banco?connect_timeout=2"),
                 std::runtime_error);
}
