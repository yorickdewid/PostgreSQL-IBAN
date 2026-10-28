# REPORT-1: modernize the iban extension

## Summary

- M1, M2, M3, M4, M5 and M7 are done. `make`, `make install` and `make installcheck` (tests `iban` and
  `iban_upgrade`) are green on PG 14, 15, 16, 17 and 18 with the final tree.
- PG 19: there is no `postgres:19` image, so it could not be built.
- **M6 (registry audit) is blocked.** The official SWIFT IBAN registry could not be downloaded from this
  environment (HTTP 403 from swift.com, see M6). No country entry was added or changed. The report lists what to check.
- The suite found three real defects in 1.1, all now fixed:
  1. There was no btree/hash support, so `ORDER BY`, `DISTINCT`, `GROUP BY` and indexes on an `iban` column all failed.
  2. Check digits containing letters were accepted (19 variants of `NL91ABNA0417164300`).
  3. Binary input (`COPY ... FORMAT binary`) skipped validation entirely, so `garbage` could be stored as an `iban`.
- Validation is about 80x faster per row (16.6 s → 0.21 s for 1M rows). The spec's benchmark query does not measure
  this, because the planner folds it to a constant (see M4).
- One deviation from the spec: `ibanrecv`/`ibansend` are marked `STABLE`, not `IMMUTABLE`, matching core
  `textrecv`/`textsend` (see Disagreements).

Files changed or added: `iban.cpp`, `Makefile`, `iban.control`, `README.md`, `.gitignore`. New: `iban--1.2.sql`,
`iban--1.1--1.2.sql`, `sql/iban.sql`, `expected/iban.out`, `sql/iban_upgrade.sql`, `expected/iban_upgrade.out`,
`REPORT-1.md`. Not touched: `.github/`, `contrib/testcase.sh`, `iban--1.1.sql`. No git commands were run.
Temporary files used for probing and benchmarking (`sql/probe.sql`, `sql/bench.sql`, their `expected/` files,
`iban_old.cpp`) were deleted; the working tree contains only the files listed above.
`.claude-run-1.err/.json` were present before I started and were left alone.

## Per item

### M1 Build matrix

**Baseline before any change.** The helper used `contrib/testcase.sh`. PG 14–18 were all green.

`pgext_test.sh 19` failed, both at baseline and again with the final tree:

```
Error: unable to copy from source docker://postgres:19: initializing source docker://postgres:19: reading manifest 19 in docker.io/library/postgres: manifest unknown
== pgext_test: NOT GREEN (no 'RESULT rc=0' line)
```

**Final tree.** No `PG_VERSION_NUM` guards were needed beyond the existing `varatt.h` one and the new
`PG_MODULE_MAGIC_EXT` one (M7). Each of the five runs printed no compiler warnings. Decisive lines from each run of
`/home/eve/.hermes/scripts/pgext_test.sh <major> /home/eve/Projects/PostgreSQL-IBAN`:

```
== PG PostgreSQL 14.24 (Debian 14.24-1.pgdg13+2)
test iban                         ... ok           23 ms
test iban_upgrade                 ... ok           12 ms
 All 2 tests passed.
== RESULT rc=0

== PG PostgreSQL 15.19 (Debian 15.19-1.pgdg13+2)
test iban                         ... ok           22 ms
test iban_upgrade                 ... ok           12 ms
 All 2 tests passed.
== RESULT rc=0

== PG PostgreSQL 16.15 (Debian 16.15-1.pgdg13+2)
ok 1         - iban                                       22 ms
ok 2         - iban_upgrade                               12 ms
# All 2 tests passed.
== RESULT rc=0

== PG PostgreSQL 17.11 (Debian 17.11-1.pgdg13+2)
ok 1         - iban                                       22 ms
ok 2         - iban_upgrade                               13 ms
# All 2 tests passed.
== RESULT rc=0

== PG PostgreSQL 18.6 (Debian 18.6-1.pgdg13+2)
ok 1         - iban                                       23 ms
ok 2         - iban_upgrade                               12 ms
# All 2 tests passed.
== RESULT rc=0
```

