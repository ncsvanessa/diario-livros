# Imagem de produção do Diário de Livros.
# Estágio 1 compila; estágio 2 leva só o binário, a interface e as bibliotecas de runtime.

FROM debian:bookworm-slim AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake git ca-certificates libpq-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY include include
COPY src src
COPY web web
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DDIARIO_BUILD_TESTS=OFF -DDIARIO_WITH_POSTGRES=ON \
 && cmake --build build --parallel

FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends libpq5 ca-certificates \
 && rm -rf /var/lib/apt/lists/*
RUN useradd --system --create-home diario
WORKDIR /app
COPY --from=build /src/build/diario_server ./
COPY web ./web
USER diario

# A plataforma de nuvem informa a porta em PORT; 8080 é o padrão para rodar localmente.
# DATABASE_URL (segredo) é configurada na plataforma; sem ela, os dados vão para um
# arquivo dentro do container e se perdem quando ele reinicia.
ENV PORT=8080
EXPOSE 8080
CMD ["sh", "-c", "exec ./diario_server --host 0.0.0.0 --porta \"$PORT\" --web /app/web --dados /home/diario/diario.json"]
