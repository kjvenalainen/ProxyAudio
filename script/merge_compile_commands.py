#!/usr/bin/env python3

import os
import sys
import json
import argparse
import typing


def merge_compile_commands(input_files: list[str], output_file: str) -> None:
    """Merge multiple compile_commands.json files into one."""
    compile_commands: dict[str, typing.Any] = {}
    
    # If output file exists, load it first
    if os.path.exists(output_file):
        try:
            with open(output_file, "r") as f:
                existing_compile_commands: list[dict[str, typing.Any]] = json.load(f)
                for entry in existing_compile_commands:
                    compile_commands[entry["file"]] = entry
            print(f"Found {len(compile_commands)} existing entries in {output_file}")
        except (json.JSONDecodeError, KeyError) as e:
            print(f"Warning: Could not parse existing {output_file}: {e}")
    
    files_updated = 0
    files_read = 0
    for input_file in input_files:
        if not os.path.exists(input_file):
            continue
        
        try:
            print(f"Reading {input_file}")
            with open(input_file, "r") as f:
                entries: list[dict[str, typing.Any]] = json.load(f)
                for entry in entries:
                    file_path = entry.get("file")
                    if file_path:
                        if file_path not in compile_commands:
                            files_updated += 1
                        compile_commands[file_path] = entry
                files_read += 1
        except (json.JSONDecodeError, KeyError) as e:
            print(f"Warning: Could not parse {input_file}: {e}")
            continue
    
    if files_read == 0 and len(compile_commands) == 0:
        print("Error: No valid compile_commands.json files found")
        sys.exit(1)
    
    print(f"Updated {files_updated} entries from {files_read} file(s)")
    
    # Write merged compile_commands.json
    with open(output_file, "w") as f:
        json.dump(list(compile_commands.values()), f, indent=2)
    
    print(f"Wrote {len(compile_commands)} total entries to {output_file}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Merge multiple compile_commands.json files into one"
    )
    parser.add_argument(
        "output_file",
        help="Path to output compile_commands.json file",
    )
    parser.add_argument(
        "input_files",
        nargs="+",
        help="Paths to input compile_commands.json files to merge",
    )
    args = parser.parse_args()
    
    merge_compile_commands(args.input_files, args.output_file)

