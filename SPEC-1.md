# SPEC-1: modernize the iban extension

Repo: PostgreSQL extension `iban` (C++ in `iban.cpp`, SQL in `iban--1.1.sql`). Maintainer: Yorick. Today it
builds on PG 14 (CI) and on PG 18 (fixed in #7). Nothing else is tested.

## Hard rules (read first)
1. **Do not run any git command** and do not touch `.github/`. Leave a clean, reviewable working tree; the
   orchestrator reviews the diff and commits. (The workflow matrix is done separately by the orchestrator.)
2. **Never weaken a test, an expected output, or a validation rule to make something pass.** If a test is wrong,
   say why in the report and fix the test with a citation; never delete it.
3. **Do not change accept/reject behaviour** of existing valid/invalid IBANs unless the SWIFT IBAN registry proves
   the old behaviour wrong. Every behaviour change is listed in the report under "Behaviour changes", with the source.
4. Keep `contrib/testcase.sh` working untouched (current CI still calls it until the matrix lands).
5. The only way to build/test is the helper (no `pg_config` on this machine):
   `/home/eve/.hermes/scripts/pgext_test.sh <major> /home/eve/Projects/PostgreSQL-IBAN`
   It ships the tree to a throw-away container for PG `<major>`, runs `make`, `make install`, and
   `make installcheck` when the Makefile has `REGRESS`, else `contrib/testcase.sh`.
   Exit 0 with a final `== RESULT rc=0` line is the only meaning of "green". Read the output, do not trust summaries.
6. Never claim something works unless you ran it. Anything not run goes under "Not verified".

## Items (do them in this order; each has an acceptance test)

### M1 Build matrix
`make` and the tests are green on PG **14, 15, 16, 17, 18**. Fix any breakage with `PG_VERSION_NUM` guards, never by
dropping a version. Also try **19** (`pgext_test.sh 19`; the `postgres:19` image or its dev package may not exist
yet: if so, record the exact error and move on, it is not blocking).
Acceptance: five green runs quoted in the report.

### M2 pg_regress suite
Add `sql/iban.sql` + `expected/iban.out` and `REGRESS = iban` in the Makefile so `make installcheck` works.
Cover at least: valid IBANs for NL, DE, GB, FR, BE, ES, IT, CH, MA, KZ (use the registry examples), invalid checksum
(`NL91ABNA0417164301`), wrong length, unknown country, lowercase and space handling (document the CURRENT behaviour
and pin it), NULL, empty string, over-long input, casts iban to text/varchar/bpchar, text to iban, binary I/O
round-trip (`ibansend`/`ibanrecv`, e.g. via `COPY ... (FORMAT binary)` to/from a temp table or direct calls),
ordering/equality/DISTINCT/GROUP BY/index usage on an `iban` column (if the type has no btree opclass today, record
that as a finding and add the default btree + hash operator classes with tests), and `iban_validate`.
Acceptance: `installcheck` green on 14..18 and at least ONE deliberately broken variant (e.g. mutate a country
pattern, run, see red, restore) quoted in the report as proof that the suite can fail.

### M3 Exception safety and memory
No C++ exception may escape into PostgreSQL (`std::regex_error`, `std::bad_alloc`, `std::out_of_range`...).
Wrap every `Datum` entry point so exceptions become `ereport(ERROR, ...)` with a proper SQLSTATE
(`ERRCODE_INVALID_TEXT_REPRESENTATION` for bad input). Check input length before any regex work. Review the file for
undefined behaviour, off-by-ones, static-init-order and leak issues; list EVERY finding in the report, fixed or not.
Acceptance: tests for over-long and pathological input; findings list.

### M4 Performance
`parseStructure` currently builds a `std::regex` on every validation. Compile once per country (cache), keep it
thread-agnostic (a backend is single-threaded; plain static map is fine). Measure before and after with
`SELECT count(*) FROM generate_series(1,1000000) g WHERE iban_validate('NL91ABNA0417164300');` on PG 18, same
container run, report both timings and the speed-up.
Acceptance: numbers in the report; all tests still green.

### M5 SQL object hygiene + upgrade path
Mark `ibanin`, `ibanout`, `ibanrecv`, `ibansend`, `iban_validate` `IMMUTABLE STRICT PARALLEL SAFE` (check each is
really true; `ibanin` can raise errors but is still immutable). Add `iban--1.1--1.2.sql` (ALTER FUNCTION ...),
a full `iban--1.2.sql`, bump `default_version` to `1.2`, list both in `DATA` in the Makefile.
Add a regress test that does `CREATE EXTENSION iban VERSION '1.1'; ALTER EXTENSION iban UPDATE;` and verifies
`provolatile`/`proparallel`/`proisstrict` in `pg_proc`.
Acceptance: upgrade test green on 14..18.

### M6 Registry audit
Compare the country table in `iban.cpp` with the current official SWIFT IBAN registry (ISO 13616). Cite the exact
source URL and edition. Output in the report: countries missing, countries whose length/structure differ, and
countries that are in our table but not in the registry. Only ADD or CORRECT an entry when the registry text for
that country is unambiguous; every change gets a regress test using the registry example IBAN (verify its mod-97
yourself with a throw-away script before adding it). Ambiguous cases go in the report, not in the code.
Acceptance: audit table (as plain text) + tests for each change.

### M7 Small modernization
Use `-std=c++17` only if all of 14..18 build with it; add `PG_MODULE_MAGIC_EXT(.name = "iban", .version = "1.2")`
under a `PG_VERSION_NUM >= 180000` guard in place of `PG_MODULE_MAGIC`. Remove dead code. Update the README
(supported PG versions 14-18, build, `make installcheck`, the new functions/operators, example for MA). No marketing text.

## Final report (required): write `REPORT-1.md` in the repo root
Sections: **Summary**, **Per item** (what changed, evidence = command + the decisive output lines),
**Behaviour changes**, **Findings not fixed**, **Not verified**, **Disagreements with this spec** (if you think a
requirement was wrong, say so and say what you did instead). Be specific; no "all good".
