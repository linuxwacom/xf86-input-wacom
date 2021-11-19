CLEANFILES=

if HAS_XORG_CONF_DIR
dist_config_DATA = 70-wacom.conf
else
fdipolicydir = $(datadir)/hal/fdi/policy/20thirdparty
dist_fdipolicy_DATA = wacom.fdi
endif

if USE_HAL_FDI_PREPROBE_QUIRK
fdipreprobedir = $(datadir)/hal/fdi/preprobe/20thirdparty
dist_fdipreprobe_DATA = wacom-preprobe.fdi
endif

if HAVE_SYSTEMD_UNIT_DIR
unitdir = $(SYSTEMD_UNIT_DIR)
unit_DATA = wacom-inputattach@.service

$(unit_DATA): $(unit_DATA).in
	$(AM_V_GEN)$(SED) -e 's|__BIN_PREFIX__|$(bindir)|g' < $< > $@

CLEANFILES += wacom-inputattach@.service
endif

if HAVE_UDEV_RULES_DIR
udevdir=$(UDEV_RULES_DIR)
udev_DATA = wacom.rules
endif

EXTRA_DIST = wacom.rules wacom-inputattach@.service.in
