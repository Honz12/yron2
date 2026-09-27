#!/bin/python3

"""
DOCS

- FS Header - first 32 bytes of disk
    - 16 bytes disk name
    - 4 bytes root directory file pointer
    - 4 bytes size of disk
    - 4 bytes first free section

- ROOT Directory
    - Entry
        - 24 bytes file name
        - 4 bytes start section
        - 4 bytes size
"""

TEXT_ENCODING = "utf-8"
SECTION_SIZE = 512
VERBOSE = False

import shlex
from dataclasses import dataclass
import os
import subprocess
import tempfile

def edit_text_in_editor(initial_text=""):
    # 1. Detect the user's preferred system editor, or fall back to a safe default
    # On Windows, 'notepad' is standard; on Unix systems, 'nano' or 'vi' is typically available
    default_editor = "notepad" if os.name == "nt" else "nano"
    editor = os.environ.get("EDITOR", default_editor)

    # 2. Create a temporary file and write the initial template text into it
    with tempfile.NamedTemporaryFile(
        suffix=".txt", delete=False, mode="w+", encoding="utf-8"
    ) as tf:
        tf.write(initial_text)
        temp_file_path = tf.name

    try:
        # 3. Launch the text editor and wait for the user to close it
        # split() ensures arguments like 'code --wait' are parsed correctly as list elements
        subprocess.run(editor.split() + [temp_file_path], check=True)
        
        # 4. Read the modified content back from the file
        with open(temp_file_path, "r", encoding="utf-8") as tf:
            edited_text = tf.read()

            return edited_text

    finally:
        # 5. Always delete the temporary file to avoid cluttering the system
        if os.path.exists(temp_file_path):
            os.remove(temp_file_path)

def get_u32_from_int(u: list[int]):
    return (
        u[0] |
        (u[1] << 8) |
        (u[2] << 16) |
        (u[3] << 24)
    )

class Disk:
    def __init__(self, data: list[int]):
        self.data = data
        self.size = len(data)

        self.seeking = 0
    
    @staticmethod
    def new(size: int):
        return Disk([0 for _ in range(size)])
    
    def seek(self, position: int):
        self.seeking = position
    
    def seek_section(self, section: int):
        self.seeking = section * SECTION_SIZE
    
    def write_byte(self, byte: int):
        if not 0 <= byte < 256:
            print(f"DISK INTERFACE: Value {byte} does not fit into a byte (0-255)")
            exit(1)
        self.data[self.seeking] = byte
        self.seeking += 1
    
    def write_data(self, data: str | bytes | list[int], padded: int = 0):
        if isinstance(data, bytes) or isinstance(data, list):
            for b in data:
                self.write_byte(b)
        if isinstance(data, str):
            self.write_data(bytes(data.ljust(padded, "\0"), TEXT_ENCODING), padded)
    
    def write_u32(self, u: int):
        self.write_byte(u % 256)
        self.write_byte((u >> 8) % 256)
        self.write_byte((u >> 16) % 256)
        self.write_byte((u >> 24) % 256)
    
    def write_u16(self, u: int):
        self.write_byte(u % 256)
        self.write_byte((u >> 8) % 256)
    
    def get_byte(self):
        r = self.data[self.seeking]
        self.seeking += 1
        return r
    
    def get_u32(self):
        return (
            self.get_byte() |
            (self.get_byte() << 8) |
            (self.get_byte() << 16) |
            (self.get_byte() << 24)
        )

    def get_str(self, size: int):
        chars = ""
        for _ in range(size):
            chars += chr(self.get_byte())
        return chars

def format_byte_size(i):
    endfixes = ["B", "KiB", "MiB", "GiB"]
    efid = 0
    while i >= 1024:
        i /= 1024
        efid += 1
    return f"{round(i, 2)} {endfixes[min(efid, len(endfixes) - 1)]}"

