--
-- Regression tests for the iban extension
--
\set VERBOSITY terse
\pset format unaligned
CREATE EXTENSION iban;
SELECT extversion FROM pg_extension WHERE extname = 'iban';

-- Independent reference implementation of ISO 7064 MOD 97-10 (numeric arithmetic)
CREATE FUNCTION pg_temp.ref_mod97(iban text) RETURNS integer
LANGUAGE sql IMMUTABLE AS $$
  SELECT (string_agg(CASE WHEN c ~ '[0-9]' THEN c ELSE (ascii(c) - 55)::text END, '' ORDER BY n)::numeric % 97)::integer
  FROM regexp_split_to_table(substr(upper(iban), 5) || substr(upper(iban), 1, 4), '') WITH ORDINALITY AS s(c, n)
$$;
-- Build an IBAN with correct check digits for a country and BBAN
CREATE FUNCTION pg_temp.with_check(cc text, bban text) RETURNS text
LANGUAGE sql IMMUTABLE AS $$
  SELECT cc || lpad((98 - pg_temp.ref_mod97(cc || '00' || bban))::text, 2, '0') || bban
$$;

--
-- Valid IBANs (SWIFT IBAN registry examples)
--
CREATE TABLE fixtures (cc text, iban text);
INSERT INTO fixtures VALUES
  ('BE', 'BE68539007547034'),
  ('CH', 'CH9300762011623852957'),
  ('DE', 'DE89370400440532013000'),
  ('ES', 'ES9121000418450200051332'),
  ('FR', 'FR1420041010050500013M02606'),
  ('GB', 'GB29NWBK60161331926819'),
  ('IT', 'IT60X0542811101000000123456'),
  ('KZ', 'KZ86125KZT5004100100'),
  ('MA', 'MA64011519000001205000534921'),
  ('NL', 'NL91ABNA0417164300');
SELECT cc, iban, length(iban), pg_temp.ref_mod97(iban) AS mod97, iban_validate(iban), iban::iban AS cast
  FROM fixtures ORDER BY cc;

--
-- Invalid IBANs
--
-- invalid checksum
SELECT pg_temp.ref_mod97('NL91ABNA0417164301') AS mod97, iban_validate('NL91ABNA0417164301');
SELECT 'NL91ABNA0417164301'::iban;
-- wrong length (one short, one long)
SELECT iban_validate('NL91ABNA041716430'), iban_validate('NL91ABNA04171643000');
SELECT 'DE8937040044053201300'::iban;
-- unknown country (correct check digits, so only the country is wrong)
SELECT pg_temp.with_check('XX', 'ABNA0417164300') AS iban, iban_validate(pg_temp.with_check('XX', 'ABNA0417164300'));
SELECT pg_temp.with_check('US', '021000021123456') AS iban, iban_validate(pg_temp.with_check('US', '021000021123456'));
-- correct check digits but a digit where the NL structure requires a letter
SELECT pg_temp.with_check('NL', '1BNA0417164300') AS iban, iban_validate(pg_temp.with_check('NL', '1BNA0417164300'));
-- check digits must be numeric; these satisfy MOD 97 when letters are expanded
SELECT pg_temp.ref_mod97('NL0SABNA0417164300') AS mod97, iban_validate('NL0SABNA0417164300'), iban_validate('NLV9ABNA0417164300');
-- check digits 00, 01 and 99 are never valid for this account
SELECT iban_validate('NL00ABNA0417164300'), iban_validate('NL01ABNA0417164300'), iban_validate('NL99ABNA0417164300');

--
-- Case and whitespace handling (current behaviour, pinned)
--
-- lowercase is accepted and stored exactly as typed
SELECT iban_validate('nl91abna0417164300'), 'nl91abna0417164300'::iban, 'Nl91aBnA0417164300'::iban;
-- spaces anywhere are rejected (no normalisation of the print format)
SELECT iban_validate('NL91 ABNA 0417 1643 00'), iban_validate(' NL91ABNA0417164300'), iban_validate('NL91ABNA0417164300 ');
SELECT 'NL91 ABNA 0417 1643 00'::iban;
-- equality is exact, so differently cased values are different values
SELECT 'nl91abna0417164300'::iban = 'NL91ABNA0417164300'::iban AS equal;

