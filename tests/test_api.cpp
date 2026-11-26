#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "diario/api.hpp"
#include "repositorio_memoria.hpp"

using diario::Api;
using diario::OpcoesApi;
using diario::Requisicao;
using diario::Resposta;
using diario::teste::RepositorioMemoria;
using nlohmann::json;
using namespace std::chrono_literals;

namespace {

const std::string kSegredo = "segredo-de-teste-com-mais-de-32-caracteres";

json livro(const std::string& titulo, const std::string& autor, int nota) {
    return json{{"titulo", titulo}, {"autor", autor}, {"nota", nota}};
}

json credenciais(const std::string& login, const std::string& senha) {
    return json{{"login", login}, {"senha", senha}};
}

std::vector<std::string> titulos(const json& livros) {
    std::vector<std::string> out;
    for (const auto& l : livros) out.push_back(l.at("titulo").get<std::string>());
    return out;
}

// "sessao=TOKEN; Path=/; ..." (Set-Cookie) -> "sessao=TOKEN" (como o navegador devolve em Cookie).
std::string cookieDe(const Resposta& r) {
    const auto& setCookie = r.cookies.at(0);
    return setCookie.substr(0, setCookie.find(';'));
}

// Api sobre um repositório em memória, com relógio controlado pelo teste.
class ApiTest : public ::testing::Test {
protected:
    ApiTest() : api(repo, OpcoesApi{kSegredo, false, [this] { return agora; }}) {}

    // Cadastra um usuário e devolve o cookie da sessão dele.
    std::string entrarComo(const std::string& login) {
        const auto r = api.cadastrar({{}, credenciais(login, "senha-de-teste").dump(), ""});
        return cookieDe(r);
    }

    Requisicao req(const std::string& cookie, const json& corpo = nullptr,
                   std::map<std::string, std::string> parametros = {}) {
        return {std::move(parametros), corpo.is_null() ? "" : corpo.dump(), cookie};
    }

