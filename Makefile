.PHONY: all build pkg clean

all: pkg

build:
	$(MAKE) -C EMU build
	$(MAKE) -C MENU build

pkg: build
	mkdir -p EMU/pkgfiles/USRDIR MENU/pkgfiles/USRDIR
	cp EMU/ps3vecX_EMU.self MENU/pkgfiles/USRDIR/VECXEMU.SELF
	cp MENU/ps3vecX.self MENU/pkgfiles/USRDIR/VECXMENU.SELF
	$(MAKE) -C MENU pkg

clean:
	$(MAKE) -C EMU clean
	$(MAKE) -C MENU clean
