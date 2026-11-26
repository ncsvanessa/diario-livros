#pragma once

#include <string>

namespace diario {

// Hash de senha com Argon2id, nos parâmetros mínimos recomendados pela OWASP
// (19 MiB de memória, 2 passagens, 1 lane). Cada chamada usa um salt aleatório novo,
// então a mesma senha gera hashes diferentes.
//
// Formato armazenado: argon2id$<memória em KiB>$<passagens>$<salt hex>$<hash hex>.
// Os parâmetros vão junto para que possam ser aumentados no futuro sem invalidar
// os hashes antigos.
std::string gerarHashSenha(const std::string& senha);

// true se `senha` corresponde ao hash. A comparação final é em tempo constante.
// Hash mal formado devolve false (não lança).
bool verificarSenha(const std::string& senha, const std::string& hashArmazenado);

}  // namespace diario
