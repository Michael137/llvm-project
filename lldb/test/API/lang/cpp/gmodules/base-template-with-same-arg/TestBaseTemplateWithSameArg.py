"""
Tests the scenario where we evaluate expressions
of two types in different modules that reference
a base class template instantiated with the same
template argument.

Note that,
1. Since the decls originate from modules, LLDB
   marks them as such and Clang doesn't create
   a LookupPtr map on the corresponding DeclContext.
   This prevents regular DeclContext::lookup from
   succeeding.
2. Because we reference the same base template
   from two different modules we get a redeclaration
   chain for the base class's ClassTemplateSpecializationDecl.
   The importer will import all FieldDecls into the
   same DeclContext on the redeclaration chain. If
   we don't do the bookkeeping correctly we end up
   with duplicate decls on the same DeclContext leading
   to crashes down the line.
"""

import lldb
import os
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestBaseTemplateWithSameArg(TestBase):

    @add_test_categories(["gmodules"])
    @skipIf(bugnumber='rdar://96581048')
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
