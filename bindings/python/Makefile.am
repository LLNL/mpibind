.NOTPARALLEL:

SUBDIRS= _mpibind mpibind

installable:
	$(PYTHON) $(srcdir)/setup.py \
	sdist \
	bdist_wheel

install-exec-local:
	(cd $(srcdir); $(PYTHON) setup.py sdist bdist_wheel --verbose)

clean-local:
	-rm -f *.pyc *.pyo
	-rm -rf __pycache__
	-rm -rf mpibind.egg-info
	-rm -rf build