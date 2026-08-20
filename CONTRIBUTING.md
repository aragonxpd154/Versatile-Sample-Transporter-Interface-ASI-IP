# Como contribuir

## Antes de mais nada

```sh
make test
```

Todo commit precisa passar a suíte. Ela roda em segundos e inclui um teste de
ponta a ponta que compara byte a byte o fluxo emitido e o recebido — se algo
quebrar no encapsulamento, você descobre imediatamente.

Antes de abrir um PR, rode também:

```sh
make asan
```

que compila com AddressSanitizer e UndefinedBehaviorSanitizer e roda tudo de
novo. Código que manipula buffers vindos da rede não tem margem para erro de
limites.

## Estilo de código

- C11, sem extensões do compilador.
- Indentação de 4 espaços, sem tabulação.
- Chaves na mesma linha para blocos de controle, em linha nova para funções.
- Nomes públicos com prefixo `vsti_`; nomes internos de arquivo, `static`.
- Limite de 88 colunas.
- Nada de alocação dinâmica no caminho crítico de `stream` e `receive`.

O build usa `-Wall -Wextra -Wpedantic` mais uma dúzia de avisos adicionais. O
job de CMake no CI compila com `-Werror`: qualquer aviso novo derruba o build.

## Comentários

A regra aqui é específica e importa: **comente o porquê, não o quê.**

```c
/* Ruim: repete o que o código já diz */
/* Incrementa a sequência */
s->sequence++;

/* Bom: explica a decisão */
/* Wrap-around de 16 bits e o comportamento correto e esperado. */
s->sequence = (uint16_t)(s->sequence + 1u);
```

Quando um trecho existe por causa de uma exigência de norma, cite a norma e a
seção. Quando existe por causa de um comportamento sutil de protocolo, explique
o sintoma que o descuido produziria — é isso que impede alguém de "simplificar"
o código seis meses depois e reintroduzir o bug.

Comentários em português; nomes de identificadores em inglês.

## Testes

Todo comportamento novo precisa de teste. Os arquivos em `tests/` seguem um
padrão simples: um `main()` por arquivo, macros de verificação em
`test_util.h`, sem framework externo.

Ao corrigir um bug, escreva primeiro o teste que falha. Vale especialmente para
parsing: quase todo defeito nessa área é um caso de borda que ninguém pensou em
exercitar.

Casos que merecem teste por padrão:

- valores mínimos e máximos de cada campo de bits;
- comprimentos declarados que ultrapassam o buffer;
- wrap-around de contadores;
- entrada vazia e entrada truncada no meio de um cabeçalho.

## Commits

Mensagem no imperativo, primeira linha com até 72 caracteres, explicando o
efeito e não o mecanismo:

```
Corrigir remontagem quando o pointer_field aponta para a cauda

Sem tratar o ponteiro, a ultima secao de cada ciclo de repeticao
nunca era entregue. Como as tabelas se repetem, o sintoma parecia
perda ocasional em vez de defeito sistematico.
```

## Reportando problemas

Inclua:

- a linha de comando exata;
- a saída com `-vv`;
- se possível, um trecho do fluxo que reproduza o problema (`vsti analyze -i
  arquivo.ts --packets 5000` costuma ser suficiente para diagnosticar).

Se o problema for de rede, informe também se é unicast ou multicast, e o
resultado de `vsti receive` rodando na mesma máquina do emissor via
`127.0.0.1` — isso separa defeito da ferramenta de problema de caminho.
