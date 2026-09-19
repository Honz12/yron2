from dataclasses import dataclass
from typing import Any

TT_INT = "int"
TT_STRING = "string"

TT_SEMI = "semi"
TT_COMMA = "comma"

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
TT_KW_ELSE = "kw_else"
TT_KW_WHILE = "kw_while"
TT_KW_RETURN = "kw_return"

TT_KW_LOC = "kw_loc"

TT_KW_ALC = "kw_alc"

@dataclass
class Token:
    t: str
    v: Any
    start: Position
    end: Position

    def __str__(self):
        if self.v is not None:
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

@dataclass
class CompilerError:
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

    "func": TT_KW_FUNC,
    "if": TT_KW_IF,
    "else": TT_KW_ELSE,
    "while": TT_KW_WHILE,
    "return": TT_KW_RETURN,

    "loc": TT_KW_LOC,

    "alc": TT_KW_ALC,
}

SYMBOL_MAP = {
    ";": TT_SEMI,
    ",": TT_COMMA,

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

OPT_CAN_SIMPLIFY_EXPRESIONS = True
OPT_CAN_REMOVE_STANDALONE_LITERALS = True
OPT_CAN_SIMPLIFY_IF_STATEMENTS = True
OPT_INCLUDE_STD_CODE = True

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
                error = CompilerError(f"LEXER - Got unepected character {repr(self.c)}.", self.pos)
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

@dataclass
class VariableData:
    name: str
    size: int
    signed: bool

    def __str__(self):
        o = "VariableData"
        o += AstNode.format_as_child(repr(self.name), False, "NAME")
        o += AstNode.format_as_child(self.size, False, "SIZE")
        o += AstNode.format_as_child(self.signed, True, "SIGNED")

        return o

@dataclass
class AstNode:
    start_pos: Position
    end_pos: Position

    @staticmethod
    def format_as_child(node: object, last: bool = False, label: str = None):
        s = str(node)
        if label:
            if last:
                s = s.replace("\n", "\n        ")
                return f"\n\x1b[90m ╰─ \x1b[0m{label}\n\x1b[90m     ╰─ \x1b[0m{s}"
            else:
                s = s.replace("\n", "\n\x1b[90m │      \x1b[0m")
                return f"\n\x1b[90m ├─ \x1b[0m{label}\n\x1b[90m │   ╰─ \x1b[0m{s}"
        else:
            if last:
                s = s.replace("\n", "\n    ")
                return f"\n\x1b[90m ╰─ \x1b[0m{s}"
            else:
                s = s.replace("\n", "\n\x1b[90m │  \x1b[0m")
                return f"\n\x1b[90m ├─ \x1b[0m{s}"

    def optimize(self):
        return self

    def force_optimize(self):
        return self

@dataclass
class LiteralIntNode(AstNode):
    number: int

    def __str__(self):
        o = f"LiteralInt ({self.number})"
        return o

@dataclass
class BinOpNone(AstNode):
    left: AstNode
    right: AstNode
    optok: Token

    def __str__(self):
        o = f"BinOp {self.optok}"
        o += self.format_as_child(self.left)
        o += self.format_as_child(self.right, True)
        return o

    def optimize(self):
        self.left = self.left.optimize()
        self.right = self.right.optimize()
        
        if isinstance(self.left, LiteralIntNode) and isinstance(self.right, LiteralIntNode) and OPT_CAN_SIMPLIFY_EXPRESIONS:
            if self.optok.t == TT_PLUS:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number + self.right.number)
            if self.optok.t == TT_MINUS:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number - self.right.number)
            if self.optok.t == TT_MUL:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number * self.right.number)
            if self.optok.t == TT_DIV:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number // self.right.number if self.right.number != 0 else 0)
            if self.optok.t == TT_MOD:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number % self.right.number)
            if self.optok.t == TT_EQUAL:
                return LiteralIntNode(self.start_pos, self.end_pos, 1 if self.left.number == self.right.number else 0)
            if self.optok.t == TT_LESSER:
                return LiteralIntNode(self.start_pos, self.end_pos, 1 if self.left.number > self.right.number else 0)
            if self.optok.t == TT_GREATER:
                return LiteralIntNode(self.start_pos, self.end_pos, 1 if self.left.number < self.right.number else 0)
            
        if isinstance(self.left, LiteralIntNode) and isinstance(self.right, VariableReferenceNode) and OPT_CAN_SIMPLIFY_EXPRESIONS:
            if self.optok.t == TT_MUL and self.left.number == 0:
                return LiteralIntNode(self.start_pos, self.end_pos, 0)
            
        if isinstance(self.left, VariableReferenceNode) and isinstance(self.right, LiteralIntNode) and OPT_CAN_SIMPLIFY_EXPRESIONS:
            if self.optok.t == TT_MUL and self.right.number == 0:
                return LiteralIntNode(self.start_pos, self.end_pos, 0)
        return super().optimize()

    def force_optimize(self):
        self.left = self.left.optimize()
        self.right = self.right.optimize()
        
        if isinstance(self.left, LiteralIntNode) and isinstance(self.right, LiteralIntNode):
            if self.optok.t == TT_PLUS:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number + self.right.number)
            if self.optok.t == TT_MINUS:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number - self.right.number)
            if self.optok.t == TT_MUL:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number * self.right.number)
            if self.optok.t == TT_DIV:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number // self.right.number if self.right.number != 0 else 0)
            if self.optok.t == TT_MOD:
                return LiteralIntNode(self.start_pos, self.end_pos, self.left.number % self.right.number)
            if self.optok.t == TT_EQUAL:
                return LiteralIntNode(self.start_pos, self.end_pos, 1 if self.left.number == self.right.number else 0)
            if self.optok.t == TT_LESSER:
                return LiteralIntNode(self.start_pos, self.end_pos, 1 if self.left.number > self.right.number else 0)
            if self.optok.t == TT_GREATER:
                return LiteralIntNode(self.start_pos, self.end_pos, 1 if self.left.number < self.right.number else 0)
            
        if isinstance(self.left, LiteralIntNode) and isinstance(self.right, VariableReferenceNode):
            if self.optok.t == TT_MUL and self.left.number == 0:
                return LiteralIntNode(self.start_pos, self.end_pos, 0)
            
        if isinstance(self.left, VariableReferenceNode) and isinstance(self.right, LiteralIntNode):
            if self.optok.t == TT_MUL and self.right.number == 0:
                return LiteralIntNode(self.start_pos, self.end_pos, 0)
        return super().optimize()