--
-- NULL, empty and short input
--
SELECT NULL::iban IS NULL AS is_null, iban_validate(NULL) IS NULL AS validate_null;
SELECT iban_validate(''), iban_validate('N'), iban_validate('NL9'), iban_validate('NL91');
SELECT ''::iban;

--
-- Over-long and pathological input
--
SELECT iban_validate(repeat('NL91ABNA0417164300', 100000)) AS one_point_eight_mb;
SELECT iban_validate(rpad('NL91ABNA0417164300', 34, '0')) AS len34, iban_validate(rpad('NL91ABNA0417164300', 35, '0')) AS len35;
DO $$
BEGIN
  PERFORM repeat('NL91ABNA0417164300', 100000)::iban;
EXCEPTION WHEN invalid_text_representation THEN
  RAISE NOTICE 'over-long input rejected with SQLSTATE %', SQLSTATE;
END
$$;
SELECT n, length(s), iban_validate(s) FROM (VALUES
  (1, 'NL91%s%s%s%s%n%n%n%n0'),
  (2, 'NL91ABNA04171643.*'),
  (3, 'NL91ABNA041716430' || chr(10)),
  (4, 'NL91' || chr(196) || 'BNA0417164300'),
  (5, repeat('9', 34)),
  (6, repeat('Z', 34)),
  (7, '    '),
  (8, 'NL!!ABNA0417164300'),
  (9, 'NL91[A-Z]{4}[0-9]{10}'),
  (10, 'NL91\d{4}0417164300')
) v(n, s) ORDER BY n;
SELECT count(*) AS accepted FROM generate_series(0, 255) b
  WHERE b <> 0 AND iban_validate('NL91ABNA041716430' || chr(CASE WHEN b > 127 AND getdatabaseencoding() <> 'UTF8' THEN 63 ELSE b END));

--
-- Casts
--
SELECT 'NL91ABNA0417164300'::iban::text AS t, pg_typeof('NL91ABNA0417164300'::iban::text);
SELECT 'NL91ABNA0417164300'::iban::varchar AS v, pg_typeof('NL91ABNA0417164300'::iban::varchar);
SELECT 'NL91ABNA0417164300'::iban::bpchar AS b, pg_typeof('NL91ABNA0417164300'::iban::bpchar);
SELECT 'NL91ABNA0417164300'::text::iban AS i, pg_typeof('NL91ABNA0417164300'::text::iban);
SELECT 'NL91ABNA0417164301'::text::iban;
-- text to iban also works on assignment (iban is in the string category) and is validated
CREATE TABLE ibans (name text, account iban);
INSERT INTO ibans SELECT 'assign', iban FROM fixtures WHERE cc = 'BE';
INSERT INTO ibans SELECT 'assign', 'NL91ABNA0417164301'::text;
-- iban to text is implicit
SELECT length(account), upper(account), account || '!' AS concat FROM (SELECT 'nl91abna0417164300'::iban AS account) s;

--
-- Binary I/O
--
SELECT ibansend('NL91ABNA0417164300'::iban);
INSERT INTO ibans SELECT cc, iban::iban FROM fixtures;
INSERT INTO ibans VALUES ('lower', 'nl91abna0417164300'), ('dup', 'NL91ABNA0417164300');
COPY (SELECT name, account FROM ibans ORDER BY name) TO '/tmp/iban_regress.bin' (FORMAT binary);
CREATE TABLE ibans_copy (name text, account iban);
COPY ibans_copy FROM '/tmp/iban_regress.bin' (FORMAT binary);
SELECT count(*) AS copied, count(*) FILTER (WHERE c.account::text = i.account::text) AS identical
  FROM ibans_copy c JOIN ibans i USING (name);
-- binary input is validated like text input
CREATE TABLE texts (name text, account text);
INSERT INTO texts VALUES ('checksum', 'NL91ABNA0417164301');
COPY texts TO '/tmp/iban_regress_bad.bin' (FORMAT binary);
COPY ibans_copy FROM '/tmp/iban_regress_bad.bin' (FORMAT binary);
DO $$
BEGIN
  COPY ibans_copy FROM '/tmp/iban_regress_bad.bin' (FORMAT binary);
