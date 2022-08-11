"""
Test lldb breakpoint setting by source regular expression.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestSourceRegexBreakpointsWithAsm(TestBase):

    @skipIfWindows
    def test_restrictions(self):
        self.build()
        self.source_regex_restrictions()

    def source_regex_restrictions(self):
        """ Test restricting source expressions to to functions with non-standard mangling."""
        # Create a target by the debugger.
        exe = self.getBuildArtifact("a.out")
        target = self.dbg.CreateTarget(exe)
        self.assertTrue(target, VALID_TARGET)

        asm_tag = "NonStandardMangling"

        # Sanity check that we can set breakpoint on non-standard mangled name
        main_break = target.BreakpointCreateByName(asm_tag)

        expected_num_locations = 1
        num_locations = main_break.GetNumLocations()
        self.assertEqual(
            num_locations, expected_num_locations,
            "We should have gotten %d matches, got %d." %
            (expected_num_locations, num_locations))

        # Check regex on asm tag restricted to function names
        func_names = lldb.SBStringList()
        func_names.AppendString('main')
        func_names.AppendString('func')
        func_names.AppendString('')
        func_names.AppendString('NonStandardMangling')

        main_break = target.BreakpointCreateBySourceRegex(
            asm_tag, lldb.SBFileSpecList(), lldb.SBFileSpecList(), func_names)

        expected_num_locations = 0
        num_locations = main_break.GetNumLocations()
        self.assertEqual(
            num_locations, expected_num_locations,
            "We should have gotten %d matches, got %d." %
            (expected_num_locations, num_locations))