@dataclass
class ProgramNode(AstNode):
    statements: list[AstNode]

    def __str__(self):
        o = "Program"

        for i, s in enumerate(self.statements):
            o += self.format_as_child(s, i == len(self.statements) - 1)

        return o

    def optimize(self):
        i = 0
        while i < len(self.statements):
            self.statements[i] = self.statements[i].optimize()
            if isinstance(self.statements[i], LiteralIntNode) and OPT_CAN_REMOVE_STANDALONE_LITERALS:
                self.statements.pop(i)
                i -= 1
            elif isinstance(self.statements[i], ProgramNode):
                if len(self.statements[i].statements) == 0:
                    self.statements.pop(i)
                    i -= 1
            i += 1
        return super().optimize()


@dataclass
class VariableDeclarationNode(AstNode):
    data: VariableData
    value: AstNode | None = None

    def __str__(self):
        o = "VariableDeclaration"
        o += self.format_as_child(repr(self.data.name), False, "NAME")
        o += self.format_as_child(f"{self.data.size} bytes", False, "SIZE")
        if self.value:
            o += self.format_as_child("SIGNED" if self.data.signed else "UNSIGNED")
            o += self.format_as_child(self.value, True, "VALUE")
        else:
            o += self.format_as_child("SIGNED" if self.data.signed else "UNSIGNED", True)
        return o

    def optimize(self):
        if self.value:
            self.value = self.value.optimize()
        return super().optimize()

@dataclass
class VariableReferenceNode(AstNode):
    name: str

    def __str__(self):
        o = "VariableReference"
        o += self.format_as_child(repr(self.name), True)
        return o

