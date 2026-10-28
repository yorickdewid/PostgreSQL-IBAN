![C/C++ CI](https://github.com/yorickdewid/PostgreSQL-IBAN/workflows/C/C++%20CI/badge.svg)

# PostgreSQL IBAN
PostgreSQL IBAN extension that can verify International Bank Account Numbers.
This ensures that only valid bank account numbers are stored.

Supported PostgreSQL versions: 14, 15, 16, 17 and 18.

### Example
```sql
CREATE TABLE test_iban (
  name text,
  account iban
);

--
-- Insert data
--

-- Dutch IBAN format
INSERT INTO test_iban (name, account) VALUES ('John', 'NL91ABNA0417164300');
-- German IBAN format
INSERT INTO test_iban (name, account) VALUES ('Doe', 'DE89370400440532013000');
-- Moroccan IBAN format
INSERT INTO test_iban (name, account) VALUES ('Jane', 'MA64011519000001205000534921');

-- Invalid data
INSERT INTO test_iban (name, account) VALUES ('Dean', 'AZ22NABZ00000000137010001944');

--
-- Show output
--

SELECT * FROM test_iban ORDER BY account;

```

Manually test input
```sql
SELECT iban_validate('KW81CBKU0000000000001234560101');
-- Or cast
SELECT 'KZ86125KZT5004100100'::iban;
```

the `::iban` datatype can be cast to an text to perform string operations
```sql
SELECT 'KZ86125KZT5004100100'::iban::text;
```

### Input rules

* Values are stored in canonical form: spaces are removed and letters are upper-cased. So
  `NL91 ABNA 0417 1643 00` (print format), `nl91abna0417164300` and `NL91ABNA0417164300` are
  the same value, and all three are stored as `NL91ABNA0417164300`. Only the plain space is
  dropped (not tabs, newlines or other whitespace); input longer than 64 bytes is rejected.
* Rows stored by an older version may still contain lowercase values. Normalise them with
  `UPDATE t SET account = account::text::iban;`
* The check digits (positions 3 and 4) must be digits.
* Binary input (`COPY ... (FORMAT binary)`) is validated and normalised the same way as text input.
* An error message repeats at most the first 40 bytes of the rejected value.

### Functions and operators

| Name | Description |
| --- | --- |
| `iban` | Data type, rejects invalid IBANs on input |
| `iban_validate(text) → boolean` | Returns whether the text is a valid IBAN |
| `=`, `<>`, `<`, `<=`, `>`, `>=` | Comparison of two `iban` values (byte order) |
| `iban_cmp(iban, iban) → integer` | Comparison support function for btree |
| `iban_hash(iban)`, `iban_hash_extended(iban, bigint)` | Hash support functions |

Default btree and hash operator classes are provided, so `iban` columns can be used
in `ORDER BY`, `DISTINCT`, `GROUP BY`, joins, primary keys and indexes.

## Build

Make sure PostgreSQL development tools are installed (check `pg_config`). A C++17
compiler is required.

On Debian based systems:
```sh
sudo apt install postgresql-server-dev-all
```

```bash
git clone https://github.com/yorickdewid/PostgreSQL-IBAN
cd PostgreSQL-IBAN
make
sudo make install
```

Run the regression tests against a running server (as a user that may create
extensions):

```bash
make installcheck
```

Login to the database and load the extension

```sql
CREATE EXTENSION iban;
```

An existing installation of version 1.1 is upgraded with

```sql
ALTER EXTENSION iban UPDATE;
```
