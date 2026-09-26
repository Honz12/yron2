#!/bin/python3

"""
DOCS

- FS Header - first 32 bytes of disk
    - 16 bytes disk name
    - 4 bytes root directory file pointer
    - 4 bytes size of disk

- ROOT Directory
    - Entry
        - 16 bytes file name
        - 4 bytes start section
        - 4 bytes size
"""

TEXT_ENCODING = "utf-8"
SECTION_SIZE = 512

import shlex
from dataclasses import dataclass

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
            (self.get_byte() << 32)
        )

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
    free = find_free_sections(disk, header)

    start = None

    data_size = len(data)
    print("|\x1b[90m-- Writing raw bytes as file\x1b[0m")
    print(f"|\x1b[90m   |-- Size: {data_size}\x1b[0m")

    while len(data) > 0:
        section = free.pop(0)
        if not start: start = section
        disk.seek_section(section)
        disk.write_data(data[:SECTION_SIZE])
        data = data[SECTION_SIZE:]
    
    disk.seek(SECTION_SIZE + pointer * 4)
    disk.write_u32(0) # EOF
    
    header.first_free_section = free[0]
    print(f"|\x1b[90m   `-- Start: {start}\x1b[0m")

    return start

def free_raw_file(disk: Disk, header: FsHeader, start_section: int):
    occupied = find_file_occupying(start_section)
    header.fi

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

    header.root_dir_file_section = write_raw_file(disk, header, [0 for _ in range(SECTION_SIZE)])
    

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

def run_command(disk: Disk, cmd: list[str]):
    if len(cmd) == 1:
        if cmd[0] == "exit":
            exit(0)
    if len(cmd) == 2:
        if cmd[0] == "format":
            format_disk(disk, cmd[1])
        if cmd[0] == "dump":
            dump_section(disk, int(cmd[1], 0))

def main():
    disk = Disk.new(1024 ** 2 * 2)

    print("Disk:")
    print(f"`-- Size: {format_byte_size(disk.size)}")

    print()

    while True:
        inp = input(">>> ")

        for l in inp.split(";"):
            cmd = shlex.split(l.strip())

            run_command(disk, cmd)

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
    main()
