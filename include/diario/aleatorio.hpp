#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace diario {

// `n` bytes do gerador criptográfico do sistema operacional: /dev/urandom no Linux e no
// macOS, BCryptGenRandom no Windows. Usado para salts de senha e segredos de sessão.
//
// Não usa std::random_device: o padrão não garante que ele seja criptográfico (versões
// antigas do MinGW chegaram a devolver sempre a mesma sequência).
//
// Lança std::runtime_error se o gerador do sistema não puder ser usado.
std::vector<std::uint8_t> bytesAleatorios(std::size_t n);

}  // namespace diario
