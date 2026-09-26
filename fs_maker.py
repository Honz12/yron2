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
    - 24 bytes file name, 4 bytes file section pointer, 4 bytes file size in bytes
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
    file_size: int
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

SECTION_META_SIZE = 4
SECTION_DATA_SIZE = 256 - SECTION_META_SIZE
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
        s = get_section(disk, section)
        data += s.data
        meta = make_int_from_bytes(s.meta)

        if meta == 0:
            return data
        
        section = meta

def format_disk(disk: list[int]):
    header_section = DiskSection.new()

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

    file_table.write_data(bytes(".FILE_TABLE".ljust(24, "\0"), "utf-8"))
    file_table.write_uint32(0x0001)
    file_table.write_uint32(SECTION_DATA_SIZE)
    
    write_section(disk, 0x0001, file_table)

    for i in range(2, section_count):
        free_s = DiskSection.new()
        if i < section_count - 1:
            free_s.write_uint32(i + 1)

        write_section(disk, i, free_s)

def get_disk_data(disk: list[int]):
    try:
        header_section = get_section(disk, 0x0000)
        header = HeaderSection(
            make_int_from_bytes(header_section.read_bytes(4)),
            
            bytes(header_section.read_bytes(32)).decode("utf-8").replace("\0", ""),
            make_int_from_bytes(header_section.read_bytes(4)),
            make_int_from_bytes(header_section.read_bytes(4)),
            
            make_int_from_bytes(header_section.read_bytes(4)),
        )
    except:
        print("INVALID FS HEADER")
        exit(1)
    print(header)

    try:
        files = []
        ft_data = get_file_from(disk, header.file_table_pointer)
        for fes in range(0, len(ft_data), 32):
            data = ft_data[fes:fes+32]
            fn = data[:24]
            pointer = data[24:28]
            size = data[28:]

            if len(data) != 32:
                continue
            entry = FileEntry(
                make_int_from_bytes(pointer),
                make_int_from_bytes(size),
                bytes(fn).decode("utf-8").replace("\0", "")
            )
            if entry.start_pointer != 0:
                print(entry)
                files.append(entry)
        file_table = FileTable(files)
    except:
        print("INVALID FILE TABLE")
        exit(1)

    try:
        free_section_pointer = header.first_free_section
        free_sections = []
        while free_section_pointer != 0:
            section = get_section(disk, free_section_pointer)
            free_sections.append(free_section_pointer)
            free_section_pointer = make_int_from_bytes(section.read_bytes(4))

        print(f"{len(free_sections)} free sections.")
    except:
        print("FAILED TO COLLECT FREE SECTIONS")
        exit(1)

    return header, file_table, free_sections

def format_byte_count(count: int):
    if count < 1024:
        return f"{count} B"
    elif count < 1024 * 1024:
        return f"{round(count / 1024, 2)} KiB"
    elif count < 1024 * 1024 * 1024:
        return f"{round(count / (1024 * 1024), 2)} MiB"
    else:
        return f"{round(count / (1024 * 1024 * 1024), 2)} GiB"

def get_file_occupying(disk: list[int], section: int):
    occupying = list()
    while True:
        s = get_section(disk, section)
        occupying.append(section)
        meta = make_int_from_bytes(s.meta)

        if meta == 0:
            return occupying
        
        section = meta

def cmd_ls(disk, list_all: bool):
    TABLE_WIDTH = 48 + 3
    FILE_NAME_SIZE = 24

    header, file_table, free_sections = get_disk_data(disk)
    print("|", (' ' + header.disk_name + ' ').center(TABLE_WIDTH, "="), "|")
    print("|", "Available space:".ljust(TABLE_WIDTH), "|")
    print("|", f"Free:   {len(free_sections)} ({format_byte_count(len(free_sections) * header.disk_section_data_size)})".ljust(TABLE_WIDTH), "|")
    print("|", f"Out of: {header.disk_section_count} ({format_byte_count(header.disk_section_count * header.disk_section_data_size)})".ljust(TABLE_WIDTH), "|")
    print("|", '-' * TABLE_WIDTH, "|")

    print("|", "FILE NAME".center(FILE_NAME_SIZE), "|", "SIZE".center(TABLE_WIDTH - 3 - FILE_NAME_SIZE), "|")

    for f in file_table.entries:
        if not f.name.startswith(".") or list_all:
            print("|", f.name.ljust(FILE_NAME_SIZE), "|", str(format_byte_count(f.file_size)).ljust(TABLE_WIDTH - 3 - FILE_NAME_SIZE), "|")