@dataclass
class FunctionDeclarationNode(AstNode):
    name: str
    args: list[VariableData]
    body: AstNode

    def __str__(self):
        o = "FunctionDeclaration"
        o += self.format_as_child(repr(self.name))
        args_str = ""
        for i, arg in enumerate(self.args):
            args_str += self.format_as_child(arg, i == len(self.args) - 1)
        if len(self.args) > 0:
            o += self.format_as_child("ARGS" + args_str, False)
        o += self.format_as_child(self.body, True)

        return o

    def optimize(self):
        self.body = self.body.optimize()
        return super().optimize()

@dataclass
class FunctionCallNode(AstNode):
    args: list[AstNode]
    name: str

    def __str__(self):
        o = "FunctionCall"
        if len(self.args) > 0:
            o += self.format_as_child(repr(self.name))
            args_str = "ARGS"
            for i, a in enumerate(self.args):
                args_str += self.format_as_child(a, i == len(self.args) - 1)
            o += self.format_as_child(args_str, True)
        else:
            o += self.format_as_child(repr(self.name), True)

        return o

@dataclass
class LocationOfSymbolNode(AstNode):
    name: str

    def __str__(self):
        o = "LocationOfSymbol"
        o += self.format_as_child(repr(self.name), True)

        return o

@dataclass
class IfStatementNode(AstNode):
    condition: AstNode
    if_branch: AstNode
    else_branch: AstNode | None = None

    def __str__(self):
        o = "IfStatement"
        o += self.format_as_child(self.condition, False, "CONDITION")
        if self.else_branch:
            o += self.format_as_child(self.if_branch, False, "IF BLOCK")
            o += self.format_as_child(self.else_branch, True, "ELSE BLOCK")
        else:
            o += self.format_as_child(self.if_branch, True, "IF BLOCK")
        return o

    def optimize(self):
        self.condition = self.condition.optimize()
        self.if_branch = self.if_branch.optimize()
        if self.else_branch:
            self.else_branch = self.else_branch.optimize()
        if isinstance(self.condition, LiteralIntNode) and OPT_CAN_SIMPLIFY_IF_STATEMENTS:
            if self.condition.number != 0:
                return self.if_branch
            if self.condition.number == 0:
                if self.else_branch:
                    return self.else_branch
                return ProgramNode(self.start_pos, self.end_pos, [])
        return super().optimize()

@dataclass
class AllocateSpaceNode(AstNode):
    space: AstNode
    cells: list[AstNode]

VARIBLE_TYPES = {
    TT_KW_U8: (1, False),
    TT_KW_U16: (2, False),
    TT_KW_U32: (4, False),
    TT_KW_I8: (1, True),
    TT_KW_I16: (2, True),
    TT_KW_I32: (4, True),
}

