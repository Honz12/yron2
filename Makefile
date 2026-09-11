CC=gcc

.PHONY: run

build/exe: build/ src/main.c
	$(CC) src/main.c -o build/exe

build/:
	mkdir build

run: build/exe
	./build/exe

clean:
	rm build/ -fr
