import { test, expect, cadastrarPelaApi } from "./fixtures";

test.describe("Edição e remoção", () => {
  test.beforeEach(async ({ diario, request, page }) => {
    await cadastrarPelaApi(request, [{ titulo: "O Cortiço", autor: "Aluísio Azevedo", nota: 2 }]);
    await page.reload();
    await expect(diario.cartao("O Cortiço")).toBeVisible();
  });

  test("editar um livro altera o cartão sem criar outro", async ({ diario }) => {
    await diario.cartao("O Cortiço").getByRole("button", { name: "Editar" }).click();

    await expect(diario.page.getByRole("heading", { name: "Editar leitura" })).toBeVisible();
    await expect(diario.titulo).toHaveValue("O Cortiço");
    await expect(diario.autor).toHaveValue("Aluísio Azevedo");

    await diario.escolherNota(5);
    await diario.salvar.click();

    await expect(diario.cartao("O Cortiço").getByLabel("5 de 5 estrelas")).toBeVisible();
    await expect(diario.cartoes).toHaveCount(1);
    await expect(diario.media).toHaveText("5,0");
    await expect(diario.page.getByRole("heading", { name: "Registrar leitura" })).toBeVisible();
  });

  test("cancelar a edição descarta as mudanças", async ({ diario }) => {
    await diario.cartao("O Cortiço").getByRole("button", { name: "Editar" }).click();
    await diario.titulo.fill("Outro título");

    await diario.page.getByRole("button", { name: "Cancelar" }).click();

    await expect(diario.titulo).toHaveValue("");
    await expect(diario.cartao("O Cortiço")).toBeVisible();
  });

  test("remover um livro, após confirmar, tira o cartão da lista", async ({ diario, page }) => {
    page.once("dialog", (dialogo) => dialogo.accept());

    await diario.cartao("O Cortiço").getByRole("button", { name: "Remover" }).click();

    await expect(diario.cartoes).toHaveCount(0);
    await expect(page.getByText("Seu diário está em branco")).toBeVisible();
    await expect(diario.total).toHaveText("0");
  });

  test("recusar a confirmação mantém o livro", async ({ diario, page }) => {
    page.once("dialog", (dialogo) => dialogo.dismiss());

    await diario.cartao("O Cortiço").getByRole("button", { name: "Remover" }).click();

    await expect(diario.cartao("O Cortiço")).toBeVisible();
    await expect(diario.total).toHaveText("1");
  });
});
