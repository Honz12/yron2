#!/usr/bin/env python3
import argparse
import os
import struct
import subprocess
import sys
import tempfile

import disk_maker


class SimpleFS:
    BLOCK_SIZE = 512
    TOTAL_BLOCKS = 128  # 64 KiB total space
    MAGIC = b"SFAT"

    SUPERBLOCK_IDX = 0
    ALLOCATION_MAP_IDX = 1
    FAT_TABLE_IDX = 2
    ROOT_DIR_IDX = 3  # Root directory spans blocks 3 and 4
    DATA_START_IDX = 5

    MAX_ENTRIES_PER_DIR = 16
    DIR_ENTRY_SIZE = 64
    EOF_MARKER = 255

    # Entry types
    TYPE_EMPTY = 0
    TYPE_FILE = 1
    TYPE_DIR = 2

    def __init__(self, disk_file):
        self.disk_file = disk_file
        disk_maker.ensure_disk_exists(disk_file=self.disk_file, disk_size=self.BLOCK_SIZE * self.TOTAL_BLOCKS)

    def _block_to_hex(self, block_idx: int) -> str:
        return hex(block_idx * self.BLOCK_SIZE)

    def _write_at_block(self, block_idx: int, data: bytes):
        with tempfile.NamedTemporaryFile(delete=False) as tmp:
            tmp.write(data)
            tmp_name = tmp.name
        try:
            disk_maker.modify_disk("put", tmp_name, self._block_to_hex(block_idx), disk_file=self.disk_file)
        finally:
            if os.path.exists(tmp_name):
                os.remove(tmp_name)

    def _read_disk_data(self, offset: int, size: int) -> bytes:
        with open(self.disk_file, "rb") as f:
            f.seek(offset)
            return f.read(size)

    # --- Disk Metadata Operations ---

    def _read_dir_at_block(self, block_idx: int):
        """Reads up to 16 entries starting at block_idx (spans 2 blocks for 1024B/16 entries)."""
        num_blocks = 2 if block_idx == self.ROOT_DIR_IDX else 1
        max_entries = 16 if block_idx == self.ROOT_DIR_IDX else 8  # 512B / 64B = 8 entries per standard block
        
        dir_bytes = self._read_disk_data(block_idx * self.BLOCK_SIZE, self.DIR_ENTRY_SIZE * max_entries)
        entries = []
        for i in range(max_entries):
            chunk = dir_bytes[i * self.DIR_ENTRY_SIZE : (i + 1) * self.DIR_ENTRY_SIZE]
            entry_type, name_bytes, file_size, start_block = struct.unpack(">B31sIH", chunk[:38])
            name = name_bytes.rstrip(b"\x00").decode("utf-8", errors="ignore")
            entries.append({
                "index": i,
                "type": entry_type,
                "name": name,
                "size": file_size,
                "start_block": start_block,
                "dir_block": block_idx
            })
        return entries

    def _write_dir_entry(self, parent_block: int, entry_index: int, entry_type: int, name: str, size: int, start_block: int):
        num_blocks = 2 if parent_block == self.ROOT_DIR_IDX else 1
        name_bytes = name.encode("utf-8")[:31].ljust(31, b"\x00")
        packed = struct.pack(">B31sIH", entry_type, name_bytes, size, start_block).ljust(self.DIR_ENTRY_SIZE, b"\x00")

        full_dir = bytearray(self._read_disk_data(parent_block * self.BLOCK_SIZE, self.BLOCK_SIZE * num_blocks))
        start_offset = entry_index * self.DIR_ENTRY_SIZE
        full_dir[start_offset : start_offset + self.DIR_ENTRY_SIZE] = packed

        self._write_at_block(parent_block, bytes(full_dir[:self.BLOCK_SIZE]))
        if num_blocks > 1:
            self._write_at_block(parent_block + 1, bytes(full_dir[self.BLOCK_SIZE:]))

    def _read_fat(self):
        return bytearray(self._read_disk_data(self.FAT_TABLE_IDX * self.BLOCK_SIZE, self.TOTAL_BLOCKS))

    def _write_fat(self, fat: bytearray):
        self._write_at_block(self.FAT_TABLE_IDX, bytes(fat))

    def _read_alloc_map(self):
        return bytearray(self._read_disk_data(self.ALLOCATION_MAP_IDX * self.BLOCK_SIZE, self.TOTAL_BLOCKS))

    def _write_alloc_map(self, alloc_map: bytearray):
        self._write_at_block(self.ALLOCATION_MAP_IDX, bytes(alloc_map))

    def _allocate_block(self) -> int:
        alloc_map = self._read_alloc_map()
        for i in range(self.DATA_START_IDX, self.TOTAL_BLOCKS):
            if alloc_map[i] == 0:
                alloc_map[i] = 1
                self._write_alloc_map(alloc_map)
                return i
        raise RuntimeError("Disk full: No free blocks remaining.")

    def _free_chain(self, start_block: int):
        if start_block == 0 or start_block == self.EOF_MARKER:
            return
        alloc_map = self._read_alloc_map()
        fat = self._read_fat()

        curr = start_block
        while curr != self.EOF_MARKER and curr < self.TOTAL_BLOCKS:
            next_b = fat[curr]
            alloc_map[curr] = 0
            fat[curr] = self.EOF_MARKER
            curr = next_b

        self._write_alloc_map(alloc_map)
        self._write_fat(fat)

    # --- Path Resolution Helper ---

    def _resolve_path(self, path: str, create_parents=False):
        """Resolves a string path like 'OS/kernel' into (parent_dir_block, target_name)."""
        parts = [p for p in path.strip("/").split("/") if p]
        if not parts:
            return self.ROOT_DIR_IDX, ""

        current_block = self.ROOT_DIR_IDX
        for part in parts[:-1]:
            entries = self._read_dir_at_block(current_block)
            target = next((e for e in entries if e["name"] == part and e["type"] == self.TYPE_DIR), None)
            if not target:
                if create_parents:
                    # Automatically create parent directory block
                    new_blk = self._allocate_block()
                    self._write_at_block(new_blk, b"\x00" * self.BLOCK_SIZE)
                    free_slot = next(e["index"] for e in entries if e["type"] == self.TYPE_EMPTY)
                    self._write_dir_entry(current_block, free_slot, self.TYPE_DIR, part, 0, new_blk)
                    current_block = new_blk
                else:
                    raise FileNotFoundError(f"Directory '{part}' not found in path '{path}'.")
            else:
                current_block = target["start_block"]

        return current_block, parts[-1]

    # --- Commands ---

    def format(self):
        disk_maker.modify_disk("clean", disk_file=self.disk_file)

        sb = struct.pack(">4sHH", self.MAGIC, self.BLOCK_SIZE, self.TOTAL_BLOCKS).ljust(self.BLOCK_SIZE, b"\x00")
        self._write_at_block(self.SUPERBLOCK_IDX, sb)

        alloc_map = bytearray(self.TOTAL_BLOCKS)
        for i in range(self.DATA_START_IDX):
            alloc_map[i] = 1
        self._write_alloc_map(alloc_map)

        fat = bytearray([self.EOF_MARKER] * self.TOTAL_BLOCKS)
        self._write_fat(fat)

        empty_dir = b"\x00" * (self.BLOCK_SIZE * 2)
        self._write_at_block(self.ROOT_DIR_IDX, empty_dir[:self.BLOCK_SIZE])
        self._write_at_block(self.ROOT_DIR_IDX + 1, empty_dir[self.BLOCK_SIZE:])

    def create_entry(self, path: str, entry_type: int):
        parent_block, name = self._resolve_path(path, create_parents=True)
        entries = self._read_dir_at_block(parent_block)

        free_slot = None
        for e in entries:
            if e["name"] == name and e["type"] != self.TYPE_EMPTY:
                print(f"Error: Target '{name}' already exists.")
                return
            if e["type"] == self.TYPE_EMPTY and free_slot is None:
                free_slot = e["index"]

        if free_slot is None:
            print("Error: Target directory is full.")
            return

        start_block = 0
        if entry_type == self.TYPE_DIR:
            # Allocate a block to hold the subdirectory's entries
            start_block = self._allocate_block()
            self._write_at_block(start_block, b"\x00" * self.BLOCK_SIZE)

        self._write_dir_entry(parent_block, free_slot, entry_type, name, 0, start_block)

    def write_file_data(self, path: str, content: bytes):
        parent_block, name = self._resolve_path(path)
        entries = self._read_dir_at_block(parent_block)
        target = next((e for e in entries if e["name"] == name and e["type"] == self.TYPE_FILE), None)

        if not target:
            print(f"Error: File '{name}' not found.")
            return

        if target["start_block"] != 0:
            self._free_chain(target["start_block"])

        if len(content) == 0:
            self._write_dir_entry(parent_block, target["index"], self.TYPE_FILE, name, 0, 0)
            return

        chunks = [content[i:i + self.BLOCK_SIZE] for i in range(0, len(content), self.BLOCK_SIZE)]
        fat = self._read_fat()

        first_block = None
        prev_block = None

        for chunk in chunks:
            blk = self._allocate_block()
            padded = chunk.ljust(self.BLOCK_SIZE, b"\x00")
            self._write_at_block(blk, padded)

            if first_block is None:
                first_block = blk
            if prev_block is not None:
                fat[prev_block] = blk
            prev_block = blk

        fat[prev_block] = self.EOF_MARKER
        self._write_fat(fat)
        self._write_dir_entry(parent_block, target["index"], self.TYPE_FILE, name, len(content), first_block)

    def read_file_data(self, path: str) -> bytes:
        parent_block, name = self._resolve_path(path)
        entries = self._read_dir_at_block(parent_block)
        target = next((e for e in entries if e["name"] == name and e["type"] == self.TYPE_FILE), None)
        if not target or target["start_block"] == 0:
            return b""

        fat = self._read_fat()
        data = bytearray()
        curr = target["start_block"]

        while curr != self.EOF_MARKER and curr < self.TOTAL_BLOCKS:
            block_bytes = self._read_disk_data(curr * self.BLOCK_SIZE, self.BLOCK_SIZE)
            data.extend(block_bytes)
            curr = fat[curr]

        return bytes(data[:target["size"]])

    def edit(self, path: str):
        try:
            parent_block, name = self._resolve_path(path)
            entries = self._read_dir_at_block(parent_block)
            target = next((e for e in entries if e["name"] == name and e["type"] == self.TYPE_FILE), None)
        except FileNotFoundError:
            target = None

        if not target:
            self.create_entry(path, self.TYPE_FILE)

        existing_content = self.read_file_data(path)

        with tempfile.NamedTemporaryFile(suffix=".txt", delete=False) as tmp:
            tmp.write(existing_content)
            tmp_path = tmp.name

        try:
            editor = os.environ.get("EDITOR", "nano")
            subprocess.call([editor, tmp_path])
            with open(tmp_path, "rb") as f:
                new_content = f.read()
            self.write_file_data(path, new_content)
        except Exception:
            print(f"--- Editing '{path}' (Enter content, press Ctrl+D to save) ---")
            new_content = sys.stdin.read().encode("utf-8")
            self.write_file_data(path, new_content)
        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)

    def remove(self, path: str, expected_type: int):
        parent_block, name = self._resolve_path(path)
        entries = self._read_dir_at_block(parent_block)
        target = next((e for e in entries if e["name"] == name), None)

        if not target or target["type"] == self.TYPE_EMPTY:
            print(f"Error: '{path}' does not exist.")
            return

        if target["type"] != expected_type:
            kind = "directory" if expected_type == self.TYPE_DIR else "file"
            print(f"Error: '{path}' is not a {kind}.")
            return

        if target["start_block"] != 0:
            self._free_chain(target["start_block"])

        self._write_dir_entry(parent_block, target["index"], self.TYPE_EMPTY, "", 0, 0)

    def ls(self, path=""):
        if not path or path == "/":
            target_block = self.ROOT_DIR_IDX
        else:
            try:
                parent_block, name = self._resolve_path(path)
                entries = self._read_dir_at_block(parent_block)
                target = next((e for e in entries if e["name"] == name), None)
                if not target or target["type"] != self.TYPE_DIR:
                    print(f"Error: '{path}' is not a directory.")
                    return
                target_block = target["start_block"]
            except FileNotFoundError as e:
                print(f"Error: {e}")
                return

        entries = self._read_dir_at_block(target_block)
        active = [e for e in entries if e["type"] != self.TYPE_EMPTY]
        if not active:
            print("(empty directory)")
            return
        for e in active:
            kind = "DIR " if e["type"] == self.TYPE_DIR else "FILE"
            print(f"{kind}\t{e['size']} B\t{e['name']}")


