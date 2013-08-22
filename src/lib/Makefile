include config.mk

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: options $(EXEC)

options: config.mk
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "EXTRA   = $(EXTRA_CFLAGS)"

$(EXEC): $(OBJ)
	mkdir -p $(BINPATH)
	$(CC) -o $(BINPATH)$@ $^ $(LDFLAGS)
	
$(OBJ): $(SRC)
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) $*.c

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

install:
	cp $(EXEC) $(BINPATH)
