# Makefile for JVMGCProf

CC=gcc
uname_S:=$(shell sh -c 'uname -s 2>/dev/null || echo not')

ifeq ($(uname_S),Linux)
  JAVA_HOME=/usr/java/default/
  JAVA_HEADERS=$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
endif

ifeq ($(uname_S),Darwin)
  JAVA_HOME=$(shell /usr/libexec/java_home)
  JAVA_HEADERS=/System/Library/Frameworks/JavaVM.framework/Versions/A/Headers
endif

CFLAGS=-Ijava_crw_demo -fno-strict-aliasing                                  \
        -fPIC -fno-omit-frame-pointer -W -Wall  -Wno-unused -Wno-parentheses \
        -I$(JAVA_HEADERS) -Iinclude
LDFLAGS=-fno-strict-aliasing -fPIC -fno-omit-frame-pointer \
        -static-libgcc -shared
OBJS=gcprof.o u.o java_crw_demo/java_crw_demo.o

all: libgcprof.so GcProf.class

libgcprof.so: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lc

%.class: %.java
	javac -Xlint:unchecked $<

clean:
	rm -f libgcprof.* $(OBJS) *.class 

.PHONY: all clean