def main():
    parser = argparse.ArgumentParser(description="FAT File System CLI Wrapper")
    parser.add_argument("diskfile", help="Path to the disk file (e.g., disk.bin)")

    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("format", help="Format the disk")

    mkdir_p = subparsers.add_parser("mkdir", help="Create directory")
    mkdir_p.add_argument("name", help="Directory path")

    touch_p = subparsers.add_parser("touch", help="Create file")
    touch_p.add_argument("name", help="File path")

    edit_p = subparsers.add_parser("edit", help="Edit file content")
    edit_p.add_argument("name", help="File path")

    rm_p = subparsers.add_parser("rm", help="Remove file")
    rm_p.add_argument("name", help="File path")

    rmdir_p = subparsers.add_parser("rmdir", help="Remove directory")
    rmdir_p.add_argument("name", help="Directory path")

    ls_p = subparsers.add_parser("ls", help="List directory contents")
    ls_p.add_argument("path", nargs="?", default="", help="Subdirectory path (optional)")

    args = parser.parse_args()
    fs = SimpleFS(args.diskfile)

    if args.command == "format":
        fs.format()
    elif args.command == "mkdir":
        fs.create_entry(args.name, SimpleFS.TYPE_DIR)
    elif args.command == "touch":
        fs.create_entry(args.name, SimpleFS.TYPE_FILE)
    elif args.command == "edit":
        fs.edit(args.name)
    elif args.command == "rm":
        fs.remove(args.name, SimpleFS.TYPE_FILE)
    elif args.command == "rmdir":
        fs.remove(args.name, SimpleFS.TYPE_DIR)
    elif args.command == "ls":
        fs.ls(args.path)


if __name__ == "__main__":
    main()
