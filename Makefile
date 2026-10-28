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

# Release archive for PGXN (name and prefix must match META.json): make dist
DISTVERSION = $(shell sed -n 's/^ *"version": *"\([^"]*\)".*/\1/p' META.json | head -n 1)

.PHONY: dist
dist:
	git archive --format zip --prefix=$(EXTENSION)-$(DISTVERSION)/ -o $(EXTENSION)-$(DISTVERSION).zip HEAD
