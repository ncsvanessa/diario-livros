#pragma once

#include <string>

#include "diario/diario.hpp"

namespace diario {

// Onde o diário é guardado entre execuções do servidor.
// `salvar` grava o estado completo (livros e contador de ids); `carregar` o reconstrói.
class Repositorio {
public:
    virtual ~Repositorio() = default;

    // Diário vazio se ainda não houver nada salvo.
    virtual Diario carregar() = 0;

    // Substitui todo o conteúdo salvo pelo estado de `diario`.
    virtual void salvar(const Diario& diario) = 0;
};

// Persistência em arquivo JSON local (comportamento original do sistema).
class RepositorioArquivo : public Repositorio {
public:
    explicit RepositorioArquivo(std::string caminho);

    Diario carregar() override;
    void salvar(const Diario& diario) override;

private:
    std::string caminho_;
};

}  // namespace diario
