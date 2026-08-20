# Guia de uso

## Compilação

```sh
make            # binário em build/vsti
make test       # compila e roda toda a suíte
make asan       # build com AddressSanitizer + UBSan
sudo make install
```

Requisitos: um compilador C11 e libc POSIX. Nada além disso.

Alternativa com CMake:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## `vsti stream` — arquivo para a rede

```sh
vsti stream -i ARQUIVO -d HOST:PORTA [opções]
```

### Cenários comuns

**Enviar para um grupo multicast em uma interface específica**

```sh
vsti stream -i captura.ts -d 239.1.1.1:1234 --iface eth0
```

**Enviar em unicast para um decodificador na bancada**

```sh
vsti stream -i captura.ts -d 192.168.0.50:5000
```

**Loop infinito para teste de longa duração**

```sh
vsti stream -i captura.ts -d 239.1.1.1:1234 --loop
```

**Taxa fixa em vez de PCR** — útil quando o arquivo não tem PCR confiável:

```sh
vsti stream -i captura.ts -d 239.1.1.1:1234 --pace cbr --bitrate 17.27M
```

**UDP puro, sem RTP** — alguns equipamentos de broadcast esperam MPEG-TS
diretamente sobre UDP:

```sh
vsti stream -i captura.ts -d 239.1.1.1:1234 --raw
```

**Marcar o tráfego para QoS** — AF41 (DSCP 34) é a marcação usual para vídeo
de contribuição:

```sh
vsti stream -i captura.ts -d 239.1.1.1:1234 --dscp 34 --ttl 16
```

**A partir de um pipe**

```sh
ffmpeg -i entrada.mp4 -c copy -f mpegts - | vsti stream -i - -d 239.1.1.1:1234
```

### Modos de ritmo (`--pace`)

| Modo | Quando usar |
|---|---|
| `pcr` (padrão) | Arquivo com PCR válido. A taxa é medida do próprio fluxo e ajustada continuamente. É o que reproduz fielmente a emissão original. |
| `cbr` | Taxa fixa informada em `--bitrate`. Use quando o PCR está ausente ou quebrado, ou quando você quer forçar uma taxa diferente da original. |
| `none` | Sem controle: envia o mais rápido possível. Serve para teste de vazão e para o teste de integração. **Não use para alimentar decodificador.** |

No modo `pcr`, a ferramenta faz uma sondagem rápida do início do arquivo antes
de começar a emitir. Isso importa: os primeiros segundos são justamente quando
o buffer do receptor está enchendo, e sair emitindo com a taxa errada nesse
momento é o que provoca o travamento inicial clássico.

### Opções completas

```
  -i, --input ARQ       arquivo de entrada ('-' para stdin)
  -d, --dest HOST:PORTA destino unicast ou grupo multicast
      --raw             UDP puro, sem cabeçalho RTP
      --pkts N          pacotes TS por datagrama, 1 a 7 (padrão 7)
      --ssrc N          SSRC do RTP (padrão: aleatório)
      --seq N           sequência RTP inicial (padrão: aleatória)
      --pace MODO       pcr | cbr | none
      --bitrate TAXA    aceita 17274150, 17.27M, 2500k, 1G
      --iface NOME      interface de saída (nome ou IP local)
      --ttl N           TTL / hop limit (padrão 8)
      --dscp N          valor DSCP 0-63
      --sndbuf BYTES    buffer de envio do socket
      --loop            reinicia o arquivo no fim
      --duration SEG    encerra após N segundos
      --stats SEG       intervalo do relatório (0 desliga)
  -v, --verbose         mais log; -q para só erros
```

---

## `vsti receive` — rede para arquivo

```sh
vsti receive -l [HOST:]PORTA -o ARQUIVO [opções]
```

**Gravar um grupo multicast**

```sh
vsti receive -l 239.1.1.1:1234 -o gravacao.ts --iface eth0
```

**Escutar uma porta em todas as interfaces**

```sh
vsti receive -l 5000 -o gravacao.ts
```

**Encaminhar para um player**

```sh
vsti receive -l 239.1.1.1:1234 -o - | ffplay -
```

**Gravar por tempo determinado e sair**

```sh
vsti receive -l 239.1.1.1:1234 -o gravacao.ts --duration 300
```

**Encerrar sozinho se o fluxo sumir**

```sh
vsti receive -l 239.1.1.1:1234 -o gravacao.ts --timeout 10
```

O formato (RTP ou UDP puro) é detectado automaticamente no primeiro datagrama.
A distinção é confiável porque TS puro é sempre múltiplo exato de 188 bytes, e
os 12 bytes de cabeçalho RTP quebram essa divisibilidade. Use `--rtp` ou
`--raw` se quiser forçar.

