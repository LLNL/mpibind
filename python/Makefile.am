#######################################################
# mpibind python bindings
#######################################################

# copy bindings, example, readme, and setup to the install tree
if HAVE_CFFI
data_DATA = mpibind.py mpibind_example.py setup.py README.md 

# version of mpibind used for testing, references the shared library in the build tree
mpibind.py: mpibind.py.in
	sed 's|\@mpibindlib\@|$(abs_top_srcdir)/src/.libs/libmpibind.so|g' mpibind.py.in > $@

# after installation modify mpibind.py to point to libmpibind in the install tree
install-data-hook:
	sed -i 's|$(abs_top_srcdir)/src/.libs/libmpibind.so|$(libdir)/libmpibind.so|g' $(datadir)/mpibind.py
	
CLEANFILES = mpibind.py 

endif