    RepositorioMemoria repo;
    diario::AssinadorSessao::Instante agora{std::chrono::seconds{1'800'000'000}};
    Api api;
};

// Usuária "ana" logada, com três livros cadastrados pela própria Api.
class ApiComLivros : public ApiTest {
protected:
    void SetUp() override {
        ana = entrarComo("ana");
        api.criarLivro(req(ana, livro("Dom Casmurro", "Machado de Assis", 5)));
        api.criarLivro(req(ana, livro("Iracema", "José de Alencar", 2)));
        api.criarLivro(req(ana, livro("Memórias Póstumas", "Machado de Assis", 4)));
    }

    std::string ana;
};

}  // namespace

// ---- Cadastro -----------------------------------------------------------------

TEST_F(ApiTest, CadastroValidoResponde201ComOLogin) {
    const auto r = api.cadastrar(req("", credenciais("ana", "senha-segura")));

    EXPECT_EQ(r.status, 201);
    EXPECT_EQ(r.corpo.at("login"), "ana");
}

TEST_F(ApiTest, CadastroJaDeixaOUsuarioLogado) {
    const auto r = api.cadastrar(req("", credenciais("ana", "senha-segura")));

    EXPECT_EQ(api.usuarioAtual(req(cookieDe(r))).corpo.at("login"), "ana");
}

TEST_F(ApiTest, CadastroNormalizaOLogin) {
    const auto r = api.cadastrar(req("", credenciais("  Ana ", "senha-segura")));

    EXPECT_EQ(r.corpo.at("login"), "ana");
}

TEST_F(ApiTest, CadastroNaoGuardaASenhaEmTextoPuro) {
    api.cadastrar(req("", credenciais("ana", "senha-segura")));

    EXPECT_EQ(repo.usuarios.at(1).senhaHash.find("senha-segura"), std::string::npos);
}

TEST_F(ApiTest, CadastroComLoginEmUsoResponde409) {
    api.cadastrar(req("", credenciais("ana", "senha-segura")));

    const auto r = api.cadastrar(req("", credenciais("ANA", "outra-senha-123")));

    EXPECT_EQ(r.status, 409);
}

TEST_F(ApiTest, CadastroComSenhaCurtaResponde400SemCriarConta) {
    const auto r = api.cadastrar(req("", credenciais("ana", "curta")));

    EXPECT_EQ(r.status, 400);
    EXPECT_TRUE(repo.usuarios.empty());
}

TEST_F(ApiTest, CadastroComArmazenamentoIndisponivelResponde503) {
    repo.indisponivel = true;

    const auto r = api.cadastrar(req("", credenciais("ana", "senha-segura")));

    EXPECT_EQ(r.status, 503);
}

class CorpoDeCredenciaisInvalido : public ApiTest, public ::testing::WithParamInterface<std::string> {};

TEST_P(CorpoDeCredenciaisInvalido, CadastroResponde400JsonInvalido) {
    const auto r = api.cadastrar({{}, GetParam(), ""});

    EXPECT_EQ(r.status, 400);
    EXPECT_EQ(r.corpo.at("erro"), "JSON inválido");
}

INSTANTIATE_TEST_SUITE_P(Casos, CorpoDeCredenciaisInvalido,
                         ::testing::Values("",                                   // vazio
                                           "{",                                  // incompleto
                                           "[\"ana\", \"senha\"]",               // não é objeto
                                           R"({"login": 7, "senha": "x"})"));    // login não é texto

// ---- Login e logout -----------------------------------------------------------

TEST_F(ApiTest, EntrarComSenhaCorretaRespondeComCookieDeSessao) {
    api.cadastrar(req("", credenciais("ana", "senha-segura")));

    const auto r = api.entrar(req("", credenciais("ana", "senha-segura")));

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(api.usuarioAtual(req(cookieDe(r))).corpo.at("login"), "ana");
}

TEST_F(ApiTest, EntrarIgnoraMaiusculasEEspacosNoLogin) {
    api.cadastrar(req("", credenciais("ana", "senha-segura")));

    const auto r = api.entrar(req("", credenciais(" ANA ", "senha-segura")));

    EXPECT_EQ(r.status, 200);
}

TEST_F(ApiTest, EntrarComSenhaErradaResponde401SemCookie) {
    api.cadastrar(req("", credenciais("ana", "senha-segura")));

    const auto r = api.entrar(req("", credenciais("ana", "senha-errada")));

    EXPECT_EQ(r.status, 401);
    EXPECT_TRUE(r.cookies.empty());
}

TEST_F(ApiTest, LoginInexistenteESenhaErradaRecebemAMesmaMensagem) {
    // Mensagens diferentes revelariam quais logins existem.
    api.cadastrar(req("", credenciais("ana", "senha-segura")));

    const auto senhaErrada = api.entrar(req("", credenciais("ana", "senha-errada")));
    const auto loginInexistente = api.entrar(req("", credenciais("bia", "senha-segura")));

    EXPECT_EQ(senhaErrada.corpo, loginInexistente.corpo);
}

TEST_F(ApiTest, EntrarComArmazenamentoIndisponivelResponde503) {
    repo.indisponivel = true;

    const auto r = api.entrar(req("", credenciais("ana", "senha-segura")));

    EXPECT_EQ(r.status, 503);
}

TEST_F(ApiTest, SairApagaOCookieNoNavegador) {
    const auto r = api.sair(req(entrarComo("ana")));

    EXPECT_EQ(r.status, 204);
    EXPECT_NE(r.cookies.at(0).find("Max-Age=0"), std::string::npos);
}

// ---- Cookie e sessão ----------------------------------------------------------

TEST_F(ApiTest, CookieDeSessaoEhHttpOnlyESameSiteLax) {
    const auto r = api.cadastrar(req("", credenciais("ana", "senha-segura")));

    EXPECT_NE(r.cookies.at(0).find("HttpOnly"), std::string::npos);
    EXPECT_NE(r.cookies.at(0).find("SameSite=Lax"), std::string::npos);
}

TEST_F(ApiTest, CookieNaoEhSecureQuandoNaoConfigurado) {
    const auto r = api.cadastrar(req("", credenciais("ana", "senha-segura")));

    EXPECT_EQ(r.cookies.at(0).find("Secure"), std::string::npos);
}

TEST(ApiCookieSeguro, CookieEhSecureQuandoConfigurado) {
    RepositorioMemoria repo;
    Api api(repo, OpcoesApi{kSegredo, true});

    const auto r = api.cadastrar({{}, credenciais("ana", "senha-segura").dump(), ""});

    EXPECT_NE(r.cookies.at(0).find("; Secure"), std::string::npos);
}

TEST_F(ApiTest, SemCookieOUsuarioAtualResponde401) {
    EXPECT_EQ(api.usuarioAtual(req("")).status, 401);
}

TEST_F(ApiTest, SessaoExpiradaResponde401) {
    const auto cookie = entrarComo("ana");
    agora += diario::AssinadorSessao::kValidade;

    EXPECT_EQ(api.usuarioAtual(req(cookie)).status, 401);
}

TEST_F(ApiTest, CookieAdulteradoResponde401) {
    auto cookie = entrarComo("ana");
    cookie.back() = cookie.back() == '0' ? '1' : '0';  // altera um caractere da assinatura

    EXPECT_EQ(api.usuarioAtual(req(cookie)).status, 401);
}

TEST_F(ApiTest, CookieDeSessaoEhEncontradoEntreOutrosCookies) {
    const auto sessao = entrarComo("ana");

    const auto r = api.usuarioAtual(req("tema=escuro; " + sessao + "; idioma=pt"));

    EXPECT_EQ(r.status, 200);
}

TEST_F(ApiTest, SessaoDeUsuarioQueNaoExisteMaisResponde401) {
    const auto cookie = entrarComo("ana");
    repo.usuarios.clear();

    EXPECT_EQ(api.usuarioAtual(req(cookie)).status, 401);
}

enum class Rota { Listar, Estatisticas, Criar, Atualizar, Remover };

class RotaDeLivrosSemSessao : public ApiTest, public ::testing::WithParamInterface<Rota> {
protected:
    Resposta chamar(Rota rota, const Requisicao& r) {
        switch (rota) {
            case Rota::Listar: return api.listarLivros(r);
            case Rota::Estatisticas: return api.estatisticas(r);
            case Rota::Criar: return api.criarLivro(r);
            case Rota::Atualizar: return api.atualizarLivro(r, 1);
            case Rota::Remover: return api.removerLivro(r, 1);
        }
        return {};
    }
};

TEST_P(RotaDeLivrosSemSessao, Responde401) {
    const auto r = chamar(GetParam(), req("", livro("A", "B", 3)));

    EXPECT_EQ(r.status, 401);
}

INSTANTIATE_TEST_SUITE_P(Rotas, RotaDeLivrosSemSessao,
                         ::testing::Values(Rota::Listar, Rota::Estatisticas, Rota::Criar, Rota::Atualizar,
                                           Rota::Remover));

// ---- Isolamento entre usuários ------------------------------------------------

TEST_F(ApiComLivros, OutroUsuarioNaoVeOsLivrosDaAna) {
    const auto bia = entrarComo("bia");

    EXPECT_TRUE(api.listarLivros(req(bia)).corpo.empty());
}

TEST_F(ApiComLivros, IdsDeLivrosComecamEmUmParaCadaUsuario) {
    const auto bia = entrarComo("bia");

    const auto r = api.criarLivro(req(bia, livro("Vidas Secas", "Graciliano Ramos", 4)));

    EXPECT_EQ(r.corpo.at("id"), 1);
}

TEST_F(ApiComLivros, OutroUsuarioNaoConsegueRemoverLivroDaAna) {
    const auto bia = entrarComo("bia");
    api.criarLivro(req(bia, livro("Vidas Secas", "Graciliano Ramos", 4)));

    api.removerLivro(req(bia), 1);  // id 1 da Bia, não o da Ana

    EXPECT_EQ(api.listarLivros(req(ana)).corpo.size(), 3u);
}

TEST_F(ApiComLivros, EstatisticasSaoDoUsuarioDaSessao) {
    const auto bia = entrarComo("bia");

    EXPECT_EQ(api.estatisticas(req(bia)).corpo.at("total"), 0);
}

// ---- Listagem -----------------------------------------------------------------

TEST_F(ApiComLivros, ListarSemParametrosDevolveTodosNaOrdemDeInsercao) {
    const auto r = api.listarLivros(req(ana));

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Iracema", "Memórias Póstumas"}));
}

TEST_F(ApiComLivros, ListarComOrdemNotaDevolveMaiorNotaPrimeiro) {
    const auto r = api.listarLivros(req(ana, nullptr, {{"ordem", "nota"}}));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas", "Iracema"}));
}

TEST_F(ApiComLivros, ListarComBuscaDevolveSoOsLivrosQueCombinam) {
    const auto r = api.listarLivros(req(ana, nullptr, {{"q", "machado"}}));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas"}));
}

TEST_F(ApiComLivros, ListarComNotaMinimaExcluiNotasMenores) {
    const auto r = api.listarLivros(req(ana, nullptr, {{"notaMin", "3"}}));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas"}));
}

TEST_F(ApiComLivros, ListarCombinaBuscaNotaMinimaEOrdenacao) {
    const auto r = api.listarLivros(req(ana, nullptr, {{"q", "a"}, {"notaMin", "4"}, {"ordem", "nota"}}));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas"}));
}

class NotaMinimaInvalida : public ApiTest, public ::testing::WithParamInterface<std::string> {};

TEST_P(NotaMinimaInvalida, Responde400) {
    const auto r = api.listarLivros(req(entrarComo("ana"), nullptr, {{"notaMin", GetParam()}}));

    EXPECT_EQ(r.status, 400);
}

INSTANTIATE_TEST_SUITE_P(Casos, NotaMinimaInvalida, ::testing::Values("x", "12", "-1", "6", "9"));

TEST_F(ApiTest, ListarComArmazenamentoIndisponivelResponde503) {
    const auto cookie = entrarComo("ana");
    repo.indisponivel = true;

    EXPECT_EQ(api.listarLivros(req(cookie)).status, 503);
}

// ---- Estatísticas -------------------------------------------------------------

TEST_F(ApiComLivros, EstatisticasDevolvemTotalMediaEAutoresMaisLidos) {
    const auto r = api.estatisticas(req(ana));

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo.at("total"), 3);
    EXPECT_NEAR(r.corpo.at("media").get<double>(), 11.0 / 3.0, 1e-9);
    EXPECT_EQ(r.corpo.at("autoresMaisLidos").at(0), (json{{"autor", "Machado de Assis"}, {"livros", 2}}));
}

// ---- Criação ------------------------------------------------------------------

TEST_F(ApiTest, LivroValidoResponde201ComOIdAtribuido) {
    const auto r = api.criarLivro(req(entrarComo("ana"), livro("Vidas Secas", "Graciliano Ramos", 4)));

    EXPECT_EQ(r.status, 201);
    EXPECT_EQ(r.corpo.at("id"), 1);
    EXPECT_EQ(r.corpo.at("titulo"), "Vidas Secas");
}

TEST_F(ApiTest, LivroCriadoEhGravadoNoDiarioDoUsuario) {
    api.criarLivro(req(entrarComo("ana"), livro("Vidas Secas", "Graciliano Ramos", 4)));

    EXPECT_EQ(repo.diarios.at(1).tamanho(), 1u);
}

TEST_F(ApiTest, LivroCriadoRegistraOInstanteDoServidorEmUtc) {
    // agora = 1.800.000.000 s desde 1970 = 15/01/2027 08:00:00 UTC.
    const auto r = api.criarLivro(req(entrarComo("ana"), livro("Vidas Secas", "Graciliano Ramos", 4)));

    EXPECT_EQ(r.corpo.at("registradoEm"), "2027-01-15T08:00:00Z");
}

TEST_F(ApiTest, LivroCriadoIgnoraRegistroEnviadoPeloCliente) {
    auto corpo = livro("Vidas Secas", "Graciliano Ramos", 4);
    corpo["registradoEm"] = "1999-01-01T00:00:00Z";

    const auto r = api.criarLivro(req(entrarComo("ana"), corpo));

    EXPECT_EQ(r.corpo.at("registradoEm"), "2027-01-15T08:00:00Z");
}

TEST_F(ApiComLivros, AtualizarLivroMantemQuandoEleFoiRegistrado) {
    agora += std::chrono::hours{24};

    const auto r = api.atualizarLivro(req(ana, livro("Dom Casmurro (2a ed.)", "Machado de Assis", 4)), 1);

    EXPECT_EQ(r.corpo.at("registradoEm"), "2027-01-15T08:00:00Z");
}

TEST_F(ApiTest, LivroInvalidoResponde400ComAMensagemDoNucleo) {
    const auto r = api.criarLivro(req(entrarComo("ana"), livro("", "Autor", 3)));

    EXPECT_EQ(r.status, 400);
    EXPECT_EQ(r.corpo.at("erro"), "título é obrigatório");
}

class CorpoDeLivroInvalido : public ApiTest, public ::testing::WithParamInterface<std::string> {};

TEST_P(CorpoDeLivroInvalido, Responde400JsonInvalido) {
    const auto r = api.criarLivro({{}, GetParam(), entrarComo("ana")});

    EXPECT_EQ(r.status, 400);
    EXPECT_EQ(r.corpo.at("erro"), "JSON inválido");
}

INSTANTIATE_TEST_SUITE_P(Casos, CorpoDeLivroInvalido,
                         ::testing::Values("",                                              // vazio
                                           "{",                                             // incompleto
                                           "[1, 2]",                                        // não é objeto
                                           R"({"titulo":"A","autor":"B","nota":"5"})"));  // nota como texto

TEST_F(ApiTest, FalhaAoGravarResponde503ENaoMostraOLivro) {
    const auto ana = entrarComo("ana");
    api.listarLivros(req(ana));  // carrega o diário antes da falha
    repo.indisponivel = true;

    const auto r = api.criarLivro(req(ana, livro("Vidas Secas", "Graciliano Ramos", 4)));
    repo.indisponivel = false;

    EXPECT_EQ(r.status, 503);
    EXPECT_TRUE(api.listarLivros(req(ana)).corpo.empty());
}

TEST_F(ApiTest, IdNaoEhConsumidoQuandoAGravacaoFalha) {
    const auto ana = entrarComo("ana");
    api.listarLivros(req(ana));
    repo.indisponivel = true;
    api.criarLivro(req(ana, livro("A", "X", 1)));
    repo.indisponivel = false;

    const auto r = api.criarLivro(req(ana, livro("B", "X", 1)));

    EXPECT_EQ(r.corpo.at("id"), 1);
}

// ---- Atualização --------------------------------------------------------------

TEST_F(ApiComLivros, AtualizarLivroExistenteDevolveOsNovosDados) {
    const auto r = api.atualizarLivro(req(ana, livro("Dom Casmurro (2a ed.)", "Machado de Assis", 4)), 1);

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo.at("titulo"), "Dom Casmurro (2a ed.)");
    EXPECT_EQ(r.corpo.at("id"), 1);
}

