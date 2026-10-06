CC=gcc
PYTHON=python

.PHONY: run clean rm_cb os fs_shell 

build/exe: build/ src/main.c src/helpers.c
	$(CC) src/main.c src/helpers.c src/cjson/cJSON.c -o build/exe -lncurses

build/:
	mkdir build

os: cbuild/rom.bin

cbuild/kernel.bin: os/kernel.yr2 cbuild/
	$(PYTHON) assembler.py os/kernel.yr2 -o cbuild/kernel.bin
	$(PYTHON) disassembler.py cbuild/kernel.bin -o test_KERNEL.BIN_dec.yr2

cbuild/main.bin: cbuild/main.yr2 os/corelinks.yr2 cbuild/
	$(PYTHON) assembler.py cbuild/main.yr2 -o cbuild/main.bin
	$(PYTHON) disassembler.py cbuild/main.bin -o test_MAIN.BIN_dec.yr2

cbuild/rom.bin: cbuild/kernel.bin cbuild/main.bin cbuild/
	cat cbuild/kernel.bin cbuild/main.bin > cbuild/rom.bin

cbuild/main.yr2: os/main.yc cbuild/
	$(PYTHON) compiler.py os/main.yc -o cbuild/main.yr2 -s test_MAIN.YC_sym.txt --include-std-code false

cbuild/:
	mkdir cbuild

run: build/exe cbuild/rom.bin disk.bin
	./build/exe cbuild/rom.bin

test: build/exe cbuild/rom.bin disk.bin
	./build/exe cbuild/rom.bin -p -timeout 100000

clean:
	rm build/ -fr
	rm cbuild/ -fr
	rm test_*
	rm disk.bin

rm_cb:
	rm cbuild/ -fr

disk.bin:
	$(PYTHON) disk_maker.py disk.bin new 65536
	$(PYTHON) fs_maker.py disk.bin format "FungOS"
	$(PYTHON) fs_maker.py disk.bin fhost os/other/SH_HELP_text.txt .SH_HELP

fs_shell: disk.bin
	$(PYTHON) fs_maker.py disk.bin