class Parser:
    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.t = None
        self.i = -1
        self.t_spos = Position(0, 0, 0, "")
        self.t_epos = Position(0, 0, 0, "")

        self.advance()

    def advance(self):
        self.i += 1
        if self.i < len(self.tokens):
            self.t = self.tokens[self.i]
            self.t_spos = self.t.start
            self.t_epos = self.t.end
        else:
            self.t = None

    def get_ast(self):
        ast = ProgramNode(self.t_spos, None, [])

        while self.t is not None:
            ast.statements.append(self.make_statement())

        ast.end_pos = self.t_epos

        return ast

    def make_statement(self) -> AstNode:
        base_token = self.t
        
        if base_token.t in VARIBLE_TYPES:
            self.advance()
            return self.make_var_decl(base_token, VARIBLE_TYPES[base_token.t][0], VARIBLE_TYPES[base_token.t][1])

        elif base_token.t == TT_KW_IF:
            self.advance()

            self.consume(TT_LPAREN)

            condition = self.make_expr()

            self.consume(TT_RPAREN)

            if_branch = self.make_statement()

            else_branch = None

            if self.t is not None:
                if self.t.t == TT_KW_ELSE:
                    self.advance()
                    else_branch = self.make_statement()

            return IfStatementNode(base_token.start, (else_branch if else_branch else if_branch).end_pos, condition, if_branch, else_branch)

        elif base_token.t == TT_KW_FUNC:
            self.advance()

            iden = self.consume(TT_IDEN)

            self.consume(TT_LPAREN)

            args: list[VariableData] = []

            if self.t is not None:
                if self.t.t != TT_RPAREN:
                    while self.t is not None:
                        if self.t.t in VARIBLE_TYPES:
                            dat = VARIBLE_TYPES[self.t.t]
                            self.advance()

                            arg_iden = self.consume(TT_IDEN)

                            args.append(VariableData(arg_iden.v, dat[0], dat[1]))
                        else:
                            CompilerError("PARSER - Expected varible type for argument declaration.", self.t.start).throw()

                        if self.t.t != TT_COMMA:
                            break
                        self.consume(TT_COMMA)

            self.consume(TT_RPAREN)

            body = self.make_statement()

            return FunctionDeclarationNode(base_token.start, body.end_pos, iden.v, args, body)

        elif base_token.t == TT_LBRACE:
            self.advance()

            block = ProgramNode(base_token.start, None, [])

            while self.t is not None:
                if self.t.t == TT_RBRACE:
                    block.end_pos = self.t.end
                    self.advance()
                    break
                block.statements.append(self.make_statement())

            if block.end_pos is None:
                CompilerError("PARSER - Expected `}` after code block", self.t_epos).throw()

            return block

        elif base_token.t in (TT_INT, TT_LPAREN, TT_IDEN, TT_KW_LOC, TT_KW_ALC):
            expr = self.make_expr()
            self.consume(TT_SEMI)
            return expr

        else:
            CompilerError(f"PARSER - Token {str(self.t)} can't start a statement.", self.t.start).throw()

    def make_var_decl(self, base_token: Token, size: int, signed: bool):
        iden = self.consume(TT_IDEN)
        semi_or_assign = self.consume(TT_SEMI, TT_ASSIGN)

        if semi_or_assign.t == TT_SEMI:
            return VariableDeclarationNode(base_token.start, semi_or_assign.end, VariableData(iden.v, size, signed))

        # user has `=` after identifier

        expr = self.make_expr()

        end_pos = self.consume(TT_SEMI).end

        return VariableDeclarationNode(base_token.start, end_pos, VariableData(iden.v, size, signed), expr)

    def make_expr(self):
        return self.make_com_ops()

    def make_bin_op(self, get_sub_method, *allowed_ops: str):
        left = get_sub_method()

        while self.t is not None:
            if self.t.t not in allowed_ops:
                break

            op = self.t
            self.advance()

            right = get_sub_method()
            left = BinOpNone(left.start_pos, right.end_pos, left, right, op)

        return left

    def make_com_ops(self):
        return self.make_bin_op(self.make_mul_div_mod, TT_EQUAL, TT_LESSER, TT_GREATER)

    def make_mul_div_mod(self):
        return self.make_bin_op(self.make_add_sub, TT_MUL, TT_DIV, TT_MOD)

    def make_add_sub(self):
        return self.make_bin_op(self.make_factor, TT_PLUS, TT_MINUS)

    def make_factor(self):
        if self.t.t == TT_INT:
            t = self.t
            self.advance()
            return LiteralIntNode(t.start, t.end, t.v)
        if self.t.t == TT_LPAREN:
            op = self.t
            self.advance()
            expr = self.make_expr()
            cp = self.consume(TT_RPAREN)
            expr.start_pos = op.start
            expr.end_pos = cp.end
            return expr
        if self.t.t == TT_IDEN:
            t = self.t
            self.advance()
            if self.t is not None:
                if self.t.t == TT_LPAREN:
                    self.advance()

                    args: list[AstNode] = []

                    while self.t is not None:
                        if self.t.t == TT_RPAREN:
                            break
                        args.append(self.make_expr())
                        if self.t.t != TT_COMMA:
                            break
                        self.consume(TT_COMMA)

                    rparen = self.consume(TT_RPAREN)

                    return FunctionCallNode(t.start, rparen.end, args, t.v)
            return VariableReferenceNode(t.start, t.end, t.v)

        if self.t.t == TT_KW_LOC:
            t = self.t
            self.advance()

            self.consume(TT_LPAREN)
            iden = self.consume(TT_IDEN)
            end = self.consume(TT_RPAREN).end

            return LocationOfSymbolNode(t.start, end, iden.v)

        if self.t.t == TT_KW_ALC:
            t = self.t
            self.advance()

            self.consume(TT_LPAREN)
            expr = self.make_expr().force_optimize()

            values = []

            while self.t is not None:
                if self.t.t != TT_COMMA:
                    break
                self.consume(TT_COMMA)

                byte = self.make_expr().force_optimize()
                values.append(byte)

            end = self.consume(TT_RPAREN).end

            return AllocateSpaceNode(t.start, end, expr, values)
        
        CompilerError(f"PARSER - Unexpected token in factor {str(self.t)}.", self.t.start)

    def consume(self, *types: str):
        t = self.t

        allowed = ", ".join(
            map(
                lambda a: a.upper(),
                types
            )
        )

        if self.t is None:
            if len(types) > 1:
                CompilerError(f"PARSER - Expected any of: {allowed}", self.t_epos).throw()
            
            CompilerError(f"PARSER - Expected {allowed}", self.t_epos).throw()

        if self.t.t not in types:
            if len(types) > 1:
                CompilerError(f"PARSER - Expected any of: {allowed}", self.t_epos).throw()
            
            CompilerError(f"PARSER - Expected {allowed}", self.t_epos).throw()

        self.advance()
        return t

