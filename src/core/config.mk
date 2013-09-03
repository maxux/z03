EXEC = z03-bot

BINPATH = ../../bin/
CFLAGS  = -W -Wall -O2 -pipe -std=gnu99 -g # -D__DEBUG__
LDFLAGS = -rdynamic -ldl -lssl -lcrypto -lpthread

CC = gcc