**`contrib/testcase.sh` (rule 4).** The file is unchanged. I checked it against the new build by temporarily removing
the `REGRESS` line, so the helper falls back to `testcase.sh`, then restored the line. Both runs were green:

- PG 14.24: `iban | 1.2 | | IBAN functionality for PostgreSQL`, then `CREATE EXTENSION`, both `INSERT 0 1` lines,
  `iban_validate t`, KZ and MA cast output, and `== RESULT rc=0`.
- PG 18.6: identical output, `== RESULT rc=0`.

### M2 pg_regress suite

`Makefile` now has `REGRESS = iban iban_upgrade`. The new files are `sql/iban.sql`/`expected/iban.out` and
`sql/iban_upgrade.sql`/`expected/iban_upgrade.out` (the upgrade test belongs to M5).

What `sql/iban.sql` covers:

- **Independent checksum.** A SQL reference implementation of ISO 7064 MOD 97-10 (`pg_temp.ref_mod97`, numeric
  arithmetic) checks every fixture independently of the C code. `pg_temp.with_check` builds IBANs with correct check
  digits, which lets the tests isolate failures caused by structure or country alone.
- **Valid IBANs:** NL, DE, GB, FR, BE, ES, IT, CH, MA, KZ. For each: length, reference mod-97 = 1, `iban_validate` = t,
  and a cast to `iban`.
- **Invalid IBANs:**
  - invalid checksum `NL91ABNA0417164301` (reference mod-97 = 28);
  - wrong length, one short and one long;
  - unknown country with correct check digits (`XX62...`, `US22...`);
  - a structure violation with correct check digits (`NL251BNA0417164300`);
  - letter check digits (`NL0SABNA0417164300` has reference mod-97 = 1 but is now rejected);
  - check digits 00/01/99.
- **Pinned current behaviour:**
  - lowercase and mixed case are accepted and stored as typed;
  - spaces (print format, leading, trailing) are rejected;
  - case-different values are not equal.
- **NULL, empty and short input:** `NULL::iban`, `iban_validate(NULL)`, `''`, 1–4 characters.
- **Over-long and pathological input** (M3):
  - 1.8 MB input to `iban_validate` and to the cast (SQLSTATE 22P02 checked in a DO block);
  - lengths 34 and 35;
  - format strings, regex metacharacters, newline, non-ASCII, 34 x `9`, 34 x `Z`, `NL!!...`;
  - all 255 possible last bytes, of which only one is accepted.
- **Casts:** iban to text/varchar/bpchar (checked with `pg_typeof`), and text to iban (explicit, plus the
  assignment case, which is validated).
- **Binary I/O:**
  - `ibansend` bytes;
  - `COPY ... (FORMAT binary)` round trip of 13 rows, all identical after reload;
  - invalid binary input is rejected with SQLSTATE 22P03, both for a bad checksum and for `garbage`.
- **Comparison and indexes:**
  - the operators and opclasses are listed from the catalog;
  - all 6 operators and `iban_cmp`;
  - `ORDER BY`, `ORDER BY DESC`, `DISTINCT`, `GROUP BY`/`HAVING`, and hash aggregation with `enable_sort=off`;
  - hash functions;
  - a hash index scan and a btree index scan, each shown by `EXPLAIN`;
  - primary key violation, and a join on `iban`.
- **`iban_validate`** is exercised throughout.

**Finding: no btree/hash opclass in 1.1.** Probe run against the unmodified 1.1 code on PG 18.6:

```
SELECT a FROM t ORDER BY a;
ERROR:  could not determine which collation to use for string comparison
SELECT DISTINCT a FROM t;
ERROR:  could not determine which collation to use for string hashing
SELECT a, count(*) FROM t GROUP BY a;
ERROR:  could not determine which collation to use for string hashing
CREATE INDEX ON t (a);
ERROR:  could not determine which collation to use for string comparison
CREATE INDEX ON t USING hash (a);
ERROR:  could not determine which collation to use for string hashing
```

