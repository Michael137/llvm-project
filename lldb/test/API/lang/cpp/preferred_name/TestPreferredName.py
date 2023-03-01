"""
Test formatting of types annotated with
[[clang::preferred_name]] attributes.
"""

import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.lldbtest import *
from lldbsuite.test import decorators


class TestPreferredName(TestBase):

    def test(self):
        self.build()
        lldbutil.run_to_source_breakpoint(self, "return", lldb.SBFileSpec("main.cpp"))

        self.expect("frame variable varInt", substrs=["BarInt"])
        self.expect("frame variable varDouble", substrs=["BarDouble"])
        self.expect("frame variable varShort", substrs=["Bar<short>"])
        self.expect("frame variable varChar", substrs=["Bar<char>"])
