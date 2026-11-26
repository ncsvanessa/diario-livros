#include <string>

#include <gtest/gtest.h>

#include "diario/conta.hpp"

using diario::normalizarLogin;
using diario::validarCadastro;

namespace {
const std::string kSenhaValida = "senha-valida-123";
const std::string kLoginValido = "vanessa";
}  // namespace

TEST(Conta, NormalizarLoginRemoveEspacosEPassaParaMinusculas) {
    EXPECT_EQ(normalizarLogin("  Vanessa.N "), "vanessa.n");
}

TEST(Conta, NormalizarLoginSoComEspacosResultaEmVazio) {
    EXPECT_EQ(normalizarLogin("   "), "");
}

TEST(Conta, CadastroComLoginESenhaValidosNaoTemErros) {
    EXPECT_TRUE(validarCadastro(kLoginValido, kSenhaValida).empty());
}

class LoginAceito : public ::testing::TestWithParam<std::string> {};

TEST_P(LoginAceito, NaoGeraErro) {
    EXPECT_TRUE(validarCadastro(GetParam(), kSenhaValida).empty());
}

INSTANTIATE_TEST_SUITE_P(Casos, LoginAceito,
                         ::testing::Values("ana",                              // tamanho mínimo
                                           std::string(30, 'a'),               // tamanho máximo
                                           "maria.silva", "joao_2026", "leitor-voraz"));

class LoginRecusado : public ::testing::TestWithParam<std::string> {};

TEST_P(LoginRecusado, GeraUmErro) {
    EXPECT_EQ(validarCadastro(GetParam(), kSenhaValida).size(), 1u);
}

INSTANTIATE_TEST_SUITE_P(Casos, LoginRecusado,
                         ::testing::Values("",                     // vazio
                                           "ab",                   // curto demais
                                           std::string(31, 'a'),   // longo demais
                                           "com espaco",           // espaço
                                           "joão",                 // acento
                                           "Maiuscula",            // não normalizado
                                           "a/b"));                // símbolo não permitido

class TamanhoDeSenha : public ::testing::TestWithParam<std::pair<std::size_t, bool>> {};

TEST_P(TamanhoDeSenha, RespeitaOsLimitesDe8a128Caracteres) {
    const auto [tamanho, aceita] = GetParam();

    const auto erros = validarCadastro(kLoginValido, std::string(tamanho, 'x'));

    EXPECT_EQ(erros.empty(), aceita);
}

INSTANTIATE_TEST_SUITE_P(Limites, TamanhoDeSenha,
                         ::testing::Values(std::make_pair(std::size_t{0}, false),
                                           std::make_pair(std::size_t{7}, false),
                                           std::make_pair(std::size_t{8}, true),
                                           std::make_pair(std::size_t{128}, true),
                                           std::make_pair(std::size_t{129}, false)));

TEST(Conta, LoginESenhaInvalidosGeramDoisErros) {
    EXPECT_EQ(validarCadastro("x", "curta").size(), 2u);
}
