#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "diario/diario.hpp"

using diario::Diario;
using diario::Livro;

namespace {

Livro livro(const std::string& titulo, const std::string& autor, int nota) {
    Livro l;
    l.titulo = titulo;
    l.autor = autor;
    l.nota = nota;
    return l;
}

}  // namespace

// ---- Validação de dados -----------------------------------------------------

TEST(Validacao, AceitaDatasReais) {
    EXPECT_TRUE(diario::dataValida("1899-01-01"));
    EXPECT_TRUE(diario::dataValida("2024-02-29"));  // bissexto
    EXPECT_TRUE(diario::dataValida("2000-02-29"));  // divisível por 400
}

TEST(Validacao, RejeitaDatasInvalidas) {
    EXPECT_FALSE(diario::dataValida(""));
    EXPECT_FALSE(diario::dataValida("2023-02-29"));  // não bissexto
    EXPECT_FALSE(diario::dataValida("1900-02-29"));  // divisível por 100, não por 400
    EXPECT_FALSE(diario::dataValida("2024-13-01"));
    EXPECT_FALSE(diario::dataValida("2024-04-31"));
    EXPECT_FALSE(diario::dataValida("2024-4-01"));
    EXPECT_FALSE(diario::dataValida("01/04/2024"));
    EXPECT_FALSE(diario::dataValida("abcd-ef-gh"));
}

TEST(Validacao, NotaEntreZeroECinco) {
    EXPECT_TRUE(diario::notaValida(0));
    EXPECT_TRUE(diario::notaValida(5));
    EXPECT_FALSE(diario::notaValida(-1));
    EXPECT_FALSE(diario::notaValida(6));
}

TEST(Validacao, CapaPrecisaSerUrlOuImagemEmbutida) {
    EXPECT_TRUE(diario::capaValida(""));
    EXPECT_TRUE(diario::capaValida("https://exemplo.com/capa.jpg"));
    EXPECT_TRUE(diario::capaValida("data:image/png;base64,AAAA"));
    EXPECT_FALSE(diario::capaValida("javascript:alert(1)"));
    EXPECT_FALSE(diario::capaValida("capa.jpg"));
}

TEST(Validacao, ExigeTituloEAutorNaoVazios) {
    EXPECT_EQ(diario::validar(livro("   ", "Autor", 3)).size(), 1u);
    EXPECT_EQ(diario::validar(livro("Título", "", 3)).size(), 1u);
    EXPECT_TRUE(diario::validar(livro("Dom Casmurro", "Machado de Assis", 5)).empty());
}

TEST(Validacao, LeituraNaoPodeSerAntesDoLancamento) {
    Livro l = livro("Torto Arado", "Itamar Vieira Junior", 5);
    l.dataLancamento = "2019-08-01";
    l.dataLeitura = "2018-01-01";
    EXPECT_EQ(diario::validar(l).size(), 1u);
    l.dataLeitura = "2019-08-01";
    EXPECT_TRUE(diario::validar(l).empty());
}

// ---- Operações do diário ----------------------------------------------------

TEST(Diario, AdicionarAtribuiIdsSequenciais) {
    Diario d;
    EXPECT_EQ(d.adicionar(livro("A", "X", 1)), 1);
    EXPECT_EQ(d.adicionar(livro("B", "Y", 2)), 2);
    EXPECT_EQ(d.tamanho(), 2u);
}

TEST(Diario, AdicionarLivroInvalidoLancaExcecaoENaoAltera) {
    Diario d;
    EXPECT_THROW(d.adicionar(livro("A", "X", 7)), std::invalid_argument);
    EXPECT_EQ(d.tamanho(), 0u);
}

TEST(Diario, RemoverNaoReutilizaId) {
    Diario d;
    const int id = d.adicionar(livro("A", "X", 1));
    EXPECT_TRUE(d.remover(id));
    EXPECT_FALSE(d.remover(id));
    EXPECT_EQ(d.adicionar(livro("B", "Y", 2)), 2);
}

TEST(Diario, AtualizarPreservaIdEValida) {
    Diario d;
    const int id = d.adicionar(livro("A", "X", 1));
    EXPECT_TRUE(d.atualizar(id, livro("A (2a ed.)", "X", 4)));
    EXPECT_EQ(d.buscar(id)->titulo, "A (2a ed.)");
    EXPECT_EQ(d.buscar(id)->id, id);
    EXPECT_FALSE(d.atualizar(999, livro("Z", "Z", 1)));
    EXPECT_THROW(d.atualizar(id, livro("", "X", 1)), std::invalid_argument);
    EXPECT_EQ(d.buscar(id)->nota, 4);  // falha não altera o registro
}