TEST_F(ApiComLivros, AtualizarLivroInexistenteResponde404) {
    EXPECT_EQ(api.atualizarLivro(req(ana, livro("Z", "Z", 1)), 999).status, 404);
}

TEST_F(ApiComLivros, AtualizarComDadosInvalidosResponde400EMantemOOriginal) {
    const auto r = api.atualizarLivro(req(ana, livro("Dom Casmurro", "Machado de Assis", 9)), 1);

    EXPECT_EQ(r.status, 400);
    EXPECT_EQ(api.listarLivros(req(ana)).corpo.at(0).at("nota"), 5);
}

TEST_F(ApiComLivros, AtualizarComFalhaAoGravarResponde503EMantemOOriginal) {
    repo.indisponivel = true;

    const auto r = api.atualizarLivro(req(ana, livro("Outro título", "Machado de Assis", 5)), 1);
    repo.indisponivel = false;

    EXPECT_EQ(r.status, 503);
    EXPECT_EQ(api.listarLivros(req(ana)).corpo.at(0).at("titulo"), "Dom Casmurro");
}

// ---- Remoção ------------------------------------------------------------------

TEST_F(ApiComLivros, RemoverLivroExistenteResponde204SemCorpo) {
    const auto r = api.removerLivro(req(ana), 2);

    EXPECT_EQ(r.status, 204);
    EXPECT_TRUE(r.corpo.is_null());
}

