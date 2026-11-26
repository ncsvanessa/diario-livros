import { test, expect, cadastrarPelaApi } from "./fixtures";

test.describe("Filtro por nota mínima", () => {
  test.beforeEach(async ({ diario, request, page }) => {
    await cadastrarPelaApi(request, [
      { titulo: "Dom Casmurro", autor: "Machado de Assis", nota: 5 },
      { titulo: "O Alienista", autor: "Machado de Assis", nota: 3 },
      { titulo: "Capitães da Areia", autor: "Jorge Amado", nota: 4 },
      { titulo: "Iracema", autor: "José de Alencar", nota: 1 },
    ]);
    await page.reload();
    await expect(diario.cartoes).toHaveCount(4);
  });

  test("mostra só os livros com a nota escolhida ou maior", async ({ diario }) => {
    await diario.notaMinima.selectOption({ label: "4★ ou mais" });

    await expect(diario.cartoes).toHaveCount(2);
    expect(await diario.titulosNaGrade()).toEqual(["Dom Casmurro", "Capitães da Areia"]);
    // As estatísticas continuam considerando o diário inteiro.
    await expect(diario.total).toHaveText("4");
  });

  test("combina com a busca por autor", async ({ diario }) => {
    await diario.notaMinima.selectOption({ label: "4★ ou mais" });
    await diario.busca.fill("machado");

    await expect(diario.cartoes).toHaveCount(1);
    expect(await diario.titulosNaGrade()).toEqual(["Dom Casmurro"]);
  });

  test("voltar para qualquer nota mostra todos os livros de novo", async ({ diario, page }) => {
    await diario.notaMinima.selectOption({ label: "Só 5★" });
    await expect(diario.cartoes).toHaveCount(1);

    await diario.notaMinima.selectOption({ label: "Qualquer nota" });

    await expect(diario.cartoes).toHaveCount(4);
    await expect(page.getByText("Nada encontrado")).toBeHidden();
  });
});
