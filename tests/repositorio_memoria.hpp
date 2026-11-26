#pragma once

// Dublê do Repositorio para os testes da Api: o armazenamento é a dependência externa
// (arquivo/banco), então é o único ponto substituído. O núcleo (Diario) é o real.

#include <map>
#include <stdexcept>

#include "diario/repositorio.hpp"

namespace diario::teste {

class RepositorioMemoria : public Repositorio {
public:
    int criarUsuario(const std::string& login, const std::string& senhaHash) override {
        falharSeIndisponivel();
        for (const auto& [id, u] : usuarios) {
            if (u.login == login) throw LoginEmUso{};
        }
        const int id = static_cast<int>(usuarios.size()) + 1;
        usuarios[id] = Usuario{id, login, senhaHash};
        return id;
    }

    std::optional<Usuario> buscarUsuarioPorLogin(const std::string& login) override {
        falharSeIndisponivel();
        for (const auto& [id, u] : usuarios) {
            if (u.login == login) return u;
        }
        return std::nullopt;
    }

    std::optional<Usuario> buscarUsuarioPorId(int id) override {
        falharSeIndisponivel();
        const auto it = usuarios.find(id);
        if (it == usuarios.end()) return std::nullopt;
        return it->second;
    }

    Diario carregarDiario(int usuarioId) override {
        falharSeIndisponivel();
        return diarios[usuarioId];
    }

    void salvarDiario(int usuarioId, const Diario& diario) override {
        falharSeIndisponivel();
        diarios[usuarioId] = diario;
    }

    std::map<int, Usuario> usuarios;
    std::map<int, Diario> diarios;
    bool indisponivel = false;  // simula banco fora do ar

private:
    void falharSeIndisponivel() const {
        if (indisponivel) throw std::runtime_error("armazenamento indisponível (simulado)");
    }
};

}  // namespace diario::teste