TEST_F(ApiComLivros, LivroRemovidoSaiDaListagem) {
    api.removerLivro(req(ana), 2);

    EXPECT_EQ(titulos(api.listarLivros(req(ana)).corpo),
              (std::vector<std::string>{"Dom Casmurro", "Memórias Póstumas"}));
}

TEST_F(ApiComLivros, RemoverLivroInexistenteResponde404) {
    EXPECT_EQ(api.removerLivro(req(ana), 999).status, 404);
}

TEST_F(ApiComLivros, RemoverComFalhaAoGravarResponde503EMantemOLivro) {
    repo.indisponivel = true;

    const auto r = api.removerLivro(req(ana), 2);
    repo.indisponivel = false;

    EXPECT_EQ(r.status, 503);
    EXPECT_EQ(api.listarLivros(req(ana)).corpo.size(), 3u);
}

// ---- Busca de pessoas ---------------------------------------------------------

TEST_F(ApiTest, BuscarUsuariosDevolveLoginsQueContemOTrecho) {
    const auto ana = entrarComo("ana");
    entrarComo("mariana");
    entrarComo("bruno");

    const auto r = api.buscarUsuarios(req(ana, nullptr, {{"q", "ana"}}));

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo, json::parse(R"([{"login": "ana"}, {"login": "mariana"}])"));
}

