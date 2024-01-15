# contrib/hocotext/Makefile

MODULE_big = hocotext
OBJS = \
	$(WIN32RES) \
	hocotext.o \
	hocotext_rle.o\
	hocotext_lz4.o

EXTENSION = hocotext
DATA =  hocotext--1.0.sql
PGFILEDESC = "hocotext - Homomorphic Compression Schemes data type"

REGRESS = hocotext hocotext_utf8

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/hocotext
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif