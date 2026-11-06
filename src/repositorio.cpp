#include "diario/repositorio.hpp"

#include <utility>

namespace diario {

RepositorioArquivo::RepositorioArquivo(std::string caminho) : caminho_(std::move(caminho)) {}

Diario RepositorioArquivo::carregar() { return Diario::carregar(caminho_); }

void RepositorioArquivo::salvar(const Diario& diario) { diario.salvar(caminho_); }

}  // namespace diario