@dataclass
class FsHeader:
    disk_name: str
    root_dir_file_section: int
    disk_size: int
    first_free_section: int

    def get_bytes(self):
        return bytes(
            list(
                bytes(self.disk_name.ljust(16, "\0"), TEXT_ENCODING)
            ) + [
                self.root_dir_file_section % 256,
                (self.root_dir_file_section >> 8) % 256,
                (self.root_dir_file_section >> 16) % 256,
                (self.root_dir_file_section >> 24) % 256,
            ] + [
                self.disk_size % 256,
                (self.disk_size >> 8) % 256,
                (self.disk_size >> 16) % 256,
                (self.disk_size >> 24) % 256,
            ] + [
                self.first_free_section % 256,
                (self.first_free_section >> 8) % 256,
                (self.first_free_section >> 16) % 256,
                (self.first_free_section >> 24) % 256,
            ]
        )

@dataclass
class FsFileEntry:
    file_name: str
    start_section: int
    size: int

    def get_bytes(self):
        return bytes(
            list(
                bytes(self.file_name.ljust(24, "\0"), TEXT_ENCODING)
            ) + [
                self.start_section % 256,
                (self.start_section >> 8) % 256,
                (self.start_section >> 16) % 256,
                (self.start_section >> 24) % 256,
            ] + [
                self.size % 256,
                (self.size >> 8) % 256,
                (self.size >> 16) % 256,
                (self.size >> 24) % 256,
            ]
        )

@dataclass
class FsDirectory:
    files: list[FsFileEntry]

    def get_bytes(self):
        data = []

        for f in self.files:
            data += list(f.get_bytes())
        
        return bytes(data)

def find_free_sections(disk: Disk, header: FsHeader):
    pointer = header.first_free_section
    free = []
    while pointer != 0:
        free.append(pointer)
        disk.seek(SECTION_SIZE + pointer * 4)
        pointer = disk.get_u32()
    return free

def find_file_occupying(disk: Disk, start_section: int):
    pointer = start_section
    occupied = []
    while pointer != 0:
        occupied.append(pointer)
        disk.seek(SECTION_SIZE + pointer * 4)
        pointer = disk.get_u32()
    return occupied

def write_raw_file(disk: Disk, header: FsHeader, data: list[int]):
    current_seek = disk.seeking

    free = find_free_sections(disk, header)

    start = None

    data_size = len(data)
    print("|\x1b[90m-- Writing raw bytes as file\x1b[0m")
    print(f"|\x1b[90m   |-- Size: {data_size}\x1b[0m")

    loop_run = True
    while loop_run:
        section = free.pop(0)
        if not start: start = section
        disk.seek_section(section)
        disk.write_data(data[:SECTION_SIZE])
        data = data[SECTION_SIZE:]
        loop_run = len(data) > 0
    
    disk.seek(SECTION_SIZE + section * 4)
    disk.write_u32(0) # EOF
    
    header.first_free_section = free[0]
    print(f"|\x1b[90m   `-- Start: {start}\x1b[0m")

    disk.seek(current_seek)

    return start

def read_raw_file(disk: Disk, start_section: int):
    occupied = find_file_occupying(disk, start_section)

    data = []

    for i in occupied:
        data += disk.data[
            i * SECTION_SIZE:
            (i + 1) * SECTION_SIZE
        ]

    return data

def delete_raw_file(disk: Disk, header: FsHeader, start_section: int):
    occupied = find_file_occupying(disk, start_section)

    disk.seek(SECTION_SIZE + occupied[-1] * 4)
    disk.write_u32(header.first_free_section)
    header.first_free_section = occupied[0]

    disk.seek_section(0)
    disk.write_data(header.get_bytes())

