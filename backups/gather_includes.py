import os
import re
import argparse
from pathlib import Path

def gather_includes(input_dir, output_file):
    # Regex to match #include <header> or #include "header"
    # It ignores leading whitespace but focuses on the path inside the brackets/quotes
    include_pattern = re.compile(r'^\s*#include\s*[<"]([^>"]+)[>"]')
    
    # Set of C++ related extensions
    extensions = {'.cpp', '.hpp', '.h', '.cc', '.cxx', '.c', '.hh', '.hxx'}
    
    unique_includes = set()

    print(f"Scanning directory: {input_dir}")

    # Walk through the directory recursively
    for root, _, files in os.walk(input_dir):
        for file in files:
            if Path(file).suffix.lower() in extensions:
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        for line in f:
                            match = include_pattern.match(line)
                            if match:
                                # Extract the include path (e.g., 'vector' or 'my_header.h')
                                include_path = match.group(1)
                                unique_includes.add(include_path)
                except Exception as e:
                    print(f"Could not read {file_path}: {e}")

    # Sort the results alphabetically
    sorted_includes = sorted(list(unique_includes))

    # Write to output file
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            for inc in sorted_includes:
                f.write(inc + '\n')
        print(f"Successfully wrote {len(sorted_includes)} unique dependencies to: {output_file}")
    except Exception as e:
        print(f"Error writing to file: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Gather unique C++ #include dependencies.")
    parser.add_argument(
        "directory", 
        help="The directory to scan for C++ files"
    )
    parser.add_argument(
        "-o", "--output", 
        default="dependencies.txt", 
        help="The output file (default: dependencies.txt)"
    )

    args = parser.parse_args()

    if os.path.isdir(args.directory):
        gather_includes(args.directory, args.output)
    else:
        print(f"Error: {args.directory} is not a valid directory.")