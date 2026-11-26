#include <cstddef>

#include <gtest/gtest.h>

#include "diario/aleatorio.hpp"

using diario::bytesAleatorios;

class BytesAleatoriosTamanho : public ::testing::TestWithParam<std::size_t> {};

TEST_P(BytesAleatoriosTamanho, RetornaExatamenteAQuantidadePedida) {
    const auto bytes = bytesAleatorios(GetParam());

    EXPECT_EQ(bytes.size(), GetParam());
}

INSTANTIATE_TEST_SUITE_P(Tamanhos, BytesAleatoriosTamanho, ::testing::Values(0u, 1u, 16u, 1024u));

TEST(BytesAleatorios, DuasChamadasProduzemValoresDiferentes) {
    // 32 bytes: a chance de duas leituras iguais de um gerador correto é 2^-256.
    const auto primeira = bytesAleatorios(32);
    const auto segunda = bytesAleatorios(32);

    EXPECT_NE(primeira, segunda);
}
