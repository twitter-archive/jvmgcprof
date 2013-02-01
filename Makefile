CC=gcc
OS=$(shell uname -s | tr '[A-Z]' '[a-z]')

ifeq ("$(OS)", "darwin")
JAVA_HOME?=$(shell /usr/libexec/java_home)
JAVA_HEADERS?=/System/Library/Frameworks/JavaVM.framework/Versions/A/Headers
#JAVA_HEADERS?=/Developer/SDKs/MacOSX10.6.sdk/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Headers/
PLATFORM_LDFLAGS=-mimpure-text
endif

ifeq ("$(OS)", "linux")
JAVA_HOME?=/usr/java/default/
JAVA_HEADERS=$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
endif

CFLAGS=-Ijava_crw_demo -fno-strict-aliasing                                  \
        -fPIC -fno-omit-frame-pointer -W -Wall  -Wno-unused -Wno-parentheses \
        -I$(JAVA_HEADERS) -Iinclude
LDFLAGS=-fno-strict-aliasing -fPIC -fno-omit-frame-pointer \
        -static-libgcc -shared $(PLATFORM_LDFLAGS)

all: libgcprof.jnilib GcProf.class

libgcprof.jnilib: gcprof.o u.o java_crw_demo/java_crw_demo.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lc

%.class: %.java
	javac -Xlint:unchecked $<

clean:
	rm -f *.o
	rm -f libgcprof.jnilib
	rm -f java_crw_demo/*.o

.PHONY: all clean
