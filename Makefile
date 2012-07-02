# Makefile for JVMGCProf

CC=cc
RM=rm -f

uname_S:=$(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
uname_R := $(shell sh -c 'uname -r 2>/dev/null || echo not')
uname_P := $(shell sh -c 'uname -p 2>/dev/null || echo not')
uname_V := $(shell sh -c 'uname -v 2>/dev/null || echo not')

#
# Platform specific tweaks
#

ifeq ($(uname_S),Linux)
  JAVA_HOME=/usr/java/default/
  JAVA_HEADERS=$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
  GCPROF_LIB=libgcprof.$(uname_S).$(uname_P).so
endif

ifeq ($(uname_S),Darwin)
  JAVA_HOME=$(shell /usr/libexec/java_home)
  JAVA_HEADERS=/System/Library/Frameworks/JavaVM.framework/Versions/A/Headers
  GCPROF_LIB=libgcprof.$(uname_S).$(uname_P).jnilib
endif

CFLAGS=-Ijava_crw_demo -fno-strict-aliasing                                  \
        -fPIC -fno-omit-frame-pointer -W -Wall  -Wno-unused -Wno-parentheses \
        -I$(JAVA_HEADERS) -Iinclude
LDFLAGS=-fno-strict-aliasing -fPIC -fno-omit-frame-pointer \
        -static-libgcc -shared
OBJS=gcprof.o u.o java_crw_demo/java_crw_demo.o

# 
# Build rules
#

all: $(GCPROF_LIB) GcProf.class

$(GCPROF_LIB): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lc

%.class: %.java
	javac -Xlint:unchecked $<

#
# Cleaning rules
#

clean:
	$(RM) $(GCPROF_LIB) $(OBJS) *.class 

.PHONY: all clean
