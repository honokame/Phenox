TOPDIR = .
 -include $(TOPDIR)/include.mk

BRANCHES = $(sort $(dir $(shell find . -mindepth 1 -name "*.c")))
BRANCHES += $(sort $(dir $(shell find . -mindepth 1 -name "*.cpp")))
HEADERS = $(shell find . -name "*.h")
TARGET = px_library.a
AR = ar
ARFLAGS = r

.PHONY:all clean

all: $(TARGET)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJDIR)/*

$(TARGET):	
	mkdir -p $(HEADDIR) $(OBJDIR)	
	for d in $(HEADERS); \
	do \
	cp -f $$d $(HEADDIR); \
	done

	for d in $(BRANCHES); \
	do \
	make -C $$d; \
	done
clean:
	rm -rf $(HEADDIR) $(OBJDIR) $(TARGET) 
	for d in $(BRANCHES); \
	do \
	make -C $$d clean; \
	done