Ao final, o resumo traz perdas, reordenações e duplicatas de RTP, além de erros
de continuidade — é o suficiente para separar problema de rede de problema de
origem.

### Buffer de recepção

Em fluxo de alta taxa, o buffer padrão do kernel pode não dar conta e o socket
descarta pacotes silenciosamente. Aumente:

```sh
vsti receive -l 239.1.1.1:1234 -o out.ts --rcvbuf 16777216
```

Se o kernel recusar o valor, ajuste o limite do sistema:

```sh
sudo sysctl -w net.core.rmem_max=16777216
```

---

## `vsti analyze` — inspeção

```sh
vsti analyze -i ARQUIVO [--json] [--packets N] [--seconds N]
```

**Relatório legível**

```sh
vsti analyze -i one-seg/TVGAZETA1SEG_20211027_183507.mpg
```

**JSON para processar depois**

```sh
vsti analyze -i captura.ts --json > relatorio.json
jq '.programs[].streams[] | {pid, type_name}' relatorio.json
```

**Só os primeiros segundos** — útil em arquivo grande:

```sh
vsti analyze -i captura_de_2h.ts --seconds 30
```

### O que o relatório mostra

- **Transporte**: total de pacotes, nulos, inválidos, erros de continuidade,
  `transport_error_indicator`, seções PSI/SI válidas, `transport_stream_id`,
  taxa média/mínima/máxima medida pelo PCR e duração.
- **Programas**: para cada entrada da PAT, o PID da PMT, o PCR PID, e a lista
  de fluxos elementares com tipo, idioma e participação percentual no
  transporte.
- **Serviços**: nome, provedor, tipo e estado, decodificados da SDT.
- **Eventos**: grade de programação da EIT com horário UTC, duração, título e
  sinopse.
- **PIDs**: censo completo com contagem, percentual, erros de CC, TEI e
  quantidade de PCRs.

---

## Ciclo completo em uma máquina só

Útil para validar a instalação:

```sh
vsti receive -l 5000 -o volta.ts --timeout 3 &
sleep 0.5
vsti stream -i original.ts -d 127.0.0.1:5000 --pace cbr --bitrate 40M
wait
cmp original.ts volta.ts && echo "idêntico"
```

Note que `original.ts` precisa ter um número inteiro de pacotes de 188 bytes
para a comparação bater — o leitor descarta qualquer cauda incompleta. Para
truncar:

```sh
dd if=entrada.ts of=original.ts bs=188 count=$(( $(wc -c < entrada.ts) / 188 ))
```

---

## Diagnóstico

**"Nenhum PCR utilizável na sondagem inicial"**
O arquivo não tem PCR, ou os PCRs estão espaçados de forma anômala. Use
`--pace cbr --bitrate` com a taxa correta.

**Muitos erros de continuidade no receptor, nenhum no emissor**
Perda na rede. Confira o resumo de RTP: se `RTP perdidos` for alto, o problema
é o caminho. Aumente `--rcvbuf` e verifique se algum switch no meio está
descartando por falta de IGMP snooping.

**`sendto: Network is unreachable` em multicast**
Falta rota para a faixa multicast. Ou informe `--iface`, ou adicione a rota:

```sh
sudo ip route add 239.0.0.0/8 dev eth0
```

**O receptor não recebe nada em multicast, mas o emissor não acusa erro**
Multicast não gera erro quando ninguém escuta. Confira, nessa ordem: se emissor
e receptor usam a mesma `--iface`; se o TTL é suficiente para o número de
roteadores no caminho; e se o switch tem IGMP snooping ativo sem um querier na
rede — combinação que costuma bloquear o tráfego.

**"falhas de envio N de M datagramas"**
Em unicast, o socket é conectado, então o kernel converte o ICMP
port-unreachable em erro de `send()`. Na prática isso quase sempre significa
que **não há ninguém escutando no destino**. Se houver receptor e o aviso
persistir, o motivo costuma ser buffer de envio pequeno para a taxa pedida —
aumente com `--sndbuf`. Em multicast esse aviso não aparece, porque não existe
retorno de quem (não) está ouvindo.

**Erros de continuidade ao usar `--loop`**
Esperado, e não é defeito. Ao voltar para o início do arquivo, o contador de
continuidade de cada PID salta do valor final para o inicial. Um decodificador
real acusa isso a cada volta. Se o objetivo for teste de longa duração sem
esses artefatos, prepare um arquivo longo o suficiente em vez de repetir um
curto.

**Recebendo o próprio tráfego ao testar na mesma máquina**
Por padrão o loopback multicast está desligado. É intencional, para não
duplicar o fluxo em produção. Para teste local, use unicast em `127.0.0.1`.
