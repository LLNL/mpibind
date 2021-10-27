
#!/usr/bin/env python3

import unittest
from test_utils import *

topology_file = "../topo-xml/epyc-corona.xml"
answer_file = "./expected/expected.epyc-corona"

# test class that inherits from unittest
# test cases are added in main body
class TestEpycCorona(unittest.TestCase):
    pass

if __name__ == "__main__":
    # read expected file
    test_info = parse_expected(answer_file)

    # add setup and teardown functions
    setattr(TestEpycCorona, 'setUp', setup_generator(topology_file))
    setattr(TestEpycCorona, 'tearDown', teardown_generator())

    # build and add test cases from the expected file
    for single_test_info in test_info:
        setattr(TestEpycCorona, make_test_name(single_test_info['description']), 
            test_generator(single_test_info))

    #use pycotap to emit TAP from python unit tests
    from pycotap import TAPTestRunner
    suite = unittest.TestLoader().loadTestsFromTestCase(TestEpycCorona)
    TAPTestRunner().run(suite)