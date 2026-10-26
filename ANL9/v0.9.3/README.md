# ANL9 0.9.3

ANL9 0.9.3 is a compatible maintenance release based on the 0.9.2 sources. It retains the existing ANL9 wire format (magic `ANL9`, version field `0x0902`) so 0.9.2 archives remain readable. The on-disk version field was deliberately not changed because the record layout did not change.

## Architecture and mode choice

Files are split into 64 KiB blocks. The encoder keeps all 0.9.2 methods: RAW blocks, references to identical earlier blocks, and MiniBlocks with literal data, backward references, and repeated-byte operations. CRC32 protects the archive header, each record payload, and the reconstructed file. Blocks are emitted and restored in input order.

For each block, the encoder constructs the MiniBlock candidate and compares its complete incremental archive cost (record header plus payload, with the shared archive header included in both alternatives) against RAW's complete incremental cost. Identical earlier blocks use the smaller block-reference record. Thus headers common to both alternatives are accounted for, and no MiniBlock is chosen when its complete record is larger. This decision preserves the 0.9.2 output policy because both candidate records have the same fixed header size. It does not remove the fixed 40-byte archive header or the per-block record header; tiny and random files can therefore produce archives larger than the input. A separate external stored-file/container mode is outside the current ANL9 format.

## Safety changes

The 0.9.3 decoder checks the declared block count against the output size, validates record flags, lengths and payload CRCs, bounds MiniBlock operations and references, rejects trailing bytes, and verifies final size and CRC before reporting success. This remains a single-file, in-memory decoder; it allocates the declared output size after checking the header and representability. The checker validates header and record structure and payload CRCs; use the decoder as well to verify reconstructed content and final CRC.

## Build (MSYS2 UCRT64)

Run from the UNA project root:

```sh
gcc -std=c17 -O2 -Wall -Wextra -pedantic -o ANL9/v0.9.3/anlc93.exe ANL9/v0.9.3/anlc93.c
gcc -std=c17 -O2 -Wall -Wextra -pedantic -o ANL9/v0.9.3/anlr93.exe ANL9/v0.9.3/anlr93.c
gcc -std=c17 -O2 -Wall -Wextra -pedantic -o ANL9/v0.9.3/anlcheck93.exe ANL9/v0.9.3/anlcheck93.c
```

Usage:

```sh
ANL9/v0.9.3/anlc93.exe input.bin output.anl9
ANL9/v0.9.3/anlcheck93.exe output.anl9
ANL9/v0.9.3/anlr93.exe output.anl9 restored.bin
cmp input.bin restored.bin
```

## Results

All listed files were compiled with GCC C17 and tested by creating an archive, running `anlcheck93`, unpacking with `anlr93`, and comparing bytes. Reduction is `(input - archive) / input`; negative values indicate overhead.

| Input | Input bytes | CRC32 | 0.9.2 archive | 0.9.3 archive | Change | Reduction | Check / unpack / byte match |
|---|---:|---:|---:|---:|---:|---:|---|
| `reference_big.bin` | 2,097,152 | `04CABAD9` | 86,435 | 86,435 | 0 | 95.88% | PASS / PASS / yes |
| `million.txt` | 1,000,002 | `24DAA839` | 418 | 418 | 0 | 99.96% | PASS / PASS / yes |
| `random.bin` | 102,400 | `013F0F20` | 102,472 | 102,472 | 0 | -0.07% | PASS / PASS / yes |
| `reference.txt` | 36 | `F10116EA` | 92 | 92 | 0 | -155.56% | PASS / PASS / yes |
| `small.bin` | 30 | `48B1FC81` | 86 | 86 | 0 | -186.67% | PASS / PASS / yes |
| `hello.txt` | 133 | `47B23A9D` | 189 | 189 | 0 | -42.11% | PASS / PASS / yes |
| `test.txt` | 38 | `31002EBB` | 94 | 94 | 0 | -147.37% | PASS / PASS / yes |

Additional cases: empty input (40-byte archive), 1 byte (57), 512 random bytes (568), 40,000 repeated bytes (80), and 2 MiB random bytes (2,097,704) all passed check, unpack and byte comparison. A flipped payload byte was rejected by the checker; a truncated archive was rejected by the decoder. The existing generated 2 MiB reference file also exercises repeated blocks and MiniBlock references.

| Capability / measurement | Result |
|---|---|
| MiniBlocks, refs, repeat operations | Preserved; reference archive size unchanged |
| 0.9.2 vs 0.9.3 archive sizes on the same seven files | Identical |
| Speed and peak memory comparison | Not measured |
| Inputs absent from the project | None of the seven required inputs were absent |

## Known limitations and 0.9.4 direction

The 0.9.3 cost comparison makes the shared archive header explicit, but it cannot save its fixed cost under the existing format; no compressed method is beneficial for most tiny files. There is no output streaming, and the decoder currently allocates the declared output buffer. `anlcheck93` performs structural and record-payload validation; it does not independently reconstruct the stream to check final CRC. For 0.9.4, consider a versioned multi-file/container layer with a stored-file option, and a streaming shared decode/validation path with allocation limits, while retaining the existing ANL9 reader for compatibility.
