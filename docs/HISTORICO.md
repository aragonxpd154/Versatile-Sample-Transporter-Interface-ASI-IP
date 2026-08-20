# Histórico do projeto

## Origem

O projeto nasceu como trabalho acadêmico, com um escopo declarado bem amplo.
O texto original do `README.md`, preservado aqui como registro:

> Estou atualmente engajado em um projeto acadêmico que tem como objetivo o
> desenvolvimento de um sistema de conversão e descompressão de fluxos de
> transporte broadcast MPEG, bem como o encapsulamento desses fluxos no
> protocolo IP Ethernet. Esse projeto é motivado pela crescente demanda por
> sistemas de transmissão de vídeo de alta qualidade e pelo crescente uso da
> Internet como um meio de distribuição de conteúdo audiovisual.
>
> O projeto em questão é composto por duas principais etapas: a primeira
> envolve a descompressão dos fluxos de transporte broadcast MPEG, que são
> codificados usando os padrões MPEG-2 ou MPEG-4, e a conversão desses fluxos
> para um formato compatível com o protocolo IP Ethernet. A segunda etapa
> envolve o encapsulamento dos fluxos de transporte IP em pacotes Ethernet,
> permitindo assim a transmissão desses fluxos através de redes Ethernet.
>
> Para a implementação dessas etapas, é necessário o uso de técnicas avançadas
> de processamento de sinais digitais, bem como a utilização de algoritmos de
> compressão e descompressão de vídeo, como o padrão H.264. Além disso, a
> implementação do encapsulamento IP Ethernet requer conhecimentos em
> protocolos de rede, como o protocolo de controle de transmissão (TCP) e o
> protocolo de internet (IP).
>
> O projeto é baseado em uma arquitetura de hardware FPGA (Field Programmable
> Gate Array), que oferece flexibilidade e alta capacidade de processamento
> para a realização das etapas de conversão, descompressão e encapsulamento. A
> implementação do sistema é realizada através da programação do FPGA
> utilizando linguagens de descrição de hardware, como a Verilog ou VHDL.

O material acumulado no repositório reflete essa pesquisa: datasheets do
Cyclone III e da memória de configuração N25Q, dumps de firmware e configuração
do equipamento de bancada, capturas reais de one-seg da TV Gazeta, e uma
biblioteca de definições de tabelas PSI/SI usada como referência.

## Onde o projeto estava

Até esta revisão, o repositório continha material de pesquisa, mas nenhum
código executável:

- Os arquivos em `libs/*.c` não eram C. Eram scripts `.scp` de definição de
  tabelas, renomeados para `.c` — o que fazia o GitHub classificar o projeto
  como "Tcl" e dava a impressão de haver 4.500 linhas de C que não existiam.
- O workflow de CI executava `./configure && make`. Como não havia `configure`
  nem `Makefile`, ele falhava em todo push desde que foi criado.
- Não havia build system, testes, nem qualquer ponto de entrada.

## O que foi implementado

A decisão foi construir a **metade que roda em CPU de propósito geral** — a
camada de transporte —, que é a parte independente de hardware e a que pode ser
verificada de forma rigorosa contra o material de teste que o próprio
repositório já continha.

O resultado é a ferramenta `vsti`, com três subcomandos:

- `stream` — lê um transport stream e o emite encapsulado em RTP/UDP conforme a
  RFC 2250, com ritmo derivado do PCR do próprio fluxo;
- `receive` — recebe, detecta o formato, desencapsula e grava, reportando
  perda, reordenação e duplicação;
- `analyze` — percorre o fluxo e relata PIDs, erros de continuidade, taxa
  medida pelo PCR e o conteúdo decodificado de PAT, PMT, SDT e EIT.

A validação usa as capturas one-seg do próprio repositório. O ciclo completo
— emitir 14.070 pacotes por UDP e recebê-los de volta — resulta em um arquivo
**idêntico byte a byte** ao original, em RTP e em UDP puro.

Uma verificação cruzada independente vale registro: o metadado do gravador que
produziu a captura (`one-seg/data/TVGAZETA1SEG_20211027_183507.xml`) registra
`<ES-CC-ERRORS>5</ES-CC-ERRORS>`, e o analisador escrito aqui, sem qualquer
conhecimento desse arquivo, conta exatamente 5 erros de continuidade no mesmo
fluxo.

## O que ficou fora do escopo

**Descompressão de vídeo.** Não está implementada e, sinceramente, não deveria
estar: um *transport interface* existe justamente para mover fluxos elementares
sem transcodificá-los. Decodificar H.264 no caminho seria trabalho de um
decodificador, não de uma interface de transporte. Se em algum momento for
necessário, o lugar certo é integrar um decodificador existente, não reescrever
um.

**Implementação em FPGA.** Não há HDL no repositório. Os datasheets e os dumps
de firmware documentam o alvo de hardware pretendido e ficam como referência
para uma eventual etapa de gateware. A arquitetura do código foi pensada com
isso em mente: as camadas de parsing e encapsulamento são independentes de I/O,
então a fronteira entre "o que roda em software" e "o que iria para o FPGA"
está clara.

**FEC (SMPTE 2022-1).** Seria a evolução natural para uso em produção sobre
rede não confiável, e é o item mais óbvio da lista de próximos passos.

## Próximos passos sugeridos

1. **FEC SMPTE 2022-1** — proteção contra perda em rede de contribuição.
2. **Remultiplexação** — filtrar PIDs e reconstruir PAT/PMT para extrair um
   SPTS de um MPTS.
3. **Saída SRT ou RIST** — transporte confiável sobre internet pública, que é o
   que a motivação original do projeto descrevia.
4. **Análise de jitter de PCR** — medir a conformidade do PCR com a norma,
   complementando o que o `analyze` já reporta.
5. **Higiene do repositório** — ver [`LICENCIAMENTO.md`](LICENCIAMENTO.md).
