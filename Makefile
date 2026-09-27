CC=gcc
PYTHON=python

.PHONY: run clean rm_cb os 

build/exe: build/ src/main.c
	$(CC) src/main.c -o build/exe

build/:
	mkdir build

os: cbuild/rom.bin

cbuild/kernel.bin: os/kernel.yr2 cbuild/
	$(PYTHON) assembler.py os/kernel.yr2 -o cbuild/kernel.bin

cbuild/main.bin: cbuild/main.yr2 os/main_helpers.yr2 cbuild/
	$(PYTHON) assembler.py cbuild/main.yr2 -o cbuild/main.bin

cbuild/rom.bin: cbuild/kernel.bin cbuild/main.bin cbuild/
	cat cbuild/kernel.bin cbuild/main.bin > cbuild/rom.bin

cbuild/main.yr2: os/main.yc cbuild/
	$(PYTHON) compiler.py os/main.yc -o cbuild/cmain.yr2 --include-std-code false
	cat cbuild/cmain.yr2 os/main_helpers.yr2 > cbuild/main.yr2

cbuild/:
	mkdir cbuild

run: build/exe cbuild/rom.bin disk.bin
	./build/exe cbuild/rom.bin

clean:
	rm build/ -fr
	rm cbuild/ -fr

rm_cb:
	rm cbuild/ -fr

test_fs:
	$(PYTHON) disk_maker.py disk.bin new 4194304
	$(PYTHON) fs_maker.py disk.bin format "FungOS"
