# OCB BIOS Import Handoff

Date: 2026-04-23
Workspace: `D:\14900K\ocb-studio-native`

## Goal

Make `ocb_studio` export `.ocb` files that MSI BIOS will import as valid `MsOcFile.ocb`.

This handoff captures what is already proven, what was changed in code, what user tests were run, and what still looks unresolved.

## Important reference files

- Original profile:
  - `D:\14900K\original\MsOcFile.ocb`
  - SHA-256: `E5E2CE9DCC263C3CF5B5253664124FD6CF82DE306A1FB70DBD1902FDFC50D0E8`
- Known BIOS-accepted profile:
  - `D:\14900K\try_02_conservative_sum_comp\MsOcFile.ocb`
  - SHA-256: `5B26264366D120D714AB03346DC6D022DD7BF098720F6AC39078BB8564744248`
- Current file on flash drive when this note was written:
  - `E:\MsOcFile.ocb`
  - SHA-256: `DAACAB59FCEBBF28DBE41D05CCA6423D58DFCE4DDF214BA05DFC1D3E7D0AAE67`
- Reproduced app-generated `CPU Current Limit = 502` file:
  - `D:\14900K\_repro_502.ocb`
  - SHA-256: `0CF0FA469E735450BD3B99F204D432A4B4107EAAA98A201C63A45EEA2EAC2375`

## Core proven findings

### 1. The old "legacy checksum compensation" idea was not the real format rule

The compensation area at `0x2D84..0x2D93` is not what makes BIOS accept a file.

- Original file has:
  - `00 C3 00 00 00 00 00 00 00 00 00 00 00 00 00 00`
- `try_02` has the same area.
- BIOS-saved `502` file also had the same area.

The old broken exports were changing this area incorrectly, but fixing only this area does not make modified files importable.

### 2. The `CPU Current Limit (A)` field mapping is correct

There was earlier suspicion that the field mapping might be wrong. That turned out to be false.

`core/src/BuiltinFields.cpp` defines:

- `CPU Current Limit (A)` = VarStore `Setup`, offset `0xF2A`, size `16`

`Setup` base is `0x001F`, so actual file offset is:

- `0x001F + 0x0F2A = 0x0F49`

This was confirmed by a BIOS-saved file where the only user-facing change was `CPU Current Limit = 502`:

- `0x0F49..0x0F4A = F6 01` which is `0x01F6 = 502`

### 3. BIOS metadata / service bytes are now mostly understood

The following service areas were reverse engineered from:

- original profile
- known BIOS-accepted `try_02`
- BIOS-saved profile with `CPU Current Limit = 502`

#### 3a. ASCII timestamp string

At `0x2E6B..0x2E78`, BIOS stores a 14-byte ASCII timestamp:

- format: `YYYYMMDDHHMMSS`

Examples:

- original: `20260423195302`
- `try_02`: `20260422195141`
- BIOS-saved 502: `20260423204311`

#### 3b. BCD-style timestamp block

At `0x2D3C..0x2D43`, BIOS stores a BCD-like save timestamp:

- `0x2D3C` = seconds in BCD
- `0x2D3D` = `0x00`
- `0x2D3E` = minutes in BCD
- `0x2D3F` = `0x00`
- `0x2D40` = hour in BCD
- `0x2D41` = `0x00`
- `0x2D42` = weekday from `std::tm::tm_wday`
- `0x2D43` = day-of-month in BCD

Example for BIOS-saved `2026-04-23 20:43:11`:

- `2D3C = 0x11`
- `2D3E = 0x43`
- `2D40 = 0x20`
- `2D42 = 0x04`
- `2D43 = 0x23`

#### 3c. CRC-like service bytes

Three single-byte service values are now fully reproduced:

- `0x0011` = CRC-8 over bytes `0x2D3C..0x2E89` with:
  - polynomial `0xC1`
  - init `0x00`
  - xorout `0x00`
  - no input reflection
  - no output reflection

