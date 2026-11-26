#pragma once

// Dublê do Repositorio para os testes da Api: o armazenamento é a dependência externa
// (arquivo/banco), então é o único ponto substituído. O núcleo (Diario) é o real.

#include <algorithm>
#include <map>
#include <set>
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

    std::vector<Usuario> buscarUsuarios(const std::string& trecho, std::size_t limite) override {
        falharSeIndisponivel();
        std::vector<Usuario> encontrados;
        if (trecho.empty()) return encontrados;
        // std::map já percorre por id; ordena por login como os repositórios reais.
        for (const auto& [id, u] : usuarios) {
            if (u.login.find(trecho) != std::string::npos) encontrados.push_back(u);
        }
        std::sort(encontrados.begin(), encontrados.end(),
                  [](const Usuario& a, const Usuario& b) { return a.login < b.login; });
        if (encontrados.size() > limite) encontrados.resize(limite);
        return encontrados;
    }

    void seguir(int seguidorId, int seguidoId) override {
        falharSeIndisponivel();
        seguindo[seguidorId].insert(seguidoId);
    }

    void deixarDeSeguir(int seguidorId, int seguidoId) override {
        falharSeIndisponivel();
        seguindo[seguidorId].erase(seguidoId);
    }

    std::vector<int> seguidos(int usuarioId) override {
        falharSeIndisponivel();
        const auto& s = seguindo[usuarioId];
        return {s.begin(), s.end()};
    }

    std::vector<int> seguidores(int usuarioId) override {
        falharSeIndisponivel();
        std::vector<int> ids;
        for (const auto& [seguidor, seguidos] : seguindo) {
            if (seguidos.count(usuarioId)) ids.push_back(seguidor);
        }
        return ids;
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
    std::map<int, std::set<int>> seguindo;  // seguidor -> seguidos
    bool indisponivel = false;  // simula banco fora do ar

private:
    void falharSeIndisponivel() const {
        if (indisponivel) throw std::runtime_error("armazenamento indisponível (simulado)");
    }
};

}  // namespace diario::teste
