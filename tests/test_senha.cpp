#include <string>

#include <gtest/gtest.h>

#include "diario/senha.hpp"

using diario::gerarHashSenha;
using diario::verificarSenha;

TEST(Senha, SenhaCorretaEhAceita) {
    const auto hash = gerarHashSenha("capitu-e-bentinho");

    EXPECT_TRUE(verificarSenha("capitu-e-bentinho", hash));
}

TEST(Senha, SenhaErradaEhRecusada) {
    const auto hash = gerarHashSenha("capitu-e-bentinho");

    EXPECT_FALSE(verificarSenha("capitu-e-escobar", hash));
}

TEST(Senha, DiferencaDeMaiusculasTornaASenhaErrada) {
    const auto hash = gerarHashSenha("Macunaima1928");

    EXPECT_FALSE(verificarSenha("macunaima1928", hash));
}

TEST(Senha, HashNaoContemASenhaEmTextoPuro) {
    const auto hash = gerarHashSenha("senha-secreta-123");

    EXPECT_EQ(hash.find("senha-secreta-123"), std::string::npos);
}

TEST(Senha, MesmaSenhaGeraHashesDiferentes) {
    // Salt aleatório: dois usuários com a mesma senha não ficam com o mesmo hash.
    const auto primeiro = gerarHashSenha("mesma-senha");
    const auto segundo = gerarHashSenha("mesma-senha");

    EXPECT_NE(primeiro, segundo);
}

TEST(Senha, HashGeradoIndicaOAlgoritmoArgon2id) {
    const auto hash = gerarHashSenha("qualquer-senha");

    EXPECT_EQ(hash.rfind("argon2id$", 0), 0u);
}

TEST(Senha, AceitaHashArgon2idGeradoPorOutraImplementacao) {
    // Gerado com a implementação de referência do Argon2 (Python argon2-cffi):
    //   hash_secret_raw(b"dom-casmurro", bytes(range(16)), time_cost=1, memory_cost=64,
    //                   parallelism=1, hash_len=32, type=Type.ID, version=19)
    // Garante que o hash é Argon2id padrão, e não algo próprio desta biblioteca.
    const std::string hashDeReferencia =
        "argon2id$64$1$000102030405060708090a0b0c0d0e0f$"
        "d3c9d2a4255b9ca589c38f8947bc307a36c35db28097512643c7b6a2c57a3a72";

    EXPECT_TRUE(verificarSenha("dom-casmurro", hashDeReferencia));
}

class HashMalFormado : public ::testing::TestWithParam<std::string> {};

TEST_P(HashMalFormado, EhRecusadoSemLancarExcecao) {
    EXPECT_FALSE(verificarSenha("qualquer-senha", GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    Casos, HashMalFormado,
    ::testing::Values(
        "",                                                     // vazio
        "texto-qualquer",                                       // sem separadores
        "bcrypt$19456$2$00112233445566778899aabbccddeeff$00",   // outro algoritmo
        "argon2id$19456$2$00112233445566778899aabbccddeeff",    // faltando o hash
        "argon2id$abc$2$00112233445566778899aabbccddeeff$"
        "0011223344556677889900112233445566778899001122334455667788990011",  // memória não numérica
        "argon2id$19456$2$zz112233445566778899aabbccddeeff$"
        "0011223344556677889900112233445566778899001122334455667788990011",  // salt não hexadecimal
        "argon2id$19456$2$00112233445566778899aabbccddeeff$0011",           // hash curto
        "argon2id$999999999$2$00112233445566778899aabbccddeeff$"
        "0011223344556677889900112233445566778899001122334455667788990011"  // memória absurda
        ));
