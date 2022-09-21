import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestGModules(TestBase):

    @gmodules_test
    @expectedFailureAll
    def test_gmodules(self):
        self.build()
        lldbutil.run_to_source_breakpoint(self, "Break here", lldb.SBFileSpec("main.cpp"))

        name = lldbutil.frame().FindVariable('m') \
                .GetChildAtIndex(0).GetChildAtIndex(0).GetChildAtIndex(0).GetName()

        self.assertEqual(name, 'buffer', 'find template specializations in imported modules')
