# Scripts de definição de tabelas SI

Estes arquivos **não fazem parte do build** e não são usados em tempo de
execução. Estão aqui como material de referência.

## O que são

Definições declarativas de tabelas e descritores PSI/SI, escritas na linguagem
de script de um multiplexador de broadcast comercial. Cobrem ATSC (incluindo
A/90, EAS e PIT), DVB, DSM-CC v1 e v2, SCTE-35 e descritores regionais
(australianos, DTG).

A sintaxe usa construções como `macro`, `loop ... looplen`, `valid_descriptors`
e `#include` de outros `.scp` — é uma linguagem própria do equipamento, não C,
não Tcl.

## Por que estão em `reference/`

No repositório original estes arquivos estavam em `libs/` com extensão `.c`.
Isso produzia dois efeitos indesejados:

1. O GitHub classificava o projeto inteiro como "Tcl", porque o detector de
   linguagem tentava interpretar 4.500 linhas de sintaxe desconhecida.
2. Dava a impressão de que o projeto tinha milhares de linhas de C — quando o
   código C real só passou a existir na versão 1.0.0.

Foram movidos para cá, com a extensão `.scp` correta, e marcados como
`linguist-vendored` no `.gitattributes`.

## Situação de direitos

São produto de terceiros. Veja [`../../docs/LICENCIAMENTO.md`](../../docs/LICENCIAMENTO.md).

A informação estrutural que eles carregam está disponível nas normas públicas
e resumida em [`../../docs/PROTOCOLOS.md`](../../docs/PROTOCOLOS.md) — que é a
referência recomendada para quem for trabalhar no código.
