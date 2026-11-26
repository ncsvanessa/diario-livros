# Diário de Livros

[![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

Rede de leitura em C++: cada pessoa registra os livros que leu e acompanha o que os amigos estão lendo.

**Em produção:** https://diario-livros.onrender.com

Trabalho prático de Engenharia de Software II (DCC/UFMG).

## Integrantes

- Artur Fonseca Souza
- Lucas Dolabella de Castro Lopes
- Vanessa Nascimento Silva

## Sobre o sistema

Funcionalidades:

- **Contas:** cadastro e login com usuário e senha. Cada pessoa tem um diário próprio.
- **Diário de leituras:** registrar, editar e remover livros com título, autor, capa (URL ou arquivo), data de lançamento, data de leitura e nota de 0 a 5 estrelas.
- **Sugestões ao digitar o título:** busca o livro na [Open Library](https://openlibrary.org) e preenche título, autor, capa e ano de lançamento.
- **Busca e filtros:** por título ou autor, ordenação por nota e filtro por nota mínima.
- **Estatísticas:** total de livros, nota média e os três autores mais lidos.
- **Pessoas:** buscar outros usuários pelo nome e abrir o perfil de cada um, com os livros e as estatísticas dele.
- **Seguir:** seguir e deixar de seguir outras pessoas.
- **Feed:** os livros registrados por quem você segue, do mais recente para o mais antigo.

Todo diário é visível para qualquer usuário logado. Só o dono pode alterar o próprio diário.

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
| Testes de unidade | [GoogleTest](https://github.com/google/googletest) 1.15 | Núcleo, API, contas e repositórios |
| Testes e2e | [Playwright](https://playwright.dev) 1.49 (Chromium) | Fluxos completos no navegador contra o servidor real |
| Cobertura | [gcovr](https://gcovr.com) 8.8 (gcov / llvm-cov) | Mede a cobertura de linhas e falha abaixo de 80% |
| CI | GitHub Actions | Build, testes e cobertura em Ubuntu, macOS e Windows |
| Produção | Docker + [Render](https://render.com) | Imagem Debian com o servidor e PostgreSQL gerenciado |

### Estrutura

| Caminho | Conteúdo |
|---|---|
| `include/diario/diario.hpp`, `src/diario.cpp` | Núcleo: modelo `Livro`, validação e classe `Diario` |
| `include/diario/api.hpp`, `src/api.cpp` | Regras das rotas `/api/*`, testáveis sem subir o servidor |
| `include/diario/conta.hpp`, `senha.hpp`, `sessao.hpp`, `aleatorio.hpp` | Contas, hash de senha, token de sessão e bytes aleatórios |
| `include/diario/repositorio.hpp`, `src/repositorio*.cpp` | Persistência em arquivo JSON e em PostgreSQL |
| `src/main.cpp` | Servidor HTTP: liga as rotas do cpp-httplib à classe `Api` |
| `web/index.html` | Interface |
| `tests/` | Testes de unidade (GoogleTest) |
| `e2e/` | Testes end-to-end (Playwright) |
| `.github/workflows/ci.yml` | Pipeline de CI |

## Execução dos testes

Requisitos: CMake 3.20+, um compilador C++17 (GCC, Clang ou MSVC), Git e, para os testes e2e, Node.js 18+.

### Testes de unidade

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

São **206 testes** (GoogleTest) do núcleo, da API, das contas e do repositório em arquivo. Os testes da API usam um repositório em memória, então não precisam de banco nem de servidor.

Os **21 testes do repositório PostgreSQL** só são compilados com `-DDIARIO_WITH_POSTGRES=ON` e precisam de um banco em `DIARIO_TEST_DATABASE_URL`:

```bash
cmake -S . -B build-pg -DDIARIO_WITH_POSTGRES=ON
cmake --build build-pg
DIARIO_TEST_DATABASE_URL=postgresql://postgres:postgres@localhost:5432/postgres ctest --test-dir build-pg
```

### Cobertura

```bash
pip install -r requirements-dev.txt
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DDIARIO_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov
gcovr build-cov                                         # Linux (GCC)
gcovr --gcov-executable "xcrun llvm-cov gcov" build-cov  # macOS (AppleClang)
```

O `gcovr.cfg` mede só o código do sistema (`src/` e `include/`) e falha se a cobertura de linhas ficar abaixo de 80%. Hoje ela está em cerca de **94%**.

### Testes end-to-end (Playwright)

Os testes e2e usam o Chromium contra o servidor C++ de verdade. O Playwright sobe o `diario_server` sozinho, na porta 8089 e com um arquivo de dados temporário, então o seu `diario.json` não é tocado. A Open Library é simulada, e os testes não dependem da internet. Compile o servidor antes (veja "Testes de unidade").

```bash
cd e2e
npm ci
npx playwright install --with-deps --no-shell chromium
npx playwright test             # modo headless
npx playwright test --headed    # vendo o navegador
npx playwright test --ui        # modo interativo
```

São **32 testes**:

| Arquivo | Cenários |
|---|---|
| `e2e/tests/contas.spec.ts` | criar conta, senha errada, login repetido, sair, isolamento entre usuários |
| `e2e/tests/cadastro.spec.ts` | adicionar livro, validação de campos obrigatórios e de datas, persistência após recarregar |
| `e2e/tests/edicao.spec.ts` | editar, cancelar edição, remover e recusar remoção |
| `e2e/tests/busca.spec.ts` | busca por autor, busca sem resultado, ordenação por nota, busca + ordenação |
| `e2e/tests/sugestoes.spec.ts` | sugestões da Open Library (simulada) e falha da API externa |
| `e2e/tests/filtro-nota.spec.ts` | filtro por nota mínima, sozinho e combinado com a busca |
| `e2e/tests/autores.spec.ts` | ranking dos autores mais lidos: vazio, top 3 com desempate, atualização ao adicionar |
| `e2e/tests/social.spec.ts` | buscar pessoa e abrir perfil, seguir, feed em ordem, deixar de seguir, próprio perfil |
| `e2e/tests/api.spec.ts` | `PUT` e `DELETE` com id grande demais respondem 404, e não erro interno |

### CI

O GitHub Actions roda a cada push e pull request:

| Job | Sistemas | O que faz |
|---|---|---|
| Build e testes | Ubuntu (GCC), macOS (AppleClang), Windows (MSVC), cada um com C++17 e C++20 | Compila e roda os testes de unidade |
| Cobertura | Ubuntu com PostgreSQL, macOS, Windows (MinGW) | Testes com cobertura. No Ubuntu, inclui os testes do PostgreSQL |
| E2E | Ubuntu, macOS, Windows | Testes do Playwright |
| Imagem Docker | Ubuntu | Constrói a imagem de produção e testa cadastro e sessão dentro dela |

## Como rodar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
./build/diario_server                 # Linux/macOS
.\build\Release\diario_server.exe     # Windows (Visual Studio)
```

Depois, abra http://localhost:8080 e crie uma conta.

Opções do servidor: `--porta 8080`, `--host 127.0.0.1`, `--dados diario.json`, `--web <pasta da interface>` e `--cookie-seguro sim`.

Variáveis de ambiente:

| Variável | Para quê |
|---|---|
| `DATABASE_URL` | Usa PostgreSQL em vez do arquivo JSON (o servidor precisa ter sido compilado com `-DDIARIO_WITH_POSTGRES=ON`) |
| `SESSION_SECRET` | Segredo de ao menos 32 caracteres que assina os cookies de sessão. Sem ele, um segredo aleatório é gerado a cada início, e todos precisam entrar de novo quando o servidor reinicia |

Os segredos vêm do ambiente, e não de argumentos, porque argumentos aparecem em listagens de processos e em logs.

> **`diario.json` antigo:** arquivos de antes das contas de usuário (só com `"livros"`, sem `"usuarios"`) não são aceitos. O servidor responde "não foi possível acessar os dados" e registra o motivo no terminal. Renomeie ou apague o arquivo para começar um novo.

## Persistência

| Modo | Quando | Como |
|---|---|---|
| Arquivo JSON | Padrão (desenvolvimento local e testes e2e) | `--dados <arquivo>` |
| PostgreSQL | Produção | Build com `-DDIARIO_WITH_POSTGRES=ON` (requer libpq) e `DATABASE_URL` |

As tabelas do PostgreSQL (`usuarios`, `livros`, `diario_meta` e `seguidores`) são criadas sozinhas na primeira conexão. Colunas novas, como `livros.registrado_em`, são adicionadas a bancos que já existem.

## Docker

```bash
docker build -t diario-livros .
docker run -p 8080:8080 -e DATABASE_URL="postgresql://..." -e SESSION_SECRET="..." diario-livros
```

A imagem liga o cookie seguro (`COOKIE_SEGURO=sim`), que só trafega por HTTPS. Para testar a imagem localmente, por HTTP, use `-e COOKIE_SEGURO=nao`.

## API

Todas as rotas, exceto `/api/saude`, `/api/cadastro` e `/api/login`, exigem a sessão (cookie `sessao`) e respondem `401` sem ela.

| Método | Rota | Descrição |
|---|---|---|
| `GET` | `/api/saude` | Verifica se o servidor está no ar |
| `POST` | `/api/cadastro` | Cria a conta (`{"login", "senha"}`) e já entra (`201`, `400` ou `409` se o login já existe) |
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
- `capaUrl` deve ser vazia, `http(s)://...` ou `data:image/...`. Isso impede, por exemplo, `javascript:`.
- Ids nunca são reaproveitados, nem depois de recarregar os dados.
- Login: de 3 a 30 caracteres entre `a-z`, `0-9`, `.`, `_` e `-` (maiúsculas viram minúsculas). Senha: de 8 a 128 caracteres.