EXCEPTION WHEN invalid_binary_representation THEN
  RAISE NOTICE 'binary input rejected with SQLSTATE %', SQLSTATE;
END
$$;
TRUNCATE texts;
INSERT INTO texts VALUES ('garbage', 'garbage');
COPY texts TO '/tmp/iban_regress_bad.bin' (FORMAT binary);
COPY ibans_copy FROM '/tmp/iban_regress_bad.bin' (FORMAT binary);
SELECT count(*) AS copied FROM ibans_copy;

--
-- Equality, ordering, DISTINCT, GROUP BY and indexes
--
SELECT oprname, oprcanhash, oprcanmerge FROM pg_operator
  WHERE oprleft = 'iban'::regtype AND oprright = 'iban'::regtype ORDER BY oprname COLLATE "C";
SELECT opcname, amname, opcdefault FROM pg_opclass c JOIN pg_am a ON a.oid = c.opcmethod
  WHERE opcintype = 'iban'::regtype ORDER BY amname;
SELECT 'DE89370400440532013000'::iban < 'NL91ABNA0417164300'::iban AS lt,
       'DE89370400440532013000'::iban <= 'DE89370400440532013000'::iban AS le,
       'DE89370400440532013000'::iban = 'DE89370400440532013000'::iban AS eq,
       'DE89370400440532013000'::iban <> 'NL91ABNA0417164300'::iban AS ne,
       'NL91ABNA0417164300'::iban >= 'DE89370400440532013000'::iban AS ge,
       'NL91ABNA0417164300'::iban > 'DE89370400440532013000'::iban AS gt,
       iban_cmp('BE68539007547034', 'BE68539007547034') AS cmp_eq,
       iban_cmp('BE68539007547034', 'CH9300762011623852957') AS cmp_lt,
       iban_cmp('CH9300762011623852957', 'BE68539007547034') AS cmp_gt;
-- byte order: uppercase sorts before lowercase, shorter prefix first
SELECT account FROM ibans ORDER BY account;
SELECT account FROM ibans ORDER BY account DESC LIMIT 3;
SELECT DISTINCT account FROM ibans WHERE name IN ('BE', 'NL', 'dup', 'lower') ORDER BY 1;
SELECT account, count(*) FROM ibans GROUP BY account HAVING count(*) > 1 ORDER BY account;
SET enable_sort = off;
SELECT count(*) AS groups FROM (SELECT account FROM ibans GROUP BY account) s;
RESET enable_sort;
SELECT iban_hash('NL91ABNA0417164300') = iban_hash('NL91ABNA0417164300'::text::iban) AS hash_stable,
       iban_hash_extended('NL91ABNA0417164300', 0) = iban_hash_extended('NL91ABNA0417164300', 0) AS hash_ext_stable,
       iban_hash('NL91ABNA0417164300') = iban_hash('DE89370400440532013000') AS hash_collide;
SET enable_seqscan = off;
SET enable_bitmapscan = off;
CREATE INDEX ibans_account_hash ON ibans USING hash (account);
EXPLAIN (COSTS OFF) SELECT name FROM ibans WHERE account = 'NL91ABNA0417164300';
SELECT name FROM ibans WHERE account = 'NL91ABNA0417164300' ORDER BY name COLLATE "C";
DROP INDEX ibans_account_hash;
CREATE INDEX ibans_account_btree ON ibans (account);
EXPLAIN (COSTS OFF) SELECT name FROM ibans WHERE account > 'IT60X0542811101000000123456' ORDER BY account;
SELECT account FROM ibans WHERE account > 'IT60X0542811101000000123456' ORDER BY account;
RESET enable_seqscan;
RESET enable_bitmapscan;
-- unique constraint and join on iban
CREATE TABLE unique_ibans (account iban PRIMARY KEY);
INSERT INTO unique_ibans SELECT DISTINCT account FROM ibans;
INSERT INTO unique_ibans VALUES ('NL91ABNA0417164300');
SELECT count(*) AS joined FROM ibans JOIN unique_ibans USING (account);

DROP TABLE fixtures, ibans, ibans_copy, texts, unique_ibans;
DROP EXTENSION iban;
