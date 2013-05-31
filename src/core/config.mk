EXEC = z03-bot

# flags
EXTRA_CFLAGS = # -D__DEBUG__

CFLAGS  = -W -Wall -O2 -pipe -std=gnu99 -g
LDFLAGS = -rdynamic -ldl -lssl -lpthread
BINPATH = ../../bin/

CC = gcc
