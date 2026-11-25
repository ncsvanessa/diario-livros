import { test, expect } from "./fixtures";

test.describe("Cadastro de livros", () => {
  test("adicionar um livro mostra o cartão e atualiza as estatísticas", async ({ diario }) => {
    await diario.adicionar({
      titulo: "Vidas Secas",
      autor: "Graciliano Ramos",
      dataLancamento: "1938-01-01",
      dataLeitura: "2026-10-20",
      nota: 4,
    });

    const cartao = diario.cartao("Vidas Secas");
    await expect(cartao).toBeVisible();
    await expect(cartao).toContainText("Graciliano Ramos");
    await expect(cartao).toContainText("Lançado em 1938 · lido em 20/10/2026");
    await expect(cartao.getByLabel("4 de 5 estrelas")).toBeVisible();
    await expect(diario.total).toHaveText("1");
    await expect(diario.media).toHaveText("4,0");
    // O formulário volta ao estado inicial, pronto para o próximo livro.
    await expect(diario.titulo).toHaveValue("");
  });

  test("livro sem título e autor não é salvo e mostra os erros", async ({ diario }) => {
    await diario.salvar.click();

    await expect(diario.erro).toHaveText("Título é obrigatório. Autor é obrigatório.");
    await expect(diario.cartoes).toHaveCount(0);
    await expect(diario.total).toHaveText("0");
  });

  test("leitura anterior ao lançamento é rejeitada e os dados digitados são mantidos", async ({ diario }) => {
    await diario.adicionar({
      titulo: "Dom Casmurro",
      autor: "Machado de Assis",
      dataLancamento: "1899-01-01",
      dataLeitura: "1800-05-10",
    });

    await expect(diario.erro).toHaveText("Data de leitura não pode ser anterior à de lançamento.");
    await expect(diario.cartoes).toHaveCount(0);
    await expect(diario.titulo).toHaveValue("Dom Casmurro");
  });

  test("livros cadastrados continuam lá depois de recarregar a página", async ({ diario, page }) => {
    await diario.adicionar({ titulo: "Grande Sertão: Veredas", autor: "Guimarães Rosa", nota: 5 });
    await expect(diario.cartao("Grande Sertão: Veredas")).toBeVisible();

    await page.reload();

    await expect(diario.cartao("Grande Sertão: Veredas")).toBeVisible();
    await expect(diario.total).toHaveText("1");
  });
});
