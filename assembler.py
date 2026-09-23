#!/bin/python3
from dataclasses import dataclass
from typing import Any, List, Optional
import sys

TT_CONST = "const"
TT_INST = "inst"
TT_STRING = "string"
TT_IDEN = "iden"
TT_LDEF = "ldef"

TT_DIR_STR = "dir-str"
TT_DIR_DUMP8 = "dir-dump8"
TT_DIR_DUMP16 = "dir-dump16"
TT_DIR_DUMP32 = "dir-dump32"
TT_DIR_ORG = "dir-org"
TT_DIR_FILL = "dir-fill"

IA_1B = "1 byte"
IA_2B = "2 bytes"
IA_4B = "4 bytes"

@dataclass
class InstructionData:
    opcode: int
    args: List[str]

INST_FORMATS = {
    "NOP":      InstructionData(0x00, []),

    "CALL":     InstructionData(0x01, [IA_4B]),
    "RET":      InstructionData(0x02, []),
    "INT":      InstructionData(0x03, [IA_1B]),
    "IRET":     InstructionData(0x04, []),

    "MOV":      InstructionData(0x05, [IA_1B, IA_1B]),

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
    "ST32":     InstructionData(0x1e, [IA_1B, IA_4B]),

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

    "EQ":       InstructionData(0x40, [IA_1B, IA_1B, IA_1B]),
    "GT":       InstructionData(0x41, [IA_1B, IA_1B, IA_1B]),
    "GTE":      InstructionData(0x42, [IA_1B, IA_1B, IA_1B]),
    "LT":       InstructionData(0x43, [IA_1B, IA_1B, IA_1B]),
    "LTE":      InstructionData(0x44, [IA_1B, IA_1B, IA_1B]),

    "JMP":      InstructionData(0x48, [IA_4B]),
    "JZ":       InstructionData(0x49, [IA_4B, IA_1B]),
    "JNZ":      InstructionData(0x4a, [IA_4B, IA_1B]),
    "LJMP":     InstructionData(0x4b, [IA_1B, IA_1B]),

    "SND":      InstructionData(0x50, [IA_1B, IA_1B]),
    "RCV":      InstructionData(0x51, [IA_1B, IA_1B]),
}

DIRECTIVES = {
    "str": TT_DIR_STR,
    "d8": TT_DIR_DUMP8, "dump8": TT_DIR_DUMP8,
    "d16": TT_DIR_DUMP16, "dump16": TT_DIR_DUMP16,
    "d32": TT_DIR_DUMP32, "dump32": TT_DIR_DUMP32,
    "org": TT_DIR_ORG,
    "fill": TT_DIR_FILL,
}

@dataclass
class Token:
    t: str
    v: Any
    line: int = 1

DIGITS = "0123456789"
HEX_CHARS = DIGITS + "abcdefABCDEF"
BIN_CHARS = "01"
OCT_CHARS = "01234567"
ALPHA = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_."
ALNUM = ALPHA + DIGITS

