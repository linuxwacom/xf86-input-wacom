
if HAVE_DOXYGEN

# Developers documentation is not installed
noinst_DATA = html/index.html
dist_noinst_DATA = doxygen.conf.in

html/index.html:
	$(AM_V_GEN)$(DOXYGEN) doxygen.conf

clean-local:
	$(AM_V_at)rm -fr html/

endif HAVE_DOXYGEN
