#include "diario/diario.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace diario {

namespace {

bool apenasDigitos(const std::string& s, std::size_t inicio, std::size_t n) {
    for (std::size_t i = inicio; i < inicio + n; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

bool comecaCom(const std::string& s, const std::string& prefixo) {
    return s.size() >= prefixo.size() && s.compare(0, prefixo.size(), prefixo) == 0;
}

std::string minusculo(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Remove espaços nas pontas; usado para não aceitar título "   ".
std::string aparar(const std::string& s) {
    const auto inicio = s.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) return "";
    const auto fim = s.find_last_not_of(" \t\r\n");
    return s.substr(inicio, fim - inicio + 1);
}

std::string juntar(const std::vector<std::string>& partes) {
    std::string out;
    for (std::size_t i = 0; i < partes.size(); ++i) {
        if (i) out += "; ";
        out += partes[i];
    }
    return out;
}

}  // namespace

// ---- Validação --------------------------------------------------------------

bool dataValida(const std::string& data) {
    // AAAA-MM-DD
    if (data.size() != 10 || data[4] != '-' || data[7] != '-') return false;
    if (!apenasDigitos(data, 0, 4) || !apenasDigitos(data, 5, 2) || !apenasDigitos(data, 8, 2))
        return false;

    const int ano = std::stoi(data.substr(0, 4));
    const int mes = std::stoi(data.substr(5, 2));
    const int dia = std::stoi(data.substr(8, 2));

    if (ano < 1 || mes < 1 || mes > 12 || dia < 1) return false;

    const bool bissexto = (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0;
    static const int diasNoMes[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int maxDia = (mes == 2 && bissexto) ? 29 : diasNoMes[mes - 1];
    return dia <= maxDia;
}

bool notaValida(int nota) { return nota >= 0 && nota <= 5; }

bool capaValida(const std::string& url) {
    return url.empty() || comecaCom(url, "http://") || comecaCom(url, "https://") ||
           comecaCom(url, "data:image/");
}

std::vector<std::string> validar(const Livro& l) {
    std::vector<std::string> erros;
    if (aparar(l.titulo).empty()) erros.emplace_back("título é obrigatório");
    if (aparar(l.autor).empty()) erros.emplace_back("autor é obrigatório");
    if (!notaValida(l.nota)) erros.emplace_back("nota deve estar entre 0 e 5");
    if (!capaValida(l.capaUrl)) erros.emplace_back("capa deve ser uma URL http(s) ou data:image");
    if (!l.dataLancamento.empty() && !dataValida(l.dataLancamento))
        erros.emplace_back("data de lançamento inválida (use AAAA-MM-DD)");
    if (!l.dataLeitura.empty() && !dataValida(l.dataLeitura))
        erros.emplace_back("data de leitura inválida (use AAAA-MM-DD)");

    // Datas ISO podem ser comparadas como string.
    if (dataValida(l.dataLancamento) && dataValida(l.dataLeitura) &&
        l.dataLeitura < l.dataLancamento) {
        erros.emplace_back("data de leitura não pode ser anterior à de lançamento");
    }
    return erros;
}

// ---- JSON -------------------------------------------------------------------

void to_json(nlohmann::json& j, const Livro& l) {
    j = nlohmann::json{{"id", l.id},
                       {"titulo", l.titulo},
                       {"autor", l.autor},
                       {"capaUrl", l.capaUrl},
                       {"dataLancamento", l.dataLancamento},
                       {"dataLeitura", l.dataLeitura},
                       {"nota", l.nota}};
}

void from_json(const nlohmann::json& j, Livro& l) {
    // Campos ausentes recebem o valor padrão; tipos errados lançam nlohmann::json::type_error.
    l.id = j.value("id", 0);
    l.titulo = aparar(j.value("titulo", std::string{}));
    l.autor = aparar(j.value("autor", std::string{}));
    l.capaUrl = aparar(j.value("capaUrl", std::string{}));
    l.dataLancamento = j.value("dataLancamento", std::string{});
    l.dataLeitura = j.value("dataLeitura", std::string{});
    l.nota = j.value("nota", 0);
}

// ---- Diário -----------------------------------------------------------------

int Diario::adicionar(Livro livro) {
    const auto erros = validar(livro);
    if (!erros.empty()) throw std::invalid_argument(juntar(erros));
    livro.id = proximoId_++;
    livros_.push_back(std::move(livro));
    return livros_.back().id;
}

bool Diario::atualizar(int id, Livro livro) {
    auto it = std::find_if(livros_.begin(), livros_.end(),
                           [id](const Livro& l) { return l.id == id; });
    if (it == livros_.end()) return false;
    const auto erros = validar(livro);
    if (!erros.empty()) throw std::invalid_argument(juntar(erros));
    livro.id = id;
    *it = std::move(livro);
    return true;
}

bool Diario::remover(int id) {
    const auto antes = livros_.size();
    livros_.erase(std::remove_if(livros_.begin(), livros_.end(),
                                 [id](const Livro& l) { return l.id == id; }),
                  livros_.end());
    return livros_.size() != antes;
}

std::optional<Livro> Diario::buscar(int id) const {
    auto it = std::find_if(livros_.begin(), livros_.end(),
                           [id](const Livro& l) { return l.id == id; });
    if (it == livros_.end()) return std::nullopt;
    return *it;
}

std::vector<Livro> Diario::pesquisar(const std::string& termo) const {
    const auto t = minusculo(aparar(termo));
    if (t.empty()) return livros_;
    std::vector<Livro> out;
    std::copy_if(livros_.begin(), livros_.end(), std::back_inserter(out), [&t](const Livro& l) {
        return minusculo(l.titulo).find(t) != std::string::npos ||
               minusculo(l.autor).find(t) != std::string::npos;
    });
    return out;
}

std::vector<Livro> Diario::ordenadosPorNota() const {
    auto out = livros_;
    // stable_sort mantém a ordem de inserção entre livros com mesma nota e título.
    std::stable_sort(out.begin(), out.end(), [](const Livro& a, const Livro& b) {
        if (a.nota != b.nota) return a.nota > b.nota;
        return a.titulo < b.titulo;
    });
    return out;
}

double Diario::mediaNotas() const {
    if (livros_.empty()) return 0.0;
    const int soma = std::accumulate(livros_.begin(), livros_.end(), 0,
                                     [](int acc, const Livro& l) { return acc + l.nota; });
    return static_cast<double>(soma) / static_cast<double>(livros_.size());
}

nlohmann::json Diario::paraJson() const {
    return nlohmann::json{{"proximoId", proximoId_}, {"livros", livros_}};
}

Diario Diario::deJson(const nlohmann::json& j) {
    Diario d;
    int maiorId = 0;
    for (const auto& item : j.value("livros", nlohmann::json::array())) {
        Livro l = item.get<Livro>();
        const auto erros = validar(l);
        if (!erros.empty()) throw std::invalid_argument("livro inválido no arquivo: " + juntar(erros));
        maiorId = std::max(maiorId, l.id);
        d.livros_.push_back(std::move(l));
    }
    // Garante que ids novos nunca colidam com os existentes, mesmo com arquivo editado à mão.
    d.proximoId_ = std::max(j.value("proximoId", 1), maiorId + 1);
    return d;
}

void Diario::salvar(const std::string& caminho) const {
    std::ofstream out(caminho, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("não foi possível abrir " + caminho + " para escrita");
    out << paraJson().dump(2);
}

Diario Diario::carregar(const std::string& caminho) {
    std::ifstream in(caminho, std::ios::binary);
    if (!in) return Diario{};
    return deJson(nlohmann::json::parse(in));
}

}  // namespace diario