TEST_F(ApiTest, BuscarUsuariosIgnoraMaiusculasEEspacosNasPontas) {
    const auto ana = entrarComo("ana");
    entrarComo("bruno");

    const auto r = api.buscarUsuarios(req(ana, nullptr, {{"q", "  BRU "}}));

    EXPECT_EQ(r.corpo, json::parse(R"([{"login": "bruno"}])"));
}

TEST_F(ApiTest, BuscarUsuariosSemTermoDevolveListaVazia) {
    const auto ana = entrarComo("ana");

    EXPECT_EQ(api.buscarUsuarios(req(ana)).corpo, json::array());
}

TEST_F(ApiTest, BuscarUsuariosDevolveNoMaximoOLimite) {
    const auto ana = entrarComo("ana");
    for (int i = 0; i < 25; ++i) entrarComo("leitor" + std::to_string(i));

    const auto r = api.buscarUsuarios(req(ana, nullptr, {{"q", "leitor"}}));

    EXPECT_EQ(r.corpo.size(), Api::kLimiteBuscaUsuarios);
}

TEST_F(ApiTest, BuscarUsuariosSemSessaoResponde401) {
    entrarComo("ana");

    EXPECT_EQ(api.buscarUsuarios(req("", nullptr, {{"q", "ana"}})).status, 401);
}

