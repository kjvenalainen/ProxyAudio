#!/usr/bin/env python3

import json
import sys
import re
from pathlib import Path


def fix_compile_commands(compile_commands_path: str, project_root: str) -> None:
    """Fix include paths in compile_commands.json to point to source instead of build directories."""

    with open(compile_commands_path, "r") as f:
        compile_commands = json.load(f)

    project_root_path = Path(project_root).resolve()

    # Pattern to match build directory include paths
    # e.g., build/ProxyAudio/Release/libASPL-prefix/include -> libASPL/include
    build_include_pattern = re.compile(
        r"-I([^\s]*)/build/[^/]+/[^/]+/libASPL-prefix/include"
    )

    modified_count = 0

    for entry in compile_commands:
        command = entry.get("command", "")

        # Replace build include paths with source include paths
        def replace_include(match):
            nonlocal modified_count
            modified_count += 1
            # Extract the project root from the matched path
            root = match.group(1)
            # Return the source include path
            return f"-I{root}/libASPL/include"

        new_command = build_include_pattern.sub(replace_include, command)

        if new_command != command:
            entry["command"] = new_command

    # Write back the modified compile_commands.json
    with open(compile_commands_path, "w") as f:
        json.dump(compile_commands, f, indent=2)

    print(f"Fixed {modified_count} include paths in {compile_commands_path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(
            "Usage: fix_compile_commands_paths.py <compile_commands.json> [project_root]"
        )
        sys.exit(1)

    compile_commands_path = sys.argv[1]
    project_root = (
        sys.argv[2] if len(sys.argv) > 2 else str(Path(compile_commands_path).parent)
    )

    fix_compile_commands(compile_commands_path, project_root)
