EXEC  = libz03.so

BINPATH = ../../bin/
CFLAGS  = -fpic -W -Wall -O2 -pipe -g \
          -ansi -pedantic -std=gnu99 -I/usr/include/libxml2 # -D__DEBUG__
LDFLAGS = -shared -ldl -rdynamic \
          -lcurl -lcrypto -lssl -lm -lxml2 -ljansson \
          -lsqlite3 -lmagic

CC = gcc
