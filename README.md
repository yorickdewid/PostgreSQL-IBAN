![C/C++ CI](https://github.com/yorickdewid/PostgreSQL-IBAN/workflows/C/C++%20CI/badge.svg)

# PostgreSQL IBAN
PostgreSQL IBAN extension that can verify International Bank Account Numbers.
This ensures that only valid bank account numbers are stored. 

### Example
```sql
CREATE TABLE test_iban (
  name text,
  account iban
)

--
-- Insert data
--

-- Dutch IBAN format
INSERT INTO test_iban (name, account) VALUES ('John', 'NL91ABNA0417164300');
-- German IBAN format
INSERT INTO test_iban (name, account) VALUES ('Doe', 'DE89370400440532013000');

-- Invalid data
INSERT INTO test_iban (name, account) VALUES ('Dean', 'AZ22NABZ00000000137010001944');

--
-- Show output
--

SELECT * FROM test_iban;

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

## Build

Make sure PostgreSQL development tools are installed (check `pg_config`).

On Debian based systems:
```sh
sudo apt install postgresql-server-dev-all
```

```bash
git clone https://github.com/yorickdewid/PostgreSQL-IBAN
cd PostgreSQL-IBAN
sudo make install
```

Login to the database and load the extension

```sql
CREATE EXTENSION iban;
```
