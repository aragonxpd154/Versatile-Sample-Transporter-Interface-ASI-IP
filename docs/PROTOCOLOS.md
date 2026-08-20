# Referência de protocolos

Resumo das estruturas que este projeto implementa, com os números que
realmente importam na hora de depurar. Não substitui as normas; serve para
você não precisar abrir três PDFs para conferir um deslocamento de bit.

## Normas de referência

| Norma | Assunto |
|---|---|
| ISO/IEC 13818-1 | MPEG-2 Systems: pacote de transporte, PAT, PMT, PCR |
| ETSI EN 300 468 | DVB-SI: SDT, EIT, NIT, descritores, codificação de data |
| ARIB STD-B10 | SI para ISDB, tabelas e descritores próprios |
| ARIB STD-B24 | Codificação de caracteres para ISDB |
| ABNT NBR 15603 | SBTVD (ISDB-Tb): perfil brasileiro de SI |
| RFC 3550 | RTP: cabeçalho, sequência, timestamp, SSRC |
| RFC 2250 | Transporte de MPEG-1/2 sobre RTP |
| RFC 3551 | Perfil de áudio/vídeo: payload type 33 = MP2T |

---

## Pacote de transporte

188 bytes, sempre. Cabeçalho de 4 bytes:

```
 byte 0    byte 1                byte 2      byte 3
┌────────┬─┬─┬─┬─────────────────────────┬───┬───┬────────┐
│  0x47  │E│S│P│         PID (13)        │TSC│AFC│  CC(4) │
└────────┴─┴─┴─┴─────────────────────────┴───┴───┴────────┘
          │ │ │                            │   │
          │ │ └ transport_priority         │   └ adaptation_field_control
          │ └── payload_unit_start (PUSI)  └──── transport_scrambling_control
          └──── transport_error_indicator (TEI)
```

`adaptation_field_control`:

| Valor | Significado |
|---|---|
| `00` | Reservado — pacote deve ser descartado |
| `01` | Só payload |
| `10` | Só campo de adaptação |
| `11` | Campo de adaptação seguido de payload |

### Campo de adaptação

Byte 4 é o comprimento (sem contar ele mesmo). Comprimento 0 é legal: significa
um único byte de enchimento, sem flags. Byte 5, quando existe, traz as flags:

```
 bit  7        6        5        4      3      2       1        0
     disc   random    ES     PCR    OPCR  splice  private   extension
    inuity  access  priority flag   flag   point    data      flag
```

### PCR

6 bytes, presente quando `PCR_flag` está ligado:

```
program_clock_reference_base        33 bits   (unidade 1/90.000 s)
reserved                             6 bits
program_clock_reference_extension    9 bits   (unidade 1/27.000.000 s)
```

Valor unificado: `base × 300 + extension`, em unidades de 27 MHz.

Os 33 bits da base cruzam a fronteira do byte 4, o que explica o deslocamento
de 7 posições no código. É onde mais se erra — por isso o teste usa o valor
máximo de 33 bits e extensão 299.

**Estimativa de taxa a partir do PCR:**

```
bitrate = bytes_entre_dois_PCRs × 8 × 27.000.000 / (PCR₂ − PCR₁)
```

A norma exige um PCR a cada 100 ms no máximo (40 ms em DVB). Intervalos muito
fora disso indicam descontinuidade, não taxa real.

---

## Seções PSI/SI

Todas as tabelas compartilham o mesmo envelope:

```
table_id                      8 bits
section_syntax_indicator      1 bit
'0' + reserved                3 bits
section_length               12 bits   ← bytes APÓS este campo, incluindo o CRC
──────────────────────────────────── (fim do cabeçalho curto)
table_id_extension           16 bits
reserved                      2 bits
version_number                5 bits
current_next_indicator        1 bit
section_number                8 bits
last_section_number           8 bits
──────────────────────────────────── (corpo específico da tabela)
CRC_32                       32 bits
```

Tamanho total = `3 + section_length`. Máximo de 1024 bytes para a maioria das
tabelas, 4096 para EIT e tabelas privadas.

### O `pointer_field`

Quando PUSI está ligado, o **primeiro byte do payload não é dado**: é o
`pointer_field`, que diz quantos bytes de cauda da seção anterior vêm antes do
início da próxima seção.

```
payload de um pacote com PUSI = 1:

┌───┬─────────────────┬──────────────────────────────┬─────────┐
│ N │ cauda da seção  │      nova seção começa       │ 0xFF... │
│   │ anterior (N B)  │                              │ padding │
└───┴─────────────────┴──────────────────────────────┴─────────┘
  ↑
  pointer_field
```

Ignorar esse campo é o erro mais comum em parsers caseiros. O sintoma é sutil:
a última seção de cada ciclo de repetição nunca é entregue, e como as tabelas
se repetem, parece só uma perda ocasional.

Um byte `0xFF` onde se esperaria um `table_id` marca o início do enchimento até
o fim do pacote.

### CRC-32/MPEG-2

- Polinômio `0x04C11DB7`
- Valor inicial `0xFFFFFFFF`
- Sem reflexão de entrada ou saída, sem XOR final
- Vetor de verificação: `"123456789"` → `0x0376E6E7`

