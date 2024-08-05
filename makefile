# Micron makefile
# Copyright (c) 2024 bellrise

VER_MAJ := 0
VER_MIN := 1


__all: firmware


firmware: build
	make --no-print-directory -j $(shell nproc) -C build


build:
	mkdir -p build
	cmake -S dist -B build
	mkdir -p build/include
	dist/mkversion $(VER_MAJ) $(VER_MIN) > inc/micron/version.h
	dist/mkbuildconfig > build/include/micron_genconfig.h
	cp dist/lwipopts.h build/include/lwipopts.h


clean:
	rm -rf build


setupclangd:
	cat dist/clangd_flags.txt | sed "s;{{PICO_SDK}};$(PICO_SDK_PATH);g" > compile_flags.txt


usbdebug:
	picotool load build/micron.uf2 -f
	picotool reboot
	while [ ! -e /dev/ttyACM0 ]; do sleep 0.5; done \
		&& picocom -b 115200 --imap lfcrlf /dev/ttyACM0
