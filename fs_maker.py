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
        uint32_t file_table_pointer;        // Pointer to the file table

        uint8_t[32] disk_name;              // Information about the disk
        uint32_t disk_section_count;
        uint32_t disk_section_data_size;

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
    disk_section_count: int
    disk_section_data_size: int

    first_free_section: int

@dataclass
class FileEntry:
    start_pointer: int
    name: str

class FreeSection:
    next_free_section_pointer: int

@dataclass
class FileTable:
    entries: List[FileEntry]

@dataclass
class DiskData:
    data: list[int]
    header: HeaderSection
    file_table: FileTable

SECTION_DATA_SIZE = 252
SECTION_META_SIZE = 4
SECTION_SIZE = SECTION_DATA_SIZE + SECTION_META_SIZE

@dataclass
class DiskSection:
    data: list[int]
    meta: list[int]
    seeking: int = 0

    @staticmethod
    def new():
        return DiskSection([0x00 for _ in range(SECTION_DATA_SIZE)], [0x00 for _ in range(SECTION_META_SIZE)])

    @staticmethod
    def from_bytes(data: bytes):
        return DiskSection(data[:SECTION_DATA_SIZE], data[SECTION_DATA_SIZE:SECTION_DATA_SIZE + SECTION_META_SIZE])
    
    def write_byte(self, byte: int):
        if self.seeking < SECTION_DATA_SIZE:
            self.data[self.seeking] = byte
            self.seeking += 1
        else:
            self.seeking = 0

    def write_data(self, written: bytes):
        for b in written: self.write_byte(b)
    
    def read_bytes(self, num_bytes):
        data = self.data[self.seeking:self.seeking + num_bytes]
        self.seeking += num_bytes
        return data
    
    def get_bytes(self):
        return bytes(self.data + self.meta)

    def write_uint32(self, u: int):
        self.write_data(bytes([u % 256, (u >> 8) % 256, (u >> 16) % 256, (u >> 24) % 256]))

    def write_uint16(self, u: int):
        self.write_data(bytes([u % 256, (u >> 8) % 256]))

def make_int_from_bytes(bs: bytes):
    return bs[0] | bs[1] << 8 | bs[2] << 16 | bs[3] << 24

def get_section(disk: list[int], idx: int):
    return DiskSection.from_bytes(disk[idx * SECTION_SIZE:(idx + 1) * SECTION_SIZE])

def write_section(disk: list[int], idx: int, section: DiskSection):
    for i, b in enumerate(section.get_bytes()):
        disk[idx * SECTION_SIZE + i] = b
    #print(f"Written {len(section.get_bytes())} bytes to disk.")

def get_file_from(disk: list[int], section: int):
    data = list()
    while True:
        print(f"<CONTINUES DATA READ {section}>")

        s = get_section(disk, section)
        data += s.data
        meta = make_int_from_bytes(s.meta)

        if meta == 0:
            return data
        
        section = meta

def format_disk(disk: list[int]):
    header_section = get_section(disk, 0x0000)

    header_section.write_uint32(0x0001)
    header_section.write_data(bytes("FungOS DISK".ljust(32, '\0'), "utf-8"))

    section_count = len(disk) // SECTION_SIZE
    unused_space = len(disk) - section_count * SECTION_SIZE

    if unused_space:
        print(f"Unused disk space: {unused_space} bytes")
    print(f"Section count: {section_count}")
    
    header_section.write_uint32(section_count)
    header_section.write_uint32(SECTION_DATA_SIZE)

    header_section.write_uint32(0x0002)

    write_section(disk, 0x0000, header_section)

    file_table = DiskSection.new()

    file_table.write_data(bytes("A.TXT".ljust(28, "\0"), "utf-8"))
    file_table.write_uint32(0x0001)
    
    write_section(disk, 0x0001, file_table)

    for i in range(2, section_count):
        free_s = DiskSection.new()
        if i < section_count - 1:
            free_s.write_uint32(i + 1)

        write_section(disk, i, free_s)

def get_disk_data(disk: list[int]):
    header_section = get_section(disk, 0x0000)
    header = HeaderSection(
        make_int_from_bytes(header_section.read_bytes(4)),
        
        bytes(header_section.read_bytes(32)).decode("utf-8").replace("\0", ""),
        make_int_from_bytes(header_section.read_bytes(4)),
        make_int_from_bytes(header_section.read_bytes(4)),
        
        make_int_from_bytes(header_section.read_bytes(4)),
    )

    print(header)

    files = []
    ft_data = get_file_from(disk, header.file_table_pointer)
    print(ft_data)
    for fes in range(0, len(ft_data), 32):
        data = ft_data[fes:fes+32]
        fn = data[:28]
        pointer = data[28:]

        if len(data) != 32:
            continue
        entry = FileEntry(
            make_int_from_bytes(pointer),
            bytes(fn).decode("utf-8").replace("\0", "")
        )
        if entry.start_pointer != 0:
            print(entry)
            files.append(entry)
    file_table = FileTable(files)

    free_section_pointer = header.first_free_section
    free_sections = []
    while free_section_pointer != 0:
        section = get_section(disk, free_section_pointer)
        free_sections.append(free_section_pointer)
        free_section_pointer = make_int_from_bytes(section.read_bytes(4))

    print(f"{len(free_sections)} free sections.")

    return header, file_table

def cmd_ls(disk):
    header, file_table = get_disk_data(disk)
    print((' ' + header.disk_name + ' ').center(48, "="))

    for f in file_table.entries:
        print(f.name.ljust(28), "|", f.start_pointer)

if __name__ == "__main__":
    from sys import argv
    if len(argv) < 3:
        print("Not enough arguments.")
        exit(1)
    
    with open(argv[1], "r+b") as f:
        disk: list[int] = list(f.read())

        match argv[2]:
            case "format":
                format_disk(disk)
            case "check":
                get_disk_data(disk)
            case "ls":
                cmd_ls(disk)

        f.seek(0x0000)
        f.write(bytes(disk))