`iban` is binary-coercible to `text`, so PG picks `text_ops`, but `iban` is not collatable. I added C functions
`iban_eq/ne/lt/le/gt/ge/cmp/hash/hash_extended`. They compare bytes, which matches `COLLATE "C"`. On top of them sit
the operators `= <> < <= > >=` (`=` is `HASHES, MERGES`) and the default operator classes `iban_ops` for btree and
hash. The `iban_upgrade` test pins the 1.1 error: `ORDER BY` on 1.1 fails, and the same data sorts after
`ALTER EXTENSION iban UPDATE`.

**The suite can fail.** I changed the NL pattern in `iban.cpp` from `{"NL", 18, "U04F10"}` to `"F04F10"` and ran on
PG 18.6:

```
not ok 1     - iban                                       22 ms
not ok 2     - iban_upgrade                               12 ms
# 2 of 2 tests failed.
-NL|NL91ABNA0417164300|18|1|t|NL91ABNA0417164300
-(10 rows)
+ERROR:  invalid iban format for value: "NL91ABNA0417164300"
...
 accepted
-1
+0
== RESULT rc=1
== pgext_test: NOT GREEN (no 'RESULT rc=0' line)
```

I then restored `U04F10`. The final matrix in M1 was run after the restore.

Expected-output corrections made while writing the tests. None of these weakens a test:

- **Removed blank lines.** pg_regress does not echo blank input lines in this setup.
- **Text-to-iban assignment.** I first expected `INSERT INTO ibans SELECT 'text', <text column>` to fail. It
  succeeds on 1.1 and 1.2 alike, because the type is declared `CATEGORY 'S'` and PG allows I/O-conversion assignment
  casts into string-category types. That is existing behaviour, so I pinned it. I also added a negative case
  (`'NL91ABNA0417164301'::text` assigned → rejected) showing the path still validates.
- **Array type in the object list.** The object list in `iban_upgrade` leaves out `type iban[]`. PG 17 and 18 record
  the implicit array type as an extension member; PG 14–16 do not (seen in the runs: 17/18 printed an extra
  `type iban[]` row). The upgraded-vs-fresh comparison below it still includes all objects.

### M3 Exception safety and memory

What changed in `iban.cpp`:

- **Exception handling.**
  - All C++ work runs inside one function, `account_validate()`. It catches `std::bad_alloc` (→
    `ERRCODE_OUT_OF_MEMORY`), `std::exception` (→ `ERRCODE_INTERNAL_ERROR`) and `...`.
  - The message is copied into a stack buffer, and `ereport(ERROR)` is called only after the handler has finished.
    No longjmp ever crosses a live C++ object or an active `catch`.
  - No PostgreSQL function that can `elog` (palloc etc.) is called while C++ objects are alive.
- **Error codes for bad input.** Bad text input → `ERRCODE_INVALID_TEXT_REPRESENTATION` (22P02, tested). Bad binary
  input → `ERRCODE_INVALID_BINARY_REPRESENTATION` (22P03, tested).
- **Length checked first.** The length (4..34, ISO 13616 maximum) is checked before any copy, map lookup or regex work.
  The working copy is a fixed `char[34]`.
- **No load-time construction.** The country table is a constant array. The validator is built lazily (function-local
  static) inside the guarded region, not at `dlopen` time.

Every finding from reviewing the 1.1 `iban.cpp`:

