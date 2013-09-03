include config.mk

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: options $(EXEC)

options: config.mk
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"

$(EXEC): $(OBJ)
	mkdir -p $(BINPATH)
	$(CC) -o $(BINPATH)$@ $^ $(LDFLAGS)

$(OBJ):
	$(CC) -c $(CFLAGS) $*.c

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

install:
	cp $(EXEC) $(BINPATH)

