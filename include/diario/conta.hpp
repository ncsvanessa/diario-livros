#pragma once

#include <string>
#include <vector>

namespace diario {

// Uma conta de usuário. A senha nunca é guardada; só o hash (ver senha.hpp).
struct Usuario {
    int id = 0;
    std::string login;      // já normalizado (ver normalizarLogin)
    std::string senhaHash;
};

constexpr std::size_t kLoginMin = 3;
constexpr std::size_t kLoginMax = 30;
constexpr std::size_t kSenhaMin = 8;
constexpr std::size_t kSenhaMax = 128;

// Remove espaços nas pontas e passa para minúsculas: "  Vanessa " e "vanessa" são o mesmo login.
std::string normalizarLogin(const std::string& login);

// Mensagens de erro para um cadastro; vazia se login (já normalizado) e senha são aceitáveis.
// Login: 3 a 30 caracteres entre a-z, 0-9, '.', '_' e '-'. Senha: 8 a 128 bytes.
std::vector<std::string> validarCadastro(const std::string& loginNormalizado, const std::string& senha);

}  // namespace diario
