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
    // Conecta e cria as tabelas se necessário; lança std::runtime_error se falhar.
    explicit RepositorioPostgres(std::string url);

    Diario carregar() override;

    // Regrava todos os livros numa única transação: ou tudo é salvo, ou nada muda.
    void salvar(const Diario& diario) override;

private:
    std::string url_;
};

}  // namespace diario