- `0x2D7F` = CRC-8 over bytes `0x2E3C..0x2E89` with:
  - polynomial `0x27`
  - init `0xFF`
  - xorout `0xFF`
  - no input reflection
  - output reflection = true

- `0x2E8A` = CRC-8 over bytes `0x2D3C..0x2D7E` with:
  - polynomial `0x7F`
  - init `0xFF`
  - xorout `0x00`
  - input reflection = true
  - output reflection = true

These formulas were validated by reconstructing the BIOS-saved `502` file exactly.

#### 3d. Two still-partly heuristic metadata bytes

- `0x0012`
  - currently reproduced as:
  - `((tm_mon) << 4) | ((year + 1900) % 10)`
  - note: `tm_mon` here is zero-based from `std::tm`
  - this reproduces `0x36` for April 2026

- `0x2E26`
  - currently reproduced as:
  - `0x00` if `(bytes[0x0F49] & 0x80) != 0`
  - else `0x80`
  - this exactly reproduces the BIOS-saved `502` file
  - still treated as heuristic because it is not yet proven for all fields

### 4. The current app can exactly reproduce the BIOS-saved `CPU Current Limit = 502` case

This is the strongest confirmed point.

Using:

- input: `D:\14900K\original\MsOcFile.ocb`
- field write: `CPU Current Limit (A) = 502`
- env override: `OCB_EXPORT_TIMESTAMP=20260423204311`

the current built app generated:

- `D:\14900K\_repro_502.ocb`
- SHA-256: `0CF0FA469E735450BD3B99F204D432A4B4107EAAA98A201C63A45EEA2EAC2375`

The exact diffs from original were:

- `0011: D3 -> C7`
- `0F49: 33 -> F6`
- `2D3C: 02 -> 11`
- `2D3E: 53 -> 43`
- `2D40: 19 -> 20`
- `2D7F: 58 -> 28`
- `2E26: 80 -> 00`
- `2E73: 31 -> 32`
- `2E74: 39 -> 30`
- `2E75: 35 -> 34`
- `2E77: 30 -> 31`
- `2E78: 32 -> 31`
- `2E8A: A0 -> 9C`

That matches the previously observed BIOS-saved `502` pattern exactly.

## What this means

The app no longer appears to have a general "metadata generation" problem.

The remaining issue is narrower:

- a file with only `CPU Current Limit = 502` can be generated in BIOS-style form
- but user-generated files with other modified parameters still fail BIOS import

This strongly suggests that some fields require extra mirrored fields, dependent flags, or secondary state that BIOS also updates when it saves those settings.

## Current code state

### `core/src/OcbProfile.cpp`

This file now applies BIOS-style metadata when exported bytes differ from original bytes.

Key behavior:

- `applyBiosStyleMetadata(output)` runs when `output != original_`
- legacy compensation remains optional and separate
- metadata generation includes:
  - BCD save block at `0x2D3C..0x2D43`
  - ASCII timestamp at `0x2E6B..0x2E78`
  - `0x0012`
  - `0x2E26`
  - CRC bytes `0x0011`, `0x2D7F`, `0x2E8A`

### `core/include/ocb/core/OcbProfile.hpp`

Default export path is now:

- `saveToFile(..., bool compensateChecksums = false)`
- `exportBytes(..., bool compensateChecksums = false)`

### `app/main.cpp`

Defaults were already moved to:

- no legacy checksum compensation by default
- GUI checkbox label is `Legacy checksum compensation`
- CLI still supports `--compensate`

### `core/src/ChecksumCompensator.cpp`

This already has an early return:

- if current computed sums already match original target sums, do not rewrite the compensation area

That fixed the earlier broken behavior where even unchanged files could get a mutated compensation region.

## User-tested files and outcomes

### 1. Known accepted file

The user confirmed:

- `D:\14900K\try_02_conservative_sum_comp\MsOcFile.ocb`
- imports in BIOS

### 2. Current app-generated target file failed

