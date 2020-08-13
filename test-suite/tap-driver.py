#!/usr/bin/env python
import sys
import os
import argparse

def get_tap_driver(driver_path):
    return os.path.join(os.path.dirname(os.path.realpath(__file__)), "tap-driver.sh")

def get_python_executable():
    return sys.executable

def split_arguments(args):
    args_split_point = args.index('--')
    return args[:args_split_point], args[args_split_point+1:]

def build_command(driver_path, python_executable, args):
    driver_args, test_path = split_arguments(args)
    return [driver_path] + driver_args + ['--', python_executable] + test_path

if __name__ == "__main__":
    driver_path = get_tap_driver(sys.argv[1])
    args = sys.argv[2:]

    full_command = build_command(driver_path, get_python_executable(), args)
    print(full_command)
    #os.execv(driver_path, full_command)