class Lexer:
    def __init__(self, code: str, verbose: bool = False):
        self.code = code
        self.idx = -1
        self.c: Optional[str] = None
        self.line = 1
        self.verbose = verbose
        self.advance()

    def advance(self):
        if self.c == "\n":
            self.line += 1
        self.idx += 1
        self.c = self.code[self.idx] if self.idx < len(self.code) else None

    def get_tokens(self) -> Optional[List[Token]]:
        tokens: List[Token] = []

        while self.c is not None:
            if self.c == ";":
                while self.c is not None and self.c != "\n":
                    self.advance()
            elif self.c in " \n\t\r":
                self.advance()
            elif self.c in DIGITS:
                t = self.get_int()
                if t is None: return None
                tokens.append(t)
            elif self.c in ALPHA:
                t = self.get_iden()
                if t is None: return None
                tokens.append(t)
            elif self.c == '"':
                t = self.get_string()
                if t is None: return None
                tokens.append(t)
            elif self.c == "'":
                t = self.get_char()
                if t is None: return None
                tokens.append(t)
            else:
                print(f"Error: Unexpected character {repr(self.c)} at line {self.line}")
                return None

        return tokens

    def get_int(self) -> Optional[Token]:
        base = 10
        allowed = DIGITS
        str_repr = ""
        current_line = self.line

        if self.c == "0":
            self.advance()
            if self.c == "x":
                self.advance(); base = 16; allowed = HEX_CHARS
            elif self.c == "b":
                self.advance(); base = 2; allowed = BIN_CHARS
            elif self.c == "o":
                self.advance(); base = 8; allowed = OCT_CHARS
            else:
                str_repr = "0"

        while self.c is not None and self.c in allowed:
            str_repr += self.c
            self.advance()

        try:
            val = int(str_repr, base)
        except ValueError:
            print(f"Error: Invalid numeric literal at line {current_line}")
            return None

        return Token(TT_CONST, val, current_line)

    def get_iden(self) -> Optional[Token]:
        str_repr = ""
        current_line = self.line

        while self.c is not None and self.c in ALNUM:
            str_repr += self.c
            self.advance()

        if str_repr.upper() in INST_FORMATS:
            return Token(TT_INST, str_repr.upper(), current_line)
        elif str_repr.lower() in DIRECTIVES:
            return Token(DIRECTIVES[str_repr.lower()], None, current_line)
        elif self.c == ":":
            self.advance()
            return Token(TT_LDEF, str_repr, current_line)
        else:
            return Token(TT_IDEN, str_repr, current_line)

    def get_string(self) -> Optional[Token]:
        current_line = self.line
        self.advance()
        chars = ""
        
        while self.c is not None:
            if self.c == '"':
                self.advance()
                break
            elif self.c == "\\":
                self.advance()
                escape_map = {"\\": "\\", "n": "\n", "t": "\t", "r": "\r", "a": "\a", "e": "\x1b", "0": "\0"}
                chars += escape_map.get(self.c, self.c if self.c else "")
                self.advance()
            else:
                chars += self.c
                self.advance()
        else:
            print(f"Error: Unterminated string literal starting at line {current_line}")
            return None

        return Token(TT_STRING, chars, current_line)

    def get_char(self) -> Optional[Token]:
        current_line = self.line
        self.advance()

        if self.c is None:
            print(f"Error: Expected character after `'` at line {current_line}")
            return None

        if self.c == '\\':
            self.advance()
            cmap = {"\\": ord("\\"), "n": ord("\n"), "t": ord("\t"), "a": ord("\a"), "e": ord("\x1b"), "0": 0}
            c = cmap.get(self.c, 0)
        else:
            c = ord(self.c)

        self.advance()
        if self.c != "'":
            print(f"Error: Expected `'` after character literal at line {current_line}")
            return None

        self.advance()
        return Token(TT_CONST, c, current_line)

