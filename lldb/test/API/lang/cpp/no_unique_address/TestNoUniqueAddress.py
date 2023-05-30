"""
Test that we correctly handle [[no_unique_address]] attribute.
"""

import lldb

from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestNoUniqueAddress(TestBase):

    @skipIf(debug_info=no_match(["dsym"]))
    def test(self):
        self.build()

        lldbutil.run_to_source_breakpoint(self,
            "// Set breakpoint here.", lldb.SBFileSpec("main.cpp"))


        self.expect_expr("_f3", result_type="Foo3")
        self.expect_expr("_f7", result_type="Foo7")

        self.expect_expr("_f1.a", result_type="long", result_value="42")
        self.expect_expr("_f1.b", result_type="long", result_value="52")
        self.expect_expr("_f2.v", result_type="long", result_value="42")
        self.expect_expr("_f3.v", result_type="long", result_value="42")
        self.expect_expr("_f4.v", result_type="long", result_value="42")
        self.expect_expr("_f5.v1", result_type="long", result_value="42")
        self.expect_expr("_f5.v2", result_type="long", result_value="52")
        self.expect_expr("_f6.v1", result_type="long", result_value="42")
        self.expect_expr("_f6.v2", result_type="long", result_value="52")
        self.expect_expr("_f7.v", result_type="long", result_value="42")
