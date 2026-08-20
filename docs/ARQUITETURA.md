# Arquitetura

Este documento explica como o código está organizado e, principalmente, **por
que** cada decisão foi tomada. Se você só quer usar a ferramenta, comece por
[USO.md](USO.md).

## Visão geral

O projeto é dividido em duas camadas: uma biblioteca de núcleo, sem estado
global e sem I/O de rede embutido, e uma casca de linha de comando que a
utiliza. A separação não é cosmética — ela é o que permite que os testes
exercitem o parser diretamente, sem subir socket nenhum, e o que deixa a porta
aberta para reaproveitar o núcleo em outro programa.

```
                      ┌─────────────────────────────────┐
    src/main.c        │  despacho de subcomandos        │
                      └───────────────┬─────────────────┘
                                      │
        ┌─────────────────────────────┼─────────────────────────────┐
        │                             │                             │
  cmd_stream.c                  cmd_receive.c                cmd_analyze.c
  arquivo → rede                rede → arquivo               arquivo → relatório
        │                             │                             │
        └─────────────────────────────┼─────────────────────────────┘
                                      │
        ┌──────────┬──────────┬───────┴──┬──────────┬──────────┐
     reader.c    ts.c      psi.c      desc.c     rtp.c      net.c
     leitura   pacotes   seções    descritores  RFC 2250  sockets UDP
                            │
                        pacing.c
                     ritmo de emissão
```

## Os módulos, um a um

### `ts.c` — a camada de pacote

Interpreta os 188 bytes de um pacote de transporte. Não aloca memória, não faz
I/O e não guarda estado entre chamadas: recebe um ponteiro, devolve uma
estrutura preenchida. Essa disciplina é o que torna o módulo trivialmente
testável e seguro para usar em múltiplas threads sem sincronização.

Três detalhes merecem atenção:

**O campo de adaptação é validado, não presumido.** O byte de comprimento pode
declarar um tamanho que ultrapasse o pacote. Um parser que confia nesse valor
lê memória alheia. Aqui cada leitura de PCR/OPCR confere os limites antes de
acontecer.

**`adaptation_field_control == 0` é rejeitado.** A norma marca esse valor como
reservado. Preferimos contabilizá-lo como pacote inválido a chutar uma
interpretação — em um analisador, dado errado é pior que dado ausente.

**O contador de continuidade tem quatro casos, não dois.** A implementação
ingênua compara `cc == anterior + 1` e chama todo o resto de erro. Isso produz
falsos positivos em três situações perfeitamente legais: pacotes sem payload
não incrementam o contador; duplicatas exatas são permitidas uma vez por
pacote; e o `discontinuity_indicator` autoriza o transmissor a reiniciar a
contagem. `vsti_cc_check()` distingue os quatro casos.

### `psi.c` — remontagem de seções

Esta é a parte com mais armadilhas do projeto.

Uma seção PSI/SI pode ser menor que um pacote, exatamente do tamanho de um, ou
se espalhar por vários. E o mecanismo que a norma usa para marcar onde uma
seção começa dentro de um pacote — o `pointer_field` — é a origem do bug mais
comum em implementações caseiras.

Quando o bit PUSI está ligado, o primeiro byte do payload não é dado: é um
contador de quantos bytes da **cauda da seção anterior** vêm antes do início da
próxima. Ignorá-lo tem um sintoma característico e enganoso: tudo parece
funcionar, mas a última seção de cada ciclo de repetição nunca aparece. Como as
tabelas se repetem, o programa parece só "perder uma de vez em quando".

O montador aqui trata os três casos: cauda pendente antes do ponteiro, várias
seções completas em um mesmo pacote, e seção que atravessa a fronteira. O teste
`test_pointer_field_tail` existe especificamente para travar esse
comportamento.

Toda seção com sintaxe estendida é verificada por CRC-32/MPEG-2 antes de ser
entregue. A verificação usa uma propriedade elegante do CRC: calcular o CRC
sobre a seção inteira, **incluindo os 4 bytes do próprio CRC**, deve dar zero.
Não é preciso extrair e comparar.

O CRC é implementado bit a bit, sem tabela. É uma escolha consciente: seções
PSI são pequenas e pouco frequentes, e uma tabela estática exigiria estado
global mutável e cuidado com inicialização concorrente em troca de um ganho
irrelevante.

### `desc.c` — descritores e texto

O iterador de descritores para no primeiro comprimento inconsistente em vez de
tentar se recuperar. Continuar leria bytes de outro campo e produziria strings
plausíveis mas falsas — o pior tipo de defeito em uma ferramenta de análise.

A decodificação de texto merece explicação. A ARIB STD-B24 define um esquema
com troca dinâmica de conjuntos de caracteres via sequências de escape,
pensado para o japonês. A variante brasileira (ABNT NBR 15603-2) restringe isso
na prática ao alfabeto latino, o que permite um decodificador muito mais
simples: descartamos os controles C0/C1 e as sequências de escape, e
convertemos o restante de ISO/IEC 8859-15 para UTF-8. As oito posições em que a
8859-15 difere da 8859-1 estão tabeladas em `latin9_codepoint()`.

### `rtp.c` — encapsulamento

Cabeçalho RTP de 12 bytes, versão 2, payload type 33 (MP2T, alocado
estaticamente pela RFC 3551 — não precisa de negociação SDP para ser
reconhecido por VLC, ffmpeg ou analisadores de rede).

