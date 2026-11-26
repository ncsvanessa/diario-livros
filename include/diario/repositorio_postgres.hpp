#pragma once

#include <string>

#include "diario/repositorio.hpp"

namespace diario {

// Persistência em PostgreSQL via libpq (compilado só com DIARIO_WITH_POSTGRES=ON).
//
// Abre uma conexão por operação: o servidor faz poucas escritas, e conexões longas
// cairiam quando o banco gerenciado (ex.: Neon) suspende o compute por inatividade.
class RepositorioPostgres : public Repositorio {
public:
    // `url` no formato postgresql://usuario:senha@host:porta/banco?sslmode=require.
    // Conecta e cria as tabelas se necessário. Lança std::runtime_error se não conseguir
    // conectar ou se o banco ainda tiver o esquema antigo, sem usuários.
    explicit RepositorioPostgres(std::string url);

    int criarUsuario(const std::string& login, const std::string& senhaHash) override;
    std::optional<Usuario> buscarUsuarioPorLogin(const std::string& login) override;
    std::optional<Usuario> buscarUsuarioPorId(int id) override;
    Diario carregarDiario(int usuarioId) override;

    // Regrava os livros do usuário numa única transação: ou tudo é salvo, ou nada muda.
    void salvarDiario(int usuarioId, const Diario& diario) override;

private:
    std::string url_;
};

}  // namespace diario