@dataclass
class VariableSymbol:
    data: VariableData
    location: int

@dataclass
class Scope:
    symbols: list[VariableSymbol]
    parent: Scope | None = None

    def search_for_symbol(self, name: str):
        for s in self.symbols:
            if s.data.name == name:
                return s
        if self.parent is None:
            return CompilerError(f"SCOPE - Can't find symbol {repr(name)}.", None)
        return self.parent.search_for_symbol(name)

PROGRAM_ENTRY_FUNCTION = "main"

NEEDED_CODE = f"""
jmp {PROGRAM_ENTRY_FUNCTION}

; [VARIABLES]

"""

STD_CODE = f"""
jmp {PROGRAM_ENTRY_FUNCTION}

; [VARIABLES]

; ----------------------------
; JUMP POINT
; halt
;
halt:
    jmp halt

; ----------------------------
; FUNCTION
; putc
;
putc:
    push32 0x11

    ldi8 0x11 1
    snd 0x11 0x10

    pop32 0x11
    ret

; ----------------------------
; FUNCTION
; res8
;
res8:
    ldp8 0x1f 0x10
    ret

; ----------------------------
; FUNCTION
; res16
;
res16:
    ldp16 0x1f 0x10
    ret

; ----------------------------
; FUNCTION
; res32
;
res32:
    ldp32 0x1f 0x10
    ret

; ----------------------------
; FUNCTION
; wrt8
;
wrt8:
    stp8 0x10 0x11

; ----------------------------
; FUNCTION
; wrt16
;
wrt16:
    stp16 0x10 0x11

; ----------------------------
; FUNCTION
; wrt32
;
wrt32:
    stp32 0x10 0x11
""".strip() + "\n\n"

