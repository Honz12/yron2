#!/bin/python3
from dataclasses import dataclass
from typing import List

"""
Sections:
    - 256 + 4 bytes (256 bytes of data, 4 bytes of next section pointer)
    - Each section that is free contains the following C struct:
        struct {
            uint32_t next_free_section_pointer;
        }

Header Section (C struct):
    struct {
        uint32_t file_table_pointer; // Pointer to the file table
        uint8_t[32] disk_name;
        uint32_t first_free_section;
    }

File Table:
    - Contains all file names mapped to file section pointers
    - 28 bytes file name, 4 bytes file section pointer
    - It itself is a file (just not included in the file table)
"""

@dataclass
class HeaderSection:
    file_table_pointer: int
    disk_name: str

@dataclass
class FileEntry:
    start_pointer: int
    name: str

class FreeSection:
    next_free_section_pointer: int

@dataclass
class FileTable:
    entries: List[FileEntry]

SECTION_DATA_SIZE = 256
SECTION_META_SIZE = 4
SECTION_SIZE = SECTION_DATA_SIZE + SECTION_META_SIZE

@dataclass
class DiskSection:
    data: bytes
    meta: bytes
    seeking: int = 0

    @staticmethod
    def new():
        return DiskSection(bytes(SECTION_DATA_SIZE), bytes(SECTION_META_SIZE))

    @staticmethod
    def from_bytes(data: bytes):
        return DiskSection(data[:SECTION_DATA_SIZE], data[SECTION_DATA_SIZE:SECTION_DATA_SIZE + SECTION_META_SIZE])
    
    def write_byte(self, byte: int):
        if self.seeking < SECTION_DATA_SIZE:
            self.data[self.seeking]
            self.seeking += 1
        else:
            self.seeking = 0

    def write_data(self, written: bytes):
        for b in written: self.write_byte(written)
    
    def read_bytes(self, num_bytes):
        data = self.data[self.seeking:self.seeking + num_bytes]
        self.seeking += num_bytes
        return data
    
    def get_bytes(self):
        return self.data + self.meta

def make_int_from_bytes(bs: bytes):
    return bs[0] | bs[1] << 8 | bs[2] << 16 | bs[3] << 24

def get_section(disk: list[int], idx: int):
    return DiskSection.from_bytes(disk[idx * SECTION_SIZE:(idx + 1) * SECTION_SIZE])

def write_section(disk: list[int], idx: int, section: DiskSection):
    for i, b in enumerate(section.get_bytes()):
        disk[idx * SECTION_SIZE + i] = b
    print(f"Written {len(bs)} bytes to disk.")

def get_file_from(disk: list[int], section: int):
    data = bytes()
    while True:
        print(f"<CONTINUES DATA READ {section}>")

        s = get_section(disk, section)
        data += s[:SECTION_DATA_SIZE]
        meta = make_int_from_bytes(s[SECTION_DATA_SIZE:SECTION_DATA_SIZE + SECTION_META_SIZE])

        if meta == 0:
            return
        
        section = meta

if __name__ == "__main__":
    with open("disk.bin", "r+b") as f:
        disk: list[int] = list(f.read())
        header_section = get_section(disk, 0x0000)

        header_section.write_data(bytes("Hello world!\0", "utf-8"))

        write_section(disk, 0x0000, header_section)

        with open("test_header_section.bin", "wb") as test_header_section:
            test_header_section.write(get_section(disk, 0x0000).get_bytes())
        get_section(disk, 0x0000)
        f.write(bytes(disk))
