################################################################################
#
# aml_tvserver
#
################################################################################
DBUS_LIB_DIR = $(STAGING_DIR)/usr/lib
DBUS_HEADER_DIR = $(STAGING_DIR)/usr/include/dbus-1.0
LOCAL_PATH = $(shell pwd)
LDFLAGS += -lstdc++ -lpthread -lz -ldl -L$(DBUS_LIB_DIR) -ldbus-1
CFLAGS += -Wall -Wno-unknown-pragmas -Wno-format \
          -O3 -fexceptions -fnon-call-exceptions -D_GNU_SOURCE -I$(DBUS_HEADER_DIR)
################################################################################
# libtv.so - src files
################################################################################
tv_SRCS  = \
	$(LOCAL_PATH)/libtv/tvutils/CFile.cpp \
	$(LOCAL_PATH)/libtv/tvutils/ConfigFile.cpp \
	$(LOCAL_PATH)/libtv/tvutils/CSerialPort.cpp \
	$(LOCAL_PATH)/libtv/tvutils/TvConfigManager.cpp \
	$(LOCAL_PATH)/libtv/tvutils/tvutils.cpp \
	$(LOCAL_PATH)/libtv/tvutils/zepoll.cpp \
	$(LOCAL_PATH)/libtv/CHDMIRxManager.cpp \
	$(LOCAL_PATH)/libtv/CTv.cpp \
	$(LOCAL_PATH)/libtv/CTvin.cpp \
	$(LOCAL_PATH)/libtv/CTvDevicesPollDetect.cpp \
	$(LOCAL_PATH)/libtv/CTvEvent.cpp \
	$(NULL)

################################################################################
# libtvclient.so - src files
################################################################################
tvclient_SRCS  = \
	$(LOCAL_PATH)/client/TvClient.cpp \
	$(NULL)

tvclient_HEADERS = \
	$(LOCAL_PATH)/client/include \
	$(NULL)
################################################################################
# tvservice - src files
################################################################################
tvservice_SRCS  = \
	$(LOCAL_PATH)/service/main_tvservice.cpp \
	$(LOCAL_PATH)/service/TvService.cpp \
	$(NULL)

################################################################################
# tvtest - src files
################################################################################
tvtest_SRCS  = \
	$(LOCAL_PATH)/test/main_tvtest.cpp \
	$(NULL)

# ---------------------------------------------------------------------
#  Build rules
BUILD_TARGETS = libtvclient.so libtv.so tvservice tvtest

.PHONY: all install uninstall clean

libtvclient.so: $(tvclient_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -I$(tvclient_HEADERS) -o $@ $^ $(LDLIBS)

libtv.so: $(tv_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -I$(LOCAL_PATH)/libtv/tvutils -o $@ $^ $(LDLIBS)

tvservice: $(tvservice_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(tvclient_HEADERS) -I$(LOCAL_PATH)/libtv -I$(LOCAL_PATH)/libtv/tvutils -L$(LOCAL_PATH) -ltv -o $@ $^ $(LDLIBS)

tvtest: $(tvtest_SRCS)
	$(CC) $(CFLAGS) -I$(tvclient_HEADERS) -L$(LOCAL_PATH) -ltvclient $(LDFLAGS) -o $@ $^ $(LDLIBS)

all: $(BUILD_TARGETS)

clean:
	rm -f *.o $(BUILD_TARGETS)

install:
	install -m 0644 libtvclient.so $(TARGET_DIR)/usr/lib
	install -m 0644 libtv.so $(TARGET_DIR)/usr/lib/
	install -m 755 tvservice $(TARGET_DIR)/usr/bin/
	install -m 755 tvtest $(TARGET_DIR)/usr/bin/

uninstall:
	rm -f $(TARGET_DIR)/usr/lib/libtvclient.so
	rm -f $(TARGET_DIR)/usr/lib/libtv.so
	rm -f $(TARGET_DIR)/usr/bin/tvtest
	rm -f $(TARGET_DIR)/usr/bin/tvservice