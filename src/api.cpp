#include "diario/api.hpp"

#include <cctype>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "diario/aleatorio.hpp"
#include "diario/conta.hpp"
#include "diario/senha.hpp"
#include "hex.hpp"

namespace diario {

using nlohmann::json;

namespace {

Resposta erro(int status, const std::string& mensagem) { return {status, json{{"erro", mensagem}}, {}}; }

Resposta naoAutenticado() { return erro(401, "faça login para continuar"); }

// Falha de gravação (ex.: banco fora do ar) é problema do servidor, não do pedido.
Resposta falhaNoArmazenamento(const std::exception& e) {
    std::cerr << "Erro no armazenamento: " << e.what() << '\n';
    return erro(503, "não foi possível acessar os dados; tente novamente");
}

std::string parametro(const Requisicao& req, const std::string& nome) {
    const auto it = req.parametros.find(nome);
    return it == req.parametros.end() ? "" : it->second;
}

// Valor de um cookie no cabeçalho "Cookie: a=1; sessao=xyz; b=2"; vazio se ausente.
std::string valorDoCookie(const std::string& cabecalho, const std::string& nome) {
    std::size_t inicio = 0;
    while (inicio < cabecalho.size()) {
        auto fim = cabecalho.find(';', inicio);
        if (fim == std::string::npos) fim = cabecalho.size();
        auto par = cabecalho.substr(inicio, fim - inicio);
        const auto primeiro = par.find_first_not_of(' ');
        par = primeiro == std::string::npos ? "" : par.substr(primeiro);
        const auto igual = par.find('=');
        if (igual != std::string::npos && par.substr(0, igual) == nome) return par.substr(igual + 1);
        inicio = fim + 1;
    }
    return "";
}

std::string juntar(const std::vector<std::string>& partes) {
    std::string out;
    for (std::size_t i = 0; i < partes.size(); ++i) out += (i ? "; " : "") + partes[i];
    return out;
}

// {"login": ..., "senha": ...} com campos ausentes tratados como vazios.
// Lança json::exception se o corpo não for JSON ou os campos não forem texto.
std::pair<std::string, std::string> credenciais(const Requisicao& req) {
    // value() também lança json::exception se o corpo não for um objeto.
    const auto j = json::parse(req.corpo);
    return {j.value("login", std::string{}), j.value("senha", std::string{})};
}

// Instante em UTC no formato AAAA-MM-DDTHH:MM:SSZ: em texto ISO, a ordem alfabética
// coincide com a cronológica.
std::string instanteIso(AssinadorSessao::Instante instante) {
    const std::time_t t = std::chrono::system_clock::to_time_t(instante);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char texto[sizeof "AAAA-MM-DDTHH:MM:SSZ"];
    std::strftime(texto, sizeof texto, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return texto;
}

}  // namespace

Api::Api(Repositorio& repo, OpcoesApi opcoes)
    : repo_(repo),
      opcoes_(std::move(opcoes)),
      assinador_(opcoes_.segredoSessao),
      hashFicticio_(gerarHashSenha(hex::codificar(bytesAleatorios(16)))) {}

// ---- Sessão -------------------------------------------------------------------

std::optional<int> Api::usuarioDaSessao(const Requisicao& req) const {
    const auto token = valorDoCookie(req.cookie, kNomeCookie);
    if (token.empty()) return std::nullopt;
    return assinador_.validar(token, opcoes_.relogio());
}

std::string Api::cookieDeSessao(int usuarioId) const {
    const auto segundos = std::chrono::duration_cast<std::chrono::seconds>(AssinadorSessao::kValidade).count();
    // HttpOnly: o JavaScript da página não lê o token (protege contra XSS).
    // SameSite=Lax: o navegador não envia o cookie em POST/PUT/DELETE vindos de outro site (CSRF).
    return std::string(kNomeCookie) + "=" + assinador_.emitir(usuarioId, opcoes_.relogio()) +
           "; Path=/; Max-Age=" + std::to_string(segundos) + "; HttpOnly; SameSite=Lax" +
           (opcoes_.cookieSeguro ? "; Secure" : "");
}

Diario& Api::diarioDe(int usuarioId) {
    auto it = diarios_.find(usuarioId);
    if (it == diarios_.end()) it = diarios_.emplace(usuarioId, repo_.carregarDiario(usuarioId)).first;
    return it->second;
}

// ---- Contas -------------------------------------------------------------------

Resposta Api::cadastrar(const Requisicao& req) {
    try {
        auto [login, senha] = credenciais(req);
        login = normalizarLogin(login);
        const auto erros = validarCadastro(login, senha);
        if (!erros.empty()) return erro(400, juntar(erros));

        // O hash é lento de propósito (Argon2); é calculado fora da trava para não
        // bloquear as requisições dos outros usuários.
        const auto hash = gerarHashSenha(senha);
        int id = 0;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            id = repo_.criarUsuario(login, hash);
        }
        return {201, json{{"login", login}}, {cookieDeSessao(id)}};
    } catch (const json::exception&) {
        return erro(400, "JSON inválido");
    } catch (const LoginEmUso& e) {
        return erro(409, e.what());
    } catch (const std::runtime_error& e) {
        return falhaNoArmazenamento(e);
    }
}

Resposta Api::entrar(const Requisicao& req) {
    try {
        auto [login, senha] = credenciais(req);
        login = normalizarLogin(login);

        std::optional<Usuario> usuario;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            usuario = repo_.buscarUsuarioPorLogin(login);
        }
        const bool senhaConfere = verificarSenha(senha, usuario ? usuario->senhaHash : hashFicticio_);
        // Mesma mensagem para login inexistente e senha errada: não revela quais logins existem.
        if (!usuario || !senhaConfere) return erro(401, "login ou senha incorretos");

        return {200, json{{"login", usuario->login}}, {cookieDeSessao(usuario->id)}};
    } catch (const json::exception&) {
        return erro(400, "JSON inválido");
    } catch (const std::runtime_error& e) {
        return falhaNoArmazenamento(e);
    }
}