The user's current app-generated flash-drive file:

- `E:\MsOcFile.ocb`
- SHA-256: `DAACAB59FCEBBF28DBE41D05CCA6423D58DFCE4DDF214BA05DFC1D3E7D0AAE67`

Diff from original:

- `0011: D3 -> 2D`
- `0CD1: 02 -> 01`
- `0F49: 33 -> 2C`
- `0F82: 00 -> 01`
- `0F83: 32 -> 14`
- `15F9: C8 -> D2`
- `1600: DC -> FA`
- `1916: 01 -> 00`
- `1917: 01 -> 00`
- `2D3C: 02 -> 36`
- `2D3E: 53 -> 33`
- `2D40: 19 -> 21`
- `2D7F: 58 -> 3C`
- `2E73: 31 -> 32`
- `2E74: 39 -> 31`
- `2E75: 35 -> 33`
- `2E77: 30 -> 33`
- `2E78: 32 -> 36`
- `2E8A: A0 -> 0C`

This corresponds to user-facing settings:

- `CPU Current Limit = 300`
- `CPU Lite Load Control = 1`
- `CPU Lite Load = 20`
- `PL1 = 210`
- `PL2 = 250`
- `IA CEP Enable = 0`
- `GT CEP Enable = 0`

User reported this file did not import.

### 3. Retry matrix that already failed

Directory:

- `E:\ocb_retry_matrix`

Files created there:

1. `01_fields_only_exact`
2. `02_fields_only_plus_legacy_comp`
3. `03_biosstyle_metadata_no_comp`
4. `04_biosstyle_metadata_plus_legacy_comp`

User tested all 4 and reported:

- all 4 failed BIOS import

Interpretation:

- failure is not explained by "fields only vs metadata only"
- failure is not explained by legacy compensation

## Isolation matrix not yet tested by user

Directory:

- `E:\ocb_isolation_matrix`

These files were prepared to isolate which single setting breaks import:

- `00_repro_current_502\MsOcFile.ocb`
- `01_only_current_300\MsOcFile.ocb`
- `02_only_pl1_210\MsOcFile.ocb`
- `03_only_pl2_250\MsOcFile.ocb`
- `04_only_liteload_control_1\MsOcFile.ocb`
- `05_only_liteload_20\MsOcFile.ocb`
- `06_only_ia_cep_0\MsOcFile.ocb`
- `07_only_gt_cep_0\MsOcFile.ocb`
- `08_liteload_pair_control1_mode20\MsOcFile.ocb`
- `09_cep_pair_both_0\MsOcFile.ocb`
- `10_power_triplet_300_210_250\MsOcFile.ocb`
- `11_only_enhanced_turbo_1\MsOcFile.ocb`

At the time of writing, the user had not yet reported results for this isolation matrix.

## Strong current hypothesis

The remaining import failures are probably caused by field-specific dependent state, not by the generic metadata block.

The most suspicious candidates are:

### A. Lite Load related hidden state

The current failing file changes:

- `0x0CD1`
- `0x0F82`
- `0x0F83`

This is suspicious because Lite Load on MSI often has mode/control interactions and there may be extra hidden mirrors or mode flags.

### B. CEP related hidden state

The current failing file changes:

- `0x1916`
- `0x1917`

But IFR also shows other CEP-related fields:

- `IA CEP Support` at `0xFBA`
- `GT CEP Support` at `0xFBB`
- `IA CEP Support For 14th` at `0xFBC`
- `GT CEP Support For 14th` at `0xFBD`
- `IA CEP Enable` at `CpuSetup 0x334` -> file offset `0x1916`
- `GT CEP Enable` at `CpuSetup 0x335` -> file offset `0x1917`

This means BIOS may require these to stay synchronized, and the app currently only exposes / edits part of that set depending on what the user touched.

### C. Power limit mirrors or secondary caches

The current failing file changes:

- `0x15F9` = PL1
- `0x1600` = PL2

But raw-byte search showed extra copies / related regions still present in original for some values. Example:

