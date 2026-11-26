import { test, expect, cadastrarPelaApi } from "./fixtures";

test.describe("Autores mais lidos", () => {
  test("diário vazio mostra um aviso no lugar do ranking", async ({ diario, page }) => {
    await expect(diario.autoresMaisLidos).toHaveCount(0);
    await expect(page.getByText("Nenhum autor registrado ainda.")).toBeVisible();
  });

  test("mostra os 3 autores com mais livros, do maior para o menor", async ({ diario, request, page }) => {
    await cadastrarPelaApi(request, [
      { titulo: "Dom Casmurro", autor: "Machado de Assis" },
      { titulo: "Capitães da Areia", autor: "Jorge Amado" },
      { titulo: "Quincas Borba", autor: "Machado de Assis" },
      { titulo: "Gabriela", autor: "Jorge Amado" },
      { titulo: "O Alienista", autor: "Machado de Assis" },
      { titulo: "Vidas Secas", autor: "Graciliano Ramos" },
      { titulo: "Iracema", autor: "José de Alencar" },
    ]);

    await page.reload();

    await expect(diario.autoresMaisLidos).toHaveText([
      /Machado de Assis\s*3 livros/,
      /Jorge Amado\s*2 livros/,
      // Empate em 1 livro: desempata pelo nome, e José de Alencar fica de fora do top 3.
      /Graciliano Ramos\s*1 livro$/,
    ]);
    await expect(page.getByText("Nenhum autor registrado ainda.")).toBeHidden();
  });

  test("o ranking é atualizado ao adicionar um livro pela interface", async ({ diario, request, page }) => {
    await cadastrarPelaApi(request, [
      { titulo: "Capitães da Areia", autor: "Jorge Amado" },
      { titulo: "Dom Casmurro", autor: "Machado de Assis" },
    ]);
    await page.reload();
    await expect(diario.autoresMaisLidos.first()).toContainText("Jorge Amado");

    await diario.adicionar({ titulo: "Quincas Borba", autor: "Machado de Assis", nota: 4 });

    await expect(diario.autoresMaisLidos.first()).toHaveText(/Machado de Assis\s*2 livros/);
  });
});
