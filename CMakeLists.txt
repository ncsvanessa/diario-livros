cmake_minimum_required(VERSION 3.20)
project(DiarioDeLivros VERSION 1.0 LANGUAGES CXX)

# O padrão pode ser sobrescrito na linha de comando (o CI testa 17 e 20):
#   cmake -S . -B build -DCMAKE_CXX_STANDARD=20
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DIARIO_BUILD_SERVER "Compila o servidor web" ON)
option(DIARIO_BUILD_TESTS "Compila os testes de unidade" ON)

# Fontes em UTF-8 (mensagens com acento) no MSVC.
if(MSVC)
  add_compile_options(/utf-8 /W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Dependências (baixadas no configure) -------------------------------------
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# ---- Núcleo (testável, sem dependência de rede) --------------------------------
add_library(diario_core src/diario.cpp src/repositorio.cpp)
target_include_directories(diario_core PUBLIC include)
target_link_libraries(diario_core PUBLIC nlohmann_json::nlohmann_json)

# Cobertura (GCC/Clang). Só o código do sistema é instrumentado; dependências e testes não.
option(DIARIO_COVERAGE "Instrumenta o núcleo para medir cobertura" OFF)
if(DIARIO_COVERAGE)
  if(MSVC)
    message(FATAL_ERROR "DIARIO_COVERAGE requer GCC ou Clang (use MinGW no Windows)")
  endif()
  target_compile_options(diario_core PRIVATE --coverage -O0 -g)
  # PUBLIC: quem linka o núcleo (testes, servidor) também precisa do runtime do gcov.
  target_link_options(diario_core PUBLIC --coverage)
endif()

# ---- Servidor web --------------------------------------------------------------
if(DIARIO_BUILD_SERVER)
  # cpp-httplib sem TLS/compressão: só servimos em localhost.
  set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(httplib)

  add_executable(diario_server src/main.cpp)
  target_link_libraries(diario_server PRIVATE diario_core httplib::httplib)
  # Pasta padrão da interface; pode ser trocada com --web <pasta>.
  target_compile_definitions(diario_server PRIVATE
    DIARIO_WEB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/web")
endif()

# ---- Testes --------------------------------------------------------------------
if(DIARIO_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  # Evita conflito de runtime (/MT vs /MD) no Windows.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(diario_tests tests/test_diario.cpp tests/test_repositorio.cpp)
  target_link_libraries(diario_tests PRIVATE diario_core GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(diario_tests)
endif()
