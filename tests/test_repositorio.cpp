#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "diario/repositorio.hpp"

using diario::Diario;
using diario::Livro;
using diario::LoginEmUso;
using diario::RepositorioArquivo;

namespace {

Livro livro(const std::string& titulo, const std::string& autor, int nota) {
    Livro l;
    l.titulo = titulo;
    l.autor = autor;
    l.nota = nota;
    return l;
}

// Arquivo temporário exclusivo de cada teste, removido ao final.
class RepositorioArquivoTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        caminho_ = std::filesystem::temp_directory_path() /
                   (std::string("diario_repo_") + info->name() + ".json");
        std::filesystem::remove(caminho_);
    }
    void TearDown() override { std::filesystem::remove(caminho_); }

    std::filesystem::path caminho_;
};

}  // namespace

// ---- Contas -------------------------------------------------------------------

TEST_F(RepositorioArquivoTest, UsuarioCriadoPodeSerBuscadoPeloLogin) {
    RepositorioArquivo repo(caminho_.string());

    const int id = repo.criarUsuario("ana", "hash-da-ana");
    const auto usuario = repo.buscarUsuarioPorLogin("ana");

    ASSERT_TRUE(usuario.has_value());
    EXPECT_EQ(usuario->id, id);
    EXPECT_EQ(usuario->senhaHash, "hash-da-ana");
}

TEST_F(RepositorioArquivoTest, UsuarioCriadoPodeSerBuscadoPeloId) {
    RepositorioArquivo repo(caminho_.string());

    const int id = repo.criarUsuario("ana", "hash-da-ana");

    EXPECT_EQ(repo.buscarUsuarioPorId(id)->login, "ana");
}

TEST_F(RepositorioArquivoTest, BuscarLoginInexistenteDevolveVazio) {
    RepositorioArquivo repo(caminho_.string());
    repo.criarUsuario("ana", "hash");

    EXPECT_FALSE(repo.buscarUsuarioPorLogin("bia").has_value());
}

TEST_F(RepositorioArquivoTest, BuscarIdInexistenteDevolveVazio) {
    RepositorioArquivo repo(caminho_.string());
    repo.criarUsuario("ana", "hash");

    EXPECT_FALSE(repo.buscarUsuarioPorId(999).has_value());
}

TEST_F(RepositorioArquivoTest, CriarLoginRepetidoLancaLoginEmUso) {
    RepositorioArquivo repo(caminho_.string());
    repo.criarUsuario("ana", "hash");

    EXPECT_THROW(repo.criarUsuario("ana", "outro-hash"), LoginEmUso);
}

TEST_F(RepositorioArquivoTest, UsuariosRecebemIdsDiferentes) {
    RepositorioArquivo repo(caminho_.string());

    const int ana = repo.criarUsuario("ana", "hash");
    const int bia = repo.criarUsuario("bia", "hash");

    EXPECT_NE(ana, bia);
}

// ---- Diários ------------------------------------------------------------------

TEST_F(RepositorioArquivoTest, DiarioDeUsuarioSemLivrosEhVazio) {
    RepositorioArquivo repo(caminho_.string());
    const int id = repo.criarUsuario("ana", "hash");

    EXPECT_EQ(repo.carregarDiario(id).tamanho(), 0u);
}

TEST_F(RepositorioArquivoTest, CarregarDevolveOsLivrosSalvosDoUsuario) {
    RepositorioArquivo repo(caminho_.string());
    const int id = repo.criarUsuario("ana", "hash");
    Diario original;
    original.adicionar(livro("Vidas Secas", "Graciliano Ramos", 4));

    repo.salvarDiario(id, original);
    const Diario lido = repo.carregarDiario(id);

    ASSERT_EQ(lido.tamanho(), 1u);
    EXPECT_EQ(lido.listar()[0].titulo, "Vidas Secas");
}

TEST_F(RepositorioArquivoTest, DiariosDeUsuariosDiferentesSaoIndependentes) {
    RepositorioArquivo repo(caminho_.string());
    const int ana = repo.criarUsuario("ana", "hash");
    const int bia = repo.criarUsuario("bia", "hash");
    Diario daAna;
    daAna.adicionar(livro("Vidas Secas", "Graciliano Ramos", 4));

    repo.salvarDiario(ana, daAna);

    EXPECT_EQ(repo.carregarDiario(bia).tamanho(), 0u);
}

TEST_F(RepositorioArquivoTest, DadosContinuamDisponiveisParaUmaNovaInstancia) {
    // Simula o servidor reiniciando: outra instância lendo o mesmo arquivo.
    int id = 0;
    {
        RepositorioArquivo repo(caminho_.string());
        id = repo.criarUsuario("ana", "hash");
        Diario d;
        d.adicionar(livro("Vidas Secas", "Graciliano Ramos", 4));
        repo.salvarDiario(id, d);
    }

    RepositorioArquivo reaberto(caminho_.string());

    EXPECT_EQ(reaberto.buscarUsuarioPorLogin("ana")->id, id);
    EXPECT_EQ(reaberto.carregarDiario(id).tamanho(), 1u);
}

// ---- Erros --------------------------------------------------------------------

TEST_F(RepositorioArquivoTest, ArquivoNoFormatoAntigoLancaErroExplicativo) {
    std::ofstream(caminho_) << R"({"proximoId": 2, "livros": []})";
    RepositorioArquivo repo(caminho_.string());

    EXPECT_THROW(repo.buscarUsuarioPorLogin("ana"), std::runtime_error);
}

TEST_F(RepositorioArquivoTest, ArquivoCorrompidoLancaErroDeArmazenamento) {
    std::ofstream(caminho_) << "{ isto não é json";
    RepositorioArquivo repo(caminho_.string());

    EXPECT_THROW(repo.buscarUsuarioPorLogin("ana"), std::runtime_error);
}

TEST_F(RepositorioArquivoTest, SalvarEmPastaInexistenteLancaErro) {
    RepositorioArquivo repo((caminho_.parent_path() / "pasta_que_nao_existe_987" / "d.json").string());

    EXPECT_THROW(repo.criarUsuario("ana", "hash"), std::runtime_error);
}
