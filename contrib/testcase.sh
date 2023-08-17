#!/bin/bash

psql -c "SELECT * FROM pg_available_extensions WHERE name = 'iban';"

psql -c "CREATE EXTENSION iban;"

psql -c "CREATE TABLE test_iban (name text, account iban);"

psql -c "INSERT INTO test_iban (name, account) VALUES ('John', 'NL91ABNA0417164300');"
if [ $? -ne 0 ]; then
    echo "ERROR: Inserting IBAN failed"
    exit 1
fi
psql -c "INSERT INTO test_iban (name, account) VALUES ('Doe', 'DE89370400440532013000');"
if [ $? -ne 0 ]; then
    echo "ERROR: Inserting IBAN failed"
    exit 1
fi

psql -c "SELECT 'KZ86125KZT5004100100'::iban;"
