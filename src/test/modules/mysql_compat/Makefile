# src/test/modules/mysql_compat/Makefile

REGRESS = mysql_compat
PGFILEDESC = "mysql_compat - MySQL-compatible SHOW statements"

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = src/test/modules/mysql_compat
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
