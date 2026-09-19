#!/bin/python3
from dataclasses import dataclass
from typing import Any

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
    args: list[str]

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
    "LJMP":     InstructionData(0x4b, [IA_4B, IA_4B]),

    "SND":      InstructionData(0x50, [IA_1B, IA_1B]),
    "RCV":      InstructionData(0x51, [IA_1B, IA_1B]),
}

DIRECTIVES = {
    "str": TT_DIR_STR,

    "d8": TT_DIR_DUMP8,
    "dump8": TT_DIR_DUMP8,

    "d16": TT_DIR_DUMP16,
    "dump16": TT_DIR_DUMP16,

    "d32": TT_DIR_DUMP32,
    "dump32": TT_DIR_DUMP32,

    "org": TT_DIR_ORG,

    "fill": TT_DIR_FILL,
}

@dataclass
class Token:
    t: str
    v: Any

DIGITS = "0123456789"
HEX_CHARS = DIGITS + "abcdefABCDEF"
BIN_CHARS = "01"
OCT_CHARS = "01234567"
ALPHA = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_."
ALNUM = ALPHA + DIGITS

class Lexer:
    def __init__(self, code: str, verbose = False):
        self.code = code
        self.idx = -1
        self.c = None
        self.verbose = verbose

        self.advance()

    def advance(self):
        self.idx += 1

        if self.idx < len(self.code):
            self.c = self.code[self.idx]
        else:
            self.c = None

    def get_tokens(self):
        tokens: list[Token] = []

        while self.c is not None:
            if self.c == ";":
                while self.c is not None and self.c != "\n":
                    self.advance()
            elif self.c in " \n\t":
                self.advance()
            elif self.c in DIGITS:
                t = self.get_int()
                if t is None:
                    return
                tokens.append(t)
            elif self.c in ALPHA:
                t = self.get_iden()
                if t is None:
                    return
                tokens.append(t)
            elif self.c == '"':
                t = self.get_string()
                if t is None:
                    return
                tokens.append(t)
            elif self.c == "'":
                t = self.get_char()
                if t is None:
                    return
                tokens.append(t)

        return tokens

    def get_int(self):
        base = 10
        allowed = DIGITS
        str_repr = ""

        if self.c == "0":
            self.advance()

            if self.c == "x":
                self.advance()
                base = 16
                allowed = HEX_CHARS

            elif self.c == "b":
                self.advance()
                base = 2
                allowed = BIN_CHARS

            elif self.c == "o":
                self.advance()
                base = 8
                allowed = OCT_CHARS

            else:
                str_repr = "0"

        while self.c is not None:
            if self.c not in allowed:
                break

            str_repr += self.c
            self.advance()

        return Token(TT_CONST, int(str_repr, base))

    def get_iden(self):
        str_repr = ""

        while self.c is not None:
            if self.c not in ALNUM:
                break

            str_repr += self.c
            self.advance()

        if str_repr.upper() in INST_FORMATS:
            return Token(TT_INST, str_repr.upper())
        elif str_repr.lower() in DIRECTIVES:
            return Token(DIRECTIVES[str_repr.lower()], None)
        elif self.c == ":":
            self.advance()
            return Token(TT_LDEF, str_repr)
        else:
            return Token(TT_IDEN, str_repr)

    def get_string(self):
        self.advance()
        chars = ""
        
        while self.c is not None:
            if self.c == '"':
                self.advance()
                break
            elif self.c == "\\":
                self.advance()
                match self.c:
                    case "\\":
                        chars += "\\"
                    case "n":
                        chars += "\n"
                    case "t":
                        chars += "\t"
                    case "r":
                        chars += "\r"
                    case "a":
                        chars += "\a"
                    case "e":
                        chars += "\x1b"
                    case "0":
                        chars += "\0"
                self.advance()
            else:
                chars += self.c
                self.advance()

        return Token(TT_STRING, chars)

    def get_char(self):
        self.advance()

        if self.c is None:
            print("EXPECTED CHARACTER AFTER `'`")
            return

        if self.c == '\\':
            self.advance()
            if self.c is None:
                print("EXPECTED CHARACTER AFTER `\\` IN CHARACTER LITERAL")
                return
            cmap = {
                "\\": ord("\\"),
                "n": ord("\n"),
                "t": ord("\t"),
                "a": ord("\a"),
                "e": ord("\x1b"),
                "0": 0,
            }
            c = cmap.get(self.c, 0)
        else:
            c = ord(self.c)

        self.advance()

        if self.c != "'":
            print("EXPECTED `'` AFTER CHARACTER LITERAL")
            return

        self.advance()

        return Token(TT_CONST, c)

