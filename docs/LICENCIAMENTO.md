# Licenciamento e material de terceiros

## Resumo

O código-fonte da ferramenta — `src/`, `include/`, `tests/`, `docs/`, os
arquivos de build e o `README.md` — é trabalho original e está sob a licença
MIT, conforme o arquivo [`LICENSE`](../LICENSE).

**A licença MIT não se estende ao restante do repositório.** Vários diretórios
contêm material produzido por terceiros, incluído durante o desenvolvimento
como referência técnica. Este documento identifica cada um deles para que
ninguém — inclusive quem clonar o projeto — presuma permissão que não existe.

---

## Material de terceiros presente no repositório

### `system/` — firmware e configuração de equipamento

Contém imagens de boot, binários de FPGA e a distribuição Tcl embarcada de um
multiplexador/processador de broadcast (`vxworks`, `theos`, `secboot`,
`fpga_*`, `app_main`, entre outros), em duas versões de firmware.

São binários proprietários do fabricante do equipamento. Não são
redistribuíveis sob MIT nem sob nenhuma licença livre, e a presença deles em um
repositório público pode configurar distribuição não autorizada.

**Recomendação:** manter apenas o que for tecnicamente necessário para
documentar o projeto — tipicamente os arquivos de configuração em texto e os
manifests — e remover as imagens binárias do histórico. Se elas foram úteis
para a pesquisa, descrevê-las em texto é suficiente e não cria exposição.

### `boot-teste/` — logs e configuração de bancada

`bootA12.hex`, logs de bootloader e `netvx.txt` capturados do equipamento
durante os testes. Logs e saídas de terminal produzidos por você são seus; o
firmware em formato hex, não.

`config.xml` descreve a configuração de saída ASI e os parâmetros TMCC
(modulação, FEC, entrelaçamento, segmentação por camada) usados nos ensaios. É
documento técnico do seu trabalho e pode ficar.

### `reference/si-scripts/` — definições de tabelas SI

Vinte e quatro arquivos `.scp` com definições declarativas de tabelas e
descritores PSI/SI (ATSC, DVB, DSM-CC, SCTE-35, ARIB). Pela sintaxe e pelos
comentários assinados, vêm da biblioteca de script de um multiplexador
comercial.

Esses arquivos **não são código C**, apesar de estarem no repositório com
extensão `.c` originalmente — foi essa renomeação que fez o GitHub classificar
o projeto inteiro como "Tcl". Eles foram movidos para `reference/si-scripts/`
com a extensão correta e marcados como `linguist-vendored`.

Eles não são compilados nem usados em tempo de execução. Servem como referência
de estrutura de tabelas — o mesmo papel que as normas cumprem, com a diferença
de que as normas você pode citar livremente.

**Recomendação:** como a informação que eles carregam está disponível nas
normas públicas (ISO/IEC 13818-1, ETSI EN 300 468, ARIB STD-B10) e agora está
resumida em [`PROTOCOLOS.md`](PROTOCOLOS.md), o custo de removê-los é baixo e o
benefício de clareza jurídica é alto.

### `datasheets/` — folhas de dados de componentes

PDFs da Altera/Intel (Cyclone III), Micron (N25Q) e Maxim (MAX3160). São
publicações dos fabricantes, redistribuíveis apenas nos termos de cada um —
geralmente permitido para uso interno, não para redistribuição.

**Recomendação:** substituir os PDFs por uma lista de links e números de peça.
Além de resolver a questão de direitos, tira ~40 MB do clone.

### `one-seg/` — capturas de transmissão

Três arquivos `.mpg` com sinal one-seg da TV Gazeta, capturados em 27/10/2021,
somando cerca de 20 MB.

Conteúdo audiovisual transmitido é obra protegida da emissora. Trechos curtos
usados para pesquisa e teste técnico têm justificativa razoável, mas não são
livres de direitos.

Do ponto de vista prático eles são o melhor material de teste que este projeto
tem — foi contra eles que o encapsulador foi validado byte a byte. Se algum dia
for preciso removê-los, o teste de integração já tem um gerador de fluxo
sintético como alternativa.

### `json-analysis/` — grade de programação

`epg_path_extrator_test_bts.json` contém metadados de programação com URLs de
imagens de um provedor comercial. São dados factuais de grade, mas as sinopses
são textos de terceiros.

### `Table of log.html`

Arquivo de log em HTML na raiz do repositório, sem relação com o build. Se não
tiver função, o lugar dele é fora do repositório ou dentro de `docs/`.

---

## O que fazer com isso

Nada aqui exige ação imediata, e nada disso é incomum em repositório de
projeto acadêmico — material de referência se acumula naturalmente durante a
pesquisa. Mas o repositório é público e está sob MIT, o que declara ao mundo
que **todo** o conteúdo pode ser reusado livremente. Para a parte de terceiros,
essa declaração não é sua para fazer.

Em ordem de custo-benefício:

1. **Já feito:** este documento e a nota no `README.md` deixam explícito que a
   MIT cobre apenas o código original.
2. **Baixo custo, alto retorno:** trocar `datasheets/` por links (−40 MB) e
   avaliar a remoção de `system/boot/**`.
3. **Se optar por remover binários:** apagar o arquivo em um commit novo não
   basta — ele continua no histórico do Git. É preciso reescrever com
   `git filter-repo` e forçar o push, o que quebra clones existentes. Como o
   projeto tem poucos colaboradores, o momento de fazer isso é agora.

---

## Dependências do código

Nenhuma. A ferramenta usa apenas a biblioteca padrão C11 e a API POSIX de
sockets. Não há bibliotecas de terceiros vinculadas, e portanto nenhuma
obrigação de licença transitiva.
