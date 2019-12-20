################################################################################
#
# aml_tvserver
#
################################################################################
DBUS_LIB_DIR = $(STAGING_DIR)/usr/lib
DBUS_HEADER_DIR = $(STAGING_DIR)/usr/include/dbus-1.0
LOCAL_PATH = $(shell pwd)
LDFLAGS += -lstdc++ -lpthread -lz -ldl -lrt -L$(DBUS_LIB_DIR) -ldbus-1
CFLAGS += -Wall -Wno-unknown-pragmas -Wno-format \
          -O3 -fexceptions -fnon-call-exceptions -D_GNU_SOURCE -DHAVE_AUDIO -I$(DBUS_HEADER_DIR) \
		  -I$(DBUS_LIB_DIR)/dbus-1.0/include -I$(STAGING_DIR)/usr/include

################################################################################
# libpq.so - src files
################################################################################
CSV_RET=$(shell ($(LOCAL_PATH)/libpq/csvAnalyze.sh > /dev/zero;echo $$?))
ifeq ($(CSV_RET), 1)
  $(error "Csv file or common.h file is not exist!!!!")
else ifeq ($(CSV_RET), 2)
  $(error "Csv file's Id must be integer")
else ifeq ($(CSV_RET), 3)
  $(error "Csv file's Size must be integer or defined in common.h")
endif

pq_SRCS = \
  $(LOCAL_PATH)/libpq/CPQdb.cpp \
  $(LOCAL_PATH)/libpq/COverScandb.cpp \
  $(LOCAL_PATH)/libpq/CPQControl.cpp  \
  $(LOCAL_PATH)/libpq/CSqlite.cpp  \
  $(LOCAL_PATH)/libpq/SSMAction.cpp  \
  $(LOCAL_PATH)/libpq/SSMHandler.cpp  \
  $(LOCAL_PATH)/libpq/SSMHeader.cpp  \
  $(LOCAL_PATH)/libpq/CDevicePollCheckThread.cpp  \
  $(LOCAL_PATH)/libpq/CPQColorData.cpp  \
  $(LOCAL_PATH)/libpq/CPQLog.cpp  \
  $(LOCAL_PATH)/libpq/CPQFile.cpp  \
  $(LOCAL_PATH)/libpq/CEpoll.cpp \
  $(LOCAL_PATH)/libpq/CDynamicBackLight.cpp \
  $(LOCAL_PATH)/libpq/CConfigFile.cpp \
  $(NULL)
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
	$(LOCAL_PATH)/libtv/tvutils/CTvLog.cpp \
	$(LOCAL_PATH)/libtv/CHDMIRxManager.cpp \
	$(LOCAL_PATH)/libtv/CTv.cpp \
	$(LOCAL_PATH)/libtv/CTvin.cpp \
	$(LOCAL_PATH)/libtv/CTvDevicesPollDetect.cpp \
	$(LOCAL_PATH)/libtv/CTvAudio.cpp \
	$(NULL)

################################################################################
# libtvclient.so - src files
################################################################################
tvclient_SRCS  = \
	$(LOCAL_PATH)/client/TvClient.cpp \
	$(LOCAL_PATH)/client/CTvClientLog.cpp \
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
BUILD_TARGETS = libtvclient.so libtv.so libpq.so tvservice tvtest

.PHONY: all install uninstall clean

libtvclient.so: $(tvclient_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -I$(tvclient_HEADERS) \
	-o $@ $^ $(LDLIBS)

libpq.so: $(pq_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -lsqlite3 -I$(tvclient_HEADERS) \
	-o $@ $^ $(LDLIBS)

libtv.so: $(tv_SRCS) libpq.so
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -I$(tvclient_HEADERS) -I$(LOCAL_PATH)/libpq \
	-I$(LOCAL_PATH)/libtv/tvutils -L$(LOCAL_PATH) -lpq -laudio_client -o $@ $^ $(LDLIBS)

tvservice: $(tvservice_SRCS) libtv.so libpq.so
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(tvclient_HEADERS) \
	-I$(LOCAL_PATH)/libtv -I$(LOCAL_PATH)/libtv/tvutils -I$(LOCAL_PATH)/libpq \
	-L$(LOCAL_PATH) -ltv -L$(LOCAL_PATH) -lpq -laudio_client -o $@ $^ $(LDLIBS)

tvtest: $(tvtest_SRCS) libtvclient.so
	$(CC) $(CFLAGS) -I$(tvclient_HEADERS) -L$(LOCAL_PATH) \
	-ltvclient $(LDFLAGS) -o $@ $^ $(LDLIBS)

all: $(BUILD_TARGETS)

clean:
	rm -f *.o $(BUILD_TARGETS)

install:
	install -m 0644 libtvclient.so $(TARGET_DIR)/usr/lib
	install -m 0644 libtv.so $(TARGET_DIR)/usr/lib/
	install -m 0644 libpq.so $(TARGET_DIR)/usr/lib/
	install -m 755 tvservice $(TARGET_DIR)/usr/bin/
	install -m 755 tvtest $(TARGET_DIR)/usr/bin/

uninstall:
	rm -f $(TARGET_DIR)/usr/lib/libtvclient.so
	rm -f $(TARGET_DIR)/usr/lib/libtv.so
	rm -f $(TARGET_DIR)/usr/lib/libpq.so
	rm -f $(TARGET_DIR)/usr/bin/tvtest
	rm -f $(TARGET_DIR)/usr/bin/tvservice