// ---- Perfil -------------------------------------------------------------------

TEST_F(ApiComLivros, PerfilDeOutroUsuarioMostraOsLivrosEAsEstatisticasDele) {
    const auto bia = entrarComo("bia");

    const auto r = api.perfil(req(bia), "ana");

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo.at("login"), "ana");
    EXPECT_EQ(r.corpo.at("total"), 3);
    EXPECT_DOUBLE_EQ(r.corpo.at("media").get<double>(), 11.0 / 3);
    EXPECT_EQ(titulos(r.corpo.at("livros")),
              (std::vector<std::string>{"Dom Casmurro", "Iracema", "Memórias Póstumas"}));
    EXPECT_EQ(r.corpo.at("ehVoce"), false);
}

TEST_F(ApiComLivros, PerfilNaoExpoeOHashDaSenha) {
    const auto r = api.perfil(req(entrarComo("bia")), "ana");

    EXPECT_FALSE(r.corpo.contains("senhaHash"));
    EXPECT_EQ(r.corpo.dump().find("argon2id"), std::string::npos);
}

TEST_F(ApiComLivros, PerfilMostraSeguidoresSeguindoESeEuSigo) {
    const auto bia = entrarComo("bia");  // id 2; ana é o id 1
    entrarComo("caio");                  // id 3
    repo.seguir(2, 1);
    repo.seguir(3, 1);
    repo.seguir(1, 3);

    const auto r = api.perfil(req(bia), "ana");

    EXPECT_EQ(r.corpo.at("seguidores"), 2);
    EXPECT_EQ(r.corpo.at("seguindo"), 1);
    EXPECT_EQ(r.corpo.at("euSigo"), true);
}

