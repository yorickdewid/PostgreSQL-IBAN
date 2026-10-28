-- Fuzz workload for the iban extension (run under ASan/UBSan). Errors from invalid input are expected and swallowed.
CREATE EXTENSION iban;
SET client_min_messages = warning;

CREATE FUNCTION rnd_str(maxlen int) RETURNS text LANGUAGE sql VOLATILE AS $$
  SELECT coalesce(string_agg((ARRAY['0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
     'a','b','n','l',' ',' ',' ','é','€','日', E'\t', E'\n', '.', '*', '%', 'NL','DE','GB','MA','XX','0','0','0'])[1 + floor(random() * 57)::int], ''), '')
  FROM generate_series(1, floor(random() * (maxlen + 1))::int)
$$;

CREATE FUNCTION mutate(s text) RETURNS text LANGUAGE plpgsql VOLATILE AS $$
DECLARE r int := floor(random() * 9)::int; p int := 1 + floor(random() * greatest(length(s), 1))::int;
BEGIN
  RETURN CASE r
    WHEN 0 THEN s
    WHEN 1 THEN lower(s)
    WHEN 2 THEN overlay(s placing ' ' from p for 0)
    WHEN 3 THEN left(s, p)
    WHEN 4 THEN s || rnd_str(5)
    WHEN 5 THEN overlay(s placing rnd_str(2) from p for 1)
    WHEN 6 THEN regexp_replace(s, '(.{4})', '\1 ', 'g')
    WHEN 7 THEN repeat(' ', p % 5) || s || repeat(' ', p % 7)
    ELSE substr(s, 1, p) || substr(s, p + 2) END;
END $$;

CREATE TABLE seeds AS SELECT unnest(ARRAY['NL91ABNA0417164300','DE89370400440532013000','GB29NWBK60161331926819','FR1420041010050500013M02606',
  'BE68539007547034','ES9121000418450200051332','IT60X0542811101000000123456','CH9300762011623852957','MA64011519000001205000534921',
  'KZ86125KZT5004100100','NL91 ABNA 0417 1643 00','nl91abna0417164300']) AS s;

\echo 1: random strings through iban_validate
SELECT count(*) FILTER (WHERE iban_validate(rnd_str(90))) AS random_valid FROM generate_series(1, 200000);
\echo 2: mutated valid IBANs through iban_validate
SELECT count(*) FILTER (WHERE iban_validate(mutate(s))) AS mutated_valid FROM seeds, generate_series(1, 20000);
\echo 3: every length 0..140 with random and mutated content
SELECT count(*) FILTER (WHERE iban_validate(rpad(rnd_str(len), len, 'A'))) AS padded_valid FROM generate_series(0, 140) len, generate_series(1, 300);
SELECT count(*) FILTER (WHERE iban_validate(repeat(' ', len) || 'NL91ABNA0417164300')) AS leading_spaces FROM generate_series(0, 70) len;
SELECT count(*) FILTER (WHERE iban_validate('NL91ABNA0417164300' || repeat(' ', len))) AS trailing_spaces FROM generate_series(0, 70) len;
\echo 4: casts and error messages (text input path, ereport with %.*s clipping)
DO $$
DECLARE i int; ok int := 0; bad int := 0; v text;
BEGIN
  FOR i IN 1..60000 LOOP
    v := CASE WHEN i % 3 = 0 THEN rnd_str(100) ELSE mutate((SELECT s FROM seeds ORDER BY random() LIMIT 1)) END;
    BEGIN PERFORM v::iban; ok := ok + 1; EXCEPTION WHEN OTHERS THEN bad := bad + 1; END;
  END LOOP;
  RAISE WARNING 'cast ok=% bad=%', ok, bad;
END $$;
\echo 5: huge inputs
SELECT iban_validate(repeat('NL91ABNA0417164300', 600000)) AS ten_mb;
DO $$ BEGIN PERFORM repeat('é', 3000000)::iban; EXCEPTION WHEN OTHERS THEN RAISE WARNING 'huge multibyte: %', left(SQLERRM, 80); END $$;
DO $$ BEGIN PERFORM repeat(' ', 5000000)::iban; EXCEPTION WHEN OTHERS THEN RAISE WARNING 'huge spaces: %', left(SQLERRM, 80); END $$;
\echo 6: binary receive (ibanrecv) one value at a time
CREATE TABLE one (v text);
CREATE TABLE sink (a iban);
DO $$
DECLARE i int; v text; ok int := 0; bad int := 0;
BEGIN
  FOR i IN 1..3000 LOOP
    v := CASE WHEN i % 3 = 0 THEN rnd_str(100) ELSE mutate((SELECT s FROM seeds ORDER BY random() LIMIT 1)) END;
    TRUNCATE one; INSERT INTO one VALUES (v);
    COPY one TO '/tmp/fz.bin' (FORMAT binary);
    BEGIN COPY sink FROM '/tmp/fz.bin' (FORMAT binary); ok := ok + 1; EXCEPTION WHEN OTHERS THEN bad := bad + 1; END;
  END LOOP;
  RAISE WARNING 'binary ok=% bad=%', ok, bad;
END $$;
\echo 7: ordering, hashing, indexes on normalised values
CREATE TABLE big (a iban);
INSERT INTO big SELECT s::iban FROM seeds;
DO $$ DECLARE i int; BEGIN FOR i IN 1..40000 LOOP BEGIN INSERT INTO big SELECT mutate((SELECT s FROM seeds ORDER BY random() LIMIT 1))::iban; EXCEPTION WHEN OTHERS THEN NULL; END; END LOOP; END $$;
SELECT count(*) AS rows, count(DISTINCT a) AS distinct_values FROM big;
CREATE INDEX ON big (a);
CREATE INDEX ON big USING hash (a);
SELECT a, count(*) FROM big GROUP BY a ORDER BY 2 DESC, 1 LIMIT 3;
SELECT count(*) FROM big b1 JOIN big b2 USING (a) WHERE b1.a < 'NL91ABNA0417164300';
\echo FUZZ FINISHED
