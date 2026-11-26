#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "diario/api.hpp"
#include "repositorio_memoria.hpp"

using diario::Api;
using diario::Requisicao;
using diario::teste::RepositorioMemoria;
using nlohmann::json;

namespace {

Requisicao comCorpo(const json& corpo) { return {{}, corpo.dump()}; }

Requisicao comParametros(std::map<std::string, std::string> parametros) { return {std::move(parametros), ""}; }

json livro(const std::string& titulo, const std::string& autor, int nota) {
    return json{{"titulo", titulo}, {"autor", autor}, {"nota", nota}};
}

std::vector<std::string> titulos(const json& livros) {
    std::vector<std::string> out;
    for (const auto& l : livros) out.push_back(l.at("titulo").get<std::string>());
    return out;
}

// Api sobre um repositório em memória, já com três livros cadastrados pela própria Api.
class ApiComLivros : public ::testing::Test {
protected:
    void SetUp() override {
        api.criarLivro(comCorpo(livro("Dom Casmurro", "Machado de Assis", 5)));
        api.criarLivro(comCorpo(livro("Iracema", "José de Alencar", 2)));
        api.criarLivro(comCorpo(livro("Memórias Póstumas", "Machado de Assis", 4)));
    }

    RepositorioMemoria repo;
    Api api{repo};
};

}  // namespace

// ---- Listagem -----------------------------------------------------------------

TEST_F(ApiComLivros, ListarSemParametrosDevolveTodosNaOrdemDeInsercao) {
    const auto r = api.listarLivros({});

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Iracema", "Memórias Póstumas"}));
}

TEST_F(ApiComLivros, ListarComOrdemNotaDevolveMaiorNotaPrimeiro) {
    const auto r = api.listarLivros(comParametros({{"ordem", "nota"}}));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas", "Iracema"}));
}

TEST_F(ApiComLivros, ListarComBuscaDevolveSoOsLivrosQueCombinam) {
    const auto r = api.listarLivros(comParametros({{"q", "machado"}}));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas"}));
}

TEST_F(ApiComLivros, ListarCombinaBuscaNotaMinimaEOrdenacao) {
    const auto r = api.listarLivros(comParametros({{"q", "machado"}, {"notaMin", "4"}, {"ordem", "nota"}}));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas"}));
}

TEST_F(ApiComLivros, ListarComNotaMinimaExcluiNotasMenores) {
    const auto r = api.listarLivros(comParametros({{"notaMin", "3"}}));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas"}));
}

class NotaMinimaInvalida : public ::testing::TestWithParam<std::string> {};

TEST_P(NotaMinimaInvalida, Responde400) {
    RepositorioMemoria repo;
    Api api(repo);

    const auto r = api.listarLivros(comParametros({{"notaMin", GetParam()}}));

    EXPECT_EQ(r.status, 400);
}

INSTANTIATE_TEST_SUITE_P(Casos, NotaMinimaInvalida, ::testing::Values("x", "12", "-1", "6", "9"));

// ---- Estatísticas -------------------------------------------------------------

TEST_F(ApiComLivros, EstatisticasDevolvemTotalMediaEAutoresMaisLidos) {
    const auto r = api.estatisticas({});

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo.at("total"), 3);
    EXPECT_NEAR(r.corpo.at("media").get<double>(), 11.0 / 3.0, 1e-9);
    EXPECT_EQ(r.corpo.at("autoresMaisLidos").at(0), (json{{"autor", "Machado de Assis"}, {"livros", 2}}));
}

// ---- Criação ------------------------------------------------------------------

TEST(ApiCriar, LivroValidoResponde201ComOIdAtribuido) {
    RepositorioMemoria repo;
    Api api(repo);

    const auto r = api.criarLivro(comCorpo(livro("Vidas Secas", "Graciliano Ramos", 4)));

    EXPECT_EQ(r.status, 201);
    EXPECT_EQ(r.corpo.at("id"), 1);
    EXPECT_EQ(r.corpo.at("titulo"), "Vidas Secas");
}

TEST(ApiCriar, LivroCriadoEhGravadoNoRepositorio) {
    RepositorioMemoria repo;
    Api api(repo);

    api.criarLivro(comCorpo(livro("Vidas Secas", "Graciliano Ramos", 4)));

    EXPECT_EQ(repo.salvo.tamanho(), 1u);
}

TEST(ApiCriar, LivroInvalidoResponde400ComAMensagemDoNucleo) {
    RepositorioMemoria repo;
    Api api(repo);

    const auto r = api.criarLivro(comCorpo(livro("", "Autor", 3)));

    EXPECT_EQ(r.status, 400);
    EXPECT_EQ(r.corpo.at("erro"), "título é obrigatório");
}

