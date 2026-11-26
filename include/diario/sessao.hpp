#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace diario {

// Emite e valida o token de sessão guardado no cookie.
//
// Formato: <id do usuário>.<expira em, segundos desde 1970>.<MAC em hex>, onde o MAC é
// BLAKE2b com chave sobre "<id>.<expira>". Sem o segredo não é possível forjar nem
// alterar um token. Não há estado no servidor, então a sessão sobrevive a reinícios.
// Limitação conhecida: sair (logout) apaga o cookie, mas o token continua válido até expirar.
//
// O instante atual é sempre passado por parâmetro para que a expiração seja testável.
class AssinadorSessao {
public:
    using Instante = std::chrono::system_clock::time_point;

    static constexpr std::chrono::hours kValidade{24 * 7};
    static constexpr std::size_t kTamanhoMinimoSegredo = 32;

    // Lança std::invalid_argument se o segredo tiver menos de kTamanhoMinimoSegredo bytes.
    explicit AssinadorSessao(const std::string& segredo);

    std::string emitir(int usuarioId, Instante agora) const;

    // Id do usuário se o token for autêntico e ainda não tiver expirado em `agora`.
    std::optional<int> validar(const std::string& token, Instante agora) const;

private:
    std::array<std::uint8_t, 32> mac(const std::string& mensagem) const;

    std::array<std::uint8_t, 32> chave_{};
};

}  // namespace diario