def format_disk(disk: Disk, disk_name: str):
    print(f"FORMATING DISK {repr(disk_name)}")

    disk_sections = disk.size // SECTION_SIZE
    print(f"|-- Total disk sections: {disk_sections}")

    from math import ceil
    needed_ft_sections = ceil(disk_sections * 4 / SECTION_SIZE)
    del ceil

    print(f"|-- Needed FT sections: {needed_ft_sections}")
    print(f"|   `-- Taking: {format_byte_size(needed_ft_sections * SECTION_SIZE)}")

    first_free_section = 1 + needed_ft_sections

    # HEADER

    disk.seek_section(0)

    header = FsHeader(disk_name[:16], -1, disk.size, first_free_section)

    disk.write_data(header.get_bytes())

    # FT

    disk.seek_section(1)

    disk.write_u32(0) # section 0x0000 - EOF, because its metadata
    for _ in range(needed_ft_sections):
        disk.write_u32(0) # all needed ft sections - EOF, because its metadata
    for f in range(first_free_section, disk_sections): # all free sections - Next free section, so it forms a linked list
        if f != disk_sections - 1:
            disk.write_u32(f + 1)
        else:
            disk.write_u32(0)
    
    # ROOT DIR

    header.root_dir_file_section = write_raw_file(
        disk, header, [0]
    )

    disk.seek(header.root_dir_file_section * SECTION_SIZE)

    disk.write_data(".ROOT", 24)
    disk.write_u32(header.root_dir_file_section)
    disk.write_u32(32)
    for _ in range(SECTION_SIZE - 32):
        disk.write_byte(0)

    # rewrite header

    disk.seek_section(0)
    disk.write_data(header.get_bytes())

def dump_section(disk: Disk, section: int):
    for ls in range(section * SECTION_SIZE, (section + 1) * SECTION_SIZE, 32):
        for os in range(32):
            addr = ls + os

            if addr < disk.size:
                print(hex(disk.data[addr])[2:].ljust(2, "0"), end=" ")
            else:
                print(end="\x1b[31m..\x1b[0m ")
        for os in range(32):
            addr = ls + os

            if addr < disk.size:
                if chr(disk.data[addr]).isprintable(): print(chr(disk.data[addr]), end="")
                else: print(end="\x1b[90m.\x1b[0m")
            else:
                print(end="\x1b[31m.\x1b[0m")
        print()

def analyze_disk(disk: Disk):
    if VERBOSE: print("\x1b[90mAnalyzing disk")
    disk.seek_section(0)
    disk_name = disk.get_str(16).strip("\0")
    if VERBOSE: print(f"|-- Name: {repr(disk_name)}")
    
    disk_root_directory_pointer = disk.get_u32()
    if VERBOSE: print(f"|-- .ROOT file: {disk_root_directory_pointer}")
    disk_size = disk.get_u32()
    if VERBOSE: print(f"|-- Disk size: {disk_size} ({format_byte_size(disk_size)})")
    if VERBOSE: print(f"|   `-- {"Matches" if disk.size == disk_size else f"Real: {disk.size}"}")

    disk_first_free_section = disk.get_u32()
    if VERBOSE: print(f"|-- First free section: {disk_first_free_section}")

    header = FsHeader(disk_name, disk_root_directory_pointer, disk_size, disk_first_free_section)

    disk_free_sections = find_free_sections(disk, header)
    if VERBOSE: print(f"|-- Free section count: {len(disk_free_sections)}\x1b[0m")

    dot_root_file = read_raw_file(disk, header.root_dir_file_section)

    with open("test_DOT_ROOT", "wb") as f:
        f.write(bytes(dot_root_file))

    root_dir = FsDirectory([])
    
    for file_entry_start in range(0, len(dot_root_file), 32):
        fname = bytes(dot_root_file[file_entry_start:file_entry_start+24]).decode(TEXT_ENCODING).strip("\0")
        fstart_section = get_u32_from_int(dot_root_file[file_entry_start+24:file_entry_start+28])
        fsize = get_u32_from_int(dot_root_file[file_entry_start+28:file_entry_start+32])

        if fname != "" and fstart_section != 0:
            file = FsFileEntry(fname, fstart_section, fsize)
            root_dir.files.append(file)
    
    return header, root_dir, disk_free_sections

