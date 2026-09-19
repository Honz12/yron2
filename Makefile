CC=gcc
PYTHON=python

.PHONY: run clean setup-asm test-code

build/exe: build/ src/main.c
	$(CC) src/main.c -o build/exe

build/:
	mkdir build

cbuild/main.bin: cbuild/main.yr2 os/kernel.yr2
	$(PYTHON) assembler.py cbuild

cbuild/main.yr2: os/main.yc

run: build/exe test-code
	./build/exe test_files/out.bin

clean:
	rm build/ -fr
