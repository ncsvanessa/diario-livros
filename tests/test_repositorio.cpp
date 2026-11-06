#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "diario/repositorio.hpp"

using diario::Diario;
using diario::Livro;
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

TEST_F(RepositorioArquivoTest, CarregarSemArquivoRetornaDiarioVazio) {
    RepositorioArquivo repo(caminho_.string());

    const Diario d = repo.carregar();

    EXPECT_EQ(d.tamanho(), 0u);
}

TEST_F(RepositorioArquivoTest, CarregarDevolveOsLivrosSalvos) {
    RepositorioArquivo repo(caminho_.string());
    Diario original;
    original.adicionar(livro("Vidas Secas", "Graciliano Ramos", 4));

    repo.salvar(original);
    const Diario lido = repo.carregar();

    ASSERT_EQ(lido.tamanho(), 1u);
    EXPECT_EQ(lido.listar()[0].titulo, "Vidas Secas");
}

TEST_F(RepositorioArquivoTest, SalvarEmPastaInexistenteLancaErro) {
    RepositorioArquivo repo((caminho_.parent_path() / "pasta_que_nao_existe_987" / "d.json").string());

    EXPECT_THROW(repo.salvar(Diario{}), std::runtime_error);
}
