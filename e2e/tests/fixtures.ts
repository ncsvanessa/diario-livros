import { randomBytes } from "node:crypto";
import {
  test as base,
  expect,
  request as novaSessao,
  type APIRequestContext,
  type Locator,
  type Page,
} from "@playwright/test";

export type LivroDados = {
  titulo: string;
  autor: string;
  nota?: number;
  capaUrl?: string;
  dataLancamento?: string;
  dataLeitura?: string;
};

// Page object: concentra os seletores da página para que os testes descrevam
// só o comportamento ("adicionar um livro"), e não detalhes do HTML.
export class DiarioPage {
  readonly titulo: Locator;
  readonly autor: Locator;
  readonly lancamento: Locator;
  readonly leitura: Locator;
  readonly salvar: Locator;
  readonly erro: Locator;
  readonly busca: Locator;
  readonly ordem: Locator;
  readonly notaMinima: Locator;
  readonly autoresMaisLidos: Locator;
  readonly cartoes: Locator;
  readonly total: Locator;
  readonly media: Locator;

  constructor(readonly page: Page) {
    this.titulo = page.getByLabel("Título");
    this.autor = page.getByLabel("Autor", { exact: true });
    this.lancamento = page.getByLabel("Lançamento");
    this.leitura = page.getByLabel("Lido em");
    this.salvar = page.locator("#btn-salvar");
    this.erro = page.getByRole("alert");
    this.busca = page.getByRole("searchbox", { name: "Buscar" });
    this.ordem = page.getByRole("combobox", { name: "Ordenar" });
    this.notaMinima = page.getByRole("combobox", { name: "Nota mínima" });
    this.autoresMaisLidos = page.getByRole("list", { name: "Autores mais lidos" }).getByRole("listitem");
    this.cartoes = page.getByRole("article");
    this.total = page.locator("#st-total");
    this.media = page.locator("#st-media");
  }

  async abrir() {
    await this.page.goto("/");
    // A página confere a sessão (/api/eu) antes de mostrar o diário.
    await expect(this.page.getByRole("button", { name: "Sair" })).toBeVisible();
  }

  async escolherNota(nota: number) {
    const nome = nota === 0 ? "Nenhuma estrela" : `${nota} estrela${nota > 1 ? "s" : ""}`;
    await this.page.getByRole("radio", { name: nome, exact: true }).or(
      this.page.getByRole("button", { name: nome, exact: true }),
    ).click();
  }

  async preencher(livro: LivroDados) {
    await this.titulo.fill(livro.titulo);
    await this.autor.fill(livro.autor);
    if (livro.dataLancamento) await this.lancamento.fill(livro.dataLancamento);
    if (livro.dataLeitura) await this.leitura.fill(livro.dataLeitura);
    if (livro.nota !== undefined) await this.escolherNota(livro.nota);
  }

  async adicionar(livro: LivroDados) {
    await this.preencher(livro);
    await this.salvar.click();
  }

  cartao(titulo: string): Locator {
    return this.cartoes.filter({ has: this.page.getByRole("heading", { name: titulo, exact: true }) });
  }

  titulosNaGrade(): Promise<string[]> {
    return this.cartoes.getByRole("heading").allTextContents();
  }
}

// Tela de acesso: entrar e criar conta.
export class AcessoPage {
  readonly usuario: Locator;
  readonly senha: Locator;
  readonly erro: Locator;
  readonly sair: Locator;

  constructor(readonly page: Page) {
    this.usuario = page.getByLabel("Usuário");
    this.senha = page.getByLabel("Senha");
    this.erro = page.getByRole("alert");
    this.sair = page.getByRole("button", { name: "Sair" });
  }

  async abrir() {
    await this.page.goto("/");
  }

  async entrar(login: string, senha: string) {
    await this.usuario.fill(login);
    await this.senha.fill(senha);
    await this.page.getByRole("button", { name: "Entrar", exact: true }).click();
  }

  async criarConta(login: string, senha: string) {
    // "Criar conta" primeiro alterna o formulário; no modo cadastro vira o botão de envio.
    await this.page.getByRole("button", { name: "Criar conta" }).click();
    await this.usuario.fill(login);
    await this.senha.fill(senha);
    await this.page.getByRole("button", { name: "Criar conta" }).click();
  }
}

