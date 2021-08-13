#!/usr/bin/env python3

import unittest
from test_utils import *

topology_file = "../topo-xml/cts1-quartz-smt1.xml"
answer_file = "./expected/expected.cts1-quartz"

# test class that inherits from unittest
# test cases are added in main body
class TestCTS1Quartz(unittest.TestCase):
    pass

if __name__ == "__main__":
    # read expected file
    test_info = parse_expected(answer_file)

    # add setup and teardown functions
    setattr(TestCTS1Quartz, 'setUp', setup_generator(topology_file))
    setattr(TestCTS1Quartz, 'tearDown', teardown_generator())

    # build and add test cases from the expected file
    for single_test_info in test_info:
        setattr(TestCTS1Quartz, make_test_name(single_test_info['description']), 
            test_generator(single_test_info))

    #use pycotap to emit TAP from python unit tests
    from pycotap import TAPTestRunner
    suite = unittest.TestLoader().loadTestsFromTestCase(TestCTS1Quartz)
    TAPTestRunner().run(suite)