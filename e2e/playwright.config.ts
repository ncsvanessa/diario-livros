import { defineConfig, devices } from "@playwright/test";
import { existsSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";

// Os testes e2e rodam contra o servidor C++ de verdade, compilado antes com CMake.
// O caminho do executável muda com o gerador (Visual Studio põe em build/Release/),
// então procuramos nos lugares conhecidos; DIARIO_SERVER sobrescreve.
const raiz = path.resolve(__dirname, "..");
const candidatos = [
  process.env.DIARIO_SERVER,
  path.join(raiz, "build", "diario_server"),
  path.join(raiz, "build", "diario_server.exe"),
  path.join(raiz, "build", "Release", "diario_server.exe"),
].filter((c): c is string => Boolean(c));

const servidor = candidatos.find((c) => existsSync(c));
if (!servidor) {
  throw new Error(
    "diario_server não encontrado. Compile antes com:\n" +
      "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release\n" +
      "  cmake --build build --config Release\n" +
      "ou defina DIARIO_SERVER com o caminho do executável.",
  );
}

// Porta e arquivo próprios para não mexer no diário de quem roda os testes.
const PORTA = 8089;
const dados = path.join(tmpdir(), "diario-livros-e2e.json");

export default defineConfig({
  testDir: "./tests",
  // Todos os testes compartilham o mesmo servidor (e o mesmo diário), então rodam em série.
  workers: 1,
  fullyParallel: false,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 1 : 0,
  reporter: process.env.CI ? [["list"], ["html", { open: "never" }]] : "list",
  use: {
    baseURL: `http://127.0.0.1:${PORTA}`,
    trace: "retain-on-failure",
    screenshot: "only-on-failure",
  },
  // channel "chromium": usa o Chromium completo também no modo headless, então basta
  // `playwright install chromium --no-shell` (não precisa baixar o headless shell à parte).
  projects: [{ name: "chromium", use: { ...devices["Desktop Chrome"], channel: "chromium" } }],
  webServer: {
    command: `"${servidor}" --host 127.0.0.1 --porta ${PORTA} --dados "${dados}" --web "${path.join(raiz, "web")}"`,
    url: `http://127.0.0.1:${PORTA}/api/estatisticas`,
    // Nunca reaproveita um servidor já aberto: poderia ser o diário real do usuário.
    reuseExistingServer: false,
    timeout: 30_000,
    stdout: "pipe",
    // Sem DATABASE_URL o servidor usa o arquivo acima, mesmo que a máquina tenha a variável.
    env: { DATABASE_URL: "" },
  },
});
