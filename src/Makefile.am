SUBDIRS = . wacomxi util xdrv @WCM_KERNEL_VER@

wmanpagedir = @mandir@/man4
wmanpage_HEADERS = wacom.4x.gz

EXTRA_DIST = wacom.4x

$(wmanpage_HEADERS): $(EXTRA_DIST)
	rm -f $(wmanpage_HEADERS)
	gzip -9c < $(srcdir)/$(EXTRA_DIST) > $(wmanpage_HEADERS)