Resposta Api::sair(const Requisicao&) {
    // Max-Age=0 faz o navegador apagar o cookie.
    return {204, nullptr, {std::string(kNomeCookie) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax" +
                               (opcoes_.cookieSeguro ? "; Secure" : "")}};
}

Resposta Api::usuarioAtual(const Requisicao& req) {
    const auto id = usuarioDaSessao(req);
    if (!id) return naoAutenticado();
    try {
        std::lock_guard<std::mutex> lock(mtx_);
        const auto usuario = repo_.buscarUsuarioPorId(*id);
        if (!usuario) return naoAutenticado();
        return {200, json{{"login", usuario->login}}, {}};
    } catch (const std::runtime_error& e) {
        return falhaNoArmazenamento(e);
    }
}

// ---- Livros -------------------------------------------------------------------

Resposta Api::listarLivros(const Requisicao& req) {
    const auto usuario = usuarioDaSessao(req);
    if (!usuario) return naoAutenticado();

    const auto termo = parametro(req, "q");
    const auto ordem = parametro(req, "ordem");
    const auto notaMin = parametro(req, "notaMin");
    // Aceita só um dígito; o intervalo 0 a 5 é validado pelo núcleo.
    if (!notaMin.empty() && (notaMin.size() != 1 || !std::isdigit(static_cast<unsigned char>(notaMin[0])))) {
        return erro(400, "notaMin deve ser um inteiro de 0 a 5");
    }

    try {
        std::lock_guard<std::mutex> lock(mtx_);
        const auto& diario = diarioDe(*usuario);
        auto livros = ordem == "nota" ? diario.ordenadosPorNota() : diario.listar();
        // Cada filtro devolve um subconjunto; mantém só os livros presentes nele,
        // preservando a ordenação escolhida.
        const auto manterSo = [&livros](const std::vector<Livro>& filtrados) {
            std::vector<Livro> out;
            for (const auto& l : livros) {
                for (const auto& f : filtrados) {
                    if (f.id == l.id) { out.push_back(l); break; }
                }
            }
            livros = std::move(out);
        };
        if (!termo.empty()) manterSo(diario.pesquisar(termo));
        if (!notaMin.empty()) manterSo(diario.comNotaMinima(notaMin[0] - '0'));
        return {200, livros, {}};
    } catch (const std::invalid_argument& e) {
        return erro(400, e.what());
    } catch (const std::runtime_error& e) {
        return falhaNoArmazenamento(e);
    }
}

