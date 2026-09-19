CC=gcc
PYTHON=python

.PHONY: run clean setup-asm test-code

build/exe: build/ src/main.c
	$(CC) src/main.c -o build/exe

build/:
	mkdir build

cbuild/main.bin: os/kernel.yr2 cbuild/main.yr2 cbuild/
	$(PYTHON) assembler.py os/kernel.yr2 cbuild/main.yr2 -o cbuild/main.bin

cbuild/main.yr2: os/main.yc cbuild/
	$(PYTHON) compiler.py os/main.yc -o cbuild/main.yr2 --include-std-code false

cbuild/:
	mkdir cbuild

run: build/exe cbuild/main.bin
	./build/exe cbuild/main.bin

clean:
	rm build/ -fr
	rm cbuild/ -fr