class CorpoInvalido : public ::testing::TestWithParam<std::string> {};

TEST_P(CorpoInvalido, Responde400JsonInvalido) {
    RepositorioMemoria repo;
    Api api(repo);

    const auto r = api.criarLivro({{}, GetParam()});

    EXPECT_EQ(r.status, 400);
    EXPECT_EQ(r.corpo.at("erro"), "JSON inválido");
}

INSTANTIATE_TEST_SUITE_P(Casos, CorpoInvalido,
                         ::testing::Values("",                                              // vazio
                                           "{",                                             // incompleto
                                           "[1, 2]",                                        // não é objeto
                                           R"({"titulo":"A","autor":"B","nota":"5"})"));  // nota como texto

TEST(ApiCriar, FalhaAoGravarResponde503ENaoMostraOLivro) {
    RepositorioMemoria repo;
    Api api(repo);
    repo.falharAoSalvar = true;

    const auto r = api.criarLivro(comCorpo(livro("Vidas Secas", "Graciliano Ramos", 4)));

    EXPECT_EQ(r.status, 503);
    EXPECT_TRUE(api.listarLivros({}).corpo.empty());
}

TEST(ApiCriar, IdNaoEhConsumidoQuandoAGravacaoFalha) {
    RepositorioMemoria repo;
    Api api(repo);
    repo.falharAoSalvar = true;
    api.criarLivro(comCorpo(livro("A", "X", 1)));
    repo.falharAoSalvar = false;

    const auto r = api.criarLivro(comCorpo(livro("B", "X", 1)));

    EXPECT_EQ(r.corpo.at("id"), 1);
}

// ---- Atualização --------------------------------------------------------------

TEST_F(ApiComLivros, AtualizarLivroExistenteDevolveOsNovosDados) {
    const auto r = api.atualizarLivro(comCorpo(livro("Dom Casmurro (2a ed.)", "Machado de Assis", 4)), 1);

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo.at("titulo"), "Dom Casmurro (2a ed.)");
    EXPECT_EQ(r.corpo.at("id"), 1);
}

TEST_F(ApiComLivros, AtualizarLivroInexistenteResponde404) {
    const auto r = api.atualizarLivro(comCorpo(livro("Z", "Z", 1)), 999);

    EXPECT_EQ(r.status, 404);
}

TEST_F(ApiComLivros, AtualizarComDadosInvalidosResponde400EMantemOOriginal) {
    const auto r = api.atualizarLivro(comCorpo(livro("Dom Casmurro", "Machado de Assis", 9)), 1);

    EXPECT_EQ(r.status, 400);
    EXPECT_EQ(api.listarLivros({}).corpo.at(0).at("nota"), 5);
}

TEST_F(ApiComLivros, AtualizarComFalhaAoGravarResponde503EMantemOOriginal) {
    repo.falharAoSalvar = true;

    const auto r = api.atualizarLivro(comCorpo(livro("Outro título", "Machado de Assis", 5)), 1);

    EXPECT_EQ(r.status, 503);
    EXPECT_EQ(api.listarLivros({}).corpo.at(0).at("titulo"), "Dom Casmurro");
}

// ---- Remoção ------------------------------------------------------------------

TEST_F(ApiComLivros, RemoverLivroExistenteResponde204SemCorpo) {
    const auto r = api.removerLivro({}, 2);

    EXPECT_EQ(r.status, 204);
    EXPECT_TRUE(r.corpo.is_null());
}

TEST_F(ApiComLivros, LivroRemovidoSaiDaListagem) {
    api.removerLivro({}, 2);

    EXPECT_EQ(titulos(api.listarLivros({}).corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas"}));
}

TEST_F(ApiComLivros, RemoverLivroInexistenteResponde404) {
    EXPECT_EQ(api.removerLivro({}, 999).status, 404);
}

TEST_F(ApiComLivros, RemoverComFalhaAoGravarResponde503EMantemOLivro) {
    repo.falharAoSalvar = true;

    const auto r = api.removerLivro({}, 2);

    EXPECT_EQ(r.status, 503);
    EXPECT_EQ(api.listarLivros({}).corpo.size(), 3u);
}

// ---- Carga inicial ------------------------------------------------------------

TEST(ApiCarga, ComecaComOsLivrosJaSalvosNoRepositorio) {
    RepositorioMemoria repo;
    diario::Livro l;
    l.titulo = "Capitães da Areia";
    l.autor = "Jorge Amado";
    repo.salvo.adicionar(l);

    Api api(repo);

    EXPECT_EQ(titulos(api.listarLivros({}).corpo), (std::vector<std::string>{"Capitães da Areia"}));
}
