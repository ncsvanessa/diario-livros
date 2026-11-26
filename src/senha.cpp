#include "diario/senha.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <vector>

#include <monocypher.h>

#include "diario/aleatorio.hpp"
#include "hex.hpp"

namespace diario {

namespace {

constexpr std::uint32_t kMemoriaKiB = 19 * 1024;
constexpr std::uint32_t kPassagens = 2;
constexpr std::size_t kTamanhoSalt = 16;
constexpr std::size_t kTamanhoHash = 32;
// Limite ao ler um hash armazenado: evita que um valor adulterado no banco faça o
// servidor tentar alocar memória demais.
constexpr std::uint32_t kMemoriaMaximaKiB = 256 * 1024;

std::array<std::uint8_t, kTamanhoHash> argon2id(const std::string& senha,
                                                const std::vector<std::uint8_t>& salt,
                                                std::uint32_t memoriaKiB, std::uint32_t passagens) {
    crypto_argon2_config config{};
    config.algorithm = CRYPTO_ARGON2_ID;
    config.nb_blocks = memoriaKiB;  // blocos de 1 KiB
    config.nb_passes = passagens;
    config.nb_lanes = 1;

    crypto_argon2_inputs entrada{};
    entrada.pass = reinterpret_cast<const std::uint8_t*>(senha.data());
    entrada.pass_size = static_cast<std::uint32_t>(senha.size());
    entrada.salt = salt.data();
    entrada.salt_size = static_cast<std::uint32_t>(salt.size());

    std::vector<std::uint8_t> areaDeTrabalho(static_cast<std::size_t>(memoriaKiB) * 1024);
    std::array<std::uint8_t, kTamanhoHash> hash{};
    crypto_argon2(hash.data(), kTamanhoHash, areaDeTrabalho.data(), config, entrada,
                  crypto_argon2_no_extras);
    crypto_wipe(areaDeTrabalho.data(), areaDeTrabalho.size());
    return hash;
}

std::vector<std::string> separar(const std::string& texto, char separador) {
    std::vector<std::string> partes;
    std::string parte;
    std::istringstream in(texto);
    while (std::getline(in, parte, separador)) partes.push_back(parte);
    if (!texto.empty() && texto.back() == separador) partes.emplace_back();
    return partes;
}

// Número decimal positivo de até 9 dígitos (cabe em uint32_t).
std::optional<std::uint32_t> numero(const std::string& s) {
    if (s.empty() || s.size() > 9) return std::nullopt;
    std::uint32_t n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return std::nullopt;
        n = n * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return n;
}

}  // namespace

std::string gerarHashSenha(const std::string& senha) {
    const auto salt = bytesAleatorios(kTamanhoSalt);
    const auto hash = argon2id(senha, salt, kMemoriaKiB, kPassagens);
    return "argon2id$" + std::to_string(kMemoriaKiB) + "$" + std::to_string(kPassagens) + "$" +
           hex::codificar(salt) + "$" + hex::codificar(hash.data(), hash.size());
}

bool verificarSenha(const std::string& senha, const std::string& hashArmazenado) {
    const auto partes = separar(hashArmazenado, '$');
    if (partes.size() != 5 || partes[0] != "argon2id") return false;

    const auto memoria = numero(partes[1]);
    const auto passagens = numero(partes[2]);
    const auto salt = hex::decodificar(partes[3]);
    const auto esperado = hex::decodificar(partes[4]);
    // Argon2 exige memória >= 8 blocos por lane e ao menos 1 passagem.
    if (!memoria || *memoria < 8 || *memoria > kMemoriaMaximaKiB) return false;
    if (!passagens || *passagens < 1 || *passagens > 16) return false;
    if (!salt || salt->size() < 8 || !esperado || esperado->size() != kTamanhoHash) return false;

    const auto calculado = argon2id(senha, *salt, *memoria, *passagens);
    return crypto_verify32(calculado.data(), esperado->data()) == 0;
}

}  // namespace diario