class CodeGenerator:
    def __init__(self, tokens: list[Token], verbose = False):
        self.tokens = tokens
        self.i = -1
        self.t = None
        self.verbose = verbose

        self.advance()

    def advance(self):
        self.i += 1

        if self.i < len(self.tokens):
            self.t = self.tokens[self.i]
        else:
            self.t = None

    @staticmethod
    def int_to_2bytes(i: int):
        return [i % 256, (i >> 8) % 256]

    @staticmethod
    def int_to_4bytes(i: int):
        return [i % 256, (i >> 8) % 256, (i >> 16) % 256, (i >> 24) % 256]

    def get_bytes(self):
        @dataclass
        class ValuePostReplacement:
            address: int
            size: int
            name: str

        out: list[int] = []
        names: dict[str, int] = {}
        current_label_scope = ""
        value_post_replacements: list[ValuePostReplacement] = []

        def resolve_label(label: str):
            if label.startswith("."):
                return current_label_scope + label
            return label

        if self.verbose: print("-" * 50)

        while self.t != None:
            if self.t.t == TT_LDEF:
                name = str(self.t.v)
                if name.startswith("."):
                    name = current_label_scope + name
                else:
                    current_label_scope = name
                if self.verbose: print(f"NEW LABEL {name} AT {len(out)}")
                names[name] = len(out)
                self.advance()
            elif self.t.t == TT_INST:
                inst_tok = self.t

                self.advance()

                inst_data = INST_FORMATS[inst_tok.v]
                if self.verbose: print(inst_tok.v, "-", inst_data)

                out.append(inst_data.opcode)

                for arg_req in inst_data.args:
                    if self.t is None:
                        print(f"Expected argument after {(inst_tok.v)}")
                        return
                    elif self.t.t == TT_CONST:
                        if arg_req == IA_1B:
                            out.append(self.t.v % 256)
                        if arg_req == IA_2B:
                            out += self.int_to_2bytes(self.t.v)
                        if arg_req == IA_4B:
                            out += self.int_to_4bytes(self.t.v)

                        self.advance()
                    elif self.t.t == TT_IDEN:
                        if arg_req == IA_1B:
                            value_post_replacements.append(ValuePostReplacement(len(out), 1, resolve_label(self.t.v)))
                            out += [0xFF]
                        if arg_req == IA_2B:
                            value_post_replacements.append(ValuePostReplacement(len(out), 2, resolve_label(self.t.v)))
                            out += [0xFF, 0xFF]
                        if arg_req == IA_4B:
                            value_post_replacements.append(ValuePostReplacement(len(out), 4, resolve_label(self.t.v)))
                            out += [0xFF, 0xFF, 0xFF, 0xFF]

                        self.advance()
                    else:
                        print(f"Instrution argument can't be type {repr(self.t.t)}")
                        return
            elif self.t.t == TT_DIR_DUMP8:
                self.advance()

                if self.t is None:
                    print("Expected constant or identifier after DUMP8 (D8)")
                    return
                
                if self.t.t == TT_CONST:
                    out.append(self.t.v % 256)
                elif self.t.t == TT_IDEN:
                    value_post_replacements.append(ValuePostReplacement(len(out), 1, resolve_label(self.t.v)))
                    out += [0xFF]
                self.advance()
            elif self.t.t == TT_DIR_DUMP16:
                self.advance()

                if self.t is None:
                    print("Expected constant or identifier after DUMP16 (D16)")
                    return

                if self.t.t == TT_CONST:
                    out += self.int_to_2bytes(self.t.v)
                elif self.t.t == TT_IDEN:
                    value_post_replacements.append(ValuePostReplacement(len(out), 2, resolve_label(self.t.v)))
                    out += [0xFF, 0xFF]
                self.advance()
            elif self.t.t == TT_DIR_DUMP32:
                self.advance()

                if self.t is None:
                    print("Expected constant or identifier after DUMP32 (D32)")
                    return

                if self.t.t == TT_CONST:
                    out += self.int_to_4bytes(self.t.v)
                elif self.t.t == TT_IDEN:
                    value_post_replacements.append(ValuePostReplacement(len(out), 4, resolve_label(self.t.v)))
                    out += [0xFF, 0xFF, 0xFF, 0xFF]
                self.advance()
            elif self.t.t == TT_DIR_STR:
                self.advance()

                if self.t is None:
                    print("Expected string after STR")
                    return

                if self.t.t == TT_STRING:
                    for c in self.t.v:
                        utf = ord(c)
                        if (utf < 256):
                            out.append(utf)
                        else:
                            print(f"Character {repr(c)} can't be converted to extended ASCII (index over 255), inserting 0x00")
                            out.append(0)
                self.advance()
            elif self.t.t == TT_DIR_ORG:
                self.advance()

                if self.t is None:
                    print("Expected constant after ORG")
                    return

                if self.t.t == TT_CONST:
                    while len(out) < self.t.v:
                        out.append(0x00)
                
                self.advance()
            elif self.t.t == TT_DIR_FILL:
                self.advance()

                if self.t is None:
                    print("Expected constant after FILL")
                    return

                if self.t.t == TT_CONST:
                    out += [0x00 for _ in range(self.t.v)]

                self.advance()
            else:
                print(f"UNKNOWN TOKEN {repr(self.t.t)}:{repr(self.t.v)}")
                self.advance()
                return

        # Post Value Replacement

        if self.verbose: print("\n----- NAMED REPLACEMENTS -----")

        for replace in value_post_replacements:
            if replace.name in names:
                if self.verbose: print(replace)
                value = names[replace.name]
                number = []
                if replace.size == 1:
                    number = [value % 256]
                elif replace.size == 2:
                    number = self.int_to_2bytes(value)
                elif replace.size == 4:
                    number = self.int_to_4bytes(value)

                for o, b, in enumerate(number):
                    out[replace.address + o] = b
            else:
                print(f"Name {repr(replace.name)} does not exist.")

        if self.verbose: print("\n" + "-" * 50)

        return bytes(out)

if __name__ == "__main__":
    from sys import argv
    args = argv[1:]

    input_files = []
    output_file = "out.bin"

    verbose = False

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

    if verbose: print(f"Input files: {", ".join(input_files)}")
    if verbose: print(f"Output file: {output_file}")

    input_text: str = ""

    for ifile in input_files:
        try:
            with open(ifile, "r") as f:
                cont = f.read()
                input_text += f"; {'-' * 50}\n; FILE {ifile}\n; {'-' * 50}\n" + cont + "\n"
            if verbose: print(f"Opened file {ifile}, ({cont.count("\n") + 1} lines)")
        except Exception as e:
            print(f"Failed to open file {ifile}")
            if verbose: print(e)

    if verbose: print(f"\x1b[90m{input_text}\x1b[0m")

    lexer = Lexer(input_text, verbose)
    tokens = lexer.get_tokens()

    if tokens is None:
        exit(1)

    if verbose: print(", ".join(
        map(
            lambda a: str(a),
            tokens
        )
    ))

    code_gen = CodeGenerator(tokens, verbose)
    out = code_gen.get_bytes()

    if out is None:
        exit(1)

    with open(output_file, "wb") as of:
        of.write(out)
