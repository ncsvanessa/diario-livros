# Diário de Livros

[![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

Rede de leitura em C++: cada pessoa registra os livros que leu, dá nota e acompanha o que os amigos estão lendo.

| | |
|---|---|
| **Sistema em produção** | https://diario-livros.onrender.com |
| **Integração contínua** | [GitHub Actions](../../actions/workflows/ci.yml) |
| **Vídeo de apresentação** | [PREENCHER: link do YouTube] |

Trabalho prático de Engenharia de Software II (DCC/UFMG).

> No plano gratuito do Render, o servidor "dorme" após 15 minutos sem acesso. A primeira página depois disso pode levar cerca de 1 minuto para abrir.

## Integrantes

- Artur Fonseca Souza - 2022035520
- Lucas Dolabella de Castro Lopes - 2023001549
- Vanessa Nascimento Silva - 202300186
 
## Sobre o sistema

O Diário de Livros é uma aplicação web para registrar leituras. Cada pessoa tem uma conta e um diário próprio, pode ver o diário de outras pessoas e seguir quem quiser para acompanhar o que estão lendo.

Funcionalidades:

- **Contas:** cadastro e login com usuário e senha. A sessão fica num cookie assinado e dura 7 dias.
- **Diário de leituras:** registrar, editar e remover livros com título, autor, capa (URL ou arquivo), data de lançamento, data de leitura e nota de 0 a 5 estrelas.
- **Sugestões ao digitar o título:** busca o livro na [Open Library](https://openlibrary.org) e preenche título, autor, capa e ano de lançamento.
- **Busca e filtros:** por título ou autor, ordenação por nota e filtro por nota mínima.
- **Estatísticas:** total de livros, nota média e os três autores mais lidos.
- **Pessoas:** buscar outros usuários pelo nome e abrir o perfil de cada um, com os livros e as estatísticas dele.
- **Seguir:** seguir e deixar de seguir outras pessoas.
- **Feed:** os livros registrados por quem você segue, do mais recente para o mais antigo.

Todo diário é visível para qualquer usuário logado. Só o dono pode alterar o próprio diário.

### Limitações conhecidas

- Sair apaga o cookie de sessão, mas o token continua válido até expirar (7 dias), porque não há estado de sessão no servidor.
- Não há recuperação de senha, verificação de e-mail nem limite de tentativas de login.
- No plano gratuito do Render, o servidor dorme após 15 minutos sem acesso.

## Tecnologias

| Camada | Tecnologia | Para quê |
|---|---|---|
| Linguagem | C++17 (também compila em C++20) | Núcleo e servidor |
| Build | CMake 3.20+ com FetchContent | Compila em Linux, macOS e Windows e baixa as dependências sozinho |
| Servidor HTTP | [cpp-httplib](https://github.com/yhirose/cpp-httplib) 0.18 | API REST e arquivos da interface |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) 3.11 | Corpo das requisições e arquivo de dados |
| Senhas e sessões | [Monocypher](https://monocypher.org) 4.0 | Hash de senha com Argon2id e assinatura do cookie de sessão com BLAKE2b |
| Banco de dados | PostgreSQL (libpq) | Persistência em produção. Localmente, um arquivo JSON |
| Interface | HTML, CSS e JavaScript, sem framework | Uma única página em `web/index.html` |
| Testes de unidade | [GoogleTest](https://github.com/google/googletest) 1.15 | Núcleo, API, contas, segurança e repositórios |
| Testes e2e | [Playwright](https://playwright.dev) 1.49 (Chromium) | Fluxos completos no navegador contra o servidor real |
| Cobertura | [gcovr](https://gcovr.com) 8 (gcov / llvm-cov) | Mede a cobertura de linhas e falha abaixo de 80% |
| CI/CD | GitHub Actions | Build, testes e cobertura em Ubuntu, macOS e Windows. Deploy automático da `main` |
| Produção | Docker + [Render](https://render.com) + [Neon](https://neon.tech) | Imagem Debian com o servidor (Render) e PostgreSQL gerenciado (Neon) |

### Arquitetura

```
navegador ──HTTP──▶ src/main.cpp ──▶ diario::Api ──▶ Diario (regras dos livros)
 (web/index.html)   (cpp-httplib)      (rotas)    └─▶ Repositorio ─┬─▶ arquivo JSON (local, e2e)
                                                                  └─▶ PostgreSQL (produção)
```

O `main.cpp` só traduz entre o cpp-httplib e a classe `Api`. Toda a lógica das rotas (sessão, validação, status HTTP) fica na `Api`, que é testada por unidade com um repositório em memória, sem subir servidor nem banco.

| Caminho | Conteúdo |
|---|---|
| `include/diario/diario.hpp`, `src/diario.cpp` | Núcleo: modelo `Livro`, validação e classe `Diario` |
| `include/diario/api.hpp`, `src/api.cpp` | Regras das rotas `/api/*` |
| `include/diario/conta.hpp`, `senha.hpp`, `sessao.hpp`, `aleatorio.hpp` | Contas, hash de senha, token de sessão e bytes aleatórios do sistema operacional |
| `include/diario/repositorio*.hpp`, `src/repositorio*.cpp` | Persistência em arquivo JSON e em PostgreSQL |
| `src/main.cpp` | Servidor HTTP: liga as rotas do cpp-httplib à classe `Api` |
| `web/index.html` | Interface |
| `tests/` | Testes de unidade (GoogleTest) |
| `e2e/` | Testes end-to-end (Playwright) |
| `.github/workflows/ci.yml` | Pipeline de CI/CD |
| `Dockerfile` | Imagem de produção |

## Uso de IA

| Integrante | Ferramenta | Como contribuiu
|---|---|---|
| Vanessa | Claude (Anthropic) | Plano de adaptação do sistema aos requisitos do TP; medição de cobertura com gcovr e CI nos 3 sistemas; persistência em PostgreSQL, `Dockerfile` e deploy (Render + Neon); contas de usuário (Argon2id, cookie de sessão assinado, extração da classe `Api`) com testes de unidade e e2e; revisão final do repositório |
| Artur | [PREENCHER] | [PREENCHER] |
| Lucas | Claude Code (Anthropic) | Sugestões de livros da Open Library ao digitar o título, parte social (busca de pessoas, perfil, seguir e feed), da API à interface, com testes de unidade e e2e; script de dados para o vídeo; etapas definidas, revisadas e testadas por mim antes de cada commit |

Como a IA foi usada no processo:

- **Desenho antes do código.** As decisões com mais de um caminho possível foram discutidas antes de implementar: banco gratuito sem expiração, algoritmo de hash de senha, sessão com ou sem estado no servidor.
- **Commits pequenos.** O código gerado foi aplicado em commits pequenos, um por etapa. Por isso alguns trechos do histórico têm vários commits em poucos minutos, como o login em 26/11.
- **Validação pelos testes.** Nenhum código entrou sem passar pelo CI completo: testes de unidade, cobertura, e2e e imagem Docker nos 3 sistemas.

## Execução dos testes

Requisitos: CMake 3.20+, um compilador C++17 (GCC, Clang ou MSVC), Git e, para os testes e2e, Node.js 18+.

### Resumo

| Tipo | Quantidade | Ferramenta | Onde |
|---|---|---|---|
| Unidade | 246 casos (178 testes, alguns parametrizados) | GoogleTest | `tests/` |
| Integração com PostgreSQL | 21 | GoogleTest | `tests/test_repositorio_postgres.cpp` |
| End-to-end | 32 | Playwright | `e2e/tests/` |
| Cobertura de linhas | ~95% (mínimo exigido: 80%) | gcovr | `gcovr.cfg` |

### Testes de unidade

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

| Arquivo | O que testa |
|---|---|
| `tests/test_diario.cpp` | Validação de livros, operações do diário, busca, filtros, estatísticas e persistência em JSON |
| `tests/test_api.cpp` | Todas as rotas: cadastro, login, cookie, sessão expirada ou adulterada, isolamento entre usuários, livros, perfil, seguir e feed. Também falhas do armazenamento (503) |
| `tests/test_repositorio.cpp` | Repositório em arquivo: contas, diários por usuário, seguir, arquivo antigo ou corrompido |
| `tests/test_senha.cpp` | Hash Argon2id. Inclui a comparação com um hash gerado pela implementação de referência |
| `tests/test_sessao.cpp` | Token de sessão: validade, expiração e adulteração |
| `tests/test_conta.cpp` | Regras de login e senha |
| `tests/test_aleatorio.cpp` | Bytes aleatórios do sistema operacional |

Os testes seguem Arrange-Act-Assert e testam a API pública de cada módulo. Os da `Api` usam um repositório em memória (`tests/repositorio_memoria.hpp`), que substitui só o armazenamento. O tempo é injetado, para testar a expiração da sessão sem esperar.

### Testes de integração com PostgreSQL

São compilados só com `-DDIARIO_WITH_POSTGRES=ON`, que exige a libpq. Precisam de um banco em `DIARIO_TEST_DATABASE_URL`:

```bash
cmake -S . -B build-pg -DDIARIO_WITH_POSTGRES=ON
cmake --build build-pg
DIARIO_TEST_DATABASE_URL=postgresql://postgres:postgres@localhost:5432/postgres ctest --test-dir build-pg
```

Cada teste cria usuários com login aleatório, então não depende do estado deixado por outros testes. Não aponte essa variável para o banco de produção.

### Cobertura

```bash
pip install -r requirements-dev.txt
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DDIARIO_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov
gcovr build-cov                                         # Linux (GCC)
gcovr --gcov-executable "xcrun llvm-cov gcov" build-cov  # macOS (AppleClang)
```

O `gcovr.cfg` mede só o código do sistema (`src/` e `include/`), não as dependências nem os testes, e falha se a cobertura de linhas ficar abaixo de 80%. A cobertura atual é de cerca de 95%.

O `src/main.cpp` não é instrumentado. Ele só liga as rotas do cpp-httplib à classe `Api` e é exercitado pelos testes e2e. Mesmo contando-o como 0% coberto, a cobertura total fica em cerca de 83%.

### Testes end-to-end (Playwright)

Os testes e2e usam o Chromium contra o servidor C++ de verdade. O Playwright sobe o `diario_server` sozinho, na porta 8089 e com um arquivo de dados novo a cada execução. Cada teste cria a própria conta, então os testes não dependem uns dos outros. A Open Library é simulada, e os testes não dependem da internet. Compile o servidor antes (veja "Testes de unidade").

```bash
cd e2e
npm ci
npx playwright install --with-deps --no-shell chromium
npx playwright test             # modo headless
npx playwright test --headed    # vendo o navegador
npx playwright test --ui        # modo interativo
```

| Arquivo | Cenários |
|---|---|
| `contas.spec.ts` | criar conta, senha errada, login repetido, sair, isolamento entre usuários |
| `cadastro.spec.ts` | adicionar livro, validação de campos obrigatórios e de datas, persistência após recarregar |
| `edicao.spec.ts` | editar, cancelar edição, remover e recusar remoção |
| `busca.spec.ts` | busca por autor, busca sem resultado, ordenação por nota, busca + ordenação |
| `filtro-nota.spec.ts` | filtro por nota mínima, sozinho e combinado com a busca |
| `autores.spec.ts` | ranking dos autores mais lidos: vazio, top 3 com desempate, atualização ao adicionar |
| `sugestoes.spec.ts` | sugestões da Open Library (simulada) e falha da API externa |
| `social.spec.ts` | buscar pessoa e abrir perfil, seguir, feed em ordem, deixar de seguir, próprio perfil |
| `api.spec.ts` | `PUT` e `DELETE` com id grande demais respondem 404, e não erro interno |

Os seletores usam papéis e rótulos acessíveis (`getByRole`, `getByLabel`) e ficam num page object (`e2e/tests/fixtures.ts`). Os testes não usam esperas fixas.

### CI/CD (GitHub Actions)

O workflow `.github/workflows/ci.yml` roda a cada push e pull request:

| Job | Sistemas | O que faz |
|---|---|---|
| Build e testes | Ubuntu (GCC), macOS (AppleClang), Windows (MSVC), cada um com C++17 e C++20 | Compila e roda os testes de unidade |
| Cobertura | Ubuntu com PostgreSQL, macOS, Windows (MinGW) | Testes com cobertura e relatório do gcovr no log. No Ubuntu, inclui os testes do PostgreSQL |
| E2E | Ubuntu, macOS, Windows | Testes do Playwright |
| Imagem Docker | Ubuntu | Constrói a imagem de produção e testa cadastro e sessão dentro dela |
| Deploy (Render) | Ubuntu | Só em push na `main` e só se todos os jobs acima passarem: dispara o deploy no Render |

### Problemas que os testes pegaram

- **Erro 500 com id grande.** `PUT /api/livros/99999999999` estourava o `std::stoi` e o servidor respondia 500. O teste `api.spec.ts` reproduziu o bug antes da correção: o commit `test:` veio antes do `fix:`.
- **Referência pendente em C++.** Durante o desenvolvimento das contas, os testes do repositório em arquivo falharam de forma intermitente. A causa era um laço `for` sobre um JSON temporário já destruído. O código foi corrigido antes do commit.
- **Diário antigo em formato incompatível.** Os e2e falharam ao encontrar um arquivo de dados de uma execução anterior, ainda sem contas. Isso levou ao uso de um arquivo novo por execução e a uma mensagem de erro clara para arquivos antigos.

## Como rodar localmente

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
./build/diario_server                 # Linux/macOS
.\build\Release\diario_server.exe     # Windows (Visual Studio)
```

Depois, abra http://localhost:8080 e crie uma conta. Os dados ficam em `diario.json`, que não vai para o repositório.

Opções do servidor: `--porta 8080`, `--host 127.0.0.1`, `--dados diario.json`, `--web <pasta da interface>` e `--cookie-seguro sim`.

| Variável de ambiente | Para quê |
|---|---|
| `DATABASE_URL` | Usa PostgreSQL em vez do arquivo JSON. O servidor precisa ter sido compilado com `-DDIARIO_WITH_POSTGRES=ON` |
| `SESSION_SECRET` | Segredo de ao menos 32 caracteres que assina os cookies de sessão. Sem ele, um segredo aleatório é gerado a cada início, e todos precisam entrar de novo quando o servidor reinicia |

Os segredos vêm do ambiente, e não de argumentos, porque argumentos aparecem em listagens de processos e em logs.

> **`diario.json` antigo:** arquivos de antes das contas de usuário (só com `"livros"`, sem `"usuarios"`) não são aceitos. O servidor responde "não foi possível acessar os dados" e registra o motivo no terminal. Renomeie ou apague o arquivo para começar um novo.

## Produção

O sistema roda no [Render](https://render.com), em plano gratuito, a partir do `Dockerfile`, com PostgreSQL gerenciado no [Neon](https://neon.tech), também em plano gratuito.

- A cada push na `main`, o job **Deploy (Render)** do CI chama o deploy hook do Render, mas só depois que todos os testes passam nos 3 sistemas.
- Segredos no Render: `DATABASE_URL` e `SESSION_SECRET`. Segredo no GitHub: `RENDER_DEPLOY_HOOK_URL`.
- As tabelas do PostgreSQL (`usuarios`, `livros`, `diario_meta` e `seguidores`) são criadas sozinhas na primeira conexão. Colunas novas, como `livros.registrado_em`, são adicionadas a bancos que já existem.

```bash
docker build -t diario-livros .
docker run -p 8080:8080 -e DATABASE_URL="postgresql://..." -e SESSION_SECRET="..." diario-livros
```

A imagem liga o cookie seguro (`COOKIE_SEGURO=sim`), que só trafega por HTTPS. Para testar a imagem localmente, por HTTP, use `-e COOKIE_SEGURO=nao`.

## Segurança

- **Senhas:** guardadas só como hash Argon2id (19 MiB, 2 passagens, salt aleatório), nos parâmetros mínimos recomendados pela OWASP. O hash nunca aparece nas respostas da API.
- **Sessão:** o token é assinado com BLAKE2b. O cookie é `HttpOnly` (o JavaScript da página não o lê), `SameSite=Lax` (não vai em requisições de outros sites) e `Secure` em produção.
- **Login:** a mesma mensagem para login inexistente e senha errada, e o tempo de resposta é parecido nos dois casos, para não revelar quais logins existem.
- **SQL:** todas as consultas são parametrizadas.
- **Capa:** a URL da capa só aceita `http(s)://` ou `data:image/`, o que impede `javascript:`.

## API

Todas as rotas, exceto `/api/saude`, `/api/cadastro` e `/api/login`, exigem a sessão (cookie `sessao`) e respondem `401` sem ela.

| Método | Rota | Descrição |
|---|---|---|
| `GET` | `/api/saude` | Verifica se o servidor está no ar |
| `POST` | `/api/cadastro` | Cria a conta (`{"login", "senha"}`) e já entra (`201`; `400` se inválida; `409` se o login já existe) |
| `POST` | `/api/login` | Entra (`401` se login ou senha estiverem errados) |
| `POST` | `/api/logout` | Sai |
| `GET` | `/api/eu` | Login de quem está na sessão |
| `GET` | `/api/livros?q=<termo>&ordem=nota&notaMin=<0-5>` | Livros do seu diário, com busca, ordenação e nota mínima opcionais |
| `GET` | `/api/estatisticas` | Total, nota média e os 3 autores mais lidos |
| `POST` | `/api/livros` | Cria um livro (`201`, ou `400` com `{"erro": ...}`) |
| `PUT` | `/api/livros/{id}` | Atualiza um livro (`404` se não existir) |
| `DELETE` | `/api/livros/{id}` | Remove um livro (`204`) |
| `GET` | `/api/usuarios?q=<trecho>` | Até 20 usuários cujo login contém o trecho, em ordem alfabética |
| `GET` | `/api/usuarios/{login}` | Perfil: livros, estatísticas, seguidores, seguindo e se você segue a pessoa (`404` se não existir) |
| `POST` | `/api/usuarios/{login}/seguir` | Segue a pessoa (`400` para si mesmo, `404` se não existir) |
| `DELETE` | `/api/usuarios/{login}/seguir` | Deixa de seguir |
| `GET` | `/api/feed` | Até 50 livros de quem você segue, do mais recente para o mais antigo |

Falhas de acesso ao banco respondem `503`, e o estado em memória não muda.

Corpo de um livro:

```json
{
  "titulo": "Vidas Secas",
  "autor": "Graciliano Ramos",
  "capaUrl": "https://exemplo.com/capa.jpg",
  "dataLancamento": "1938-01-01",
  "dataLeitura": "2026-10-20",
  "nota": 4
}
```

As respostas também trazem `id` e `registradoEm` (quando o livro entrou no diário, em UTC). Os dois são definidos pelo servidor.

## Regras de validação

- `titulo` e `autor` são obrigatórios. Um campo só com espaços conta como vazio.
- `nota` é um inteiro de 0 a 5.
- Datas no formato `AAAA-MM-DD`, com dia e mês válidos e anos bissextos considerados.
- A data de leitura não pode ser anterior à de lançamento.
- `capaUrl` deve ser vazia, `http(s)://...` ou `data:image/...`.
- Ids de livros nunca são reaproveitados, nem depois de recarregar os dados. Cada usuário numera os próprios livros a partir de 1.
- Login: de 3 a 30 caracteres entre `a-z`, `0-9`, `.`, `_` e `-`. Maiúsculas viram minúsculas e espaços nas pontas são removidos.
- Senha: de 8 a 128 caracteres.