- original still contains `200` at `0x06D2` and `0x1AE5`
- changing only PL1 at `0x15F9` may not be enough if BIOS checks mirrored cached values

This is not yet proven, but it is a strong lead.

## Relevant IFR findings

From `D:\14900K\Setup_IFR.txt`:

- `Long Duration Power Limit(W)` -> VarStore `0x2`, offset `0x17`
- `Short Duration Power Limit(W)` -> VarStore `0x2`, offset `0x1E`
- `CPU Current Limit(A)` -> VarStore `0xF101`, offset `0xF2A`
- `CPU Lite Load Control` -> VarStore `0xF101`, offset `0xF63`
- `CPU Lite Load` -> VarStore `0xF101`, offset `0xF64`
- `IA CEP Support` -> VarStore `0xF101`, offset `0xFBA`
- `GT CEP Support` -> VarStore `0xF101`, offset `0xFBB`
- `IA CEP Support For 14th` -> VarStore `0xF101`, offset `0xFBC`
- `GT CEP Support For 14th` -> VarStore `0xF101`, offset `0xFBD`
- `IA CEP Enable` -> VarStore `0x2`, offset `0x334`
- `GT CEP Enable` -> VarStore `0x2`, offset `0x335`

The coexistence of both `Support` and `Enable` style CEP fields is a major clue.

## Known stale / suspicious test state

`tests/OcbCoreTests.cpp` still contains an old test that assumes conservative preset export should exactly equal `try_02`.

That assumption is no longer generally valid now that export uses dynamic BIOS-style metadata instead of the old `try_02` template.

The tests should be rewritten to check deterministic metadata behavior with a fixed `OCB_EXPORT_TIMESTAMP`, especially:

- reproduce BIOS-saved `502`
- check exact bytes at the service offsets

No final clean test pass was completed after the new metadata logic was introduced.

## Suggested next steps for the next agent

1. Run the user through `E:\ocb_isolation_matrix` and identify the first single-field file that BIOS rejects.

2. If `00_repro_current_502` fails in BIOS, re-check environmental assumptions immediately:
   - exact file copied to USB root as `MsOcFile.ocb`
   - FAT32
   - same BIOS import path
   - ensure no accidental replacement happened

3. If `00_repro_current_502` passes but one of the single-field files fails:
   - compare that file against original and against BIOS-saved references
   - hunt for hidden mirrors around the same subsystem

4. Highest-priority reverse-engineering targets:
   - Lite Load hidden state
   - CEP hidden state
   - PL1 / PL2 cached mirrors

5. Best possible new evidence would be additional BIOS-saved files, each with exactly one changed setting:
   - PL1 only
   - PL2 only
   - Lite Load only
   - CEP only

6. If continuing code work:
   - keep current BIOS-style metadata logic in `core/src/OcbProfile.cpp`
   - do not go back to the old `try_02` metadata template
   - focus on field-specific synchronization, not generic CRC/timestamp logic

## Quick command that already proved the `502` reproduction

```powershell
$env:OCB_EXPORT_TIMESTAMP='20260423204311'
& 'D:\14900K\ocb-studio-native\build\app\Debug\ocb_studio.exe' `
  --input 'D:\14900K\original\MsOcFile.ocb' `
  --output 'D:\14900K\_repro_502.ocb' `
  --write 'CPU Current Limit (A)' 502
