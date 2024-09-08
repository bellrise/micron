# Micron makefile
# Copyright (c) 2024 bellrise

VER_MAJ := 0
VER_MIN := 5

GENCONF  := build/include/micron_genconfig.h
DRVLIST  := build/include/micron_drvlist.h
LWIPCONF := build/include/lwipopts.h

BOARD := rp2040

__all: firmware


firmware: build pios $(GENCONF) $(DRVLIST) $(LWIPCONF)
	make --no-print-directory -j $(shell nproc) -C build

build:
	mkdir -p build
	cmake -S dist -B build
	mkdir -p build/include
	mkdir -p build/include/piocode
	dist/mkversion $(VER_MAJ) $(VER_MIN) > inc/micron/version.h

pios: build build/include/piocode/onewire.h

build/include/piocode/onewire.h: src/pio/onewire.pio
	pioasm -o c-sdk $< > $@

$(LWIPCONF): dist/lwipopts.h
	cp dist/lwipopts.h build/include/lwipopts.h

$(DRVLIST): $(wildcard src/drv/*.c)
	dist/mkdrv > $@

$(GENCONF): dist/default.config dist/local.config
	dist/mkgenconfig > $@

clean:
	make --no-print-directory -C build clean/fast
	rm -rf build/include

distclean:
	rm -rf build

compile_flags.txt: dist/clangd_flags.txt
	find ${PICO_SDK_PATH}/src/${BOARD} ${PICO_SDK_PATH}/src/rp2_common \
		${PICO_SDK_PATH}/src/boards ${PICO_SDK_PATH}/src/common \
		-type d -name include -exec echo -I{} \; > compile_flags.txt
	cat dist/clangd_flags.txt | sed "s;{{PICO_SDK}};$(PICO_SDK_PATH);g" >> compile_flags.txt

usbdebug:
	picotool load -u build/micron.uf2 -f
	while [ ! -e /dev/ttyACM0 ]; do sleep 0.5; done \
		&& picocom -b 115200 --imap lfcrlf /dev/ttyACM0


.PHONY: compile_flags.txt
