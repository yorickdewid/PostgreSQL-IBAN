MODULES = iban

EXTENSION = iban
DATA = iban--1.1.sql
PGFILEDESC = "iban - IBAN datatype and functions"

PG_CXXFLAGS = -std=c++14 -fPIC

PG_LDFLAGS = -lstdc++

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
