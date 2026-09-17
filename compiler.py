from dataclasses import dataclass
from typing import Any

TT_INT = "int"
TT_STRING = "string"

TT_SEMI = "semi"

TT_PLUS = "plus"
TT_MINUS = "minus"
TT_MUL = "mul"
TT_DIV = "div"
TT_MOD = "mod"
TT_LESSER = "lesser"
TT_GREATER = "greater"
TT_EQUAL = "equal"

TT_LPAREN = "lparen"
TT_RPAREN = "rparen"

TT_LBRACE = "lbrace"
TT_RBRACE = "rbrace"

TT_LBRACKET = "lbracket"
TT_RBRACKET = "rbracket"

TT_ASSIGN = "assign"

TT_IDEN = "iden"

TT_KW_U8 = "kw_u8"
TT_KW_U16 = "kw_u16"
TT_KW_U32 = "kw_u32"
TT_KW_I8 = "kw_i8"
TT_KW_I16 = "kw_i16"
TT_KW_I32 = "kw_i32"

TT_KW_FUNC = "kw_func"
TT_KW_IF = "kw_if"
TT_KW_WHILE = "kw_while"

@dataclass
class Token:
    t: str
    v: Any
    start: Position
    end: Position

    def __str__(self):
        if self.v:
            return f"({self.t.upper()}:{repr(self.v)})"
        return f"({self.t.upper()})"

@dataclass
class Position:
    i: int
    row: int
    col: int
    file: str

    def copy(self):
        return Position(self.i, self.row, self.col, self.file)

CERR_FAILED_FILE_READ = "Lexer - File Read"
CERR_UNEXPECTED_CHARACTER = "Lexer - Unexpected Character"

@dataclass
class CompilerError:
    iden: str
    message: str
    position: Position | None

    def throw(self):
        if self.position:
            print(f"\x1b[31m\x1b[1m{self.position.file}:{self.position.row}:{self.position.col}: error: {self.message}")
        else:
            print(f"\x1b[31m\x1b[1m{self.position.file}: error: {self.message}")
        exit(1)

DIGITS = "0123456789"
HEX_CHARS = DIGITS + "abcdefABCDEF"
BIN_CHARS = "01"
OCT_CHARS = "01234567"
ALPHA = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"
ALNUM = ALPHA + DIGITS

KEYWORD_MAP = {
    "u8": TT_KW_U8,
    "u16": TT_KW_U16,
    "u32": TT_KW_U32,
    "i8": TT_KW_I8,
    "i16": TT_KW_I16,
    "i32": TT_KW_I32,
}

SYMBOL_MAP = {
    ";": TT_SEMI,
    "+": TT_PLUS,
    "-": TT_MINUS,
    "*": TT_MUL,
    "/": TT_DIV,
    "%": TT_MOD,
    "<": TT_LESSER,
    ">": TT_GREATER,

    "(": TT_LPAREN,
    ")": TT_RPAREN,
    "{": TT_LBRACE,
    "}": TT_RBRACE,
    "[": TT_LBRACKET,
    "]": TT_RBRACKET,
}

class Lexer:
    def __init__(self, code: str, file_name: str):
        self.code = code
        self.pos = Position(-1, 1, 0, file_name)
        self.c = None

        self.advance()

    def advance(self):
        self.pos.i += 1
        self.pos.col += 1
        if self.pos.i < len(self.code):
            self.c = self.code[self.pos.i]
            if self.c == "\n":
                self.pos.col = 1
                self.pos.row += 1
        else:
            self.c = None

    def get_tokens(self):
        tokens: list[Token] = []

        while self.c is not None:
            if self.c in " \n\t":
                self.advance()
            elif self.c in DIGITS:
                tokens.append(self.get_int())
            elif self.c in ALPHA:
                tokens.append(self.get_iden())
            elif self.c == '"':
                tokens.append(self.get_string())
            elif self.c == "'":
                tokens.append(self.get_char())
            elif self.c in SYMBOL_MAP:
                start_pos = self.pos.copy()
                token_type = SYMBOL_MAP[self.c]
                self.advance()
                tokens.append(Token(token_type, None, start_pos, self.pos.copy()))
            elif self.c == "=":
                start_pos = self.pos.copy()
                self.advance()
                if self.c == "=":
                    self.advance()
                    tokens.append(Token(TT_EQUAL, None, start_pos, self.pos.copy()))
                else:
                    tokens.append(Token(TT_ASSIGN, None, start_pos, self.pos.copy()))
            else:
                error = CompilerError(CERR_UNEXPECTED_CHARACTER, f"Got unepected character {repr(self.c)}.", self.pos)
                error.throw()

        return tokens


    def get_int(self):
        start_pos = self.pos.copy()

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

        return Token(TT_INT, int(str_repr, base), start_pos, self.pos.copy())

    def get_iden(self):
        start_pos = self.pos.copy()

        str_repr = ""

        while self.c is not None:
            if self.c not in ALNUM:
                break

            str_repr += self.c
            self.advance()

        if str_repr in KEYWORD_MAP:
            return Token(KEYWORD_MAP[str_repr], None, start_pos, self.pos.copy())
        return Token(TT_IDEN, str_repr, start_pos, self.pos.copy())

    def get_string(self):
        start_pos = self.pos.copy()

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

        return Token(TT_STRING, chars, start_pos, self.pos.copy())

    def get_char(self):
        start_pos = self.pos.copy()

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

        return Token(TT_INT, c, start_pos, self.pos.copy())

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

    tokens = []

    for ifile in input_files:
        try:
            with open(ifile, "r") as f:
                lexer = Lexer(f.read(), ifile)
                tokens += lexer.get_tokens()
        except:
            error = CompilerError(CERR_FAILED_FILE_READ, f"Failed to read file {repr(ifile)}.", None)

    if verbose: print(", ".join(
        map(
            lambda a: str(a),
            tokens
        )
    ))
