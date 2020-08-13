.NOTPARALLEL:

SUBDIRS= _mpibind mpibind

AM_CPPFLAGS = \
	-Wall -Werror -Wno-missing-field-initializers \
	-I$(top_srcdir) -I$(top_srcdir)/src/ -L$(top_srcdir)/src/.libs \
	$(PYTHON_CPPFLAGS)

all-local:
	(cd $(srcdir); CFLAGS="$(AM_CPPFLAGS)" \
		$(PYTHON) setup.py bdist_wheel)

clean-local:
	-rm -f *.pyc *.pyo
	-rm -rf __pycache__
	-rm -rf mpibind.egg-info
	-rm -rf build
	-rm -rf dist