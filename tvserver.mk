TVSERVER_PROJDIR  := $(AML_COMMON_DIR)/tvserver
TVSERVER_OSS_LIC  := CLOSED
TVSERVER_BINARIES := usr/lib/libtvclient.so usr/lib/libpq.so  usr/lib/libtv.so usr/bin/tvservice usr/bin/tvtest
TVSERVER_ADD_SRC  := $(ROKU_OS_INCLUDE_DIRS)

$(eval $(call prep_proj,TVSERVER,tvserver,$(TVSERVER_PROJDIR),$(TVSERVER_ADD_SRC)))

LDFLAGS = $(addprefix -L, $(ROKU_3PARTY_LIB_DIRS))
TVSERVER_MAKE_OPTIONS = \
	CC=$(CC) \
	CXX=$(CXX) \
	OUT_ROOT=$(TVSERVER_MAKEDIR) \
	PORTING_KIT_OS=$(PORTING_KIT_OS) \
	LDFLAGS="$(LDFLAGS)" \
	PLATFORMDIR=$(PLATFORMDIR) \
	AML_COMMON_DIR=$(AML_COMMON_DIR) \
	ROKU_PORT_COMMON_DIR=$(ROKU_PORT_COMMON_DIR) \
	ROKU_PORT_DIR=$(ROKU_PORT_DIR) \
	ROKU_3PARTY_INCLUDE_DIRS=$(ROKU_3PARTY_INCLUDE_DIRS) \
	ROKU_STAGING_DIR=$(ROKU_STAGING_DIR) \
	CROSSTOOLS_SYSROOT_PATH=$(CROSSTOOLS_SYSROOT_PATH) \
	TV_MODEL=$(TV_MODEL)

tvserver-configure:
tvserver-build:
	(PATH=$(CROSSTOOLS_BIN):$(PATH) \
	$(MAKE) -f Makefile.roku -C $(TVSERVER_PROJDIR) $(TVSERVER_MAKE_OPTIONS) all)

tvserver-install:
	$(MAKE) -f Makefile.roku -C $(TVSERVER_PROJDIR) DESTDIR=$(TVSERVER_INSTDIR) $(TVSERVER_MAKE_OPTIONS) DESTDIR=$(ROKU_STAGING_DIR)/$(TVSERVER_DESTDIR) install

tvserver-clean:
	$(MAKE) -f Makefile.roku -C $(TVSERVER_PROJDIR) $(TVSERVER_MAKE_OPTIONS) clean || true
	$(MAKE) -f Makefile.roku -C $(TVSERVER_PROJDIR) $(TVSERVER_MAKE_OPTIONS) DESTDIR=$(ROKU_STAGING_DIR)/$(TVSERVER_DESTDIR) distclean || true