def read_file(disk: Disk, name: str):
    header, root_dir, _ = analyze_disk(disk)

    for f in root_dir.files:
        if f.file_name == name:
            data = read_raw_file(disk, f.start_section)
            hex_dump = False
            try:
                text = bytes(data[:f.size]).decode(TEXT_ENCODING)
                for i in text:
                    if not i.isprintable() and i not in "\n\t\r":
                        hex_dump = True
            except:
                hex_dump = True
            if hex_dump:
                print("Hex dump:")
                dumping = data[:f.size]
                for ls in range(0, len(dumping), 16):
                    print(end=f"\x1b[90m{hex(ls)[2:].rjust(8, "0")}\x1b[0m  ")
                    for os in range(16):
                        addr = ls + os
                        if addr < len(dumping):
                            print(hex(dumping[addr])[2:].rjust(2, "0"), end=" ")
                        else:
                            print(end="\x1b[31m..\x1b[0m ")
                    print(end=" ")
                    for os in range(16):
                        addr = ls + os
                        if addr < len(dumping):
                            c = chr(dumping[addr])
                            if c.isprintable():
                                print(c, end="")
                            else:
                                print(end="\x1b[90m.\x1b[0m")
                        else:
                            print(end="\x1b[31m.\x1b[0m")
                    print()
            else:
                print(text)
            return data[:f.size]
            break
    else:
        print(f"File not found: {repr(name)}")
    
    return []

def read_file_from_section(disk: Disk, section: int):
    data = read_raw_file(disk, section)
    text = bytes(data).decode(TEXT_ENCODING)
    hex_dump = False
    for i in text:
        if not i.isprintable():
            hex_dump = True
    if hex_dump:
        print("Hex dump:")
        dumping = data
        for ls in range(0, len(dumping), 16):
            print(end=f"\x1b[90m{hex(ls)[2:].rjust(8, "0")}\x1b[0m  ")
            for os in range(16):
                addr = ls + os
                if addr < len(dumping):
                    print(hex(dumping[addr])[2:].rjust(2, "0"), end=" ")
                else:
                    print(end="\x1b[31m..\x1b[0m ")
            print(end=" ")
            for os in range(16):
                addr = ls + os
                if addr < len(dumping):
                    c = chr(dumping[addr])
                    if c.isprintable():
                        print(c, end="")
                    else:
                        print(end="\x1b[90m.\x1b[0m")
                else:
                    print(end="\x1b[31m.\x1b[0m")
            print()
    else:
        print(text)

def new_file(disk, name: str):
    header, root_dir, _ = analyze_disk(disk)

    for i, f in enumerate(root_dir.files):
        if f.file_name == name:
            print(f"File already exists: {name}")
            break
    else:
        root_dir.files.append(FsFileEntry(name, write_raw_file(disk, header, [0x00]), 0))

        delete_raw_file(disk, header, header.root_dir_file_section)
        needed_pad = len(root_dir.get_bytes()) % SECTION_SIZE
        header.root_dir_file_section = write_raw_file(disk, header, list(root_dir.get_bytes()) + [0x00 for _ in range(needed_pad)])

        disk.seek_section(0)
        disk.write_data(header.get_bytes())

def write_file(disk: Disk, name: str, data: list[int]):
    header, root_dir, _ = analyze_disk(disk)

    for f in root_dir.files:
        if f.file_name == name:
            delete_file(disk, name)
            header, root_dir, _ = analyze_disk(disk)
            root_dir.files.append(FsFileEntry(name, write_raw_file(disk, header, data), len(data)))
            break
    else:
        root_dir.files.append(FsFileEntry(name, write_raw_file(disk, header, data), len(data)))

    delete_raw_file(disk, header, header.root_dir_file_section)
    needed_pad = len(root_dir.get_bytes()) % SECTION_SIZE
    header.root_dir_file_section = write_raw_file(disk, header, list(root_dir.get_bytes()) + [0x00 for _ in range(needed_pad)])

    disk.seek_section(0)
    disk.write_data(header.get_bytes())

