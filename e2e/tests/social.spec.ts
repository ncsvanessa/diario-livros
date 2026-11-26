import { test, expect, SocialPage, criarOutraPessoa } from "./fixtures";

test.describe("Pessoas, perfil e feed", () => {
  test("buscar uma pessoa e abrir o perfil mostra os livros dela, sem editar", async ({ diario, page, baseURL }) => {
    const outra = await criarOutraPessoa(baseURL!, [{ titulo: "Vidas Secas", autor: "Graciliano Ramos", nota: 4 }]);
    const social = new SocialPage(page);

    await social.abrirAba("Pessoas");
    await social.buscaPessoas.fill(outra);
    await expect(social.pessoasEncontradas).toHaveCount(1);
    await page.getByRole("link", { name: outra, exact: true }).click();

    await expect(page.getByRole("heading", { name: outra, exact: true })).toBeVisible();
    await expect(social.livrosDoPerfil).toHaveCount(1);
    await expect(social.livrosDoPerfil.getByRole("heading")).toHaveText("Vidas Secas");
    await expect(social.livrosDoPerfil.getByRole("button", { name: "Editar" })).toHaveCount(0);
  });

  test("seguir atualiza o botão e os seguidores, e continua valendo ao recarregar", async ({ diario, page, baseURL }) => {
    const outra = await criarOutraPessoa(baseURL!);
    const social = new SocialPage(page);
    await social.abrirPerfil(outra);
    await expect(social.seguidores).toHaveText("0");

    await social.seguir.click();

    await expect(social.seguir).toHaveText("Deixar de seguir");
    await expect(social.seguidores).toHaveText("1");
    await page.reload();
    await expect(social.seguir).toHaveText("Deixar de seguir");
  });

  test("o feed mostra os livros de quem eu sigo, do mais recente para o mais antigo", async ({
    diario,
    page,
    baseURL,
  }) => {
    const outra = await criarOutraPessoa(baseURL!, [
      { titulo: "Primeiro livro", autor: "Autora A", nota: 3 },
      { titulo: "Segundo livro", autor: "Autora B", nota: 5 },
    ]);
    const social = new SocialPage(page);
    await social.abrirPerfil(outra);
    await social.seguir.click();
    await expect(social.seguir).toHaveText("Deixar de seguir");

    await social.abrirAba("Feed");

    await expect(social.posts).toHaveCount(2);
    expect(await social.titulosNoFeed()).toEqual(["Segundo livro", "Primeiro livro"]);
    await expect(social.posts.first()).toContainText(`${outra} registrou uma leitura`);
  });

  test("deixar de seguir tira os livros da pessoa do feed", async ({ diario, page, baseURL }) => {
    const outra = await criarOutraPessoa(baseURL!, [{ titulo: "Iracema", autor: "José de Alencar", nota: 2 }]);
    const social = new SocialPage(page);
    await social.abrirPerfil(outra);
    await social.seguir.click();
    await social.abrirAba("Feed");
    await expect(social.posts).toHaveCount(1);

    await social.abrirPerfil(outra);
    await social.seguir.click();
    await expect(social.seguir).toHaveText("Seguir");
    await social.abrirAba("Feed");

    await expect(social.posts).toHaveCount(0);
    await expect(social.feedVazio).toBeVisible();
  });

  test("no próprio perfil não há botão de seguir", async ({ diario, page, login }) => {
    const social = new SocialPage(page);

    await social.abrirPerfil(login);

    await expect(page.getByText("Este é o seu perfil.")).toBeVisible();
    await expect(social.seguir).toBeHidden();
  });
});
