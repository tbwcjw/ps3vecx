.PHONY: all build pkg updatezip uploadpkg installpkg clean version

PS3_ADDR ?= 10.0.0.225
PACKAGE_NAME ?= ps3vecX
LOCAL_FILE ?= $(CURDIR)/$(PACKAGE_NAME)_$(VERSION).pkg
REMOTE_DIR ?= /dev_hdd0/packages
UPDATE_HISTORY ?= updatehistory.txt

COMMON_DIR := $(CURDIR)/common
export COMMON_DIR

SFOXML := $(CURDIR)/MENU/sfo.xml
VERSION := $(strip $(shell xmllint --xpath 'normalize-space(/sfo/value[@name="APP_VER"]/text())' $(SFOXML)))
RELEASE = release

all: clean pkg updatezip uploadpkg

build:
	$(MAKE) -C EMU build RELEASE=$(RELEASE)
	$(MAKE) -C MENU build RELEASE=$(RELEASE)
	$(MAKE) -C VECTREXASM

version:
	@echo $(VERSION)

test: 
	
pkg: build
	python3 hisgen.py $(UPDATE_HISTORY) -o MENU/pkgfiles/PARAM.HIS --tz Etc/GMT+5
	mkdir -p MENU/pkgfiles/USRDIR
	cp EMU/ps3vecX_EMU.self MENU/pkgfiles/USRDIR/VECXEMU.SELF
	cp MENU/ps3vecX.self MENU/pkgfiles/USRDIR/VECXMENU.SELF
	cp VECTREXASM/boot.vec MENU/pkgfiles/USRDIR/carts/boot.vec
	cp VECTREXASM/about.vec MENU/pkgfiles/USRDIR/carts/about.vec
	cp VECTREXASM/controls.vec MENU/pkgfiles/USRDIR/carts/controls.vec
	$(MAKE) -C MENU pkg RELEASE=$(RELEASE)
	cp MENU/$(PACKAGE_NAME).pkg ./$(PACKAGE_NAME)_$(VERSION).pkg

updatezip: pkg
	mkdir -p tmp/PS3VECX01/USRDIR/
	cp MENU/build/pkg/USRDIR/EBOOT.BIN tmp/PS3VECX01/USRDIR/EBOOT.BIN
	cp MENU/build/pkg/USRDIR/VECXEMU.SELF tmp/PS3VECX01/USRDIR/VECXEMU.SELF
	cp MENU/build/pkg/USRDIR/VECXMENU.SELF tmp/PS3VECX01/USRDIR/VECXMENU.SELF
	cp MENU/pkgfiles/PARAM.HIS tmp/PS3VECX01/PARAM.HIS

	cd tmp && zip -r ../UPDATE_$(VERSION).zip PS3VECX01
	rm -rf tmp

uploadpkg: pkg
	./uploadpkg.sh "$(PS3_ADDR)" "$(LOCAL_FILE)" "$(REMOTE_DIR)"

installpkg: uploadpkg
	curl -s -o /dev/null "http://$(PS3_ADDR)/install.ps3$(REMOTE_DIR)/$(PACKAGE_NAME)_$(VERSION).pkg"

clean:
	$(MAKE) -C EMU clean
	$(MAKE) -C MENU clean
	rm -f $(PACKAGE_NAME)_$(VERSION).pkg
	rm -rf tmp