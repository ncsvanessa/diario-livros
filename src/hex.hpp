#pragma once

// Uso interno: conversão entre bytes e texto hexadecimal (minúsculo).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace diario::hex {

inline std::string codificar(const std::uint8_t* dados, std::size_t n) {
    static const char digitos[] = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(digitos[dados[i] >> 4]);
        out.push_back(digitos[dados[i] & 0x0f]);
    }
    return out;
}

inline std::string codificar(const std::vector<std::uint8_t>& dados) {
    return codificar(dados.data(), dados.size());
}

// nullopt se o texto tiver tamanho ímpar ou caracteres fora de [0-9a-f].
inline std::optional<std::vector<std::uint8_t>> decodificar(const std::string& texto) {
    if (texto.size() % 2 != 0) return std::nullopt;
    const auto valor = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    std::vector<std::uint8_t> out;
    out.reserve(texto.size() / 2);
    for (std::size_t i = 0; i < texto.size(); i += 2) {
        const int alto = valor(texto[i]);
        const int baixo = valor(texto[i + 1]);
        if (alto < 0 || baixo < 0) return std::nullopt;
        out.push_back(static_cast<std::uint8_t>(alto * 16 + baixo));
    }
    return out;
}

}  // namespace diario::hex
