# Makefile - VSTI (Versatile Sample Transporter Interface)
#
# Build sem dependencias externas. Requer apenas um compilador C11 e libc
# POSIX. Para quem prefere CMake, ha um CMakeLists.txt equivalente.
#
# Alvos principais:
#   make            compila o binario em build/vsti
#   make test       compila e roda a suite de testes
#   make asan       build instrumentado com AddressSanitizer + UBSan
#   make install    instala em $(PREFIX)/bin (padrao: /usr/local)
#   make clean      remove os artefatos

PREFIX  ?= /usr/local
CC      ?= cc
BUILD   ?= build

CSTD    := -std=c11
WARN    := -Wall -Wextra -Wpedantic -Wshadow -Wcast-align \
           -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
           -Wwrite-strings -Wno-unused-parameter
OPT     ?= -O2
CFLAGS  ?= $(CSTD) $(WARN) $(OPT) -Iinclude -Isrc
LDFLAGS ?=

# _POSIX_C_SOURCE e definido por arquivo, mas garantimos o minimo aqui para
# compiladores que nao herdam a definicao em unidades sem ela.
CFLAGS  += -D_DEFAULT_SOURCE

LIB_SRCS := \
	src/ts.c \
	src/psi.c \
	src/desc.c \
	src/rtp.c \
	src/net.c \
	src/pacing.c \
	src/reader.c \
	src/log.c

CLI_SRCS := \
	src/cli.c \
	src/cmd_stream.c \
	src/cmd_receive.c \
	src/cmd_analyze.c \
	src/main.c

LIB_OBJS := $(LIB_SRCS:%.c=$(BUILD)/%.o)
CLI_OBJS := $(CLI_SRCS:%.c=$(BUILD)/%.o)
ALL_OBJS := $(LIB_OBJS) $(CLI_OBJS)

BIN      := $(BUILD)/vsti

TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/%.c=$(BUILD)/tests/%)

.PHONY: all clean test asan install uninstall format help

all: $(BIN)

$(BIN): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  ==> $@"

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# ---- Testes ----------------------------------------------------------
#
# Cada test_*.c vira um executavel proprio, ligado contra os objetos da
# biblioteca. Testes independentes evitam que uma falha de segmentacao em um
# caso derrube a suite inteira.

$(BUILD)/tests/%: tests/%.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_OBJS) $(LDFLAGS)

test: $(BIN) $(TEST_BINS)
	@echo "== testes unitarios =="
	@fail=0; \
	for t in $(TEST_BINS); do \
		printf "  %-28s " "$$(basename $$t)"; \
		if $$t > /tmp/vsti_test.log 2>&1; then \
			echo "OK"; \
		else \
			echo "FALHOU"; cat /tmp/vsti_test.log; fail=1; \
		fi; \
	done; \
	echo "== teste de integracao =="; \
	sh tests/integration.sh $(BIN) || fail=1; \
	exit $$fail

# ---- Build com sanitizers -------------------------------------------

asan:
	$(MAKE) clean
	$(MAKE) OPT="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
	        LDFLAGS="-fsanitize=address,undefined" test

# ---- Instalacao ------------------------------------------------------

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/vsti
	@echo "  ==> $(DESTDIR)$(PREFIX)/bin/vsti"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/vsti

clean:
	rm -rf $(BUILD)

help:
	@echo "Alvos: all test asan install uninstall clean"

-include $(ALL_OBJS:.o=.d)
