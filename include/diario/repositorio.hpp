#pragma once

#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include "diario/conta.hpp"
#include "diario/diario.hpp"

namespace diario {

// Cadastro com um login que já existe. Não deriva de std::runtime_error de propósito:
// runtime_error representa falha de armazenamento (503); isto é conflito do pedido (409).
class LoginEmUso : public std::exception {
public:
    const char* what() const noexcept override { return "login já está em uso"; }
};

// Onde contas e diários ficam guardados entre execuções do servidor.
// Cada usuário tem um diário próprio, com contador de ids próprio.
// Falhas de armazenamento lançam std::runtime_error.
class Repositorio {
public:
    virtual ~Repositorio() = default;

    // Cria a conta e devolve o id. `login` já normalizado; lança LoginEmUso se já existir.
    virtual int criarUsuario(const std::string& login, const std::string& senhaHash) = 0;
    virtual std::optional<Usuario> buscarUsuarioPorLogin(const std::string& login) = 0;
    virtual std::optional<Usuario> buscarUsuarioPorId(int id) = 0;

    // Usuários cujo login contém `trecho` (já normalizado), em ordem alfabética de login,
    // no máximo `limite`. Trecho vazio devolve lista vazia.
    virtual std::vector<Usuario> buscarUsuarios(const std::string& trecho, std::size_t limite) = 0;

    // Quem segue quem. Seguir duas vezes ou deixar de seguir quem não segue não muda nada.
    // As regras (ex.: não seguir a si mesmo) ficam na Api; aqui só se guarda a relação.
    virtual void seguir(int seguidorId, int seguidoId) = 0;
    virtual void deixarDeSeguir(int seguidorId, int seguidoId) = 0;
    // Ids em ordem crescente.
    virtual std::vector<int> seguidos(int usuarioId) = 0;    // quem o usuário segue
    virtual std::vector<int> seguidores(int usuarioId) = 0;  // quem segue o usuário

    // Diário vazio se o usuário ainda não salvou nada.
    virtual Diario carregarDiario(int usuarioId) = 0;

    // Substitui todo o diário salvo do usuário pelo estado de `diario`.
    virtual void salvarDiario(int usuarioId, const Diario& diario) = 0;
};

// Persistência em um arquivo JSON local (desenvolvimento e testes e2e).
// Cada operação lê e regrava o arquivo inteiro: simples, e suficiente para poucos usuários.
class RepositorioArquivo : public Repositorio {
public:
    explicit RepositorioArquivo(std::string caminho);

    int criarUsuario(const std::string& login, const std::string& senhaHash) override;
    std::optional<Usuario> buscarUsuarioPorLogin(const std::string& login) override;
    std::optional<Usuario> buscarUsuarioPorId(int id) override;
    std::vector<Usuario> buscarUsuarios(const std::string& trecho, std::size_t limite) override;
    void seguir(int seguidorId, int seguidoId) override;
    void deixarDeSeguir(int seguidorId, int seguidoId) override;
    std::vector<int> seguidos(int usuarioId) override;
    std::vector<int> seguidores(int usuarioId) override;
    Diario carregarDiario(int usuarioId) override;
    void salvarDiario(int usuarioId, const Diario& diario) override;

private:
    nlohmann::json ler() const;
    void gravar(const nlohmann::json& dados) const;

    std::string caminho_;
};

}  // namespace diario
