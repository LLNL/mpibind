#!/usr/bin/env python

import unittest

from mpibind.wrapper import MpibindWrapper

pympibind = MpibindWrapper()

coral_ea_topo = '../../topo-xml/coral-ea-hwloc1.xml'
coral_ea_expected = '../expected/expected.coral-ea'

class WrapperTests(unittest.TestCase):
    def test_coral_ea_1(self):

if __name__ == "__main__":
    from pycotap import TAPTestRunner
    unittest.main(testRunner=TAPTestRunner)