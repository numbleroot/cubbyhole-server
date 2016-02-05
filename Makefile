# Makefile for cubbyhole server.

PROG = cubbyhole-server
COMP = gcc
LIBS = -lpthread
CFLAGS = -Wall -Wextra -pedantic -g -std=c90 $(LIBS) -D_GNU_SOURCE

.PHONY: $(PROG)

all: $(PROG)

$(PROG):
	$(COMP) $(CFLAGS) -o $(PROG) $(PROG).c

clean:
	rm $(PROG)