Propriedade útil: calcular o CRC sobre a seção **inteira, incluindo os 4 bytes
do CRC**, resulta em zero se a seção estiver íntegra.

---

## Tabelas

### PAT — `table_id 0x00`, PID `0x0000`

Corpo: repetições de 4 bytes.

```
program_number   16 bits
reserved          3 bits
PID              13 bits
```

`program_number == 0` não é um programa: o PID associado aponta para a NIT.

### PMT — `table_id 0x02`, PID vindo da PAT

```
reserved                3 bits
PCR_PID                13 bits
reserved                4 bits
program_info_length    12 bits
  ... descritores do programa ...
para cada fluxo elementar:
  stream_type           8 bits
  reserved              3 bits
  elementary_PID       13 bits
  reserved              4 bits
  ES_info_length       12 bits
  ... descritores do fluxo ...
```

`stream_type` mais comuns:

| Valor | Fluxo |
|---|---|
| `0x02` | Vídeo MPEG-2 |
| `0x03` / `0x04` | Áudio MPEG-1 / MPEG-2 |
| `0x06` | PES com dados privados |
| `0x0B` | DSM-CC tipo B (carrossel de dados / Ginga) |
| `0x0F` | AAC ADTS |
| `0x11` | AAC LATM ← áudio típico do ISDB-Tb |
| `0x1B` | H.264 / AVC ← vídeo típico do ISDB-Tb |
| `0x24` | H.265 / HEVC |
| `0x86` | SCTE-35 (marcação de inserção comercial) |

### SDT — `table_id 0x42` (atual) / `0x46` (outro TS), PID `0x0011`

Nome do serviço e do provedor vêm no `service_descriptor` (tag `0x48`).

### EIT — PID `0x0012`

| `table_id` | Conteúdo |
|---|---|
| `0x4E` | Presente/seguinte, TS atual |
| `0x4F` | Presente/seguinte, outro TS |
| `0x50`–`0x5F` | Agenda, TS atual |
| `0x60`–`0x6F` | Agenda, outro TS |

Cada evento tem 12 bytes fixos mais os descritores. Título e sinopse vêm no
`short_event_descriptor` (tag `0x4D`).

### Codificação de data e hora

5 bytes: MJD de 16 bits seguido de HH:MM:SS em BCD.

O Anexo C da EN 300 468 traz uma fórmula de conversão de MJD para calendário
cheia de casos de borda. É desnecessária para chegar a tempo Unix — **MJD 40587
é 1970-01-01**, então:

```
unix = (MJD − 40587) × 86400 + HH×3600 + MM×60 + SS
```

Todos os bits em 1 (`FF FF FF FF FF`) significa "indefinido".

No SBTVD a referência é UTC, com o deslocamento local anunciado pelo
`local_time_offset_descriptor` (tag `0x58`).

---

## PIDs reservados

| PID | Uso |
|---|---|
| `0x0000` | PAT |
| `0x0001` | CAT |
| `0x0002` | TSDT |
| `0x0010` | NIT |
| `0x0011` | SDT / BAT |
| `0x0012` | EIT |
| `0x0013` | RST |
| `0x0014` | TDT / TOT |
| `0x0023` | SDTT (ARIB) |
| `0x0024` | BIT (ARIB) |
| `0x0029` | CDT (ARIB) |
| `0x1FC8` | PMT do one-seg (ARIB) |
| `0x1FFF` | Pacotes nulos (enchimento) |

O PID `0x1FC8` aparece nas capturas deste repositório — é a convenção ARIB para
a PMT da recepção parcial (one-seg).

---

## RTP para MPEG-TS

Cabeçalho de 12 bytes (RFC 3550):

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
│V=2│P│X│  CC   │M│     PT      │       sequence number         │
├───┴─┴─┴───────┴─┴─────────────┴───────────────────────────────┤
│                           timestamp                           │
├───────────────────────────────────────────────────────────────┤
│                             SSRC                              │
└───────────────────────────────────────────────────────────────┘
```

Para MPEG-2 TS:

- `PT = 33` (MP2T), alocado estaticamente pela RFC 3551
- `timestamp` no relógio de 90 kHz, que é exatamente a base do PCR
- Payload = N × 188 bytes, com **N = 7** por convenção
- `M` (marker) não tem significado definido; fica em 0

### A conta dos 1316 bytes

```
7 × 188 = 1316   payload TS
        +   12   cabeçalho RTP
        +    8   cabeçalho UDP
        +   20   cabeçalho IPv4
        ───────
          1356   bytes no fio  ✓ cabe na MTU de 1500
```

Com 8 pacotes seriam 1504 + 40 = 1544 bytes, forçando fragmentação IP. Em
contribuição de vídeo isso é ruim: perder um fragmento invalida o datagrama
inteiro.

Com IPv6 o cabeçalho é de 40 bytes, totalizando 1376 — ainda dentro da MTU.

### Wrap-around de sequência

A sequência é de 16 bits e envolve a cada 65536 datagramas. A 1356 bytes por
datagrama e 17 Mbit/s, isso acontece a cada ~55 segundos. Qualquer detector de
perda precisa tratar isso com aritmética de 16 bits **com sinal**, ou vai
relatar 65 mil perdas a cada minuto.
