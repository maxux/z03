include config.mk

# reading all .c
_SRC = $(wildcard *.c)
# setting registrar at first and removing it from list
SRC  = registrar.c $(filter-out registrar.c, $(_SRC))
OBJ  = $(SRC:.c=.o)

all: options $(EXEC)

options: config.mk
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"

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
