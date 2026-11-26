#include "diario/conta.hpp"

#include <algorithm>
#include <cctype>

namespace diario {

std::string normalizarLogin(const std::string& login) {
    const auto inicio = login.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) return "";
    const auto fim = login.find_last_not_of(" \t\r\n");
    std::string out = login.substr(inicio, fim - inicio + 1);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::vector<std::string> validarCadastro(const std::string& login, const std::string& senha) {
    std::vector<std::string> erros;

    const bool caracteresOk = std::all_of(login.begin(), login.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
    if (login.size() < kLoginMin || login.size() > kLoginMax || !caracteresOk) {
        erros.emplace_back("login deve ter de 3 a 30 caracteres: letras sem acento, números, '.', '_' ou '-'");
    }
    // Limite superior: o Argon2 processa a senha inteira; evita corpos enormes por requisição.
    if (senha.size() < kSenhaMin || senha.size() > kSenhaMax) {
        erros.emplace_back("senha deve ter de 8 a 128 caracteres");
    }
    return erros;
}

}  // namespace diario