TEST_F(ApiComLivros, PerfilDeQuemNaoSigoIndicaQueNaoSigo) {
    EXPECT_EQ(api.perfil(req(entrarComo("bia")), "ana").corpo.at("euSigo"), false);
}

TEST_F(ApiComLivros, PerfilProprioIndicaQueEhVoce) {
    EXPECT_EQ(api.perfil(req(ana), "ana").corpo.at("ehVoce"), true);
}

TEST_F(ApiTest, PerfilDeUsuarioInexistenteResponde404) {
    EXPECT_EQ(api.perfil(req(entrarComo("ana")), "ninguem").status, 404);
}

TEST_F(ApiComLivros, PerfilSemSessaoResponde401) {
    EXPECT_EQ(api.perfil(req(""), "ana").status, 401);
}

TEST_F(ApiComLivros, PerfilComFalhaNoArmazenamentoResponde503) {
    const auto bia = entrarComo("bia");
    repo.indisponivel = true;

    EXPECT_EQ(api.perfil(req(bia), "ana").status, 503);
}

// ---- Seguir -------------------------------------------------------------------

TEST_F(ApiTest, SeguirFazOUsuarioAparecerComoSeguidor) {
    entrarComo("ana");
    const auto bia = entrarComo("bia");

    const auto r = api.seguir(req(bia), "ana");

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo, json::parse(R"({"euSigo": true, "seguidores": 1})"));
    EXPECT_EQ(api.perfil(req(bia), "ana").corpo.at("euSigo"), true);
}

TEST_F(ApiTest, SeguirDuasVezesContaUmSeguidorSo) {
    entrarComo("ana");
    const auto bia = entrarComo("bia");

    api.seguir(req(bia), "ana");
    const auto r = api.seguir(req(bia), "ana");

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo.at("seguidores"), 1);
}

TEST_F(ApiTest, DeixarDeSeguirRemoveOSeguidor) {
    entrarComo("ana");
    const auto bia = entrarComo("bia");
    api.seguir(req(bia), "ana");

    const auto r = api.deixarDeSeguir(req(bia), "ana");

    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.corpo, json::parse(R"({"euSigo": false, "seguidores": 0})"));
}

TEST_F(ApiTest, DeixarDeSeguirQuemNaoSigoNaoEhErro) {
    entrarComo("ana");

    EXPECT_EQ(api.deixarDeSeguir(req(entrarComo("bia")), "ana").status, 200);
}

TEST_F(ApiTest, SeguirASiMesmoResponde400) {
    const auto ana = entrarComo("ana");

    const auto r = api.seguir(req(ana), "ana");

    EXPECT_EQ(r.status, 400);
    EXPECT_TRUE(repo.seguidores(1).empty());
}

TEST_F(ApiTest, SeguirUsuarioInexistenteResponde404) {
    EXPECT_EQ(api.seguir(req(entrarComo("ana")), "ninguem").status, 404);
}

TEST_F(ApiTest, SeguirSemSessaoResponde401) {
    entrarComo("ana");

    EXPECT_EQ(api.seguir(req(""), "ana").status, 401);
    EXPECT_EQ(api.deixarDeSeguir(req(""), "ana").status, 401);
}

TEST_F(ApiTest, SeguirComFalhaNoArmazenamentoResponde503) {
    entrarComo("ana");
    const auto bia = entrarComo("bia");
    repo.indisponivel = true;

    EXPECT_EQ(api.seguir(req(bia), "ana").status, 503);
}

// ---- Feed ---------------------------------------------------------------------

namespace {

std::vector<std::string> titulosDoFeed(const json& feed) {
    std::vector<std::string> out;
    for (const auto& item : feed) out.push_back(item.at("livro").at("titulo").get<std::string>());
    return out;
}

}  // namespace

TEST_F(ApiTest, FeedMostraLivrosDeQuemEuSigoComOLoginDoDono) {
    const auto ana = entrarComo("ana");
    const auto bia = entrarComo("bia");
    api.criarLivro(req(ana, livro("Dom Casmurro", "Machado de Assis", 5)));
    api.seguir(req(bia), "ana");

    const auto r = api.feed(req(bia));

    EXPECT_EQ(r.status, 200);
    ASSERT_EQ(r.corpo.size(), 1u);
    EXPECT_EQ(r.corpo[0].at("login"), "ana");
    EXPECT_EQ(r.corpo[0].at("livro").at("titulo"), "Dom Casmurro");
}

