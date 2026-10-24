# MITalk recreation -- C port.   make            -> mitalk(.exe)
#                                  make tables     -> regenerate tables.h
#                                  make test       -> compare against Python
CC = gcc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -pedantic
LDLIBS = -lm
ifeq ($(OS),Windows_NT)
LDLIBS += -lshell32 -lwinmm
endif

SRCS = main.c util.c audio.c segs.c klatt.c phonet.c prosod.c f0.c text.c parser.c lexicon.c phono.c
OBJS = $(SRCS:.c=.o)

mitalk: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): mitalk.h tables.h

tables:
	python ../tools/gen_c_tables.py

test: mitalk
	cd .. && python tools/compare_c.py

# Distributable folder and zip: ../dist/mitalk-c/ and ../dist/mitalk-c-<target>.zip
TARGET := $(shell $(CC) -dumpmachine)
DIST = ../dist/mitalk-c
dist: mitalk
	rm -rf $(DIST) ../dist/mitalk-c-$(TARGET).zip
	mkdir -p $(DIST)
	cp $(wildcard mitalk mitalk.exe) $(DIST)/
	cp ../data/lexicon.tsv ../data/cmudict.LICENSE $(DIST)/
	cp DIST_README.txt $(DIST)/README.txt
	cd ../dist && python -m zipfile -c mitalk-c-$(TARGET).zip mitalk-c

clean:
	rm -f $(OBJS) mitalk mitalk.exe

.PHONY: tables test dist clean