TEST(Diario, MediaDasNotas) {
    Diario d;
    EXPECT_DOUBLE_EQ(d.mediaNotas(), 0.0);
    d.adicionar(livro("A", "X", 5));
    d.adicionar(livro("B", "X", 2));
    d.adicionar(livro("C", "X", 0));
    EXPECT_NEAR(d.mediaNotas(), 7.0 / 3.0, 1e-9);
}

TEST(Diario, OrdenaPorNotaDecrescenteEDepoisPorTitulo) {
    Diario d;
    d.adicionar(livro("C", "X", 3));
    d.adicionar(livro("B", "X", 5));
    d.adicionar(livro("A", "X", 3));
    const auto ord = d.ordenadosPorNota();
    ASSERT_EQ(ord.size(), 3u);
    EXPECT_EQ(ord[0].titulo, "B");
    EXPECT_EQ(ord[1].titulo, "A");
    EXPECT_EQ(ord[2].titulo, "C");
}

TEST(Diario, PesquisaIgnoraMaiusculasEBuscaEmTituloEAutor) {
    Diario d;
    d.adicionar(livro("Dom Casmurro", "Machado de Assis", 5));
    d.adicionar(livro("Grande Sertão: Veredas", "Guimarães Rosa", 4));
    EXPECT_EQ(d.pesquisar("casmurro").size(), 1u);
    EXPECT_EQ(d.pesquisar("MACHADO").size(), 1u);
    EXPECT_EQ(d.pesquisar("o").size(), 2u);
    EXPECT_EQ(d.pesquisar("").size(), 2u);
    EXPECT_TRUE(d.pesquisar("tolkien").empty());
}

// ---- Filtro por nota mínima -------------------------------------------------

namespace {

std::vector<std::string> titulos(const std::vector<Livro>& livros) {
    std::vector<std::string> out;
    for (const auto& l : livros) out.push_back(l.titulo);
    return out;
}

Diario diarioComNotas() {
    Diario d;
    d.adicionar(livro("Nota 3", "X", 3));
    d.adicionar(livro("Nota 5", "X", 5));
    d.adicionar(livro("Nota 0", "X", 0));
    d.adicionar(livro("Nota 4", "X", 4));
    return d;
}

}  // namespace

TEST(FiltroNota, MantemLivrosComNotaIgualOuMaiorNaOrdemDeInsercao) {
    const auto d = diarioComNotas();
    EXPECT_EQ(titulos(d.comNotaMinima(4)), (std::vector<std::string>{"Nota 5", "Nota 4"}));
}

TEST(FiltroNota, NotaMinimaZeroRetornaTodos) {
    EXPECT_EQ(diarioComNotas().comNotaMinima(0).size(), 4u);
}

TEST(FiltroNota, NotaMinimaCincoRetornaSoNotaMaxima) {
    EXPECT_EQ(titulos(diarioComNotas().comNotaMinima(5)), (std::vector<std::string>{"Nota 5"}));
}

TEST(FiltroNota, SemLivrosNaFaixaRetornaListaVazia) {
    Diario d;
    d.adicionar(livro("A", "X", 2));
    EXPECT_TRUE(d.comNotaMinima(3).empty());
}

TEST(FiltroNota, NotaMinimaForaDoIntervaloLancaExcecao) {
    const auto d = diarioComNotas();
    EXPECT_THROW(d.comNotaMinima(-1), std::invalid_argument);
    EXPECT_THROW(d.comNotaMinima(6), std::invalid_argument);
}

// ---- Autores mais lidos -----------------------------------------------------

TEST(AutoresMaisLidos, DiarioVazioRetornaListaVazia) {
    EXPECT_TRUE(Diario{}.autoresMaisLidos().empty());
}

TEST(AutoresMaisLidos, OrdenaPelaQuantidadeDeLivros) {
    Diario d;
    d.adicionar(livro("A", "Jorge Amado", 3));
    d.adicionar(livro("B", "Machado de Assis", 3));
    d.adicionar(livro("C", "Machado de Assis", 3));

    const auto autores = d.autoresMaisLidos();
    ASSERT_EQ(autores.size(), 2u);
    EXPECT_EQ(autores[0].autor, "Machado de Assis");
    EXPECT_EQ(autores[0].livros, 2);
    EXPECT_EQ(autores[1].autor, "Jorge Amado");
    EXPECT_EQ(autores[1].livros, 1);
}

