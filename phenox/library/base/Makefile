TOPDIR = ../
-include $(TOPDIR)/include.mk

SRCS_C = $(wildcard *.c) 
OBJS_C = $(SRCS_C:.c=.o)
SOBJS_C = $(SRCS_C:.c=.so)
HEADERS_C = $(SRCS_C:.c=.h) 

SRCS_CPP = $(wildcard *.cpp) 
OBJS_CPP = $(SRCS_CPP:.cpp=.o)
SOBJS_CPP = $(SRCS_CPP:.cpp=.so)
HEADERS_CPP = $(SRCS_CPP:.cpp=.h) 

.PHONY: all clean
all: $(OBJS_C) $(OBJS_CPP) $(SOBJS_C) $(SOBJS_CPP)
%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<
	cp -f $@ $(OBJDIR)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	cp -f $@ $(OBJDIR)

%.so: %.cpp
	$(CC) $(CFLAGS) -shared -fPIC -Wl,-no-as-needed -L `pkg-config --libs opencv` -o $@ $<
	cp -f $@ $(SOBJDIR)

%.so: %.c
	$(CC) $(CFLAGS) -shared -fPIC -Wl,-no-as-needed -L `pkg-config --libs opencv` -o $@ $<
	cp -f $@ $(SOBJDIR)

clean:
	rm -rf $(OBJS_C) $(OBJS_CPP)
	rm -rf $(SOBJS_C) $(SOBJS_CPP)
