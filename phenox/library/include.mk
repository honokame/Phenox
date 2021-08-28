HEADDIR = $(TOPDIR)/headers
OBJDIR = $(TOPDIR)/objs
SOBJDIR = $(TOPDIR)/sobjs
CC = gcc
CFLAGS = -I $(HEADDIR) -g -O2 `pkg-config --cflags opencv`