class CodeGenerator:
    def __init__(self, tokens: List[Token], verbose: bool = False):
        self.tokens = tokens
        self.i = -1
        self.t: Optional[Token] = None
        self.verbose = verbose
        self.advance()

    def advance(self):
        self.i += 1
        self.t = self.tokens[self.i] if self.i < len(self.tokens) else None

    @staticmethod
    def int_to_2bytes(i: int) -> List[int]:
        return [i & 0xFF, (i >> 8) & 0xFF]

    @staticmethod
    def int_to_4bytes(i: int) -> List[int]:
        return [i & 0xFF, (i >> 8) & 0xFF, (i >> 16) & 0xFF, (i >> 24) & 0xFF]

    def get_bytes(self) -> Optional[bytes]:
        @dataclass
        class ValuePostReplacement:
            address: int
            size: int
            name: str
            line: int

        out: List[int] = []
        names: dict[str, int] = {}
        current_label_scope = ""
        value_post_replacements: List[ValuePostReplacement] = []

        def resolve_label(label: str) -> str:
            if label.startswith("."):
                return current_label_scope + label
            return label

        while self.t is not None:
            if self.t.t == TT_LDEF:
                name = str(self.t.v)
                full_name = current_label_scope + name if name.startswith(".") else name
                if not name.startswith("."):
                    current_label_scope = name
                
                if full_name in names:
                    print(f"Error: Duplicate label {repr(full_name)} at line {self.t.line}")
                    return None

                names[full_name] = len(out)
                if self.verbose: print(f"New label {full_name} at address {len(out)}")
                self.advance()

            elif self.t.t == TT_INST:
                inst_tok = self.t
                self.advance()
                inst_data = INST_FORMATS[inst_tok.v]
                out.append(inst_data.opcode)

                for arg_req in inst_data.args:
                    if self.t is None:
                        print(f"Error: Expected argument for instruction {inst_tok.v} at line {inst_tok.line}")
                        return None
                    
                    if self.t.t == TT_CONST:
                        if arg_req == IA_1B: out.append(self.t.v & 0xFF)
                        elif arg_req == IA_2B: out += self.int_to_2bytes(self.t.v)
                        elif arg_req == IA_4B: out += self.int_to_4bytes(self.t.v)
                        self.advance()
                    elif self.t.t == TT_IDEN:
                        size = 1 if arg_req == IA_1B else (2 if arg_req == IA_2B else 4)
                        value_post_replacements.append(ValuePostReplacement(len(out), size, resolve_label(self.t.v), self.t.line))
                        out += [0xFF] * size
                        self.advance()
                    else:
                        print(f"Error: Invalid argument type for instruction {inst_tok.v} at line {self.t.line}")
                        return None

            elif self.t.t in [TT_DIR_DUMP8, TT_DIR_DUMP16, TT_DIR_DUMP32]:
                dir_type = self.t.t
                self.advance()
                if self.t is None:
                    print("Error: Expected value after directive")
                    return None

                size = 1 if dir_type == TT_DIR_DUMP8 else (2 if dir_type == TT_DIR_DUMP16 else 4)
                if self.t.t == TT_CONST:
                    if size == 1: out.append(self.t.v & 0xFF)
                    elif size == 2: out += self.int_to_2bytes(self.t.v)
                    else: out += self.int_to_4bytes(self.t.v)
                elif self.t.t == TT_IDEN:
                    value_post_replacements.append(ValuePostReplacement(len(out), size, resolve_label(self.t.v), self.t.line))
                    out += [0xFF] * size
                self.advance()

            elif self.t.t == TT_DIR_STR:
                self.advance()
                if self.t is None or self.t.t != TT_STRING:
                    print("Error: Expected string literal after STR directive")
                    return None
                for c in self.t.v:
                    out.append(ord(c) & 0xFF)
                self.advance()

            elif self.t.t == TT_DIR_ORG:
                self.advance()
                if self.t is None or self.t.t != TT_CONST:
                    print("Error: Expected numeric constant after ORG directive")
                    return None
                if self.t.v < len(out):
                    print(f"Error: ORG address {self.t.v} overlaps with existing code (current length {len(out)})")
                    return None
                while len(out) < self.t.v:
                    out.append(0x00)
                self.advance()

            elif self.t.t == TT_DIR_FILL:
                self.advance()
                if self.t is None or self.t.t != TT_CONST:
                    print("Error: Expected numeric constant after FILL directive")
                    return None
                out += [0x00] * self.t.v
                self.advance()

            else:
                print(f"Error: Unknown token {repr(self.t.t)} at line {self.t.line}")
                return None

        # Resolve labels
        for replace in value_post_replacements:
            if replace.name in names:
                val = names[replace.name]
                num_bytes = [val & 0xFF] if replace.size == 1 else (self.int_to_2bytes(val) if replace.size == 2 else self.int_to_4bytes(val))
                for o, b in enumerate(num_bytes):
                    out[replace.address + o] = b
            else:
                print(f"Error: Undefined label/identifier {repr(replace.name)} referenced at line {replace.line}")
                return None

        return bytes(out)

if __name__ == "__main__":
    input_files = []
    output_file = "out.bin"
    verbose = False

    args = sys.argv[1:]
    setting_out = False
    for a in args:
        if setting_out:
            output_file = a
            setting_out = False
        elif a == "-o":
            setting_out = True
        elif a == "-v":
            verbose = True
        else:
            input_files.append(a)

    if not input_files:
        print("Usage: python assembler.py <input_file(s)> [-o output_file] [-v]")
        sys.exit(1)

    input_text = ""
    for ifile in input_files:
        try:
            with open(ifile, "r") as f:
                input_text += f"\n; --- FILE {ifile} ---\n" + f.read() + "\n"
        except Exception as e:
            print(f"Error: Failed to open file {ifile}: {e}")
            sys.exit(1)

    lexer = Lexer(input_text, verbose)
    tokens = lexer.get_tokens()
    if tokens is None:
        sys.exit(1)

    code_gen = CodeGenerator(tokens, verbose)
    out = code_gen.get_bytes()
    if out is None:
        sys.exit(1)

    try:
        with open(output_file, "wb") as of:
            of.write(out)
        print(f"Successfully assembled {len(out)} bytes into {output_file}")
    except Exception as e:
        print(f"Error: Failed to write to output file {output_file}: {e}")
        sys.exit(1)