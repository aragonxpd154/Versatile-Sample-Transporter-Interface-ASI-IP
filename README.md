# VSTI — Versatile Sample Transporter Interface

**MPEG-2 Transport Stream over IP.** A dependency-free C11 tool that reads a
broadcast transport stream, encapsulates it into RTP/UDP following RFC 2250,
and paces it onto the network at the stream's own clock rate — plus the
receiving half and a PSI/SI analyzer to prove the result is correct.

Built and validated against real ISDB-Tb (SBTVD) one-seg captures included in
this repository.

```
                 ┌──────────┐   RTP/UDP    ┌───────────┐
  file.ts ──────▶│  stream  │═════════════▶│  receive  │──────▶ file.ts
                 └──────────┘  7×188 bytes └───────────┘
                       │        per datagram      │
                       ▼                          ▼
                  PCR pacing              loss / reorder stats
                       │
                 ┌──────────┐
                 │ analyze  │──▶ PAT · PMT · SDT · EIT · PIDs · bitrate
                 └──────────┘
```

---

## Quick start

```sh
make
make test
```

No dependencies beyond a C11 compiler and a POSIX libc. There is a
`CMakeLists.txt` too, if you prefer:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build --output-on-failure
```

### Send a stream to a multicast group

```sh
./build/vsti stream -i one-seg/TVGAZETA1SEG_20211027_183507.mpg \
                    -d 239.1.1.1:1234 --iface eth0
```

### Record it back

```sh
./build/vsti receive -l 239.1.1.1:1234 -o recording.ts --iface eth0
```

### Inspect what's inside a stream

```sh
./build/vsti analyze -i one-seg/TVGAZETA1SEG_20211027_183507.mpg
```

```
Transporte
  pacotes TS ............ 14070
  erros de continuidade . 5
  transport_stream_id ... 1 (0x0001)
  taxa media (PCR) ...... 336.03 kbit/s
  duracao (PCR) ......... 00:01:02.811

Programas (1)
  Programa 1 (0x0001)
    PMT PID ......... 0x1FC8
    PCR PID ......... 0x1002
    fluxos elementares:
      PID 0x1001  H.264 / AVC                     10326 pkts (73.39%)
      PID 0x1003  AAC LATM                         2071 pkts (14.72%)
      PID 0x100C  PES Private Data                   28 pkts ( 0.20%)
```

Add `--json` for machine-readable output.

---

## What it does

| Command | Purpose |
|---|---|
| `vsti stream` | Read a TS file (or stdin), encapsulate into RTP/UDP or raw UDP, and transmit — unicast or multicast, IPv4 or IPv6, paced by PCR, CBR, or unthrottled. |
| `vsti receive` | Join a group or bind a port, auto-detect RTP vs. raw UDP, de-encapsulate to a TS file, and report packet loss, reordering, and duplicates. |
| `vsti analyze` | Single-pass inspection: PID census, continuity errors, PCR-derived bitrate, and decoded PAT / PMT / SDT / EIT tables including EPG text. |

### Implementation highlights

- **RFC 2250 encapsulation** — 7 TS packets per datagram (1316 B payload), so
  the resulting 1356-byte IP datagram never fragments on standard Ethernet.
  Fragmentation matters in video contribution: losing one fragment invalidates
  the whole datagram.
- **PCR-locked pacing** — the transmit clock is derived from the stream's own
  Program Clock Reference and tracked with a fixed-point accumulator against
  absolute `CLOCK_MONOTONIC` deadlines, so a multi-hour transmission does not
  drift. RTP timestamps are locked to the PCR and interpolated between them.
- **Robust framing** — auto-detects 188 / 192 / 204 / 208-byte packet layouts,
  finds alignment in an arbitrary byte stream, and recovers from mid-stream
  sync loss instead of aborting.
- **Correct PSI/SI reassembly** — handles the `pointer_field`, sections split
  across packets, and multiple sections in one packet; every section is CRC-32
  verified before it is trusted.
- **ISDB-Tb text decoding** — ARIB STD-B24 / ABNT NBR 15603 strings are
  stripped of control and escape sequences and converted from ISO-8859-15 to
  UTF-8, so service names and EPG titles come out readable.

---

## Verification

The test suite runs on every push:

- **Unit tests** for CRC-32 (against the canonical `"123456789"` vector), TS
  header and PCR bit-packing at boundary values, alignment detection, the four
  continuity-counter cases the standard distinguishes, section reassembly
  (including the `pointer_field` tail case that silently breaks naive
  implementations), and the RTP sequence wrap-around arithmetic.
- **An end-to-end integration test** that transmits a real 2.6 MB one-seg
  capture over loopback and compares the received file to the source **byte for
  byte** — in both RTP and raw UDP modes.
- **AddressSanitizer + UndefinedBehaviorSanitizer** builds, with leak detection.
- **cppcheck** static analysis, failing the build on real findings.

An independent cross-check worth noting: the capture's own recorder metadata
(`one-seg/data/*.xml`) records `<ES-CC-ERRORS>5</ES-CC-ERRORS>`, and this
analyzer independently counts exactly 5 continuity errors in that file.

---

## Repository layout

```
src/            C sources — core library and the three subcommands
include/vsti/   Public headers
tests/          Unit tests and the end-to-end integration script
docs/           Technical documentation (Portuguese)
one-seg/        Real ISDB-Tb one-seg captures used as test material
json-analysis/  Sample EPG extraction output
reference/      Vendor SI-table script definitions (reference only)
datasheets/     FPGA and component datasheets (Cyclone III, N25Q, MAX3160)
system/         Firmware and configuration dumps from the lab equipment
boot-teste/     Boot logs and ASI/TMCC configuration from bench testing
```

Documentation in Portuguese:

- [`docs/ARQUITETURA.md`](docs/ARQUITETURA.md) — how the code is organized and why
- [`docs/USO.md`](docs/USO.md) — practical operating guide, with real scenarios
- [`docs/PROTOCOLOS.md`](docs/PROTOCOLOS.md) — MPEG-TS, PSI/SI, ISDB-Tb, RFC 2250
- [`docs/LICENCIAMENTO.md`](docs/LICENCIAMENTO.md) — third-party material in this repo
- [`docs/HISTORICO.md`](docs/HISTORICO.md) — the project's original scope and how it evolved

---

## Scope, honestly

The original project description was broader than what exists here: it
described MPEG-2/H.264 **decompression** and an **FPGA** implementation in
Verilog/VHDL alongside the IP encapsulation.

What this repository actually contains, working and tested, is the
**transport-layer half**: TS demultiplexing, PSI/SI parsing, IP encapsulation,
and network transmission — the part that runs on a general-purpose CPU. There
is no HDL here, and no video decoding: the tool moves elementary streams around
without transcoding them, which is what a transport interface is supposed to do.

The FPGA material in `datasheets/` and `system/` documents the hardware the
project was aimed at, and is kept as reference for a future gateware stage.
See [`docs/HISTORICO.md`](docs/HISTORICO.md) for the full story.

---

## License

MIT — see [LICENSE](LICENSE).

The tool's own source (`src/`, `include/`, `tests/`) is original work under
that license. Material under `reference/`, `system/`, `boot-teste/`, and
`datasheets/` comes from third parties and is **not** covered by it — read
[`docs/LICENCIAMENTO.md`](docs/LICENCIAMENTO.md) before redistributing.
