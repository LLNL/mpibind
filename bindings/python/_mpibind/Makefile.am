

AM_CPPFLAGS = \
	-Wall -Werror -Wno-missing-field-initializers \
	-I$(top_srcdir) -I$(top_srcdir)/src/ \
	$(PYTHON_CPPFLAGS)

AM_LDFLAGS = \
	-avoid-version -module -Wl,-rpath,$(PYTHON_PREFIX)/lib

PREPROC_FLAGS=-E  '-D__attribute__(...)='

_pympibind.c: $(srcdir)/build.py _mpibind_preproc.h
	$(PYTHON) $^

_mpibind_clean.h: Makefile
	$(PYTHON) $(srcdir)/clean_header.py \
	$(top_builddir)/src/mpibind.h \
	_mpibind_clean.h 

# _mpibind_clean.h: Makefile
# 	$(PYTHON) $(srcdir)/make_clean_header.py \
# 	  --path $(top_srcdir) \
# 	  --search $(top_builddir)/src/ \
# 	  --output _mpibind_clean.h \
# 	  src/mpibind.h
	  
_mpibind_preproc.h: _mpibind_clean.h
	$(CC) $(PREPROC_FLAGS) _mpibind_clean.h\
	  | sed -e '/^# [0-9]*.*/d' > $@

BUILT_SOURCES= _pympibind.c _mpibind_preproc.h
mpibindpyso_LTLIBRARIES = _pympibind.la
mpibindpyso_PYTHON = __init__.py

nodist_mpibindbindinginclude_HEADERS = _mpibind_preproc.h
nodist__pympibind_la_SOURCES = _pympibind.c


common_libs = $(top_builddir)/src/libmpibind.la \
		  $(PYTHON_LDFLAGS) \
		  $(HWLOC_LIBS)

_pympibind_la_LIBADD = $(common_libs)

EXTRA_DIST=build.py make_clean_header.py

.PHONY: lib-copy

lib-copy-vpath: ${mpibindpyso_PYTHON} ${mpibindpyso_LTLIBRARIES}
	-echo Copying libraries to where they can be used by python in-tree
	[ "$(top_srcdir)" != "$(top_builddir)" ] && cp $(top_srcdir)/bindings/python/_mpibind/__init__.py ./; \
	for LIB in ${mpibindpyso_LTLIBRARIES:la=so} ; do \
		test -e .libs/$$LIB && \
		$(LN_S) .libs/$$LIB ./ || true; \
	done

all-local: lib-copy-vpath

clean-local:
	-rm -f *.c *.so *.pyc *.pyo *_clean.h *_preproc.h
	-rm -rf __pycache__
	-rm -rf .libs