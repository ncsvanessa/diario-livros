# Diário de Livros

[![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

Aplicação em C++ para registrar livros lidos, com título, autor, capa, data de lançamento, data de leitura e avaliação de 0 a 5 estrelas. Permite buscar por título ou autor, ordenar por nota, filtrar por nota mínima e ver o ranking dos autores mais lidos. A interface é uma página HTML servida por um servidor local.

Trabalho prático de GitHub Actions — Engenharia de Software II (DCC/UFMG).

## Estrutura

| Caminho | Conteúdo |
|---|---|
| `include/diario/diario.hpp`, `src/diario.cpp` | Núcleo: modelo `Livro`, validação, classe `Diario`, persistência em JSON |
| `src/main.cpp` | Servidor HTTP (cpp-httplib) com API REST e arquivos estáticos |
| `web/index.html` | Interface |
| `tests/test_diario.cpp`, `tests/test_repositorio.cpp` | 30 testes de unidade (GoogleTest) |
| `e2e/` | 20 testes end-to-end (Playwright) |
| `.github/workflows/ci.yml` | CI: build e testes em Ubuntu, macOS e Windows, com C++17 e C++20 |

O núcleo não depende de rede nem de interface, então os testes exercitam as regras diretamente.

## Como rodar

Requisitos: CMake 3.20+, um compilador C++17 (GCC, Clang ou MSVC) e Git. As dependências (nlohmann/json, cpp-httplib e GoogleTest) são baixadas pelo CMake.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# testes
ctest --test-dir build -C Release --output-on-failure

# servidor (abra http://localhost:8080)
./build/diario_server                 # Linux/macOS
.\build\Release\diario_server.exe     # Windows (Visual Studio)
```

Opções do servidor: `--porta 8080`, `--host 127.0.0.1`, `--dados diario.json` e `--web <pasta da interface>`.

## Persistência

| Modo | Quando | Como |
|---|---|---|
| Arquivo JSON | Padrão (desenvolvimento local) | `--dados <arquivo>` |
| PostgreSQL | Produção | Build com `-DDIARIO_WITH_POSTGRES=ON` (requer libpq) e variável de ambiente `DATABASE_URL` |

A URL do banco é lida do ambiente, não de argumento, porque contém a senha. As tabelas são criadas automaticamente na primeira conexão.

Os testes do repositório PostgreSQL só são compilados com `-DDIARIO_WITH_POSTGRES=ON` e exigem um banco em `DIARIO_TEST_DATABASE_URL`:

```bash
cmake -S . -B build -DDIARIO_WITH_POSTGRES=ON
cmake --build build
DIARIO_TEST_DATABASE_URL=postgresql://postgres:postgres@localhost:5432/postgres ctest --test-dir build
```

## Testes end-to-end (Playwright)

Os testes e2e ficam em `e2e/` e usam o navegador (Chromium) contra o servidor C++ de verdade. O Playwright sobe o `diario_server` sozinho, na porta 8089 e com um arquivo de dados temporário, então o seu `diario.json` não é tocado. A Open Library é simulada, e os testes não dependem da internet.

Requisito: Node.js 18+ e o servidor já compilado (veja "Como rodar").

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
| `e2e/tests/cadastro.spec.ts` | adicionar livro, validação de campos obrigatórios e de datas, persistência após recarregar |
| `e2e/tests/edicao.spec.ts` | editar, cancelar edição, remover e recusar remoção |
| `e2e/tests/busca.spec.ts` | busca por autor, busca sem resultado, ordenação por nota, busca + ordenação |
| `e2e/tests/sugestoes.spec.ts` | sugestões da Open Library (simulada) e falha da API externa |
| `e2e/tests/filtro-nota.spec.ts` | filtro por nota mínima, sozinho e combinado com a busca |
| `e2e/tests/autores.spec.ts` | ranking dos autores mais lidos: vazio, top 3 com desempate, atualização ao adicionar |

## Docker

```bash
docker build -t diario-livros .
docker run -p 8080:8080 -e DATABASE_URL="postgresql://..." diario-livros
```

## API

| Método | Rota | Descrição |
|---|---|---|
| `GET` | `/api/livros?q=<termo>&ordem=nota&notaMin=<0-5>` | Lista, com busca, ordenação e nota mínima opcionais (`400` se `notaMin` for inválida) |
| `GET` | `/api/estatisticas` | Total de livros, nota média e os 3 autores mais lidos |
| `POST` | `/api/livros` | Cria um livro (`201`, ou `400` com `{"erro": ...}`) |
| `PUT` | `/api/livros/{id}` | Atualiza um livro (`404` se não existir) |
| `DELETE` | `/api/livros/{id}` | Remove um livro (`204`) |

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

## Regras de validação

- `titulo` e `autor` são obrigatórios; só espaços conta como vazio.
- `nota` é um inteiro de 0 a 5.
- Datas no formato `AAAA-MM-DD`, com dia e mês válidos e anos bissextos considerados.
- A data de leitura não pode ser anterior à de lançamento.
- `capaUrl` deve ser vazia, `http(s)://...` ou `data:image/...`. Isso impede, por exemplo, `javascript:`.
- Ids nunca são reaproveitados, nem depois de recarregar o arquivo.

## CI

O workflow roda a cada push e pull request uma matrix de 3 sistemas operacionais × 2 padrões da linguagem, totalizando 6 jobs:

| | C++17 | C++20 |
|---|---|---|
| `ubuntu-latest` (GCC) | ✓ | ✓ |
| `macos-latest` (AppleClang) | ✓ | ✓ |
| `windows-latest` (MSVC) | ✓ | ✓ |
