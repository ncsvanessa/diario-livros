import { test, expect } from "@playwright/test";

// Casos de borda da API REST que a interface nunca gera, mas um cliente qualquer pode enviar.
// Usa só o `request` do Playwright (sem navegador): o servidor é o mesmo dos outros testes.

const ID_FORA_DO_INT = "99999999999"; // 11 dígitos: maior que o maior int de 32 bits

test.describe("API · ids fora do intervalo", () => {
  test("PUT com id de 11 dígitos responde 404, e não erro interno", async ({ request }) => {
    const resposta = await request.put(`/api/livros/${ID_FORA_DO_INT}`, {
      data: { titulo: "Qualquer", autor: "Alguém", nota: 3 },
    });

    expect(resposta.status()).toBe(404);
  });

  test("DELETE com id de 11 dígitos responde 404, e não erro interno", async ({ request }) => {
    const resposta = await request.delete(`/api/livros/${ID_FORA_DO_INT}`);

    expect(resposta.status()).toBe(404);
  });
});