| # | Finding (1.1) | Status |
|---|---|---|
| 1 | `elog(ERROR)` called *inside* `catch` blocks: longjmp leaves the C++ handler without `__cxa_end_catch`, leaking the exception object and corrupting the runtime's caught-exception stack (undefined behaviour) | fixed |
| 2 | `static Validate validator;` global constructor runs at library load; a `std::bad_alloc` there means `std::terminate` → backend abort → postmaster crash recovery for all sessions (static-init issue) | fixed (lazy, guarded) |
| 3 | `specifications[countryCode]` (`operator[]`) inserts an empty entry for every unknown 2-character prefix, so the per-backend map grows with arbitrary input (up to 65 536 junk entries) | fixed (`find`) |
| 4 | Two `std::regex` objects built per validation: the block splitter `std::regex("(.{3})")` in every loop iteration plus the final pattern | fixed (M4) |
| 5 | `toupper`, `std::isdigit`, `std::isupper` called with plain `char`; non-ASCII bytes are negative, which is UB for `<cctype>`. `toupper` is also locale dependent, so `IMMUTABLE` was not strictly true | fixed (ASCII-only helpers) |
| 6 | Check digits (positions 3–4) were never validated: letters were expanded to two digits and other characters skipped by the mod-97 code. Probe: 19 letter variants of `NL91ABNA0417164300` accepted, e.g. `NL0SABNA0417164300`, `NLV9ABNA0417164300` | fixed, behaviour change |
| 7 | `ibanrecv` did no validation. Probe: `COPY t FROM '/tmp/probe.bin' (FORMAT binary)` stored `NL91ABNA0417164301` and `garbage` in an `iban` column | fixed, behaviour change |
| 8 | `iso7064Mod97_10`: `numberstring.length() - step` is a `size_t` underflow when shorter than 9 digits; `substr` then throws `std::out_of_range`. Unreachable today (shortest IBAN is 15) | fixed (rewritten as a running remainder, no exceptions) |
| 9 | `iso7064Mod97_10` silently skipped characters that are neither digits nor uppercase letters | fixed (returns -1) |
| 10 | `parseStructure`: an unknown structure letter produced `[]`, i.e. `std::regex_error` at runtime; `std::stoi` on a malformed block throws `std::invalid_argument`. Unreachable with the static table | fixed (explicit check, exception becomes an ERROR) |
| 11 | Whole input copied into a `std::string` and upper-cased *before* the length check (a 1 GB value means a 1 GB copy, possibly `bad_alloc`) | fixed (length first) |
| 12 | `iban_validate` used `PG_GETARG_TEXT_P` + `text_to_cstring`: two copies per call | fixed (`PG_GETARG_TEXT_PP`, pointer + length) |
| 13 | `std::rotate`/`std::transform` used without `#include <algorithm>` (compiled only through transitive includes) | gone with the rewrite |
| 14 | Dead or misleading code: `return false` after `elog(ERROR)`, `typedef char Iban`, unused `<sstream>`, duplicated `account_validate_text`/`_str` | removed |
| 15 | No btree/hash operator class (see M2) | fixed |
| 16 | The error message echoes the full input, so a 1.8 MB value produces a 1.8 MB error message | **not fixed** (message text is existing behaviour) |
| 17 | Values are stored as typed (lowercase kept), and equality is case-sensitive | **not fixed**, pinned by tests |

Tests for over-long and pathological input are in `sql/iban.sql`, section "Over-long and pathological input". They are
green on 14–18 (M1).

### M4 Performance

The country table is now a constant array. Each country's regex is compiled once, on first use, with
`std::regex::optimize`, and kept for the life of the backend: a plain map, single-threaded. The 3-character block
splitting is a plain loop instead of a regex. Mod-97 is a running remainder.

**Method.** One PG 18.6 container run. The tree temporarily contained `iban_old.cpp`, a byte-for-byte copy of the 1.1
`iban.cpp`, built as a second module `iban_old`. A temporary regress test `bench` declared
`old_validate(text) ... AS '$libdir/iban_old', 'iban_validate'`, ran `SET jit = off`, and used `\timing on`. The
temporary files were deleted afterwards.

**The spec's query does not measure validation.** `iban_validate` is `IMMUTABLE` and its argument is a constant, so
the planner folds the call. The plan contains no filter:

```
EXPLAIN (VERBOSE, COSTS OFF) SELECT count(*) FROM generate_series(1,1000000) g WHERE iban_validate('NL91ABNA0417164300');
Aggregate
  Output: count(*)
  ->  Function Scan on pg_catalog.generate_series g
        Output: g
        Function Call: generate_series(1, 1000000)
```

