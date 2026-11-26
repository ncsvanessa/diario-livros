#include "diario/aleatorio.hpp"

#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <limits>
#else
#include <fstream>
#endif

namespace diario {

std::vector<std::uint8_t> bytesAleatorios(std::size_t n) {
    std::vector<std::uint8_t> bytes(n);
    if (n == 0) return bytes;

#ifdef _WIN32
    if (n > std::numeric_limits<ULONG>::max()) throw std::runtime_error("pedido de bytes aleatórios grande demais");
    const NTSTATUS status = BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(n),
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) throw std::runtime_error("BCryptGenRandom falhou");
#else
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(n))) {
        throw std::runtime_error("não foi possível ler /dev/urandom");
    }
#endif
    return bytes;
}

}  // namespace diario
