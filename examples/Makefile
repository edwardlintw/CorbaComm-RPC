TARGETS=rpcServer rpcClient publisher subscriber

UNAME = $(shell uname -s)

CC=g++ -c -O2 -std=c++11 -DNDEBUG  -Wall -Wno-unused -fexceptions -D__OMNIORB4__ -D_REENTRANT -I/usr/local/include -I/usr/local/include/COS -I. 

LD=g++ -o $@ -O2 -std=c++11 -DNDEBUG -Wall -Wno-unused -fexceptions -L/usr/local/lib $^ -lpthread -lcorbaComm

ifeq ($(UNAME), Linux)
	CC += -D__OSVERSION__=2 -D__linux__ 
endif
ifeq ($(UNAME), Darwin)
	CC += -D__OSVERSION__=1 -D__darwin__ -D__x86__
endif

CC += $<

all: $(TARGETS)

rpcServer: rpcServer.o
	$(LD)

rpcClient: rpcClient.o
	$(LD)

publisher: publisher.o
	$(LD)

subscriber: subscriber.o
	$(LD)

%.o: %.cc
	$(CC)

clean: 
	rm -rf *.o *.d $(TARGETS) > /dev/null 2>&1