For that reason I also timed a per-row variant: `iban_validate(CASE WHEN g > 0 THEN 'NL91ABNA0417164300' END)`. Its
plan shows `Filter: iban_validate(CASE WHEN (g.g > 0) ...)`.

| query (1M rows, PG 18.6, same container) | 1.1 (old) | 1.2 (new) | speed-up |
|---|---|---|---|
| spec query (constant-folded) | 77.097 ms | 54.458 ms | not meaningful (one call each) |
| per-row, run 1 | 16616.677 ms | 208.180 ms | 79.8x |
| per-row, run 2 | 16570.690 ms | 206.282 ms | 80.3x |

The third old/new pair was cut off by the helper's 60-line diff limit, so only two pairs are reported. All tests are
still green (M1).

### M5 SQL object hygiene + upgrade path

- **`iban--1.2.sql` (full install).** All five functions are `STRICT PARALLEL SAFE`:
  - `ibanin`, `ibanout` and `iban_validate` are `IMMUTABLE`;
  - `ibanrecv` and `ibansend` are `STABLE` (see Disagreements);
  - the new comparison and hash functions are `IMMUTABLE STRICT PARALLEL SAFE`;
  - the file also has the operators and opclasses, and an `\echo ... \quit` guard.
- **`iban--1.1--1.2.sql`.** `ALTER FUNCTION` for the five functions, then the same new objects.
- **`iban.control`.** `default_version = '1.2'`.
- **`Makefile`.** `DATA = iban--1.1.sql iban--1.2.sql iban--1.1--1.2.sql`. `iban--1.1.sql` is unchanged.

Why each marking holds:

- `ibanin` raises errors but depends only on its input and the compiled-in table. Upper-casing is ASCII-only now, so it
  no longer depends on the locale.
- `iban_validate` is the same, without the errors.
- `ibanout` is a plain copy.
- None of the functions write shared state. The lazily filled per-backend regex cache is safe in parallel workers.

`sql/iban_upgrade.sql` runs `CREATE EXTENSION iban VERSION '1.1'`, checks `pg_proc`, stores data, runs
`ALTER EXTENSION iban UPDATE`, and checks `pg_proc` again. It verifies that the stored data now sorts, groups and
takes a unique index, and that a duplicate blocks the unique index. Finally it compares every extension member of the
upgraded install with a fresh 1.2 install (the result is empty). Expected `pg_proc` output after the upgrade:

```
proname|provolatile|proparallel|proisstrict
iban_validate|i|s|t
ibanin|i|s|t
ibanout|i|s|t
ibanrecv|s|s|t
ibansend|s|s|t
```

Before the upgrade, all five rows are `i|u|t`. This test is green on 14–18 (M1).

### M6 Registry audit: NOT DONE (blocked)

I could not get the official registry. A sub-agent tried WebFetch on these URLs, and every one returned
**HTTP 403 Forbidden**:

- `https://www.swift.com/standards/data-standards/iban-international-bank-account-number`
- `https://www.swift.com/swift_resource/9606`
- `https://www.swift.com/standards/standards-resources`
- `https://www.swift.com/sites/default/files/files/iban-registry-100.pdf`
- `https://www.swift.com/swift-resource/11971/download`
- `https://www.swift.com/swift-resource/22851/download`

Shell download (curl) was not permitted in this session. Search-result snippets suggest the current edition is
"Release 103" at `https://www.swift.com/swift_resource/9606`, but I could not open it, so the source URL and edition
cannot be cited as read.

The spec only allows changes "when the registry text for that country is unambiguous". With no registry text at all,
**no entry was added or changed**. The country table in `iban.cpp` was moved from `addSpecification(...)` calls into a
constant array, copying all 88 entries (code, length, structure) in the same order. I checked this copy by reading,
not with a tool; the regress tests cover only 10 of the countries. The following list is **from memory, unverified, and not acted on**. It
is only a checklist for whoever does the audit with the real document:

- Possibly in the registry but missing from our table: BI (since 2021, length 27?), BY, DJ, FK, HN, IQ, LY, MN, NI, OM,
  RU, SC, SD, SO, SV, VA, YE.