def delete_file(disk: Disk, name: str):
    header, root_dir, _ = analyze_disk(disk)

    for i, f in enumerate(root_dir.files):
        if f.file_name == name:
            delete_raw_file(disk, header, f.start_section)

            root_dir.files.pop(i)

            delete_raw_file(disk, header, header.root_dir_file_section)
            needed_pad = len(root_dir.get_bytes()) % SECTION_SIZE
            header.root_dir_file_section = write_raw_file(disk, header, list(root_dir.get_bytes()) + [0x00 for _ in range(needed_pad)])

            disk.seek_section(0)
            disk.write_data(header.get_bytes())
            break
    else:
        print(f"File not found: {repr(name)}")

def run_command(disk: Disk, cmd: list[str]):
    if len(cmd) == 1:
        if cmd[0] == "exit":
            exit(0)
        if cmd[0] == "ls":
            _, root_dir, _ = analyze_disk(disk)
            for f in root_dir.files:
                print(f"{f.file_name:24} {format_byte_size(f.size)}")
        if cmd[0] == "header":
            header, _, _ = analyze_disk(disk)

            print("Disk name:", repr(header.disk_name))
            print("Disk size:", repr(header.disk_size), f"({format_byte_size(header.disk_size)})")
            print("Disk .ROOT file section:", repr(header.root_dir_file_section))
            print("Disk first free section:", repr(header.first_free_section))
        if cmd[0] == "free":
            _, _, free_sections = analyze_disk(disk)
            print(f"Free sections: {len(free_sections)} ({format_byte_size(len(free_sections) * SECTION_SIZE)})")
    if len(cmd) == 2:
        if cmd[0] == "format":
            format_disk(disk, cmd[1])
        if cmd[0] == "dump":
            dump_section(disk, int(cmd[1], 0))
        if cmd[0] == "cat":
            read_file(disk, cmd[1])
        if cmd[0] == "rm":
            delete_file(disk, cmd[1])
        if cmd[0] == "rread":
            read_file_from_section(disk, int(cmd[1], 0))
        if cmd[0] == "touch":
            new_file(disk, cmd[1])
        if cmd[0] == "edit":
            _, root_dir, _ = analyze_disk(disk)
            text = ""
            for f in root_dir.files:
                if f.file_name == cmd[1]:
                    text = bytes(read_raw_file(disk, f.start_section)[:f.size]).decode(TEXT_ENCODING)
            write_file(disk, cmd[1], list(bytes(edit_text_in_editor(text), TEXT_ENCODING)))
    if len(cmd) == 3:
        if cmd[0] == "fhost":
                with open(cmd[1], "rb") as f:
                    write_file(disk, cmd[2], list(f.read()))
        if cmd[0] == "ffs":
                with open(cmd[2], "wb") as f:
                    f.write(bytes(read_file(disk, cmd[1])))

def main(disk_file: str):
    with open(disk_file, "rb") as f:
        disk = Disk(list(f.read()))

    print("Disk:")
    print(f"`-- Size: {format_byte_size(disk.size)}")

    print()

    while True:
        inp = input(">>> ")

        for l in inp.split(";"):
            cmd = shlex.split(l.strip())

            run_command(disk, cmd)
        with open(sys.argv[1], "wb") as f:
            f.write(bytes(disk.data))

if __name__ == "__main__":
    import sys
    if len(sys.argv) > 2:
        with open(sys.argv[1], "rb") as f:
            disk = Disk(list(f.read()))

        print("Disk:")
        print(f"`-- Size: {format_byte_size(disk.size)}")

        run_command(disk, sys.argv[2:])
        with open(sys.argv[1], "wb") as f:
            f.write(bytes(disk.data))
        exit(0)
    if len(sys.argv) == 2:
        main(sys.argv[1])