class CodeGenerator:
    def __init__(self):
        self.scope = Scope([])
        self.allocator_bytes = []
        self.generated = STD_CODE if OPT_INCLUDE_STD_CODE else NEEDED_CODE
        self.current_indent = 0
        self.verb_output = ""

    def get_compiled(self, node: AstNode):
        err = self.generate(node)
        self.generated = self.generated.replace(
            "; [VARIABLES]",
            "\n".join([f"d8 {hex(b)}" for b in self.allocator_bytes])
        )
        return self.generated, err

    def push_scope(self):
        self.scope = Scope([], self.scope)

    def pop_scope(self):
        self.scope = self.scope.parent

    def allocate_variable(self, data: VariableData):
        self.display_compile_process("ALLOCATING VARIABLE" + AstNode.format_as_child(data, True))
        r = VariableSymbol(data, len(self.allocator_bytes) + 0x05)
        self.allocator_bytes += [0x00 for _ in range(data.size)]
        return r

    def append(self, *values: str):
        self.generated += (" " * self.current_indent * 4) + "".join(map(str, values)) + "\n"

    def indent(self, i):
        self.current_indent += i
    
    def display_compile_process(self, s: str):
        self.verb_output += AstNode.format_as_child(s)

    def generate(self, node: AstNode=0) -> CompilerError | None:
        method_name = f"gen_{type(node).__name__}"
        self.display_compile_process("COMPILING NODE" + AstNode.format_as_child(method_name, True))
        generator = getattr(self, method_name, self.generic_gen)
        return generator(node)

    def generic_gen(self, node: AstNode):
        return CompilerError(f"CODE GEN - AST node {type(node).__name__} has no compilation handler.", node.start_pos)

    def gen_ProgramNode(self, node: ProgramNode):
        self.push_scope()
        for s in node.statements:
            err = self.generate(s)
            if err: return err
        self.pop_scope()

    def gen_FunctionDeclarationNode(self, node: FunctionDeclarationNode):
        self.append("; ".ljust(30, "-"))
        if node.name == PROGRAM_ENTRY_FUNCTION:
            self.append("; ENTRY POINT")
        else:
            self.append("; FUNCTION")
        self.append("; ", node.name)
        self.append(";")
        self.append(node.name, ":")
        self.indent(1)
        body_err = self.generate(node.body)
        if node.name == PROGRAM_ENTRY_FUNCTION:
            self.append("jmp halt")
        else:
            self.append("ret")
        self.indent(-1)
        self.append()
        return body_err

    def resolve_expr_into_reg(self, node: AstNode, reg: int):
        if isinstance(node, LiteralIntNode):
            self.append("ldi32 ", hex(reg), " ", node.number)
        elif isinstance(node, VariableReferenceNode):
            symbol = self.scope.search_for_symbol(node.name)

            if isinstance(symbol, CompilerError):
                symbol.position = node.start_pos
                return symbol
            if isinstance(symbol, VariableSymbol):
                if symbol.data.size == 1:
                    self.append("ld8 ", hex(reg), " ", hex(symbol.location))
                if symbol.data.size == 2:
                    self.append("ld16 ", hex(reg), " ", hex(symbol.location))
                if symbol.data.size == 4:
                    self.append("ld32 ", hex(reg), " ", hex(symbol.location))
        elif isinstance(node, BinOpNone):
            gerr = self.resolve_expr_into_reg(node.left, 0x0d)
            if gerr: return gerr

            if isinstance(node.right, BinOpNone):
                self.append("push32 0x0d")

            gerr = self.resolve_expr_into_reg(node.right, 0x0e)
            if gerr: return gerr

            if isinstance(node.right, BinOpNone):
                self.append("pop32 0x0d")

            self.append("add 0x0d 0x0e ", hex(reg))
        elif isinstance(node, FunctionCallNode):
            gerr = self.generate(node)
            if gerr: return gerr

            if reg != 0x1f: self.append("mov 0x1f ", hex(reg))
        elif isinstance(node, LocationOfSymbolNode):
            symbol = self.scope.search_for_symbol(node.name)
            if isinstance(symbol, CompilerError):
                symbol.position = node.start_pos
                return symbol
            self.append("ldi32 ", hex(reg), " ", symbol.location)
        elif isinstance(node, AllocateSpaceNode):
            space = node.space

            if not isinstance(space, LiteralIntNode):
                return CompilerError("CODE GEN - Expected argument of alc(...) to be LiteralInt.", space.start_pos)
            
            self.append("ldi32 ", hex(reg), " ", hex(len(self.allocator_bytes) + 0x05))
            self.allocator_bytes += [((node.cells[i].number if isinstance(node.cells[i], LiteralIntNode) else 0x00) if i < len(node.cells) else 0x00) for i in range(space.number)]
        else:
            return CompilerError(f"CODE GEN - AST node {type(node).__name__} can't be an expression.", node.start_pos)

    def gen_FunctionCallNode(self, node: FunctionCallNode):
        for i, a in enumerate(node.args):
            gerr = self.resolve_expr_into_reg(a, i + 0x10)
            if gerr:
                return gerr
        self.append("call ", node.name)
    
    def gen_VariableDeclarationNode(self, node: VariableDeclarationNode):
        allocated = self.allocate_variable(node.data)
        self.append()
        self.append("; Variable ", repr(node.data.name), " declaration, allocated to ", hex(allocated.location))
        if node.value is not None:
            self.scope.symbols.append(allocated)
            gerr = self.resolve_expr_into_reg(node.value, 0x0f)
            if gerr: return gerr
            if allocated.data.size == 1:
                self.append("st8 ", hex(0x0f), " ", hex(allocated.location))
            if allocated.data.size == 2:
                self.append("st16 ", hex(0x0f), " ", hex(allocated.location))
            if allocated.data.size == 4:
                self.append("st32 ", hex(0x0f), " ", hex(allocated.location))
        self.append()

