#include "diario/sessao.hpp"

#include <stdexcept>

#include <monocypher.h>

#include "hex.hpp"

namespace diario {

namespace {

long long segundos(AssinadorSessao::Instante t) {
    return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
}

// Inteiro decimal não negativo com até `maxDigitos` dígitos; sem sinal, espaços ou zeros à esquerda.
std::optional<long long> lerInteiro(const std::string& s, std::size_t maxDigitos) {
    if (s.empty() || s.size() > maxDigitos || (s.size() > 1 && s[0] == '0')) return std::nullopt;
    long long n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return std::nullopt;
        n = n * 10 + (c - '0');
    }
    return n;
}

}  // namespace

AssinadorSessao::AssinadorSessao(const std::string& segredo) {
    if (segredo.size() < kTamanhoMinimoSegredo) {
        throw std::invalid_argument("o segredo de sessão precisa de ao menos 32 caracteres");
    }
    // Deriva uma chave de tamanho fixo de qualquer segredo (BLAKE2b aceita chave de até 64 bytes).
    crypto_blake2b(chave_.data(), chave_.size(), reinterpret_cast<const std::uint8_t*>(segredo.data()),
                   segredo.size());
}

std::array<std::uint8_t, 32> AssinadorSessao::mac(const std::string& mensagem) const {
    std::array<std::uint8_t, 32> out{};
    crypto_blake2b_keyed(out.data(), out.size(), chave_.data(), chave_.size(),
                         reinterpret_cast<const std::uint8_t*>(mensagem.data()), mensagem.size());
    return out;
}

std::string AssinadorSessao::emitir(int usuarioId, Instante agora) const {
    if (usuarioId <= 0) throw std::invalid_argument("id de usuário inválido");
    const auto corpo = std::to_string(usuarioId) + "." + std::to_string(segundos(agora + kValidade));
    const auto assinatura = mac(corpo);
    return corpo + "." + hex::codificar(assinatura.data(), assinatura.size());
}

std::optional<int> AssinadorSessao::validar(const std::string& token, Instante agora) const {
    const auto p1 = token.find('.');
    const auto p2 = p1 == std::string::npos ? std::string::npos : token.find('.', p1 + 1);
    if (p2 == std::string::npos) return std::nullopt;

    const auto id = lerInteiro(token.substr(0, p1), 9);
    const auto expira = lerInteiro(token.substr(p1 + 1, p2 - p1 - 1), 12);
    const auto recebido = hex::decodificar(token.substr(p2 + 1));
    if (!id || *id == 0 || !expira || !recebido || recebido->size() != 32) return std::nullopt;

    // Confere a assinatura antes de confiar em qualquer campo; comparação em tempo constante.
    const auto esperado = mac(token.substr(0, p2));
    if (crypto_verify32(esperado.data(), recebido->data()) != 0) return std::nullopt;

    if (segundos(agora) >= *expira) return std::nullopt;
    return static_cast<int>(*id);
}

}  // namespace diario
