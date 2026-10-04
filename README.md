# YRON2 Project

Needed thanks: This project uses code from the [cJSON repo](https://github.com/DaveGamble/cJSON) (files `src/cjson/cJSON.h`, `src/cjson/cJSON.c`)

### Done

- Custom CPU architecture
- Assembler
- Compiler of a C-like language.

### In progress

- Custom OS (named *"FungOS"*)
- Custom FAT-like filesystem

## YRON2 CPU Simulator

The YRON2 CPU is based on my previous architecture designs, most notably the YRON CPU.

### Features
- Fully featured ISA
- Port-based device support - With instructions like `SND` and `RCV`
- 32 Registers
- Stack
- Relocatable code with register `MS` (`02`)

## Assembler

Built for the YRON2 CPU architecture.

### Features

- All CPU's instructions support
- Imidiate value support
    - Decimal
    - Hexadecimal
    - Octal
    - Binary
    - Character literals (ASCII)

## Compiler

Its a C-like language compiler, with support for exporting variable names and their locations

## OS (FungOS)

### Features

- Boots

### Working on

- Custom FAT-like FS support
