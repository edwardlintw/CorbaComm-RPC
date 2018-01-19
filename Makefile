AUTOGEN=corbaComm.hh corbaCommSK.cc
COMMON_OBJ=corbaComm.o corbaComm_impl.o notify_impl.o provider.o corbaCommSK.o

UNAME = $(shell uname -s)

CC=g++ -c -fPIC -O2 -std=c++14 -DNDEBUG  -Wall -Wno-unused -fexceptions -D__OMNIORB4__ -D_REENTRANT -I/usr/local/include -I/usr/local/include/COS -I. 

ifeq ($(UNAME), Linux)
	TARGET = libcorbaComm.so.1.0
	CC += -D__OSVERSION__=2 -D__linux__
	LD = g++ -shared -Wl,-soname,libcorbaComm.so.1 
else ifeq ($(UNAME), Darwin)
	TARGET = libcorbaComm.1.0.dylib
	CC += -D__OSVERSION__=1 -D__darwin__ -D__x86__
	LD = g++ -dynamiclib -undefined suppress -flat_namespace 
endif

LD += -o $(TARGET) -O2 -std=c++14 -DNDEBUG -Wall -Wno-unused -fexceptions -L/usr/local/lib $^ -lCOSNotify4 -lAttNotification4 -lCOS4 -lCOSDynamic4 -lomniORB4 -lomniDynamic4 -lomniZIOP4 -lomnithread -lpthread

all: corbaComm.hh $(TARGET)

$(TARGET): $(COMMON_OBJ)
	$(LD)

%.o: %.cc
	$(CC) $<

corbaComm.hh: corbaComm.idl
	omniidl -bcxx -I. -C. $<
	$(CC) corbaCommSK.cc

install:
	rm -f /usr/local/lib/libcorbaComm* > /dev/null 2>&1
	rm -f /usr/local/include/corbaComm/cos.h > /dev/null 2>&1
	rm -f /usr/local/include/corbaComm/corbaComm.h > /dev/null 2>&1
	rm -f /usr/local/include/corbaComm/notify_impl.h > /dev/null 2>&1
	rm -f /usr/local/include/corbaComm/corbaComm_impl.h > /dev/null 2>&1
	rm -f /usr/local/include/corbaComm/provider.h > /dev/null 2>&1
	mkdir -p /usr/local/include/corbaComm
	install -m 644 -p cos.h corbaComm.h notify_impl.h corbaComm_impl.h provider.h /usr/local/include/corbaComm
	install -m 755 -p $(TARGET) /usr/local/lib
ifeq ($(UNAME), Linux)
	ln -s /usr/local/lib/libcorbaComm.so.1.0 /usr/local/lib/libcorbaComm.so.1
	ln -s /usr/local/lib/libcorbaComm.so.1 /usr/local/lib/libcorbaComm.so
endif
ifeq ($(UNAME), Darwin)
	ln -s /usr/local/lib/libcorbaComm.1.0.dylib /usr/local/lib/libcorbaComm.1.dylib
	ln -s /usr/local/lib/libcorbaComm.1.dylib /usr/local/lib/libcorbaComm.dylib
endif

clean: 
	rm -rf *.o *.d $(AUTOGEN) libcorbaComm* > /dev/null 2>&1
