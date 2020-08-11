mpibindpy_PYTHON=\
	      __init__.py\
		  wrapper.py

clean-local:
	-rm -f *.pyc *.pyo
	-rm -rf __pycache__

install-data-hook:
	$(AM_V_at)echo Linking python modules in non-standard location... && \
	  $(INSTALL) -d -m 0755 "$(DESTDIR)$(mpibindpylinkdir)" && \
	  target=$(mpibindpydir) && \
	  f=$${target##*/} && \
	  cd "$(DESTDIR)$(mpibindpylinkdir)" && \
	  rm -f $$f && \
	  $(LN_S) $$target .

uninstall-local:
	$(AM_V_at)target=$(mpibindpydir) && f=$${target##*/} && \
	  echo "Removing $(mpibindpylinkdir)/$$f" && \
	  rm -rf $(mpibindpylinkdir)/$$f
