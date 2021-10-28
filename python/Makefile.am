#######################################################
# mpibind python bindings
#######################################################

# I wish there was a better way to determine the type of
# library libtool creates. For now, use .dylib for MacOS
# and .so elsewhere. 
if HAVE_DARWIN_OS
mpibind_lib_ext = .dylib
else
mpibind_lib_ext = .so
endif
mpibind_lib_name = libmpibind$(mpibind_lib_ext)

# Copy bindings, example, readme, and setup to the install tree
if HAVE_CFFI
data_DATA  = mpibind.py setup.py
data_DATA += mpibind_map.py test-simple.py test-mpi.py

# Version of mpibind used for testing,
# references the shared library in the build tree
mpibind.py: mpibind.py.in
	sed 's|\@mpibindlib\@|$(abs_top_srcdir)/src/.libs/$(mpibind_lib_name)|g' \
	mpibind.py.in > $@

# After installation modify mpibind.py to point to libmpibind in the install tree
install-data-hook:
	sed -i.tmp 's|$(abs_top_srcdir)/src/.libs/$(mpibind_lib_name)|$(libdir)/$(mpibind_lib_name)|g' \
	$(datadir)/mpibind.py && \
	rm $(datadir)/mpibind.py.tmp

CLEANFILES = mpibind.py 

endif