if __name__ == "__main__":
    from sys import argv
    command_line_args = argv[1:]

    input_files = []
    output_file = "out.bin"

    verbose = False

    multi_arg_mode = ""

    for a in command_line_args:
        if multi_arg_mode == "-o":
            output_file = a
            multi_arg_mode = ""

        elif multi_arg_mode == "--can-simplify-expresions":
            OPT_CAN_SIMPLIFY_EXPRESIONS = a in ("True", "true", "1", "t")
            multi_arg_mode = ""

        elif multi_arg_mode == "--can-remove-standalone":
            OPT_CAN_REMOVE_STANDALONE_LITERALS = a in ("True", "true", "1", "t")
            multi_arg_mode = ""

        elif multi_arg_mode == "--can-simplify-if":
            OPT_CAN_SIMPLIFY_IF_STATEMENTS = a in ("True", "true", "1", "t")
            multi_arg_mode = ""

        elif multi_arg_mode == "--include-std-code":
            OPT_INCLUDE_STD_CODE = a in ("True", "true", "1", "t")
            multi_arg_mode = ""

        elif a == "-o":
            multi_arg_mode = "-o"

        elif a == "--can-simplify-expresions":
            multi_arg_mode = "--can-simplify-expresions"

        elif a == "--can-remove-standalone":
            multi_arg_mode = "--can-remove-standalone"

        elif a == "--can-simplify-if":
            multi_arg_mode = "--can-simplify-if"

        elif a == "--include-std-code":
            multi_arg_mode = "--include-std-code"

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
            error = CompilerError(f"PREPROCESSOR - Failed to read file {repr(ifile)}.", None)

    if verbose:
        print(", ".join(
            map(
                lambda a: str(a),
                tokens
            )
        ))

    parser = Parser(tokens)

    ast = parser.get_ast()

    TERMINAL_WIDTH = 60

    if verbose:
        no_opt_ast = str(ast)

        ast.optimize()

        opt_ast = str(ast)
        PAGES = True

        if PAGES:
            no_opt_ast = no_opt_ast.split("\n")
            opt_ast = opt_ast.split("\n")
            print(" Unoptimized ".center((TERMINAL_WIDTH - 4) // 2, "=") + " │ " + " Optimized ".center((TERMINAL_WIDTH - 4) // 2, "="))

            for i in range(max(len(no_opt_ast), len(opt_ast))):
                if i < len(no_opt_ast): print(no_opt_ast[i] + " " * (TERMINAL_WIDTH - len(no_opt_ast[i].replace("\x1b[90m", "").replace("\x1b[0m", ""))), end=" │ ")
                else: print(" " * TERMINAL_WIDTH, end=" │ ")
                if i < len(opt_ast): print(no_opt_ast[i] + " " * (TERMINAL_WIDTH - len(opt_ast[i].replace("\x1b[90m", "").replace("\x1b[0m", ""))))
                else: print(" " * TERMINAL_WIDTH)
        else:
            print(" Unoptimized ".center(TERMINAL_WIDTH, "="))
            print(no_opt_ast)
            print("\n" + " Optimized ".center(TERMINAL_WIDTH, "="))
            print(opt_ast)
    else:
        ast.optimize()

    code_gen = CodeGenerator()

    generated, cgen_error = code_gen.get_compiled(ast)
    if verbose:
        print("COMPILING OUTPUT", end="")
        print(code_gen.verb_output)
        print("FINISHED WITH ERROR" if cgen_error else "FINISHED SUCCESFULLY")

    if cgen_error:
        print("\n")
        cgen_error.throw()

    if verbose:
        print("\n")
        print(" CODE GENERATED ".center(TERMINAL_WIDTH, "="))

    with open(output_file, "w") as f:
        f.write(generated)
