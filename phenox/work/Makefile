
BRANCHES = $(sort $(dir $(shell find . -mindepth 2 -name "Makefile")))

.PHONY:all clean

all:
	for d in $(BRANCHES); \
	do \
	make -C $$d; \
	done
clean:
	for d in $(BRANCHES); \
	do \
	make -C $$d clean; \
	done


