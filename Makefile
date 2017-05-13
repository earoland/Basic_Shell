# 
# Makefile
# 
# Written by Drs. William Kreahling and Andrew Dalton
#            Department of Mathematics and Computer Science
#            Western Carolina University
#
# A GNU Makefile for building the simple UNIX shell.
# 
CC=cc
CFLAGS=-O -Wall -Wextra -ggdb
LIBS=-lfl
LEX=flex
RM=rm -f

OBJECTS=shellParser.o shell.o
PROG=shell

all:	$(PROG)

shellParser.c:	shellParser.l shellParser.h
	$(LEX) -t shellParser.l > shellParser.c

shellParser.o:	shellParser.c
shell.o:		shell.c shellParser.h

shell:	shell.o	shellParser.o
	$(CC) $(CFLAGS) $(OBJECTS) -o $(PROG) $(LIBS)

clean:
	$(RM) shellParser.c $(OBJECTS) $(PROG)
