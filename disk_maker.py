#!/bin/python3
# New disk_maker.py

"""
CLI usage

python3 disk_maker.py <disk_file.bin> <command> <...args>
"""

import sys

def flatten(data):
  for item in data:
    if isinstance(item, (list, tuple)):
      yield from flatten(item)
    else:
      yield item

def run_command(disk_path: str, command: str, *args: str):
    match command:
        case "new":
            if len(args) != 1:
                print(f"Expected 1 argument for new command.\nTry: 'python3 {disk_path.replace(" ", "\\ ")} new <size>'.")
                exit(1)

            with open(disk_path, "wb") as file:
                file.write(bytes([0x00 for _ in range(int(args[0], 0))]))
        case "write":
            with open(disk_path, "r+b") as file:
                if len(args) != 2:
                    print(f"Expected 2 arguments for write command.\nTry: 'python3 {disk_path.replace(" ", "\\ ")} write <file> <offset>'.")

                written = bytes()
                
                with open(args[0], "rb") as sf:
                    written = sf.read()
                
                file.seek(int(args[1], 0))
                file.write(written)

                print(f"Written {len(written)} bytes to {disk_path}.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Not enough arguments.")
        exit(1)
    disk_path = sys.argv[1]
    command = sys.argv[2]

    run_command(disk_path, command, *sys.argv[3:])
