from dataclasses import dataclass
from typing import Any


OUTPUT_SIZE = 42 * 1024

TT_CONST = "const"
TT_OPCODE = "opcode"
TT_STRING = "string"
TT_LABEL = "label"
TT_LDEF = "ldef"

IA_1B = "1 byte"
IA_2B = "2 bytes"
IA_4B = "4 bytes"

@dataclass
class InstructionData:
    opcode: int
    args: list[str]

INST_FORMATS = {
    "NOP":      InstructionData(0x00, []),

    "PUSH8":    InstructionData(0x08, [IA_1B]),
    "PUSH16":   InstructionData(0x09, [IA_1B]),
    "PUSH32":   InstructionData(0x0a, [IA_1B]),
    "POP8":     InstructionData(0x0b, [IA_1B]),
    "POP16":    InstructionData(0x0c, [IA_1B]),
    "POP32":    InstructionData(0x0d, [IA_1B]),

    "LDI8":     InstructionData(0x10, [IA_1B, IA_1B]),
    "LDI16":    InstructionData(0x11, [IA_1B, IA_2B]),
    "LDI32":    InstructionData(0x12, [IA_1B, IA_4B]),

    "LD8":      InstructionData(0x14, [IA_1B, IA_4B]),
    "LD16":     InstructionData(0x15, [IA_1B, IA_4B]),
    "LD32":     InstructionData(0x16, [IA_1B, IA_4B]),

    "LDP8":     InstructionData(0x18, [IA_1B, IA_1B]),
    "LDP16":    InstructionData(0x19, [IA_1B, IA_1B]),
    "LDP32":    InstructionData(0x1a, [IA_1B, IA_1B]),

    "ST8":      InstructionData(0x1c, [IA_1B, IA_4B]),
    "ST16":     InstructionData(0x1d, [IA_1B, IA_4B]),
    "ST32":     InstructionData(0x1d, [IA_1B, IA_4B]),

    "STP8":     InstructionData(0x20, [IA_1B, IA_1B]),
    "STP16":    InstructionData(0x21, [IA_1B, IA_1B]),
    "STP32":    InstructionData(0x22, [IA_1B, IA_1B]),

    "ADD":      InstructionData(0x24, [IA_1B, IA_1B, IA_1B]),
    "SUB":      InstructionData(0x25, [IA_1B, IA_1B, IA_1B]),
    "MUL":      InstructionData(0x26, [IA_1B, IA_1B, IA_1B]),
    "MULS":     InstructionData(0x27, [IA_1B, IA_1B, IA_1B]),
    "DIV":      InstructionData(0x28, [IA_1B, IA_1B, IA_1B]),
    "DIVS":     InstructionData(0x29, [IA_1B, IA_1B, IA_1B]),
    "MOD":      InstructionData(0x2a, [IA_1B, IA_1B, IA_1B]),
    "MODS":     InstructionData(0x2b, [IA_1B, IA_1B, IA_1B]),
    "INC":      InstructionData(0x2c, [IA_1B]),
    "DEC":      InstructionData(0x2d, [IA_1B]),

    "AND":      InstructionData(0x30, [IA_1B, IA_1B, IA_1B]),
    "NAND":     InstructionData(0x31, [IA_1B, IA_1B, IA_1B]),
    "OR":       InstructionData(0x32, [IA_1B, IA_1B, IA_1B]),
    "NOR":      InstructionData(0x33, [IA_1B, IA_1B, IA_1B]),
    "XOR":      InstructionData(0x34, [IA_1B, IA_1B, IA_1B]),

    "JMP":      InstructionData(0x40, [IA_4B]),
}

@dataclass
class Token:
    t: str
    v: Any

class Parser:
    def __init__(self, code: str):
        self.code = code
        self.idx = -1
        self.char = None

        self.advance()

    def advance(self):
        self.idx += 1

        if self.idx < len(self.code):
            self.c = self.code[self.idx]
        else:
            self.c = None

    def get_tokens(self):
        tokens: list[Token] = []

        
