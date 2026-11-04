#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

# debug / release
RELEASE = debug

#---------------------------------------------------------------------------------
#  TITLE, APPID, CONTENTID, ICON0 SFOXML before ppu_rules.
#---------------------------------------------------------------------------------
TITLE		    := VecX
APPID		    := PS3VECX01
CONTENTID	    := UP0001-$(APPID)_00-0000000000000000
PKGFILES	    := $(CURDIR)/pkgfiles
#ICON0           := $(CURDIR)/pkgfiles/ICON0.PNG
#ICON1          := $(CURDIR)/ICON1.PAM
#PIC1           := $(CURDIR)/PIC1.PNG
SFOXML          := $(CURDIR)/sfo.xml

SCETOOL_FLAGS	?=	--self-app-version=0001000000000000  --sce-type=SELF --compress-data=TRUE --self-add-shdrs=TRUE --skip-sections=FALSE --key-revision=1 \
					--self-auth-id=1010000001000003 --self-vendor-id=01000002 --self-fw-version=0004002000000000

include $(PSL1GHT)/ppu_rules

# aditional scetool flags (--self-ctrl-flags, --self-cap-flags...)
# --self-ctrl-flags Root Flags + debug_mode_capability flags (vsh)
# --self-cap-flags lv2_kernel
SCETOOL_FLAGS	+=	--self-ctrl-flags 4000000000000000000000000000000000000000000000000000000000000002
SCETOOL_FLAGS	+=	--self-cap-flags 00000000000000000000000000000000000000000000007B0000000100000000

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	ps3vecX
BUILD		:=	build
SOURCES		:=	source
DATA		:=  data
SHADERS		:= 
INCLUDES	:=	include source
COMMON_DIR	:=	$(abspath $(CURDIR)/../common)
COMMON_SRC	:=	$(COMMON_DIR)/source
COMMON_INC	:=	$(COMMON_DIR)/include
PKGFILES	:=	$(CURDIR)/pkgfiles

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
# (link order matters: -lrsx before -lgcm_sys)
#---------------------------------------------------------------------------------
LIBS		:=	-lsysfs -lfreetype -lio \
				-lz -ltiny3d -lgcm_sys -lsysutil -lsysmodule -lpngdec  -lrt \
				 -lm 

ifeq ($(RELEASE),debug)
	LIBS += -lnet -lsysmodule 
endif
#---------------------------------------------------------------------------------
# options for code generation
# -fgnu89-inline: vecx uses old-style __inline functions that otherwise give
#                 "undefined reference" errors with modern gcc
#---------------------------------------------------------------------------------

CFLAGS		=	-O2 -Wall -mcpu=cell -fgnu89-inline $(MACHDEP) $(INCLUDE) $(DFLAGS) $(DEBUG_FLAGS) -DVERSION=\"$(VERSION)\" 

ifeq ($(RELEASE),debug)
CFLAGS += -DPS3LOADX -DDEBUG -DDEBUG_ADDR=\"10.0.0.104\" -DDEBUG_PORT=\"18194\" 
endif

CXXFLAGS	=	$(CFLAGS)

LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
#We grab APP_VER from sfo.xml to name our zip archive, and display in app/web dashboard.
export VERSION := $(strip $(shell xmllint --xpath 'normalize-space(/sfo/value[@name="APP_VER"]/text())' $(SFOXML)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
					$(COMMON_SRC)

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

export BUILDDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) \
				$(notdir $(wildcard $(COMMON_SRC)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))


#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
  export LD	:=	$(CC)
else
  export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(addsuffix .o,$(VPOFILES)) \
					$(addsuffix .o,$(FPOFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o) \
					$(BINFILES:.bin=.bin.o)


#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/$(dir)) \
					-I$(COMMON_INC) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					$(LIBPSL1GHT_INC) \
					-I$(CURDIR)/$(BUILD) -I$(PORTLIBS)/include

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					$(LIBPSL1GHT_LIB) -L$(PORTLIBS)/lib

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) clean


#---------------------------------------------------------------------------------
$(BUILD): clean
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
#copy emu files to usrdir here

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg $(OUTPUT).gnpdrm.pkg EBOOT.BIN

#---------------------------------------------------------------------------------
run: $(BUILD)
	ps3load $(OUTPUT).self

#---------------------------------------------------------------------------------
pkg: $(BUILD) $(OUTPUT).pkg
	
#---------------------------------------------------------------------------------
release: clean pkg
	zip -r "$(OUTPUT)_$(VERSION).zip" $(TARGET).pkg README.md LICENSE.md
#---------------------------------------------------------------------------------

npdrm: $(BUILD)
	@$(SELF_NPDRM) $(SCETOOL_FLAGS) --np-content-id=$(CONTENTID) --encrypt $(BUILDDIR)/$(basename $(notdir $(OUTPUT))).elf $(BUILDDIR)/../EBOOT.BIN

#---------------------------------------------------------------------------------

else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf:	$(OFILES)
$(OFILES): $(VCGFILES)
$(CFILES:.c=.o): $(BINHEADERS)
$(BINHEADERS): %_bin.h: %.bin.o

export BINHEADERS := $(BINFILES:.bin=_bin.h)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin extension
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
%.vpo.o	:	%.vpo
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
%.fpo.o	:	%.fpo
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------