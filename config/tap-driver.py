#######################################################
# File originally created by S. Herbein@LLNL and
# included here with permission from M. Grondona@LLNL
#######################################################
#!/usr/bin/env python

import sys
import os
from os import path

"""
    This is the runner for the python tests. It is essentially a wrapper
    for the tap-driver.sh runner used to the c tests. This runner
    inserts the location of the correct python executable before the
    python test file and then call tap-driver.sh with the correct
    argument ordering.
"""
def driver():
    arguments = sys.argv[1:] # 0 is me
    # driver_args = arguments for tap-driver.sh
    # test_command = python test file (e.g. py-epyc-corona.py)
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

    # driver = location of tap-driver.sh
    # sys.executable = location of python binary
    # insert location of python binary before test command in full_command
    # full_command = driver + driver_args + python executable + python test
    driver = path.join(path.dirname(path.realpath(__file__)), "tap-driver.sh")
    full_command = [driver] + driver_args + ["--", sys.executable] + test_command
    os.execv(driver, full_command)

if __name__ == "__main__":
    driver()