TEST_F(ApiTest, FeedNaoMostraLivrosDeQuemNaoSigoNemOsMeus) {
    const auto ana = entrarComo("ana");
    const auto bia = entrarComo("bia");
    api.criarLivro(req(ana, livro("Dom Casmurro", "Machado de Assis", 5)));
    api.criarLivro(req(bia, livro("Iracema", "José de Alencar", 2)));

    EXPECT_EQ(api.feed(req(bia)).corpo, json::array());
}

TEST_F(ApiTest, FeedMostraOsLivrosMaisRecentesPrimeiro) {
    const auto ana = entrarComo("ana");
    const auto caio = entrarComo("caio");
    const auto bia = entrarComo("bia");
    api.criarLivro(req(ana, livro("Primeiro", "A", 3)));
    agora += std::chrono::minutes{1};
    api.criarLivro(req(caio, livro("Segundo", "B", 3)));
    agora += std::chrono::minutes{1};
    api.criarLivro(req(ana, livro("Terceiro", "C", 3)));
    api.seguir(req(bia), "ana");
    api.seguir(req(bia), "caio");

    EXPECT_EQ(titulosDoFeed(api.feed(req(bia)).corpo),
              (std::vector<std::string>{"Terceiro", "Segundo", "Primeiro"}));
}

TEST_F(ApiTest, FeedMostraLivrosSemDataDeRegistroNoFim) {
    const auto ana = entrarComo("ana");
    const auto bia = entrarComo("bia");
    // Livro salvo antes de existir registradoEm.
    diario::Diario antigo;
    antigo.adicionar(diario::Livro{0, "Antigo", "A", "", "", "", 3, ""});
    repo.diarios[1] = antigo;
    api.criarLivro(req(ana, livro("Novo", "B", 3)));
    api.seguir(req(bia), "ana");

    EXPECT_EQ(titulosDoFeed(api.feed(req(bia)).corpo), (std::vector<std::string>{"Novo", "Antigo"}));
}

TEST_F(ApiTest, FeedTemNoMaximoOLimite) {
    const auto ana = entrarComo("ana");
    const auto bia = entrarComo("bia");
    for (std::size_t i = 0; i < Api::kLimiteFeed + 5; ++i) api.criarLivro(req(ana, livro("Livro", "A", 3)));
    api.seguir(req(bia), "ana");

    EXPECT_EQ(api.feed(req(bia)).corpo.size(), Api::kLimiteFeed);
}

TEST_F(ApiTest, FeedDeixaDeMostrarQuemDeixeiDeSeguir) {
    const auto ana = entrarComo("ana");
    const auto bia = entrarComo("bia");
    api.criarLivro(req(ana, livro("Dom Casmurro", "Machado de Assis", 5)));
    api.seguir(req(bia), "ana");

    api.deixarDeSeguir(req(bia), "ana");

    EXPECT_EQ(api.feed(req(bia)).corpo, json::array());
}

TEST_F(ApiTest, FeedSemSessaoResponde401) {
    EXPECT_EQ(api.feed(req("")).status, 401);
}

TEST_F(ApiTest, FeedComFalhaNoArmazenamentoResponde503) {
    const auto bia = entrarComo("bia");
    repo.indisponivel = true;

    EXPECT_EQ(api.feed(req(bia)).status, 503);
}

// ---- Carga do repositório -----------------------------------------------------

TEST_F(ApiTest, LivrosJaSalvosNoRepositorioAparecemNaListagem) {
    // Simula o servidor reiniciando: o diário vem do repositório, não da memória da Api.
    const auto ana = entrarComo("ana");
    diario::Livro l;
    l.titulo = "Capitães da Areia";
    l.autor = "Jorge Amado";
    repo.diarios[1].adicionar(l);

    const auto r = api.listarLivros(req(ana));

    EXPECT_EQ(titulos(r.corpo), (std::vector<std::string>{"Capitães da Areia"}));
}
