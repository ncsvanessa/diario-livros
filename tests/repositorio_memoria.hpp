#pragma once

// Dublês do Repositorio para os testes da Api: o armazenamento é a dependência externa
// (arquivo/banco), então é o único ponto substituído. O núcleo (Diario) é o real.

#include <stdexcept>

#include "diario/repositorio.hpp"

namespace diario::teste {

// Guarda o último diário salvo em memória.
class RepositorioMemoria : public Repositorio {
public:
    Diario carregar() override { return salvo; }
    void salvar(const Diario& diario) override {
        if (falharAoSalvar) throw std::runtime_error("armazenamento indisponível (simulado)");
        salvo = diario;
        ++gravacoes;
    }

    Diario salvo;
    int gravacoes = 0;
    bool falharAoSalvar = false;  // simula banco fora do ar
};

}  // namespace diario::teste