**Por que 7 pacotes TS por datagrama.** 7 × 188 = 1316 bytes de payload.
Somando 12 de RTP, 8 de UDP e 20 de IPv4, chega-se a 1356 — confortavelmente
abaixo da MTU de 1500. Com 8 pacotes seriam 1504 + cabeçalhos, o que força
fragmentação IP. Em rede de contribuição de vídeo isso é grave: perder um
fragmento invalida o datagrama inteiro, multiplicando o efeito de qualquer
perda.

O detector de perda trabalha em aritmética de 16 bits **com sinal**. Essa é a
forma correta de lidar com o wrap-around da sequência RTP: a diferença passa a
ser o menor deslocamento circular, de modo que 65535 → 0 conta como avanço de
um, e não como salto de 65535 para trás. Um pacote atrasado que chega depois de
já ter sido contado como perdido abate a estatística, para não inflar a taxa de
perda relatada.

### `pacing.c` — ritmo de emissão

Um transport stream só é útil no destino se chegar na mesma taxa em que foi
produzido. Enviar "o mais rápido possível" estoura o buffer de qualquer
decodificador; enviar devagar demais o esvazia.

Duas decisões sustentam a precisão:

**Deadlines absolutos, não `sleep(X)`.** Cada espera real dura um pouco mais do
que o pedido. Somando ao longo de uma hora de transmissão, esse excesso viraria
segundos de atraso acumulado. Trabalhar com um instante-alvo absoluto de
`CLOCK_MONOTONIC` faz o erro de cada sleep se corrigir sozinho no próximo.

**Resíduo fracionário em ponto fixo.** O intervalo entre datagramas quase nunca
é inteiro em nanossegundos — 1316 bytes a 17,27 Mbit/s dá 609.451,7… ns.
Guardar a fração em ponto fixo 16.16 mantém a taxa média exata sem recorrer a
ponto flutuante no caminho crítico.

Há um limiar de 250 µs abaixo do qual não vale a pena chamar o kernel: a
latência de agendamento seria maior que a espera desejada. O crédito fica
registrado no deadline e é consumido em uma espera maior mais adiante.

Quando o atraso passa de 100 ms, o pacer **desiste de recuperar o tempo
perdido** e realinha o deadline com o presente. Insistir geraria uma rajada que
só pioraria a situação no receptor.

O estimador de bitrate mede a distância entre PCRs consecutivos:

```
bitrate = bytes_entre_PCRs × 8 × 27.000.000 / delta_PCR_em_27MHz
```

com um filtro de sanidade que descarta intervalos abaixo de 1 ms ou acima de
1 s — indicativos de descontinuidade de relógio, não de taxa real.

### `net.c` — sockets

Suporta IPv4 e IPv6 via `getaddrinfo`, unicast e multicast.

Em unicast o socket é **conectado**. Além de simplificar o envio, isso faz o
kernel reportar ICMP port-unreachable como erro em `send()`, o que dá um
diagnóstico imediato quando o receptor não está no ar. Em multicast não
conectamos, porque em alguns sistemas isso interfere com a seleção de
interface.

Na recepção multicast, o `bind` é feito **no endereço do grupo**, não em
`INADDR_ANY`. Sem isso, um analisador escutando a porta 1234 receberia todos os
fluxos da rede que usassem essa porta.

A interface pode ser informada por nome (`eth0`) ou por IP local
(`192.168.0.10`) — as duas convenções aparecem em servidores de broadcast, e
não custa aceitar ambas.

### `reader.c` — leitura e alinhamento

Entrega sempre pacotes de 188 bytes alinhados, independentemente de a fonte
usar 188, 192 (com timecode de 4 bytes), 204 ou 208 (com paridade Reed-Solomon
de ASI).

A detecção exige **cinco** bytes de sync no mesmo espaçamento antes de declarar
alinhamento. Um único `0x47` aparece o tempo todo dentro de payload de vídeo;
cinco em progressão aritmética exata, não.

Perda de sincronismo no meio do fluxo — comum em captura de ASI com erro — é
tratada avançando um byte e refazendo a detecção, contabilizando o evento em
`resyncs`. Abortar seria pior: o resto do arquivo pode estar perfeito.

## Fluxo de dados no `stream`

1. `reader` entrega um pacote de 188 bytes alinhado.
2. `ts_parse` extrai o cabeçalho; se houver PCR, o relógio de 90 kHz é travado
   nele.
3. O pacote é escrito direto no slot correspondente do buffer do datagrama —
   **sem cópia intermediária**: o leitor já escreve no lugar final.
4. Ao completar 7 pacotes, o cabeçalho RTP é escrito nos 12 bytes reservados no
   início do mesmo buffer.
5. `pacer` espera até o instante correto.
6. `sock_send` emite um único datagrama.

O timestamp RTP é congelado no início da montagem do datagrama, não no fim.
A RFC 2250 pede o instante de amostragem do **primeiro** byte do payload; usar
um PCR que chegou no quinto pacote falsearia o valor.

## O que ficou de fora, e por quê

- **Sem threads.** O caminho crítico é um laço de leitura, encapsulamento e
  envio. Threads adicionariam sincronização e jitter sem ganho: o gargalo real
  é o socket, não a CPU.
- **Sem dependências externas.** Nada de libavformat ou libdvbpsi. O objetivo
  do projeto é justamente entender e implementar essas camadas.
- **Sem alocação no caminho crítico.** Todos os buffers de `stream` e `receive`
  são de tamanho fixo, decididos na inicialização. Só o `analyze` aloca, e uma
  vez só.
- **Sem FEC (SMPTE 2022-1).** Seria a evolução natural para uso em produção
  sobre rede não confiável. Ainda não está implementado.
