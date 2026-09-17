CC=gcc
PYTHON=python

.PHONY: run clean setup-asm test-code

build/exe: build/ src/main.c
	$(CC) src/main.c -o build/exe

build/:
	mkdir build

run: build/exe test-code
	./build/exe test_files/out.bin

clean:
	rm build/ -fr

test-code:
	$(PYTHON) assembler.py test_files/kernel.yr2 test_files/main.yr2 -o test_files/out.bin
