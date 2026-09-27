#!/bin/python3

from dataclasses import dataclass
from typing import Any, List, Dict, Tuple

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
    "LJMP":     InstructionData(0x4b, [IA_1B, IA_1B]),
    "SND":      InstructionData(0x50, [IA_1B, IA_1B]),
    "RCV":      InstructionData(0x51, [IA_1B, IA_1B]),
}

OPCODE_MAP = {data.opcode: (name, data) for name, data in INST_FORMATS.items()}

class Disassembler:
    def __init__(self, binary_data: bytes, verbose: bool = False):
        self.data = binary_data
        self.verbose = verbose

    @staticmethod
    def bytes_to_int(b: bytes) -> int:
        """Convert a sequence of bytes into an integer (little-endian)."""
        val = 0
        for i, byte in enumerate(b):
            val |= byte << (8 * i)
        return val

    def pass_one_collect_labels(self) -> dict[int, str]:
        """First pass: Scan the binary to find all jump and call target addresses."""
        labels = {}
        idx = 0
        length = len(self.data)

        while idx < length:
            opcode = self.data[idx]
            if opcode in OPCODE_MAP:
                name, data = OPCODE_MAP[opcode]
                idx += 1
                arg_values = []
                for arg_req in data.args:
                    size = 1 if arg_req == IA_1B else (2 if arg_req == IA_2B else 4)
                    if idx + size <= length:
                        val = self.bytes_to_int(self.data[idx:idx+size])
                        arg_values.append(val)
                        idx += size
                    else:
                        idx = length
                        break
                
                # Record target addresses for jump and call instructions
                if name in ("CALL", "JMP", "JZ", "JNZ"):
                    if arg_values:
                        target = arg_values[0]
                        if target < length and target not in labels:
                            labels[target] = f"loc_{target:08x}"
            else:
                idx += 1

        if self.verbose:
            print(f"Discovered {len(labels)} jump/call targets.")
        return labels

    def disassemble(self) -> str:
        """Second pass: Generate readable assembly code with labels and comments."""
        labels = self.pass_one_collect_labels()
        output_lines = []
        idx = 0
        length = len(self.data)

        output_lines.append("; Disassembled automatically using disassembler.py\n")

        while idx < length:
            # Insert label if current index is a known target
            if idx in labels:
                output_lines.append(f"\n{labels[idx]}:")

            opcode = self.data[idx]
            if opcode in OPCODE_MAP:
                name, data = OPCODE_MAP[opcode]
                inst_start = idx
                idx += 1
                arg_strs = []
                
                valid_inst = True
                for arg_req in data.args:
                    size = 1 if arg_req == IA_1B else (2 if arg_req == IA_2B else 4)
                    if idx + size <= length:
                        val = self.bytes_to_int(self.data[idx:idx+size])
                        # If a 4-byte argument matches a known label, substitute it
                        if size == 4 and val in labels:
                            arg_strs.append(labels[val])
                        else:
                            arg_strs.append(f"0x{val:0>2x}")
                        idx += size
                    else:
                        valid_inst = False
                        break

                if valid_inst:
                    args_formatted = " ".join(map(
                        lambda a: hex(a) if isinstance(a, int) else a,
                        arg_strs
                    ))
                    if args_formatted:
                        output_lines.append(f"    {name.lower():<8} {args_formatted}")
                    else:
                        output_lines.append(f"    {name.lower()}")
                else:
                    output_lines.append(f"    dump8     0x{opcode:02x}  ; [Truncated instruction]")
                    idx = inst_start + 1
            else:
                output_lines.append(f"    dump8     0x{opcode:02x}  ; [Unknown Opcode]")
                idx += 1

        return "\n".join(output_lines) + "\n"

if __name__ == "__main__":
    from sys import argv
    args = argv[1:]

    input_files = []
    output_file = "out.asm"
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

    if not input_files:
        print("Usage: python disassembler.py <binary_file> [-o <output_asm>] [-v]")
        exit(1)

    if verbose:
        print(f"Input binary files: {', '.join(input_files)}")
        print(f"Output assembly file: {output_file}")

    combined_binary = bytearray()
    for ifile in input_files:
        try:
            with open(ifile, "rb") as f:
                content = f.read()
                combined_binary.extend(content)
            if verbose:
                print(f"Loaded file {ifile} ({len(content)} bytes)")
        except Exception as e:
            print(f"Failed to read binary file {ifile}")
            if verbose:
                print(e)
            exit(1)

    disassembler = Disassembler(bytes(combined_binary), verbose=verbose)
    assembly_code = disassembler.disassemble()

    try:
        with open(output_file, "w") as of:
            of.write(assembly_code)
        print(f"Successfully disassembled {len(combined_binary)} bytes into {output_file}")
    except Exception as e:
        print(f"Failed to write output assembly file {output_file}")
        if verbose:
            print(e)
        exit(1)