- Possibly different length or structure:
  - CR: table 21, registry possibly 22 (`4!n14!n` with a leading 0).
  - EG: table 27, registry possibly 29.
  - UA: table `F25`, numeric only; registry possibly `6!n19!c`, alphanumeric.
  - LC: table `U04F24`; registry possibly `4!a24!c`.
  - BI: table 16; registry possibly 27.
- Possibly in our table but not in the SWIFT registry (they look like the "experimental" list of the iban.js library
  the table came from): AO, BF, BJ, CG, CI, CM, CV, DZ, GA, IR, MG, ML, MZ, SN, and possibly MA. Removing any of
  these would change accept/reject behaviour, and per rule 3 needs the registry text.

To finish M6: put the registry PDF/TXT in the repo or in `/tmp`, or allow `curl`. The rest is mechanical: extract,
diff, and add tests with `pg_temp.ref_mod97` (already in `sql/iban.sql`).

### M7 Small modernization

- **C++17.** `PG_CXXFLAGS = -std=c++17 -fPIC`. It builds without warnings on 14–18 (the compile lines in M1 show
  `-std=c++17`).
- **Module magic.**
  - PG 18: `PG_MODULE_MAGIC_EXT(.name = "iban", .version = "1.2")` under `#if PG_VERSION_NUM >= 180000`.
  - Older versions: `PG_MODULE_MAGIC`.
  - Compiled and loaded on PG 18.6 (the regress tests load the module).
- **Dead code removed.** See M3 #14; also the unused `std::regex` block splitter and `<sstream>`.
  - The `L` and `W` structure letters are kept in `parseStructure`, although the table does not use them, because
    they are part of the structure notation.
  - The `A` class `"0-9A-ZA-Z"` was simplified to `"0-9A-Z"` (same set). `C` `"A-ZA-Z"` became `"A-Z"`.
- **README.** It now covers:
  - supported versions 14–18;
  - build steps (C++17);
  - `make installcheck`;
  - input rules (case, spaces, check digits, binary input);
  - the function and operator table, and the opclasses;
  - a Moroccan example;
  - the `ALTER EXTENSION iban UPDATE` upgrade.
- **`.gitignore`.** Added `*.bc`, `results/`, `regression.diffs`, `regression.out`.

## Behaviour changes

1. **Check digits must be two digits.** `NL0SABNA0417164300` and `NLV9ABNA0417164300` were accepted by 1.1 and are now
   rejected; the 1.1 probe found 19 such variants for one account.
   - Source: ISO 13616-1. The IBAN check digits are "2!n", and the MOD 97-10 check-digit pair is numeric 02–98. The
     SWIFT registry gives every country's IBAN structure as `CC` + `2!n` + BBAN.
   - Caveat: I could not open the registry document itself (M6). The `2!n` check-digit format is the ISO 13616 IBAN
     definition, not a per-country entry.
2. **Binary input is validated.** `COPY ... FROM ... (FORMAT binary)` and binary protocol parameters went through
   `ibanrecv` without validation. They now reject invalid values with SQLSTATE 22P03.
   - The rejected values are invalid under the same registry/ISO 7064 rules that text input already enforced. For
     example, `NL91ABNA0417164301` fails mod-97, and `garbage` has no country.
   - Existing rows in tables that were loaded this way are not re-checked.
3. **New `iban` operators change some query results.**
   - Comparisons between two `iban` values now use the new byte-wise operators instead of an implicit cast to `text`.
   - Equality results are unchanged.
   - A comparison against an untyped literal (`account < 'NL'`) now parses the literal as `iban`, so it raises an
     error if the literal is not a valid IBAN. Before, it was compared as text.
   - `ORDER BY`/`DISTINCT`/`GROUP BY`/indexes went from "error" to working. Sort order is byte order, so uppercase
     sorts before lowercase.
4. **Function markings changed.** All five functions are now `PARALLEL SAFE`, so parallel query plans can use them.
   `ibanrecv`/`ibansend` changed from `IMMUTABLE` to `STABLE`.
