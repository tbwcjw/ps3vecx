.PHONY: all build pkg clean

all: pkg

build:
	$(MAKE) -C EMU build
	$(MAKE) -C MENU build

pkg: build
	mkdir -p MENU/pkgfiles/USRDIR
	cp EMU/ps3vecX_EMU.self MENU/pkgfiles/USRDIR/VECXEMU.SELF
	cp MENU/ps3vecX.self MENU/pkgfiles/USRDIR/VECXMENU.SELF
	cp VECTREXASM/boot.vec MENU/pkgfiles/USRDIR/carts/boot.vec
	$(MAKE) -C MENU pkg

clean:
	$(MAKE) -C EMU clean
	$(MAKE) -C MENU clean
