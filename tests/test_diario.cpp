#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

std::vector<std::string> titulos(const std::vector<Livro>& livros) {
    std::vector<std::string> out;
    for (const auto& l : livros) out.push_back(l.titulo);
    return out;
}

}  // namespace

// ---- Validação de dados -----------------------------------------------------

class DataReal : public ::testing::TestWithParam<std::string> {};

TEST_P(DataReal, EhAceita) {
    EXPECT_TRUE(diario::dataValida(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(Validacao, DataReal,
                         ::testing::Values("1899-01-01",
                                           "2024-02-29",    // 29/02 em ano bissexto
                                           "2000-02-29",    // divisível por 400: bissexto
                                           "2024-12-31"));  // último dia do ano

class DataInvalida : public ::testing::TestWithParam<std::string> {};

TEST_P(DataInvalida, EhRecusada) {
    EXPECT_FALSE(diario::dataValida(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(Validacao, DataInvalida,
                         ::testing::Values("",             // vazia
                                           "2023-02-29",   // 29/02 em ano não bissexto
                                           "1900-02-29",   // divisível por 100 e não por 400: não bissexto
                                           "2024-13-01",   // mês 13
                                           "2024-00-10",   // mês 0
                                           "2024-04-31",   // abril tem 30 dias
                                           "2024-01-00",   // dia 0
                                           "0000-01-01",   // ano 0
                                           "2024-4-01",    // mês sem zero à esquerda
                                           "01/04/2024",   // formato DD/MM/AAAA
                                           "abcd-ef-gh"));  // não numérica

class LimitesDaNota : public ::testing::TestWithParam<std::pair<int, bool>> {};

TEST_P(LimitesDaNota, SoAceitaDeZeroACinco) {
    const auto [nota, aceita] = GetParam();

    EXPECT_EQ(diario::notaValida(nota), aceita);
}

INSTANTIATE_TEST_SUITE_P(Validacao, LimitesDaNota,
                         ::testing::Values(std::make_pair(-1, false), std::make_pair(0, true),
                                           std::make_pair(5, true), std::make_pair(6, false)));

const std::vector<std::pair<std::string, bool>> kCapas = {
    {"", true},                                // sem capa
    {"https://exemplo.com/capa.jpg", true},    // URL https
    {"http://exemplo.com/capa.jpg", true},     // URL http
    {"data:image/png;base64,AAAA", true},      // imagem enviada como arquivo
    {"javascript:alert(1)", false},            // script
    {"data:text/html,<b>oi</b>", false},       // data: que não é imagem
    {"capa.jpg", false},                       // caminho relativo
};

class CapaDoLivro : public ::testing::TestWithParam<std::pair<std::string, bool>> {};

TEST_P(CapaDoLivro, SoAceitaVaziaUrlHttpOuImagemEmbutida) {
    const auto& [url, aceita] = GetParam();

    EXPECT_EQ(diario::capaValida(url), aceita);
}

INSTANTIATE_TEST_SUITE_P(Validacao, CapaDoLivro, ::testing::ValuesIn(kCapas));

TEST(Validacao, LivroComTituloEAutorNaoTemErros) {
    EXPECT_TRUE(diario::validar(livro("Dom Casmurro", "Machado de Assis", 5)).empty());
}

TEST(Validacao, TituloSoComEspacosGeraErroDeTituloObrigatorio) {
    const auto erros = diario::validar(livro("   ", "Autor", 3));

    EXPECT_EQ(erros, (std::vector<std::string>{"título é obrigatório"}));
}

TEST(Validacao, AutorVazioGeraErroDeAutorObrigatorio) {
    const auto erros = diario::validar(livro("Título", "", 3));

    EXPECT_EQ(erros, (std::vector<std::string>{"autor é obrigatório"}));
}

TEST(Validacao, DataDeLancamentoMalFormadaGeraErro) {
    Livro l = livro("Torto Arado", "Itamar Vieira Junior", 5);
    l.dataLancamento = "01/08/2019";

    EXPECT_EQ(diario::validar(l), (std::vector<std::string>{"data de lançamento inválida (use AAAA-MM-DD)"}));
}

TEST(Validacao, DataDeLeituraMalFormadaGeraErro) {
    Livro l = livro("Torto Arado", "Itamar Vieira Junior", 5);
    l.dataLeitura = "2024-02-30";

    EXPECT_EQ(diario::validar(l), (std::vector<std::string>{"data de leitura inválida (use AAAA-MM-DD)"}));
}

TEST(Validacao, LeituraAntesDoLancamentoGeraErro) {
    Livro l = livro("Torto Arado", "Itamar Vieira Junior", 5);
    l.dataLancamento = "2019-08-01";
    l.dataLeitura = "2018-01-01";

    EXPECT_EQ(diario::validar(l),
              (std::vector<std::string>{"data de leitura não pode ser anterior à de lançamento"}));
}

TEST(Validacao, LeituraNoDiaDoLancamentoEhAceita) {
    Livro l = livro("Torto Arado", "Itamar Vieira Junior", 5);
    l.dataLancamento = "2019-08-01";
    l.dataLeitura = "2019-08-01";

    EXPECT_TRUE(diario::validar(l).empty());
}

// ---- Operações do diário ----------------------------------------------------

TEST(Diario, AdicionarAtribuiIdsSequenciaisAPartirDeUm) {
    Diario d;

    const int primeiro = d.adicionar(livro("A", "X", 1));
    const int segundo = d.adicionar(livro("B", "Y", 2));

    EXPECT_EQ((std::vector<int>{primeiro, segundo}), (std::vector<int>{1, 2}));
}

TEST(Diario, AdicionarLivroInvalidoLancaExcecaoENaoAltera) {
    Diario d;
    EXPECT_THROW(d.adicionar(livro("A", "X", 7)), std::invalid_argument);
    EXPECT_EQ(d.tamanho(), 0u);
}

TEST(Diario, RemoverLivroExistenteTiraEleDoDiario) {
    Diario d;
    const int id = d.adicionar(livro("A", "X", 1));

    const bool removeu = d.remover(id);

    EXPECT_TRUE(removeu);
    EXPECT_FALSE(d.buscar(id).has_value());
}

TEST(Diario, RemoverLivroJaRemovidoRetornaFalse) {
    Diario d;
    const int id = d.adicionar(livro("A", "X", 1));
    d.remover(id);

    EXPECT_FALSE(d.remover(id));
}

TEST(Diario, IdDeLivroRemovidoNaoEhReaproveitado) {
    Diario d;
    d.remover(d.adicionar(livro("A", "X", 1)));

    const int novo = d.adicionar(livro("B", "Y", 2));

    EXPECT_EQ(novo, 2);
}

TEST(Diario, AtualizarSubstituiOsDadosDoLivro) {
    Diario d;
    const int id = d.adicionar(livro("A", "X", 1));

    const bool atualizou = d.atualizar(id, livro("A (2a ed.)", "X", 4));

    EXPECT_TRUE(atualizou);
    EXPECT_EQ(d.buscar(id)->titulo, "A (2a ed.)");
}

TEST(Diario, AtualizarMantemOIdOriginal) {
    Diario d;
    const int id = d.adicionar(livro("A", "X", 1));
    Livro editado = livro("A (2a ed.)", "X", 4);
    editado.id = 999;  // um id diferente vindo do cliente é ignorado

    d.atualizar(id, editado);

    EXPECT_EQ(d.buscar(id)->id, id);
}

TEST(Diario, AtualizarIdInexistenteRetornaFalse) {
    Diario d;
    d.adicionar(livro("A", "X", 1));

    EXPECT_FALSE(d.atualizar(999, livro("Z", "Z", 1)));
}

TEST(Diario, AtualizarComDadosInvalidosLancaExcecaoEMantemOLivro) {
    Diario d;
    const int id = d.adicionar(livro("A", "X", 4));

    EXPECT_THROW(d.atualizar(id, livro("", "X", 1)), std::invalid_argument);
    EXPECT_EQ(d.buscar(id)->nota, 4);
}

TEST(Diario, AtualizarMantemQuandoOLivroFoiRegistrado) {
    Diario d;
    Livro original = livro("A", "X", 1);
    original.registradoEm = "2026-11-01T10:00:00Z";
    const int id = d.adicionar(original);

    Livro editado = livro("A", "X", 5);
    editado.registradoEm = "2030-01-01T00:00:00Z";
    d.atualizar(id, editado);

    EXPECT_EQ(d.buscar(id)->registradoEm, "2026-11-01T10:00:00Z");
}

TEST(Persistencia, JsonPreservaQuandoOLivroFoiRegistrado) {
    Diario d;
    Livro l = livro("A", "X", 1);
    l.registradoEm = "2026-11-01T10:00:00Z";
    d.adicionar(l);

    const auto copia = Diario::deJson(d.paraJson());

    EXPECT_EQ(copia.listar().at(0).registradoEm, "2026-11-01T10:00:00Z");
}

TEST(Persistencia, LivroAntigoSemRegistroCarregaComCampoVazio) {
    const auto d = Diario::deJson(
        nlohmann::json{{"proximoId", 2}, {"livros", {{{"id", 1}, {"titulo", "A"}, {"autor", "X"}, {"nota", 3}}}}});

    EXPECT_EQ(d.listar().at(0).registradoEm, "");
}

TEST(Diario, MediaDeDiarioVazioEhZero) {
    EXPECT_DOUBLE_EQ(Diario{}.mediaNotas(), 0.0);
}

TEST(Diario, MediaEhAMediaAritmeticaDasNotas) {
    Diario d;
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

    const auto ordenados = d.ordenadosPorNota();

    EXPECT_EQ(titulos(ordenados), (std::vector<std::string>{"B", "A", "C"}));
}

// Diário com dois livros, de autores diferentes, para os testes de pesquisa.
class Pesquisa : public ::testing::Test {
protected:
    void SetUp() override {
        diario.adicionar(livro("Dom Casmurro", "Machado de Assis", 5));
        diario.adicionar(livro("Grande Sertão: Veredas", "Guimarães Rosa", 4));
    }

    Diario diario;
};

TEST_F(Pesquisa, EncontraPeloTituloIgnorandoMaiusculas) {
    EXPECT_EQ(titulos(diario.pesquisar("casmurro")), (std::vector<std::string>{"Dom Casmurro"}));
}

TEST_F(Pesquisa, EncontraPeloAutorIgnorandoMaiusculas) {
    EXPECT_EQ(titulos(diario.pesquisar("MACHADO")), (std::vector<std::string>{"Dom Casmurro"}));
}

TEST_F(Pesquisa, TrechoPresenteEmVariosLivrosDevolveTodosNaOrdemDeInsercao) {
    EXPECT_EQ(titulos(diario.pesquisar("o")),
              (std::vector<std::string>{"Dom Casmurro", "Grande Sertão: Veredas"}));
}

TEST_F(Pesquisa, TermoVazioDevolveTodosOsLivros) {
    EXPECT_EQ(diario.pesquisar("").size(), 2u);
}

TEST_F(Pesquisa, TermoSemCorrespondenciaDevolveListaVazia) {
    EXPECT_TRUE(diario.pesquisar("tolkien").empty());
}

// ---- Filtro por nota mínima -------------------------------------------------

namespace {

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

class NotaMinimaForaDoIntervalo : public ::testing::TestWithParam<int> {};

TEST_P(NotaMinimaForaDoIntervalo, LancaExcecao) {
    const auto d = diarioComNotas();

    EXPECT_THROW(d.comNotaMinima(GetParam()), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(FiltroNota, NotaMinimaForaDoIntervalo, ::testing::Values(-1, 6));

// ---- Autores mais lidos -----------------------------------------------------

namespace {

// Resultado de autoresMaisLidos como pares (autor, livros), para comparar de uma vez.
std::vector<std::pair<std::string, int>> resumo(const std::vector<diario::AutorLido>& autores) {
    std::vector<std::pair<std::string, int>> out;
    for (const auto& a : autores) out.emplace_back(a.autor, a.livros);
    return out;
}

Diario diarioComQuatroAutores() {
    Diario d;
    d.adicionar(livro("Livro 1", "Autora A", 3));
    d.adicionar(livro("Livro 2", "Autora B", 3));
    d.adicionar(livro("Livro 3", "Autora C", 3));
    d.adicionar(livro("Livro 4", "Autora D", 3));
    return d;
}

}  // namespace

TEST(AutoresMaisLidos, DiarioVazioRetornaListaVazia) {
    EXPECT_TRUE(Diario{}.autoresMaisLidos().empty());
}

TEST(AutoresMaisLidos, OrdenaPelaQuantidadeDeLivros) {
    Diario d;
    d.adicionar(livro("A", "Jorge Amado", 3));
    d.adicionar(livro("B", "Machado de Assis", 3));
    d.adicionar(livro("C", "Machado de Assis", 3));

    const auto autores = d.autoresMaisLidos();

    EXPECT_EQ(resumo(autores),
              (std::vector<std::pair<std::string, int>>{{"Machado de Assis", 2}, {"Jorge Amado", 1}}));
}

TEST(AutoresMaisLidos, SemLimiteDevolveNoMaximoTres) {
    EXPECT_EQ(diarioComQuatroAutores().autoresMaisLidos().size(), 3u);
}

TEST(AutoresMaisLidos, RespeitaOLimitePedido) {
    EXPECT_EQ(diarioComQuatroAutores().autoresMaisLidos(1).size(), 1u);
}

TEST(AutoresMaisLidos, EmpateDesempataPeloNome) {
    Diario d;
    d.adicionar(livro("A", "Rachel de Queiroz", 3));
    d.adicionar(livro("B", "clarice Lispector", 3));
    d.adicionar(livro("C", "Graciliano Ramos", 3));

    const auto autores = d.autoresMaisLidos();

    // Ordem alfabética sem diferenciar maiúsculas: "clarice" vem antes de "Graciliano".
    EXPECT_EQ(resumo(autores), (std::vector<std::pair<std::string, int>>{
                                   {"clarice Lispector", 1}, {"Graciliano Ramos", 1}, {"Rachel de Queiroz", 1}}));
}

TEST(AutoresMaisLidos, AgrupaMesmoAutorComMaiusculasEEspacosDiferentes) {
    Diario d;
    d.adicionar(livro("A", "Machado de Assis", 3));
    d.adicionar(livro("B", "MACHADO DE ASSIS", 3));
    d.adicionar(livro("C", "  machado de assis ", 3));

    const auto autores = d.autoresMaisLidos();

    // Um só autor, exibido como no primeiro registro.
    EXPECT_EQ(resumo(autores), (std::vector<std::pair<std::string, int>>{{"Machado de Assis", 3}}));
}

// ---- Persistência -----------------------------------------------------------

// Arquivo temporário exclusivo de cada teste, removido ao final.
class PersistenciaEmArquivo : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        caminho = (std::filesystem::temp_directory_path() / (std::string("diario_") + info->name() + ".json"))
                      .string();
        std::filesystem::remove(caminho);
    }
    void TearDown() override { std::filesystem::remove(caminho); }

    std::string caminho;
};

TEST_F(PersistenciaEmArquivo, CarregarDevolveOLivroComTodosOsCampos) {
    Diario original;
    Livro l = livro("Vidas Secas", "Graciliano Ramos", 4);
    l.dataLancamento = "1938-01-01";
    l.dataLeitura = "2026-10-20";
    l.capaUrl = "https://exemplo.com/vidas-secas.jpg";
    l.registradoEm = "2026-11-01T10:00:00Z";
    original.adicionar(l);
    original.salvar(caminho);

    const Diario lido = Diario::carregar(caminho);

    // Compara o livro inteiro (todos os campos) pela forma serializada.
    EXPECT_EQ(nlohmann::json(lido.listar()), nlohmann::json(original.listar()));
}

TEST_F(PersistenciaEmArquivo, IdRemovidoNaoEhReaproveitadoDepoisDeCarregar) {
    Diario original;
    original.adicionar(livro("A", "X", 4));
    original.remover(original.adicionar(livro("Removido", "X", 1)));  // id 2 usado e removido
    original.salvar(caminho);

    Diario lido = Diario::carregar(caminho);

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