// Cria livros direto pela API: prepara o cenário rápido, sem passar pela interface
// que não é o foco do teste.
export async function cadastrarPelaApi(request: APIRequestContext, livros: LivroDados[]) {
  for (const livro of livros) {
    const r = await request.post("/api/livros", { data: { nota: 0, ...livro } });
    expect(r.status(), await r.text()).toBe(201);
  }
}

// Login único por teste: cada teste tem a própria conta e, portanto, um diário vazio,
// sem depender da ordem de execução nem do que outros testes deixaram no servidor.
export function loginUnico(): string {
  return `e2e-${randomBytes(6).toString("hex")}`;
}

export const SENHA = "senha-de-teste-e2e";

export async function criarContaPelaApi(request: APIRequestContext, login: string, senha = SENHA) {
  const r = await request.post("/api/cadastro", { data: { login, senha } });
  expect(r.status(), await r.text()).toBe(201);
}

// Outra pessoa, com a própria sessão (separada da do navegador do teste), e os livros dela.
// Devolve o login criado.
export async function criarOutraPessoa(baseURL: string, livros: LivroDados[] = []): Promise<string> {
  const login = loginUnico();
  const sessao = await novaSessao.newContext({ baseURL });
  try {
    await criarContaPelaApi(sessao, login);
    await cadastrarPelaApi(sessao, livros);
  } finally {
    await sessao.dispose();
  }
  return login;
}

// Abas Pessoas e Feed e a página de perfil.
export class SocialPage {
  readonly buscaPessoas: Locator;
  readonly pessoasEncontradas: Locator;
  readonly seguir: Locator;
  readonly seguidores: Locator;
  readonly livrosDoPerfil: Locator;
  readonly posts: Locator;
  readonly feedVazio: Locator;

  constructor(readonly page: Page) {
    this.buscaPessoas = page.getByRole("searchbox", { name: "Buscar pessoas" });
    this.pessoasEncontradas = page.getByRole("list", { name: "Pessoas encontradas" }).getByRole("listitem");
    this.seguir = page.locator("#btn-seguir");
    this.seguidores = page.locator("#perfil-seguidores");
    this.livrosDoPerfil = page.locator("#perfil-livros").getByRole("article");
    this.posts = page.getByRole("list", { name: "Leituras de quem você segue" }).getByRole("article");
    this.feedVazio = page.getByText("Nada por aqui ainda");
  }

  async abrirAba(nome: "Meu diário" | "Feed" | "Pessoas") {
    await this.page.getByRole("navigation", { name: "Seções" }).getByRole("link", { name: nome }).click();
  }

  async abrirPerfil(login: string) {
    await this.abrirAba("Pessoas");
    await this.buscaPessoas.fill(login);
    await this.page.getByRole("link", { name: login, exact: true }).click();
    await expect(this.page.getByRole("heading", { name: login, exact: true })).toBeVisible();
  }

  titulosNoFeed(): Promise<string[]> {
    return this.posts.getByRole("heading").allTextContents();
  }
}

export const test = base.extend<{ diario: DiarioPage; login: string }>({
  // `request` passa a ser o da página: compartilha os cookies do navegador, então as
  // chamadas de API dos testes são feitas como o usuário logado na página.
  request: async ({ page }, use) => {
    await use(page.request);
  },

  login: async ({}, use) => {
    await use(loginUnico());
  },

  diario: async ({ page, request, login }, use) => {
    // Cria a conta pela API (o cookie de sessão fica no navegador) antes de abrir a página.
    await criarContaPelaApi(request, login);

    // A página consulta a Open Library ao digitar o título. Nos testes, nada sai para
    // a internet: por padrão a busca não retorna resultados, e cada teste pode
    // registrar a sua própria resposta (page.route mais recente tem prioridade).
    await page.route("https://openlibrary.org/**", (rota) => rota.fulfill({ json: { docs: [] } }));
    await page.route("https://covers.openlibrary.org/**", (rota) => rota.abort());

    const diario = new DiarioPage(page);
    await diario.abrir();
    await use(diario);
  },
});

export { expect };