5. **No other accept/reject change.** Lowercase, spaces, lengths, countries and structures behave as in 1.1. The
   rewritten mod-97 gives the same result as 1.1 for every input that reaches it (only `[0-9A-Z]` after the new
   checks).

## Findings not fixed

- **M6 audit not done** (registry inaccessible). Country data is unchanged and may be outdated (see the M6 checklist).
- **Full input in error messages.** The text and binary error messages include the whole input value; a 1.8 MB input
  gives a 1.8 MB error message (M3 #16).
- **No normalisation.** Lowercase input is stored as typed, so `nl91…` ≠ `NL91…` and both can live in a unique index
  (M3 #17). This is pinned as current behaviour; changing it needs a decision from the maintainer.
- **Rows loaded by binary COPY under 1.1** may contain invalid values. Nothing re-validates existing data; a check is
  `SELECT * FROM t WHERE NOT iban_validate(col::text)`.
- **`probin` differs between upgraded and fresh installs.** The 1.1 functions were created `AS 'iban'`, and fresh 1.2
  uses `MODULE_PATHNAME` (`$libdir/iban`). Both resolve to the same library. `iban_upgrade` compares object identities,
  not `probin`.
- **`std::regex` is still the slowest part.** A hand-written matcher for the 3-letter structure notation would be
  faster still. Not done, because the spec asked for a cached regex.

## Not verified

- **PG 19.** Not built or tested: the `postgres:19` image does not exist (exact error in M1). No other tag (beta/rc)
  was tried.
- **Exception-to-ERROR path at runtime.** The `std::bad_alloc` / `std::exception` → `ereport` path was reviewed but
  never triggered: SQL cannot reach it without fault injection. The tests prove only that no exception escapes for the
  inputs they use.
- **Registry provenance of the M2 fixtures.** The fixtures for NL, DE, GB, FR, BE, ES, IT, CH, KZ and MA are the
  commonly published registry examples as I remember them. Their mod-97 is verified in the test by an independent SQL
  implementation (all = 1). I could not compare them against the registry document itself (M6). MA is the value
  already in `contrib/testcase.sh`.
- **Cited ISO 13616 / MOD 97-10 rules.** Behaviour change 1 cites ISO 13616 / MOD 97-10 rules from knowledge, not
  from a document I opened in this session.
- **Other platforms.** Only Debian `postgres:<major>` images on x86_64 with g++ were used. clang was used only for
  bitcode (`iban.bc`, LLVM JIT); JIT execution of the extension was not exercised (`jit` was off in the benchmark, and
  the regress tests are too small to trigger it).
- **CI workflow.** Not touched (rule 1). The orchestrator's matrix is not tested here.

## Disagreements with this spec

1. **M5: `ibanrecv`/`ibansend` are not IMMUTABLE.** The spec says to mark them `IMMUTABLE` but also to "check each is
   really true".
   - They call `pq_getmsgtext`/`pq_sendtext`, which convert between server and `client_encoding`, so the result
     depends on a GUC.
   - Core marks the equivalent functions STABLE. Observed on PG 18.6:
     `textrecv|s|s`, `textsend|s|s`, `varcharrecv|s|s`, against `textin|i|s`, `textout|i|s`.
   - I marked them `STABLE STRICT PARALLEL SAFE`. The other three are `IMMUTABLE` as asked.
2. **M4: the benchmark query is constant-folded.** As shown by EXPLAIN in M4, it times `generate_series` plus one
   validation, not a million validations. I report it as asked (77.1 → 54.5 ms). The meaningful number is the per-row
   variant (16.6 s → 0.21 s, about 80x).
3. **M6 could not be done without the source document.** The spec's rules forbid acting on unverified data, so I did
   not. A checklist is given instead.
4. **M2: some cases are pinned rather than "documented" in prose.** "Text to iban" includes the implicit-assignment
   path, which the spec did not mention. It works because the type is in the string category; it is pinned and shown
   to validate.
