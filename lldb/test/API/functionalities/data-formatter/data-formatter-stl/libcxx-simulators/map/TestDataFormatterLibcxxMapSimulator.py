"""
Test we can understand various layouts of the libc++'s std::map
"""


import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class LibcxxMapDataFormatterSimulatorTestCase(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def test(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "Break here", lldb.SBFileSpec("main.cpp")
        )

        self.expect(
            "frame variable v",
            substrs=[
                "size=4",
                '[0] = (first = 0, second = 5)',
                '[1] = (first = 1, second = 0)',
                '[2] = (first = 10, second = 10)',
                '[3] = (first = 24, second = -5)',
            ],
        )

        self.expect("frame variable v[2]", substrs=["first = 10", "second = 10"])

        self.expect_expr(
            "n",
            result_children=[
                ValueCheck(name="first", value="0"),
                ValueCheck(name="second", value="5"),
            ],
        )

        self.expect("frame variable n.first", substrs=['first = 0'])
        self.expect("frame variable n.first", substrs=["second"], matching=False)
        self.expect("frame variable n.second", substrs=["second = 5"])
        self.expect("frame variable n.second", substrs=["first"], matching=False)
