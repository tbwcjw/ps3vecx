LWASM := lwasm
ROOT := $(CURDIR)

SOURCES := $(shell find . -type f -name '*.asm')
OUTPUTS := $(foreach src,$(SOURCES),$(ROOT)/$(notdir $(src:.asm=.vec)))

.PHONY: all clean

all: $(OUTPUTS)

define COMPILE
$(2): $(1)
	$$(LWASM) --format=raw --output=$$@ $$<
endef

$(foreach src,$(SOURCES),$(eval $(call COMPILE,$(src),$(ROOT)/$(notdir $(src:.asm=.vec)))))

clean:
	rm -f $(OUTPUTS)