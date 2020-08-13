#!/usr/bin/env python

#TODO LICENSE FROM FLUX

import sys
import os

def main():
    arguments = sys.argv[1:] # 0 is me
    try:
        args_split_point = arguments.index('--')
        driver_args = arguments[:args_split_point]
        test_command = arguments[args_split_point+1:]
    except ValueError:
        for idx, value in enumerate(arguments):
            if not value.startswith('--'):
                driver_args = arguments[:idx]
                test_command = arguments[idx:]
                break

    driver = os.path.join(os.path.dirname(os.path.realpath(__file__)), "tap-driver.sh")
    full_command = [driver] + driver_args + ["--", sys.executable] + test_command
    os.execv(driver, full_command)

if __name__ == "__main__":
    main()
