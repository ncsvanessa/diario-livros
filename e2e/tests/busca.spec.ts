import { test, expect, cadastrarPelaApi } from "./fixtures";

test.describe("Busca e ordenação", () => {
  test.beforeEach(async ({ diario, request, page }) => {
    await cadastrarPelaApi(request, [
      { titulo: "Memórias Póstumas de Brás Cubas", autor: "Machado de Assis", nota: 4 },
      { titulo: "Capitães da Areia", autor: "Jorge Amado", nota: 5 },
      { titulo: "Dom Casmurro", autor: "Machado de Assis", nota: 3 },
    ]);
    await page.reload();
    await expect(diario.cartoes).toHaveCount(3);
  });

  test("buscar pelo autor mostra só os livros dele", async ({ diario }) => {
    await diario.busca.fill("machado");

    await expect(diario.cartoes).toHaveCount(2);
    expect(await diario.titulosNaGrade()).toEqual(["Memórias Póstumas de Brás Cubas", "Dom Casmurro"]);
  });

  test("busca sem resultados avisa que nada foi encontrado", async ({ diario, page }) => {
    await diario.busca.fill("tolkien");

    await expect(page.getByText("Nada encontrado")).toBeVisible();
    await expect(diario.cartoes).toHaveCount(0);
  });

  test("ordenar por maior nota reorganiza a grade", async ({ diario }) => {
    await diario.ordem.selectOption({ label: "Maior nota" });

    await expect(diario.cartoes.first()).toContainText("Capitães da Areia");
    expect(await diario.titulosNaGrade()).toEqual([
      "Capitães da Areia",
      "Memórias Póstumas de Brás Cubas",
      "Dom Casmurro",
    ]);
  });

  test("busca e ordenação funcionam juntas", async ({ diario }) => {
    await diario.ordem.selectOption({ label: "Maior nota" });
    await diario.busca.fill("machado");

    await expect(diario.cartoes).toHaveCount(2);
    expect(await diario.titulosNaGrade()).toEqual(["Memórias Póstumas de Brás Cubas", "Dom Casmurro"]);
  });
});