def repair_structure_sections(disk, header: HeaderSection, file_table: FileTable, free_sections: list[int]):
    free_sections += get_file_occupying(disk, header.file_table_pointer)
    free_sections.sort()

    # repair file table

    file_table_data = []

    for file in file_table.entries:
        print(f"Repairing file {file.name} in file table")
        file_table_data += list(bytes(file.name + "\0" * (24 - len(file.name)), "utf-8"))
        file_table_data += [
            file.start_pointer % 256,
            (file.start_pointer >> 8) % 256,
            (file.start_pointer >> 16) % 256,
            (file.start_pointer >> 24) % 256,
        ]
        file_table_data += [
            file.file_size % 256,
            (file.file_size >> 8) % 256,
            (file.file_size >> 16) % 256,
            (file.file_size >> 24) % 256,
        ]

    writing = free_sections[0]
    loop_should_run = True
    while loop_should_run:
        print(f"Allocated section {free_sections[0]} to file table during repair")
        section = DiskSection.new()
        section.write_data(file_table_data[:SECTION_DATA_SIZE])
        section.seeking = SECTION_DATA_SIZE
        write_section(disk, free_sections[0], section)
        free_sections.pop(0)
        file_table_data = file_table_data[SECTION_DATA_SIZE:]
        loop_should_run = len(file_table_data) > 0
        if loop_should_run:
            section.write_uint32(free_sections[0])
    
    print(f"Succesfuly repaired file table")

    # repair free sections

    for i, s in enumerate(free_sections):
        section = DiskSection.new()
        if i != len(free_sections) - 1:
            section.write_uint32(free_sections[i + 1])
        write_section(disk, s, section)

    header.first_free_section = free_sections[0]

    # Write process

    header_section = DiskSection.new()

    header_section.write_uint32(header.file_table_pointer)
    header_section.write_data(bytes(header.disk_name + "\0" * (32 - len(header.disk_name)), "utf-8"))
    header_section.write_uint32(header.disk_section_count)
    header_section.write_uint32(header.disk_section_data_size)
    header_section.write_uint32(header.first_free_section)

    write_section(disk, 0x0000, header_section)

def write_file(disk: list[int], name: str, data: list[int]):
    header, file_table, free_sections = get_disk_data(disk)

    file: FileEntry = None

    for file_entry in file_table.entries:
        if file_entry.name == name:
            print(f"FOUND FILE {name}")
            file = file_entry
            break
    else:
        print(f"FILE {name} NOT FOUND")
        return
    
    occupying = get_file_occupying(disk, file.start_pointer)
    print("File is occupying sections: ", occupying)
    free_sections = occupying + free_sections
    free_space = len(free_sections) * SECTION_DATA_SIZE
    needed_space = len(data)
    if free_space < needed_space:
        print(f"NOT ENOUGH SPACE, ONLY HAS {format_byte_count(free_space)} FREE, BUT NEEDS {format_byte_count(needed_space)}")

    file.start_pointer = free_sections[0]
    loop_should_run = True
    while loop_should_run:
        print(f"Allocated section {free_sections[0]} to {file.name}")
        section = DiskSection.new()
        section.write_data(data[:SECTION_DATA_SIZE])
        section.seeking = SECTION_DATA_SIZE
        write_section(disk, free_sections[0], section)
        free_sections.pop(0)
        data = data[SECTION_DATA_SIZE:]
        loop_should_run = len(data) > 0
        if loop_should_run:
            section.write_uint32(free_sections[0])

    file.file_size = needed_space
    
    print(f"Succesfuly written {format_byte_count(needed_space)} to {name}")

    repair_structure_sections(disk, header, file_table, free_sections)

def read_file(disk: list[int], name: str):
    header, file_table, free_sections = get_disk_data(disk)

    file: FileEntry = None

    for file_entry in file_table.entries:
        if file_entry.name == name:
            print(f"FOUND FILE {name}")
            file = file_entry
            break
    else:
        print(f"FILE {name} NOT FOUND")
        return
    
    file_contents = get_file_from(disk, file.start_pointer)[:file.file_size]
    
    print(f"Succesfuly read {format_byte_count(file.file_size)} from {name}")

    return bytes(file_contents)

def new_file(disk: list[int], name: str):
    header, file_table, free_sections = get_disk_data(disk)

    for file_entry in file_table.entries:
        if file_entry.name == name:
            print(f"FILE {name} ALREADY EXISTS")
            return
    
    file = FileEntry(free_sections.pop(0), 0, name)

    file_table.entries.append(file)

    repair_structure_sections(disk, header, file_table, free_sections)

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
                cmd_ls(disk, False)
            case "la":
                cmd_ls(disk, True)
            case "write":
                if len(argv) != 5:
                    print("INVALID ARGS FOR write COMMAND, EXPECTED `python3 fs_maker.py write <file_name> <source_file_on_host>")
                    exit(1)
                with open(argv[4], "rb") as host_file:
                    write_file(disk, argv[3], list(host_file.read()))
            case "get":
                if len(argv) != 5:
                    print("INVALID ARGS FOR read COMMAND, EXPECTED `python3 fs_maker.py write <file_name> <store_file_on_host>")
                    exit(1)
                with open(argv[4], "wb") as host_file:
                    host_file.write(read_file(disk, argv[3]))
            case "new":
                if len(argv) != 4:
                    print("INVALID ARGS FOR new COMMAND, EXPECTED `python3 fs_maker.py new <file_name>")
                    exit(1)
                new_file(disk, argv[3])

        f.seek(0x0000)
        f.write(bytes(disk))
