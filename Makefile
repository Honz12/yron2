CC=gcc

.PHONY: run clean setup-asm test-code

build/exe: build/ src/main.c
	$(CC) src/main.c -o build/exe

build/:
	mkdir build

run: build/exe test-code
	./build/exe test_files/out.bin

clean:
	rm build/ -fr

setup-asm:
	chmod +x assembler.py

test-code:
	./assembler.py test_files/kernel.yr2 -o test_files/out.bin
