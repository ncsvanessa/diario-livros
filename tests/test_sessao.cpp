#include <chrono>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "diario/sessao.hpp"

using diario::AssinadorSessao;
using namespace std::chrono_literals;

namespace {

const std::string kSegredo = "segredo-de-teste-com-mais-de-32-caracteres";
const std::string kOutroSegredo = "outro-segredo-de-teste-com-mais-de-32-caracteres";

// Instante fixo: os testes não dependem do relógio da máquina.
const AssinadorSessao::Instante kAgora{std::chrono::seconds{1'800'000'000}};

}  // namespace

TEST(Sessao, TokenRecemEmitidoIdentificaOUsuario) {
    const AssinadorSessao assinador(kSegredo);

    const auto token = assinador.emitir(42, kAgora);

    EXPECT_EQ(assinador.validar(token, kAgora), 42);
}

TEST(Sessao, TokenContinuaValidoNoUltimoSegundoDaValidade) {
    const AssinadorSessao assinador(kSegredo);
    const auto token = assinador.emitir(7, kAgora);

    const auto resultado = assinador.validar(token, kAgora + AssinadorSessao::kValidade - 1s);

    EXPECT_EQ(resultado, 7);
}

TEST(Sessao, TokenExpiradoEhRecusado) {
    const AssinadorSessao assinador(kSegredo);
    const auto token = assinador.emitir(7, kAgora);

    const auto resultado = assinador.validar(token, kAgora + AssinadorSessao::kValidade);

    EXPECT_EQ(resultado, std::nullopt);
}

TEST(Sessao, TokenAssinadoComOutroSegredoEhRecusado) {
    const AssinadorSessao assinador(kSegredo);
    const auto tokenDeOutroServidor = AssinadorSessao(kOutroSegredo).emitir(7, kAgora);

    EXPECT_EQ(assinador.validar(tokenDeOutroServidor, kAgora), std::nullopt);
}

TEST(Sessao, TrocarOIdDoUsuarioNoTokenInvalidaAAssinatura) {
    // Ataque clássico: editar o cookie para virar outro usuário.
    const AssinadorSessao assinador(kSegredo);
    auto token = assinador.emitir(7, kAgora);
    token[0] = '8';

    EXPECT_EQ(assinador.validar(token, kAgora), std::nullopt);
}

TEST(Sessao, EstenderAValidadeNoTokenInvalidaAAssinatura) {
    const AssinadorSessao assinador(kSegredo);
    auto token = assinador.emitir(7, kAgora);
    token[token.find('.') + 1] = '9';  // primeiro dígito do instante de expiração

    EXPECT_EQ(assinador.validar(token, kAgora), std::nullopt);
}

TEST(Sessao, SegredoCurtoDemaisLancaExcecao) {
    EXPECT_THROW(AssinadorSessao("curto"), std::invalid_argument);
}

TEST(Sessao, EmitirParaIdInvalidoLancaExcecao) {
    const AssinadorSessao assinador(kSegredo);

    EXPECT_THROW(assinador.emitir(0, kAgora), std::invalid_argument);
}

class TokenMalFormado : public ::testing::TestWithParam<std::string> {};

TEST_P(TokenMalFormado, EhRecusadoSemLancarExcecao) {
    const AssinadorSessao assinador(kSegredo);

    EXPECT_EQ(assinador.validar(GetParam(), kAgora), std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(
    Casos, TokenMalFormado,
    ::testing::Values("",                       // cookie vazio
                      "abc",                    // sem separadores
                      "7.1800604800",           // sem assinatura
                      "7.1800604800.zz",        // assinatura não hexadecimal
                      "7.1800604800.00",        // assinatura curta
                      "-7.1800604800.00",       // id negativo
                      "x.1800604800.00",        // id não numérico
                      "0.1800604800.00"));      // id zero
