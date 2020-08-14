import os
import re
import struct

import argparse


def parse_arguments():
    """
    Parse arguments for the clean header script. This will need expansion if we
    want to add from multiple headers
    """
    parser = argparse.ArgumentParser()

    parser.add_argument("header", help="header file to parse", type=str)
    parser.add_argument("output", help="destination of clean header", type=str)

    return parser.parse_args()


def remove_external_headers(line):
    """
    Remove external headers so that their content is not included when we run
    the output h file through the preprocessor
    """
    return re.sub(r"#\s*include\s+<.+>", "", line)


def clean_header(header):
    """
    Apply cleaning rules to the header
    """
    cleaned = ""
    with open(header, "r") as f:
        for line in f.readlines():
            line = remove_external_headers(line)
            cleaned += line

    return cleaned


def output_header(output_file, cleaned):
    """
    Write cleaned header to the output_file
    """
    with open(output_file, "w") as f:
        f.write(cleaned)


if __name__ == "__main__":
    args = parse_arguments()
    abs_header = os.path.abspath(args.header)
    abs_output = os.path.abspath(args.output)
    output_header(abs_output, clean_header(abs_header))
