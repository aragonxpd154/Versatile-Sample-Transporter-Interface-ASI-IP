# Changelog

Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/).
Versionamento semântico.

## [1.0.0] — 2026-08-20

Primeira versão executável do projeto. Até aqui o repositório continha material
de pesquisa, mas nenhum código que compilasse.

### Adicionado

**Ferramenta `vsti` com três subcomandos**

- `stream` — encapsulamento de MPEG-2 TS em RTP/UDP conforme RFC 2250, ou UDP
  puro; unicast e multicast, IPv4 e IPv6; ritmo derivado do PCR, taxa constante
  ou sem controle; seleção de interface, TTL, DSCP e buffer de socket; modo
  loop e limite de duração.
- `receive` — recepção com detecção automática de RTP ou UDP puro, entrada em
  grupo multicast, gravação em arquivo ou stdout, e relatório de perda,
  reordenação e duplicação de RTP.
- `analyze` — inspeção em passagem única: censo de PIDs, erros de continuidade,
  taxa medida pelo PCR, e decodificação de PAT, PMT, SDT e EIT com texto de
  EPG; saída em texto ou JSON.

**Biblioteca de núcleo**

- `ts` — parsing de pacote de 188 bytes, campo de adaptação, extração de PCR e
  OPCR, detecção de alinhamento em 188/192/204/208 bytes, e verificação de
  continuidade que distingue os quatro casos previstos na norma.
- `psi` — remontagem de seções com tratamento correto de `pointer_field`,
  seções fragmentadas e múltiplas seções por pacote; CRC-32/MPEG-2; parsers de
  PAT, PMT, SDT e EIT; conversão de data MJD + BCD para epoch Unix.
- `desc` — iterador de descritores e decodificação de texto ISDB-Tb
  (ARIB STD-B24 / ABNT NBR 15603) de ISO-8859-15 para UTF-8.
- `rtp` — escrita e leitura de cabeçalho RTP, e detector de perda com
  aritmética de 16 bits com sinal para tratar o wrap-around de sequência.
- `net` — sockets UDP com suporte a multicast, seleção de interface por nome ou
  IP, TTL, DSCP e dimensionamento de buffers.
- `pacing` — controle de ritmo com deadlines absolutos e acumulador de resíduo
  em ponto fixo, mais estimador de bitrate a partir do PCR.
- `reader` — leitura com detecção de alinhamento e recuperação de perda de
  sincronismo no meio do fluxo.

**Build e verificação**

- `Makefile` e `CMakeLists.txt` equivalentes, sem dependências externas.
- Suíte de testes unitários cobrindo CRC-32, camada TS, remontagem PSI, RTP e
  descritores.
- Teste de integração que emite uma captura real por UDP, recebe de volta e
  compara byte a byte, em RTP e em UDP puro.
- Alvo `make asan` com AddressSanitizer e UBSan.

**Documentação**

- `README.md` em inglês.
- `docs/ARQUITETURA.md`, `docs/USO.md`, `docs/PROTOCOLOS.md`,
  `docs/LICENCIAMENTO.md` e `docs/HISTORICO.md` em português.
- `CONTRIBUTING.md`.

### Corrigido

- **CI quebrado desde a criação.** O workflow executava `./configure && make`,
  mas o repositório nunca teve nem `configure` nem `Makefile`; todo push
  aparecia como falha. Substituído por um workflow que compila com GCC e Clang,
  roda a suíte de testes, executa uma passagem com sanitizers, valida o build
  alternativo por CMake e roda análise estática com cppcheck.

- **Arquivos com extensão errada.** Os 24 arquivos em `libs/` tinham extensão
  `.c` mas não eram código C — são scripts `.scp` de definição de tabelas SI.
  A renomeação fazia o GitHub classificar o projeto como Tcl e sugeria a
  existência de ~4.500 linhas de C inexistentes. Movidos para
  `reference/si-scripts/` com a extensão correta.

### Alterado

- Estrutura do repositório reorganizada: código em `src/` e `include/vsti/`,
  testes em `tests/`, documentação em `docs/`, material de referência em
  `reference/`.
- `.gitattributes` marcando material de referência como `linguist-vendored`,
  para que as estatísticas de linguagem do GitHub reflitam o código do projeto.
- `.gitignore` cobrindo artefatos de build e saídas de execução.
- Situação de licenciamento do material de terceiros documentada de forma
  explícita em `docs/LICENCIAMENTO.md`, deixando claro que a MIT cobre apenas o
  código original.
