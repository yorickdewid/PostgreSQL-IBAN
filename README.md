# PostgreSQL-IBAN
PostgreSQL IBAN extension that can verify International Bank Account Numbers.
This ensures that only valid bank account numbers are stored. 

### Example
```sql
CREATE TABLE test_iban (
  id serial NOT NULL,
  name character varying(32),
  account iban,
  CONSTRAINT test_iban_pkey PRIMARY KEY (id)
)

-- Insert data
INSERT INTO test_iban (name, account) VALUES ('John', 'NL91ABNA0417164300'); -- Dutch IBAN format
INSERT INTO test_iban (name, account) VALUES ('Doe', 'DE89370400440532013000'); -- German IBAN format

-- Invalid bank account
INSERT INTO test_iban (name, account) VALUES ('Dean', 'AZ22NABZ00000000137010001944');

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
