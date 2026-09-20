#!/bin/python3

import os
import sys

DISK_FILE = "disk.bin"
DISK_SIZE = 64 * 1024  # 64 KiB


def ensure_disk_exists(disk_file=DISK_FILE, disk_size=DISK_SIZE):
    """Create a blank disk file if it doesn't already exist."""
    if not os.path.exists(disk_file):
        with open(disk_file, "wb") as f:
            f.write(bytes([0x00] * disk_size))


def modify_disk(command, *args, disk_file=DISK_FILE, disk_size=DISK_SIZE):
    """Execute disk commands programmatically or via CLI."""
    match command:
        case "clean":
            # Overwrite entire disk with zeros
            with open(disk_file, "wb") as f:
                f.write(bytes([0x00] * disk_size))
            print(f"Cleaned {disk_file} ({disk_size} bytes).")

        case "put":
            if len(args) < 2:
                raise ValueError("Usage: modify_disk('put', <file_path>, <address>)")

            input_file = args[0]
            # Convert address string/int to integer if provided as string
            address = int(args[1], 0) if isinstance(args[1], str) else int(args[1])

            ensure_disk_exists(disk_file, disk_size)

            with open(input_file, "rb") as src:
                data = src.read()

            if address + len(data) > disk_size:
                raise OverflowError(
                    f"Data size ({len(data)} B) at address {hex(address)} exceeds disk size ({disk_size} B)."
                )

            with open(disk_file, "r+b") as f:
                f.seek(address)
                f.write(data)

            print(f"Wrote {len(data)} bytes from '{input_file}' to {disk_file} at {hex(address)}.")

        case _:
            raise ValueError(f"Unknown command: {command}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 disk_maker.py clean")
        print("  python3 disk_maker.py put <FILE> <ADDRESS>")
        sys.exit(1)

    cmd = sys.argv[1]
    cmd_args = sys.argv[2:]

    try:
        modify_disk(cmd, *cmd_args)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