```

The resulting file hash should be:

- `0CF0FA469E735450BD3B99F204D432A4B4107EAAA98A201C63A45EEA2EAC2375`

## Bottom line

The current status is not "we still do not understand the format".

The current status is:

- generic BIOS-style save metadata is largely understood and implemented
- current-limit-only BIOS-save reproduction is already working
- the unresolved part is now likely a smaller set of hidden dependent fields for specific settings such as Lite Load, CEP, and possibly PL1/PL2 mirrors

## Continuation log (2026-04-24, local model run)

### 1) Baseline re-checks completed

- Confirmed hashes still match:
  - `D:\14900K\original\MsOcFile.ocb` -> `E5E2CE9DCC263C3CF5B5253664124FD6CF82DE306A1FB70DBD1902FDFC50D0E8`
  - `D:\14900K\try_02_conservative_sum_comp\MsOcFile.ocb` -> `5B26264366D120D714AB03346DC6D022DD7BF098720F6AC39078BB8564744248`
  - `D:\14900K\_repro_502.ocb` -> `0CF0FA469E735450BD3B99F204D432A4B4107EAAA98A201C63A45EEA2EAC2375`

- Deterministic replay of `502` was re-verified from current app binary:
  - `OCB_EXPORT_TIMESTAMP=20260423204311`
  - output hash: `0CF0FA469E735450BD3B99F204D432A4B4107EAAA98A201C63A45EEA2EAC2375`

### 2) Isolation matrix files are present and hashed

Directory `E:\ocb_isolation_matrix` contains all expected cases (`00..11`) with unique hashes.

### 3) Byte-level diffs for isolation matrix were recomputed (vs original)

All cases mutate service metadata bytes plus their target field bytes.

Common service deltas seen in almost all files:

- `0011: D3 -> 2E` (or `52` for `00_repro_current_502`)
- `2D3C: 02 -> 00`
- `2D3E: 53 -> 00`
- `2D40: 19 -> 22`
- `2D7F: 58 -> D1`
- `2E73..2E78` timestamp digits changed
- `2E8A: A0 -> 53`

Target deltas by case (highlights):

- `00_repro_current_502`: `0F49: 33 -> F6` (+ `2E26: 80 -> 00`)
- `01_only_current_300`: `0F49: 33 -> 2C`
- `02_only_pl1_210`: `15F9: C8 -> D2`
- `03_only_pl2_250`: `1600: DC -> FA`
- `04_only_liteload_control_1`: `0F82: 00 -> 01`
- `05_only_liteload_20`: `0F83: 32 -> 14`
- `06_only_ia_cep_0`: `1916: 01 -> 00`
- `07_only_gt_cep_0`: `1917: 01 -> 00`
- `08_liteload_pair_control1_mode20`: `0F82` + `0F83`
- `09_cep_pair_both_0`: `1916` + `1917`
- `10_power_triplet_300_210_250`: `0F49` + `15F9` + `1600`
- `11_only_enhanced_turbo_1`: `0CD1: 02 -> 01`

### 4) Additional field-state evidence gathered

From `D:\14900K\original\MsOcFile.ocb`:

- `PL1@0x15F9 = 200`
- `PL2@0x1600 = 220`
- `Current@0x0F49 = 51`
- `LiteLoadControl@0x0F82 = 0`
- `LiteLoad@0x0F83 = 50`
- `IA_CEP_EN@0x1916 = 1`
- `GT_CEP_EN@0x1917 = 1`
- `IA_CEP_SUP@0x0FD9 = 1`
- `GT_CEP_SUP@0x0FDA = 0`
- `IA_CEP_14@0x0FDB = 1`
- `GT_CEP_14@0x0FDC = 0`

From `D:\14900K\MsOcFile.ocb` (user target-like state):

- `PL1=210`, `PL2=250`, `Current=44`, `LiteLoadControl=1`, `LiteLoad=20`
- `IA_CEP_EN=0`, `GT_CEP_EN=0`
- `IA/GT support bytes remained 1/0 and 1/0`

This strengthens hypothesis that CEP support/enable interaction may be constrained by additional rules not yet modeled.

### 5) Test suite updates in progress

`tests/OcbCoreTests.cpp` was updated to:

- drop stale strict dependency on `try_02` equality,
- add deterministic metadata tests under fixed `OCB_EXPORT_TIMESTAMP`,
- add exact-byte replay test for known `_repro_502`.

Current local run status: long-running during IFR-heavy test region; final pass/fail snapshot still needs one clean completed run capture.

