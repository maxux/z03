all:
	cd src && $(MAKE)

clean:
	cd src && $(MAKE) clean

mrproper:
	cd src && $(MAKE) mrproper

install:
	cd src && $(MAKE) install
