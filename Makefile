# titan_trophy_fix - PS5 payload (ps5-payload-sdk)
#   export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
#   make
#   make test PS5_HOST=<ps5 ip>   # sends it to elfldr (port 9021)

PS5_HOST ?= ps5
PS5_PORT ?= 9021

ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

VERSION ?= 1.0
# Payload Manager shows the version from the file name (..._v1.0.elf)
ELF := titan_trophy_fix_v$(VERSION).elf

CFLAGS := -Wall -Werror -O2 -DTTF_VERSION=\"$(VERSION)\"
LDLIBS := -lSceSystemService

all: $(ELF)

$(ELF): titan_trophy_fix.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f titan_trophy_fix*.elf

test: $(ELF)
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $^
