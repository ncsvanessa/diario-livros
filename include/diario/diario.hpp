#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace diario {

// Um livro registrado no diário de leitura.
struct Livro {
    int id = 0;                  // atribuído pelo Diario
    std::string titulo;          // obrigatório
    std::string autor;           // obrigatório
    std::string capaUrl;         // opcional: http(s)://... ou data:image/...
    std::string dataLancamento;  // opcional: AAAA-MM-DD
    std::string dataLeitura;     // opcional: AAAA-MM-DD
    int nota = 0;                // 0 a 5 estrelas
};

// ---- Validação --------------------------------------------------------------

// true se `data` está no formato AAAA-MM-DD e é uma data real (considera bissextos).
bool dataValida(const std::string& data);

// true se 0 <= nota <= 5.
bool notaValida(int nota);

// true se vazia, http(s)://... ou data:image/...
bool capaValida(const std::string& url);

// Lista de mensagens de erro; vazia se o livro é válido.
std::vector<std::string> validar(const Livro& livro);

// ---- JSON -------------------------------------------------------------------

void to_json(nlohmann::json& j, const Livro& l);
void from_json(const nlohmann::json& j, Livro& l);

// ---- Diário -----------------------------------------------------------------

class Diario {
public:
    // Valida e adiciona. Retorna o id atribuído.
    // Lança std::invalid_argument com as mensagens de erro se o livro for inválido.
    int adicionar(Livro livro);

    // Substitui os dados do livro `id` (o id é preservado). false se não existir.
    // Lança std::invalid_argument se os novos dados forem inválidos.
    bool atualizar(int id, Livro livro);

    bool remover(int id);

    std::optional<Livro> buscar(int id) const;

    // Em ordem de inserção.
    const std::vector<Livro>& listar() const { return livros_; }

    // Busca case-insensitive (ASCII) por trecho do título ou do autor.
    std::vector<Livro> pesquisar(const std::string& termo) const;

    // Maior nota primeiro; empate desempatado por título.
    std::vector<Livro> ordenadosPorNota() const;

    // Média das notas; 0.0 se o diário estiver vazio.
    double mediaNotas() const;

    std::size_t tamanho() const { return livros_.size(); }

    nlohmann::json paraJson() const;
    static Diario deJson(const nlohmann::json& j);

    // Persistência em arquivo. `carregar` retorna um diário vazio se o arquivo não existir.
    void salvar(const std::string& caminho) const;
    static Diario carregar(const std::string& caminho);

private:
    std::vector<Livro> livros_;
    int proximoId_ = 1;
};

}  // namespace diario
