--
-- Upgrade path from 1.1 to 1.2
--
\set VERBOSITY terse
\pset format unaligned
CREATE EXTENSION iban VERSION '1.1';
SELECT extversion FROM pg_extension WHERE extname = 'iban';
SELECT proname, provolatile, proparallel, proisstrict FROM pg_proc
  WHERE proname IN ('ibanin', 'ibanout', 'ibanrecv', 'ibansend', 'iban_validate') ORDER BY proname COLLATE "C";
-- data stored with 1.1 survives the upgrade; 1.1 cannot sort it
CREATE TABLE upgrade_ibans (account iban);
INSERT INTO upgrade_ibans VALUES ('NL91ABNA0417164300'), ('DE89370400440532013000'), ('NL91ABNA0417164300');
SELECT account FROM upgrade_ibans ORDER BY account;

ALTER EXTENSION iban UPDATE;
SELECT extversion FROM pg_extension WHERE extname = 'iban';
SELECT proname, provolatile, proparallel, proisstrict FROM pg_proc
  WHERE proname IN ('ibanin', 'ibanout', 'ibanrecv', 'ibansend', 'iban_validate') ORDER BY proname COLLATE "C";
SELECT account FROM upgrade_ibans ORDER BY account;
SELECT account, count(*) FROM upgrade_ibans GROUP BY account ORDER BY account;
CREATE UNIQUE INDEX upgrade_ibans_account ON upgrade_ibans (account);
DELETE FROM upgrade_ibans WHERE ctid = '(0,3)';
CREATE UNIQUE INDEX upgrade_ibans_account ON upgrade_ibans (account);

-- the upgraded extension has the same objects as a fresh 1.2 install
CREATE TEMP TABLE upgraded AS
  SELECT pg_describe_object(classid, objid, objsubid) AS object
  FROM pg_depend WHERE refclassid = 'pg_extension'::regclass
   AND refobjid = (SELECT oid FROM pg_extension WHERE extname = 'iban');
DROP TABLE upgrade_ibans;
DROP EXTENSION iban;
CREATE EXTENSION iban;
CREATE TEMP TABLE fresh AS
  SELECT pg_describe_object(classid, objid, objsubid) AS object
  FROM pg_depend WHERE refclassid = 'pg_extension'::regclass
   AND refobjid = (SELECT oid FROM pg_extension WHERE extname = 'iban');
-- PG 17+ also lists the implicit array type iban[] as a member; the diff below still covers it
SELECT object FROM fresh WHERE object <> 'type iban[]' ORDER BY object COLLATE "C";
(SELECT 'only upgraded' AS diff, object FROM upgraded EXCEPT SELECT 'only upgraded', object FROM fresh)
UNION ALL
(SELECT 'only fresh', object FROM fresh EXCEPT SELECT 'only fresh', object FROM upgraded);
SELECT proname, provolatile, proparallel, proisstrict FROM pg_proc
  WHERE proname IN ('ibanin', 'ibanout', 'ibanrecv', 'ibansend', 'iban_validate') ORDER BY proname COLLATE "C";
DROP EXTENSION iban;
