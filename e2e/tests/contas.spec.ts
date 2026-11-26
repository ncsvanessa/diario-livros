import { test, expect, AcessoPage, DiarioPage, SENHA, criarContaPelaApi, loginUnico } from "./fixtures";

test.describe("Contas de usuário", () => {
  test("criar conta pela tela abre um diário vazio com o nome do usuário", async ({ page }) => {
    const acesso = new AcessoPage(page);
    const login = loginUnico();
    await acesso.abrir();

    await acesso.criarConta(login, SENHA);

    await expect(page.getByText(`Olá, ${login}`)).toBeVisible();
    await expect(new DiarioPage(page).total).toHaveText("0");
  });

  test("senha errada mostra erro e mantém o diário fechado", async ({ page, request }) => {
    const login = loginUnico();
    await criarContaPelaApi(request, login);
    await request.post("/api/logout");
    const acesso = new AcessoPage(page);
    await acesso.abrir();

    await acesso.entrar(login, "senha-errada-123");

    await expect(acesso.erro).toHaveText("Login ou senha incorretos.");
    await expect(acesso.sair).toBeHidden();
    await expect(page.getByLabel("Título")).toBeHidden();
  });

  test("criar conta com login já usado mostra erro", async ({ page, request }) => {
    const login = loginUnico();
    await criarContaPelaApi(request, login);
    await request.post("/api/logout");
    const acesso = new AcessoPage(page);
    await acesso.abrir();

    await acesso.criarConta(login, "outra-senha-123");

    await expect(acesso.erro).toHaveText("Login já está em uso.");
  });

  test("sair volta para a tela de entrada e não mostra os livros", async ({ diario, page }) => {
    await diario.adicionar({ titulo: "Vidas Secas", autor: "Graciliano Ramos", nota: 4 });
    await expect(diario.cartao("Vidas Secas")).toBeVisible();

    await new AcessoPage(page).sair.click();

    await expect(page.getByLabel("Usuário")).toBeVisible();
    await expect(diario.cartao("Vidas Secas")).toHaveCount(0);
  });

  test("cada usuário vê só os próprios livros", async ({ diario, page, login }) => {
    const acesso = new AcessoPage(page);
    await diario.adicionar({ titulo: "Livro da primeira conta", autor: "Autora A", nota: 5 });
    await expect(diario.cartao("Livro da primeira conta")).toBeVisible();
    await acesso.sair.click();

    // Outra pessoa cria a própria conta no mesmo navegador: diário vazio.
    await acesso.criarConta(loginUnico(), SENHA);
    await expect(diario.total).toHaveText("0");
    await expect(diario.cartao("Livro da primeira conta")).toHaveCount(0);

    // A primeira conta volta e encontra o livro dela.
    await acesso.sair.click();
    await acesso.entrar(login, SENHA);
    await expect(diario.cartao("Livro da primeira conta")).toBeVisible();
  });
});
