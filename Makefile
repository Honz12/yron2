CC=gcc

.PHONY: run clean setup-asm test-code

build/exe: build/ src/main.c
	$(CC) src/main.c -o build/exe

build/:
	mkdir build

run: build/exe
	./build/exe

clean:
	rm build/ -fr

setup-asm:
	chmod +x assembler.py

test-code:
	./assembler.py test_files/main.yr2 test_files/lib.yr2 -o test_files/out.bin
