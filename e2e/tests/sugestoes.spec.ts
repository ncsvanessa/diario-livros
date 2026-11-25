import { test, expect } from "./fixtures";

// A Open Library é simulada: o teste não depende da internet nem de dados que mudam.
const respostaVidasSecas = {
  docs: [
    { title: "Vidas Secas", author_name: ["Graciliano Ramos"], first_publish_year: 1938, cover_i: 123 },
    // Mesma obra repetida (outra edição): deve aparecer uma vez só.
    { title: "Vidas Secas", author_name: ["Graciliano Ramos"], first_publish_year: 1938 },
    { title: "Vidas Secas e Outras", author_name: ["Fulano"] },
  ],
};

test.describe("Sugestões da Open Library", () => {
  test("escolher uma sugestão preenche título, autor, capa e ano", async ({ diario, page }) => {
    await page.route("https://openlibrary.org/search.json**", (rota) =>
      rota.fulfill({ json: respostaVidasSecas }),
    );

    await diario.titulo.pressSequentially("Vidas");

    const opcoes = page.getByRole("listbox", { name: "Sugestões de livros" }).getByRole("option");
    await expect(opcoes).toHaveCount(2);
    await opcoes.first().click();

    await expect(diario.titulo).toHaveValue("Vidas Secas");
    await expect(diario.autor).toHaveValue("Graciliano Ramos");
    await expect(diario.lancamento).toHaveValue("1938-01-01");
    await expect(page.getByLabel("Capa")).toHaveValue("https://covers.openlibrary.org/b/id/123-L.jpg");
    // Só o ano é conhecido: a página pede para conferir dia e mês.
    await expect(page.getByText("Só sabemos o ano. Confira o dia e o mês.")).toBeVisible();
  });

  test("falha na Open Library mostra aviso e o cadastro manual continua funcionando", async ({ diario, page }) => {
    await page.route("https://openlibrary.org/search.json**", (rota) => rota.fulfill({ status: 503 }));

    await diario.titulo.pressSequentially("Vidas");
    await expect(page.getByText("Não foi possível buscar sugestões agora.")).toBeVisible();

    await diario.autor.fill("Graciliano Ramos");
    await diario.salvar.click();

    await expect(diario.cartao("Vidas")).toBeVisible();
  });
});
