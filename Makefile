MODULES = iban

EXTENSION = iban
DATA = iban--1.1.sql iban--1.2.sql iban--1.1--1.2.sql
PGFILEDESC = "iban - IBAN datatype and functions"

REGRESS = iban iban_upgrade

PG_CXXFLAGS = -std=c++17 -fPIC

PG_LDFLAGS = -lstdc++

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
