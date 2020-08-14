AM_CPPFLAGS = \
	-Wall -Werror -Wno-missing-field-initializers \
	-I$(top_srcdir) -I$(top_srcdir)/src/ \
	$(PYTHON_CPPFLAGS)

AM_LDFLAGS = \
	-avoid-version -module -Wl,-rpath,$(PYTHON_PREFIX)/lib

_pympibind.c: $(srcdir)/build.py _mpibind_preproc.h
	$(PYTHON) $<

# CREDIT TO THE FLUX TEAM FOR prepoc rule
_mpibind_preproc.h: _mpibind_clean.h
	$(CC) -E  '-D__attribute__(...)=' _mpibind_clean.h\
	  | sed -e '/^# [0-9]*.*/d' > $@
	  
_mpibind_clean.h: Makefile
	$(PYTHON) $(srcdir)/clean_header.py \
	$(top_builddir)/src/mpibind.h \
	_mpibind_clean.h 

BUILT_SOURCES= _pympibind.c _mpibind_preproc.h
mpibindpyso_LTLIBRARIES = _pympibind.la
mpibindpyso_PYTHON = __init__.py

nodist_mpibindbindinginclude_HEADERS = _mpibind_preproc.h
nodist__pympibind_la_SOURCES = _pympibind.c

_pympibind_la_LIBADD = $(top_builddir)/src/libmpibind.la \
		  $(PYTHON_LDFLAGS)

EXTRA_DIST=build.py make_clean_header.py

clean-local:
	-rm -f *.c *.so *.pyc *.pyo *_clean.h *_preproc.h
	-rm -rf __pycache__
	-rm -rf .libs

# creates a symbolic link _pympibind.so to 
# _pympibind.so in _mpibind/.libs so that it can be imported in mpibind/wrapper
# CREDIT TO THE FLUX TEAM FOR lib-copy

.PHONY: lib-copy

lib-copy: ${mpibindpyso_PYTHON} ${mpibindpyso_LTLIBRARIES}
	-echo Copying libraries to where they can be used by python in-tree
	for LIB in ${mpibindpyso_LTLIBRARIES:la=so} ; do \
		test -e .libs/$$LIB && \
		$(LN_S) .libs/$$LIB ./ || true; \
	done

all-local: lib-copy