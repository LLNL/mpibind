import os
import re
import struct

import argparse

platform_c_maxint = 2 ** (struct.Struct('i').size * 8 - 1) - 1

def parse_arguments():
    parser = argparse.ArgumentParser()

    parser.add_argument("header", help="header file to parse", type=str)
    parser.add_argument("output", help="destination of clean header", type=str)

    return parser.parse_args()

def apply_replacement_rules(line):
    rules = {
        "__restrict": "restrict",
        "__inline__": "inline",
        "INT_MAX": str(platform_c_maxint),
    }

    for k,v in rules.items():
        line = line.replace(k, v)

    if ("inline" in line):
        print(line)
    return line

def make_enum_literal(line):
    return re.sub(r'(\d)(UL)?<<(\d)',lambda x: str(int(x.group(1))<<int(x.group(3))),line)

def remove_external_headers(line):
    return re.sub(r'#\s*include\s+<.+>', '', line)

def clean_header(header):
    cleaned = ""
    with open(header, "r") as f:
        for line in f.readlines():
            line = apply_replacement_rules(line)
            line = make_enum_literal(line)
            line = remove_external_headers(line)
            cleaned += line

    return cleaned

def output_header(output_file, cleaned):
    with open(output_file, "w") as f:
        f.write(cleaned)

if __name__ == "__main__":
    args = parse_arguments()
    abs_header = os.path.abspath(args.header)
    abs_output = os.path.abspath(args.output)
    output_header(abs_output, clean_header(abs_header))