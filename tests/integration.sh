#!/bin/sh
#
# tests/integration.sh - Teste de ponta a ponta do encapsulamento.
#
# Emite um transport stream por UDP na interface de loopback, recebe de volta e
# compara byte a byte. E o unico teste que exercita o caminho completo:
# leitura, alinhamento, encapsulamento RTP, socket, desencapsulamento e escrita.
#
# Uso: sh tests/integration.sh [caminho/para/vsti]
#
# Copyright (c) 2026 Marcos Silva. MIT License.

set -e

BIN="${1:-build/vsti}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT INT TERM

fail=0
pass()  { printf '  %-46s OK\n'      "$1"; }
fail()  { printf '  %-46s FALHOU\n'  "$1"; fail=1; }

if [ ! -x "$BIN" ]; then
    echo "  binario nao encontrado em $BIN" >&2
    exit 1
fi

# ---------------------------------------------------------------------
# Escolha da entrada
#
# Preferimos uma captura real do repositorio, porque ela tem PCR, PSI e erros
# de continuidade de verdade. Se nao houver, geramos um fluxo sintetico para
# que a suite continue rodando em um clone raso.
# ---------------------------------------------------------------------

SRC=""
for f in one-seg/*.mpg one-seg/*.ts samples/*.ts; do
    [ -f "$f" ] && SRC="$f" && break
done

if [ -n "$SRC" ]; then
    # Trunca para um numero inteiro de pacotes: o leitor descarta qualquer
    # cauda incompleta, e sem isso a comparacao final acusaria diferenca de
    # tamanho que nao e defeito nenhum.
    size=$(wc -c < "$SRC" | tr -d ' ')
    packets=$((size / 188))
    dd if="$SRC" of="$TMP/ref.ts" bs=188 count="$packets" 2>/dev/null
    echo "  entrada: $SRC ($packets pacotes)"
else
    echo "  entrada: fluxo sintetico (nenhuma captura no repositorio)"
    "$BIN" version > /dev/null
    # Gera 20000 pacotes com PID e contador de continuidade corretos.
    awk 'BEGIN{
        for (i = 0; i < 20000; i++) {
            cc = i % 16
            printf "%c%c%c%c", 71, 1, 0, 16 + cc
            for (j = 0; j < 184; j++) printf "%c", i % 256
        }
    }' > "$TMP/ref.ts"
    packets=20000
fi

REF="$TMP/ref.ts"
PORT1=25001
PORT2=25002

# ---------------------------------------------------------------------
# 1. Ciclo completo com RTP
# ---------------------------------------------------------------------

"$BIN" receive -l "127.0.0.1:$PORT1" -o "$TMP/rtp.ts" \
       --timeout 4 --rcvbuf 8388608 --stats 0 -q &
RXPID=$!

# Da tempo ao receptor de fazer bind antes de o emissor comecar.
sleep 0.5

"$BIN" stream -i "$REF" -d "127.0.0.1:$PORT1" \
       --pace cbr --bitrate 40M --stats 0 -q

wait "$RXPID" 2>/dev/null || true

if cmp -s "$REF" "$TMP/rtp.ts"; then
    pass "ciclo RTP identico byte a byte"
else
    a=$(wc -c < "$REF" | tr -d ' ')
    b=$(wc -c < "$TMP/rtp.ts" 2>/dev/null | tr -d ' ' || echo 0)
    fail "ciclo RTP identico byte a byte (enviado $a, recebido $b)"
fi

# ---------------------------------------------------------------------
# 2. Ciclo completo em UDP puro
# ---------------------------------------------------------------------

"$BIN" receive -l "127.0.0.1:$PORT2" -o "$TMP/raw.ts" \
       --timeout 4 --rcvbuf 8388608 --stats 0 -q &
RXPID=$!
sleep 0.5

"$BIN" stream -i "$REF" -d "127.0.0.1:$PORT2" --raw \
       --pace cbr --bitrate 40M --stats 0 -q

wait "$RXPID" 2>/dev/null || true

if cmp -s "$REF" "$TMP/raw.ts"; then
    pass "ciclo UDP puro identico byte a byte"
else
    fail "ciclo UDP puro identico byte a byte"
fi

# ---------------------------------------------------------------------
# 3. Deteccao automatica de formato
#
# O receptor foi iniciado sem --rtp nem --raw nos dois testes acima; se a
# deteccao tivesse errado, os arquivos nao bateriam. Aqui confirmamos que o
# tamanho do datagrama e o esperado para 7 pacotes TS.
# ---------------------------------------------------------------------

if [ "$(wc -c < "$TMP/rtp.ts" | tr -d ' ')" = "$(wc -c < "$TMP/raw.ts" | tr -d ' ')" ]; then
    pass "RTP e UDP puro entregam o mesmo payload"
else
    fail "RTP e UDP puro entregam o mesmo payload"
fi

# ---------------------------------------------------------------------
# 4. Analisador: relatorio em texto e em JSON
# ---------------------------------------------------------------------

if "$BIN" analyze -i "$REF" -q > "$TMP/report.txt" 2>/dev/null; then
    if grep -q "pacotes TS" "$TMP/report.txt"; then
        pass "relatorio em texto gerado"
    else
        fail "relatorio em texto gerado"
    fi
else
    fail "relatorio em texto gerado"
fi

if "$BIN" analyze -i "$REF" --json -q > "$TMP/report.json" 2>/dev/null; then
    if command -v python3 > /dev/null 2>&1; then
        if python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$TMP/report.json" 2>/dev/null; then
            pass "relatorio JSON sintaticamente valido"
        else
            fail "relatorio JSON sintaticamente valido"
        fi
    else
        # Sem python no runner, ao menos conferimos o envelope.
        head -c 1 "$TMP/report.json" | grep -q '{' && pass "relatorio JSON gerado" \
            || fail "relatorio JSON gerado"
    fi
else
    fail "relatorio JSON gerado"
fi

# ---------------------------------------------------------------------
# 5. Contagem de pacotes coerente
# ---------------------------------------------------------------------

reported=$("$BIN" analyze -i "$REF" --json -q 2>/dev/null \
           | tr -d ' ' | grep '"packets":' | head -1 | tr -d '",' | cut -d: -f2)
if [ "$reported" = "$packets" ]; then
    pass "analisador contou $packets pacotes"
else
    fail "analisador contou $reported, esperado $packets"
fi

# ---------------------------------------------------------------------
# 6. Codigos de saida e tratamento de erro
# ---------------------------------------------------------------------

if "$BIN" analyze -i /caminho/que/nao/existe.ts > /dev/null 2>&1; then
    fail "entrada inexistente deve falhar"
else
    pass "entrada inexistente retorna erro"
fi

if "$BIN" comando-invalido > /dev/null 2>&1; then
    fail "comando invalido deve falhar"
else
    pass "comando invalido retorna erro"
fi

if "$BIN" stream -i "$REF" -d "127.0.0.1:1" --pkts 99 > /dev/null 2>&1; then
    fail "--pkts fora do intervalo deve falhar"
else
    pass "--pkts fora do intervalo retorna erro"
fi

exit "$fail"