Resposta Api::estatisticas(const Requisicao& req) {
    const auto usuario = usuarioDaSessao(req);
    if (!usuario) return naoAutenticado();
    try {
        std::lock_guard<std::mutex> lock(mtx_);
        const auto& diario = diarioDe(*usuario);
        return {200,
                json{{"total", diario.tamanho()},
                     {"media", diario.mediaNotas()},
                     {"autoresMaisLidos", diario.autoresMaisLidos(3)}},
                {}};
    } catch (const std::runtime_error& e) {
        return falhaNoArmazenamento(e);
    }
}

Resposta Api::criarLivro(const Requisicao& req) {
    const auto usuario = usuarioDaSessao(req);
    if (!usuario) return naoAutenticado();
    try {
        auto livro = json::parse(req.corpo).get<Livro>();
        // Definido pelo servidor: um valor enviado pelo cliente é ignorado.
        livro.registradoEm = instanteIso(opcoes_.relogio());
        std::lock_guard<std::mutex> lock(mtx_);
        auto& diario = diarioDe(*usuario);
        // Altera uma cópia e só a adota depois de salvar: se a gravação falhar,
        // a memória continua igual ao que está persistido.
        auto novo = diario;
        const int id = novo.adicionar(std::move(livro));
        repo_.salvarDiario(*usuario, novo);
        diario = std::move(novo);
        return {201, *diario.buscar(id), {}};
    } catch (const std::invalid_argument& e) {
        return erro(400, e.what());
    } catch (const json::exception&) {
        return erro(400, "JSON inválido");
    } catch (const std::runtime_error& e) {
        return falhaNoArmazenamento(e);
    }
}

Resposta Api::atualizarLivro(const Requisicao& req, int id) {
    const auto usuario = usuarioDaSessao(req);
    if (!usuario) return naoAutenticado();
    try {
        auto livro = json::parse(req.corpo).get<Livro>();
        std::lock_guard<std::mutex> lock(mtx_);
        auto& diario = diarioDe(*usuario);
        auto novo = diario;
        if (!novo.atualizar(id, std::move(livro))) return erro(404, "livro não encontrado");
        repo_.salvarDiario(*usuario, novo);
        diario = std::move(novo);
        return {200, *diario.buscar(id), {}};
    } catch (const std::invalid_argument& e) {
        return erro(400, e.what());
    } catch (const json::exception&) {
        return erro(400, "JSON inválido");
    } catch (const std::runtime_error& e) {
        return falhaNoArmazenamento(e);
    }
}

Resposta Api::removerLivro(const Requisicao& req, int id) {
    const auto usuario = usuarioDaSessao(req);
    if (!usuario) return naoAutenticado();
    try {
        std::lock_guard<std::mutex> lock(mtx_);
        auto& diario = diarioDe(*usuario);
        auto novo = diario;
        if (!novo.remover(id)) return erro(404, "livro não encontrado");
        repo_.salvarDiario(*usuario, novo);
        diario = std::move(novo);
        return {204, nullptr, {}};
    } catch (const std::runtime_error& e) {
        return falhaNoArmazenamento(e);
    }
}

}  // namespace diario
