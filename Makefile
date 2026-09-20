CC=gcc
PYTHON=python

.PHONY: run clean rm_db

build/exe: build/ src/main.c
	$(CC) src/main.c -o build/exe

build/:
	mkdir build

cbuild/kernel.bin: os/kernel.yr2 cbuild/
	$(PYTHON) assembler.py os/kernel.yr2 -o cbuild/kernel.bin

cbuild/main.bin: cbuild/main.yr2 os/main_helpers.yr2 cbuild/
	$(PYTHON) assembler.py cbuild/main.yr2 os/main_helpers.yr2 -o cbuild/main.bin

cbuild/rom.bin: cbuild/kernel.bin cbuild/main.bin cbuild/
	cat cbuild/kernel.bin cbuild/main.bin > cbuild/rom.bin

cbuild/main.yr2: os/main.yc cbuild/
	$(PYTHON) compiler.py os/main.yc -o cbuild/main.yr2 --include-std-code false

disk.bin:
	dd if=/dev/zero of=disk.bin bs=1 count=65536 # 64 KiB disk

cbuild/:
	mkdir cbuild

run: rm_cb build/exe cbuild/rom.bin disk.bin
	./build/exe cbuild/rom.bin

clean:
	rm build/ -fr
	rm cbuild/ -fr

rm_cb:
	rm cbuild/ -fr