TEST(AutoresMaisLidos, LimitaAQuantidadePedida) {
    Diario d;
    for (const auto* autor : {"A", "B", "C", "D"}) d.adicionar(livro("Livro", autor, 3));
    EXPECT_EQ(d.autoresMaisLidos().size(), 3u);  // padrão: top 3
    EXPECT_EQ(d.autoresMaisLidos(1).size(), 1u);
}

TEST(AutoresMaisLidos, EmpateDesempataPeloNome) {
    Diario d;
    d.adicionar(livro("A", "Rachel de Queiroz", 3));
    d.adicionar(livro("B", "clarice Lispector", 3));
    d.adicionar(livro("C", "Graciliano Ramos", 3));

    const auto autores = d.autoresMaisLidos();
    ASSERT_EQ(autores.size(), 3u);
    // Ordem alfabética sem diferenciar maiúsculas: "clarice" vem antes de "Graciliano".
    EXPECT_EQ(autores[0].autor, "clarice Lispector");
    EXPECT_EQ(autores[1].autor, "Graciliano Ramos");
    EXPECT_EQ(autores[2].autor, "Rachel de Queiroz");
}

TEST(AutoresMaisLidos, AgrupaMesmoAutorComMaiusculasEEspacosDiferentes) {
    Diario d;
    d.adicionar(livro("A", "Machado de Assis", 3));
    d.adicionar(livro("B", "MACHADO DE ASSIS", 3));
    d.adicionar(livro("C", "  machado de assis ", 3));

    const auto autores = d.autoresMaisLidos();
    ASSERT_EQ(autores.size(), 1u);
    EXPECT_EQ(autores[0].autor, "Machado de Assis");  // nome do primeiro registro
    EXPECT_EQ(autores[0].livros, 3);
}

// ---- Persistência -----------------------------------------------------------

TEST(Persistencia, SalvarECarregarPreservaDadosEContadorDeId) {
    const auto caminho =
        (std::filesystem::temp_directory_path() / "diario_teste_persistencia.json").string();

    Diario original;
    Livro l = livro("Vidas Secas", "Graciliano Ramos", 4);
    l.dataLancamento = "1938-01-01";
    l.dataLeitura = "2026-10-20";
    l.capaUrl = "https://exemplo.com/vidas-secas.jpg";
    original.adicionar(l);
    const int removido = original.adicionar(livro("Temp", "Temp", 1));
    original.remover(removido);
    original.salvar(caminho);

    Diario lido = Diario::carregar(caminho);
    std::remove(caminho.c_str());

    ASSERT_EQ(lido.tamanho(), 1u);
    EXPECT_EQ(lido.listar()[0].titulo, "Vidas Secas");
    EXPECT_EQ(lido.listar()[0].dataLeitura, "2026-10-20");
    EXPECT_EQ(lido.listar()[0].capaUrl, "https://exemplo.com/vidas-secas.jpg");
    // O id 2 já foi usado (e removido); não pode ser reaproveitado.
    EXPECT_EQ(lido.adicionar(livro("Novo", "N", 3)), 3);
}

TEST(Persistencia, CarregarArquivoInexistenteRetornaDiarioVazio) {
    EXPECT_EQ(Diario::carregar("arquivo_que_nao_existe_12345.json").tamanho(), 0u);
}

TEST(Persistencia, JsonComIdsAltosAjustaProximoId) {
    // Arquivo editado à mão: proximoId desatualizado.
    const auto j = nlohmann::json::parse(R"({
        "proximoId": 1,
        "livros": [{"id": 10, "titulo": "A", "autor": "B", "nota": 3}]
    })");
    Diario d = Diario::deJson(j);
    EXPECT_EQ(d.adicionar(livro("C", "D", 2)), 11);
}

TEST(Persistencia, JsonComLivroInvalidoELancado) {
    const auto j = nlohmann::json::parse(R"({"livros": [{"id": 1, "titulo": "A", "autor": "B", "nota": 9}]})");
    EXPECT_THROW(Diario::deJson(j), std::invalid_argument);
}
