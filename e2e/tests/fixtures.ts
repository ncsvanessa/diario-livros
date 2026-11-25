import { test as base, expect, type APIRequestContext, type Locator, type Page } from "@playwright/test";

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
  readonly cartoes: Locator;
  readonly total: Locator;
  readonly media: Locator;

  constructor(readonly page: Page) {
    this.titulo = page.getByLabel("Título");
    this.autor = page.getByLabel("Autor");
    this.lancamento = page.getByLabel("Lançamento");
    this.leitura = page.getByLabel("Lido em");
    this.salvar = page.locator("#btn-salvar");
    this.erro = page.getByRole("alert");
    this.busca = page.getByRole("searchbox", { name: "Buscar" });
    this.ordem = page.getByRole("combobox", { name: "Ordenar" });
    this.cartoes = page.getByRole("article");
    this.total = page.locator("#st-total");
    this.media = page.locator("#st-media");
  }

  async abrir() {
    await this.page.goto("/");
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

// Cria livros direto pela API: prepara o cenário rápido, sem passar pela interface
// que não é o foco do teste.
export async function cadastrarPelaApi(request: APIRequestContext, livros: LivroDados[]) {
  for (const livro of livros) {
    const r = await request.post("/api/livros", { data: { nota: 0, ...livro } });
    expect(r.status(), await r.text()).toBe(201);
  }
}

async function esvaziarDiario(request: APIRequestContext) {
  const r = await request.get("/api/livros");
  for (const livro of (await r.json()) as { id: number }[]) {
    await request.delete(`/api/livros/${livro.id}`);
  }
}

export const test = base.extend<{ diario: DiarioPage }>({
  diario: async ({ page, request }, use) => {
    // Cada teste começa com o diário vazio, independente da ordem de execução.
    await esvaziarDiario(request);

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
