#!/usr/bin/env python

import unittest

from mpibind import wrapper

class WrapperTests(unittest.TestCase):
    def test_placeholder(self):
        self.assertTrue(True)

if __name__ == "__main__":
    from pycotap import TAPTestRunner
    unittest.main(testRunner=TAPTestRunner)