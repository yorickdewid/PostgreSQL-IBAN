INCLUDEDIRS := -I.
INCLUDEDIRS += -I$(shell pg_config --includedir-server)
INCLUDEDIRS += -I$(shell pg_config --includedir)
LIBDIR = -L$(shell pg_config --libdir)
# This is where the shared object should be installed
LIBINSTALL = $(shell pg_config --pkglibdir)
CFLAGS += -std=c++14 -fpic

all: iban.so

iban.so: iban.cc
			$(CC) $(CFLAGS) -o iban.o -c iban.cc $(INCLUDEDIRS)
			$(CC) -shared -o iban.so iban.o $(LIBDIR) -lpq -lm
			# cp iban.so $(LIBINSTALL)
