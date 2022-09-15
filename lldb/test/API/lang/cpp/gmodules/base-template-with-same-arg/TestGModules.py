import lldb
import os
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestWithGmodulesDebugInfo(TestBase):

    @add_test_categories(["gmodules"])
    def test_same_base_template_arg(self):
        self.build()

        self.main_source_file = lldb.SBFileSpec("main.cpp")

        (target, process, main_thread, _) = lldbutil.run_to_source_breakpoint(self,
                                                "Break here", self.main_source_file)

        self.expect_expr("FromMod1", result_type="ClassInMod1", result_children=[
                ValueCheck(name="VecInMod1", children=[
                        ValueCheck(name="ClassInMod3Base<int>", children=[
                            ValueCheck(name="BaseMember", value="137")
                        ])
                    ])
            ])

        self.expect_expr("FromMod2", result_type="ClassInMod2", result_children=[
                ValueCheck(name="VecInMod2", children=[
                        ValueCheck(name="ClassInMod3Base<int>", children=[
                            ValueCheck(name="BaseMember", value="42")
                        ])
                    ])
            ])
