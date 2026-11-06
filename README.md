# Diário de Livros

[![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

Aplicação em C++ para registrar livros lidos, com título, autor, capa, data de lançamento, data de leitura e avaliação de 0 a 5 estrelas. A interface é uma página HTML servida por um servidor local.

Trabalho prático de GitHub Actions — Engenharia de Software II (DCC/UFMG).

## Estrutura

| Caminho | Conteúdo |
|---|---|
| `include/diario/diario.hpp`, `src/diario.cpp` | Núcleo: modelo `Livro`, validação, classe `Diario`, persistência em JSON |
| `src/main.cpp` | Servidor HTTP (cpp-httplib) com API REST e arquivos estáticos |
| `web/index.html` | Interface |
| `tests/test_diario.cpp` | 17 testes de unidade (GoogleTest) |
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

## Docker

```bash
docker build -t diario-livros .
docker run -p 8080:8080 -e DATABASE_URL="postgresql://..." diario-livros
```

## API

| Método | Rota | Descrição |
|---|---|---|
| `GET` | `/api/livros?q=<termo>&ordem=nota` | Lista, com busca e ordenação opcionais |
| `GET` | `/api/estatisticas` | Total de livros e nota média |
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
