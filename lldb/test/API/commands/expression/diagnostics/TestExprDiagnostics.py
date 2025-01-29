"""
Test the diagnostics emitted by our embeded Clang instance that parses expressions.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil
from lldbsuite.test.decorators import *


class ExprDiagnosticsTestCase(TestBase):
    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)

        self.main_source = "main.cpp"
        self.main_source_spec = lldb.SBFileSpec(self.main_source)
        self.main_source_diag_path = os.path.join("diagnostics", "main.cpp")

    def test_source_and_caret_printing(self):
        """Test that the source and caret positions LLDB prints are correct"""
        self.build()

        (target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(
            self, "// Break here", self.main_source_spec
        )
        frame = thread.GetFrameAtIndex(0)

        # Test that source/caret are at the right position.
        value = frame.EvaluateExpression("unknown_identifier")
        self.assertFalse(value.GetError().Success())
        # We should get a nice diagnostic with a caret pointing at the start of
        # the identifier.
        self.assertIn(
            """
    1 | unknown_identifier
      | ^
""",
            value.GetError().GetCString(),
        )
        self.assertIn("<user expression 0>:1:1", value.GetError().GetCString())

        # Same as above but with the identifier in the middle.
        value = frame.EvaluateExpression("1 + unknown_identifier")
        self.assertFalse(value.GetError().Success())
        self.assertIn(
            """
    1 | 1 + unknown_identifier
      |     ^
""",
            value.GetError().GetCString(),
        )

        # Multiline expressions.
        value = frame.EvaluateExpression("int a = 0;\nfoobar +=1;\na")
        self.assertFalse(value.GetError().Success())
        # We should still get the right line information and caret position.
        self.assertIn(
            """
    2 | foobar +=1;
      | ^
""",
            value.GetError().GetCString(),
        )

        # It's the second line of the user expression.
        self.assertIn("<user expression 2>:2:1", value.GetError().GetCString())

        # Top-level expressions.
        top_level_opts = lldb.SBExpressionOptions()
        top_level_opts.SetTopLevel(True)

        value = frame.EvaluateExpression("void foo(unknown_type x) {}", top_level_opts)
        self.assertFalse(value.GetError().Success())
        self.assertIn(
            """
    1 | void foo(unknown_type x) {}
      |          ^
""",
            value.GetError().GetCString(),
        )

        # Top-level expressions might use a different wrapper code, but the file name should still
        # be the same.
        self.assertIn("<user expression 3>:1:10", value.GetError().GetCString())

        # Multiline top-level expressions.
        value = frame.EvaluateExpression("void x() {}\nvoid foo;", top_level_opts)
        self.assertFalse(value.GetError().Success())
        self.assertIn(
            """
    2 | void foo;
      |      ^
""",
            value.GetError().GetCString(),
        )

        self.assertIn("<user expression 4>:2:6", value.GetError().GetCString())

        # Test that we render Clang's 'notes' correctly.
        value = frame.EvaluateExpression(
            "struct SFoo{}; struct SFoo { int x; };", top_level_opts
        )
        self.assertFalse(value.GetError().Success())
        self.assertIn(
            "<user expression 5>:1:8: previous definition is here\n",
            value.GetError().GetCString(),
        )
        self.assertIn(
            """
    1 | struct SFoo{}; struct SFoo { int x; };
      |        ^
""",
            value.GetError().GetCString(),
        )

        # Redefine something that we defined in a user-expression. We should use the previous expression file name
        # for the original decl.
        value = frame.EvaluateExpression("struct Redef { double x; };", top_level_opts)
        value = frame.EvaluateExpression("struct Redef { float y; };", top_level_opts)
        self.assertFalse(value.GetError().Success())
        self.assertIn(
            """error: <user expression 7>:1:8: redefinition of 'Redef'
    1 | struct Redef { float y; };
      |        ^
<user expression 6>:1:8: previous definition is here
    1 | struct Redef { double x; };
      |        ^
""",
            value.GetError().GetCString(),
        )

    @add_test_categories(["objc"])
    def test_source_locations_from_objc_modules(self):
        self.build()

        (target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(
            self, "// Break here", self.main_source_spec
        )
        frame = thread.GetFrameAtIndex(0)

        # Import foundation so that the Obj-C module is loaded (which contains source locations
        # that can be used by LLDB).
        self.runCmd("expr --language objective-c++ -- @import Foundation")
        value = frame.EvaluateExpression("NSLog(1);")
        self.assertFalse(value.GetError().Success())
        # LLDB should print the source line that defines NSLog. To not rely on any
        # header paths/line numbers or the actual formatting of the Foundation headers, only look
        # for a few tokens in the output.
        # File path should come from Foundation framework.
        self.assertIn("/Foundation.framework/", value.GetError().GetCString())
        # The NSLog definition source line should be printed. Return value and
        # the first argument are probably stable enough that this test can check for them.
        self.assertIn("void NSLog(NSString *format", value.GetError().GetCString())

    def test_error_type(self):
        """Test the error reporting in the API"""
        self.build()

        (target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(
            self, "// Break here", self.main_source_spec
        )
        frame = thread.GetFrameAtIndex(0)
        value = frame.EvaluateExpression('#error("I am error.")')
        error = value.GetError()
        self.assertEqual(error.GetType(), lldb.eErrorTypeExpression)
        value = frame.FindVariable("f")
        self.assertTrue(value.IsValid())
        desc = value.GetObjectDescription()
        self.assertEqual(desc, None)

    def test_command_expr_sbdata(self):
        """Test the structured diagnostics data"""
        self.build()

        (target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(
            self, "// Break here", self.main_source_spec
        )

        def check_error(diags):
            # Version.
            version = diags.GetValueForKey("version")
            self.assertEqual(version.GetIntegerValue(), 1)

            details = diags.GetValueForKey("details")

            # Detail 1/2: undeclared 'a'
            diag = details.GetItemAtIndex(0)

            severity = diag.GetValueForKey("severity")
            message = diag.GetValueForKey("message")
            rendered = diag.GetValueForKey("rendered")
            sloc = diag.GetValueForKey("source_location")
            filename = sloc.GetValueForKey("file")
            hidden = sloc.GetValueForKey("hidden")
            in_user_input = sloc.GetValueForKey("in_user_input")

            self.assertEqual(str(severity), "error")
            self.assertIn("undeclared identifier 'a'", str(message))
            # The rendered string should contain the source file.
            self.assertIn("user expression", str(rendered))
            self.assertIn("user expression", str(filename))
            self.assertFalse(hidden.GetBooleanValue())
            self.assertTrue(in_user_input.GetBooleanValue())

            # Detail 1/2: undeclared 'b'
            diag = details.GetItemAtIndex(1)
            message = diag.GetValueForKey("message")
            self.assertIn("undeclared identifier 'b'", str(message))

        # Test diagnostics in CommandReturnObject
        interp = self.dbg.GetCommandInterpreter()
        cro = lldb.SBCommandReturnObject()
        interp.HandleCommand("expression -- a+b", cro)

        diags = cro.GetErrorData()
        check_error(diags)

        # Test diagnostics in SBError
        frame = thread.GetSelectedFrame()
        value = frame.EvaluateExpression("a+b")
        error = value.GetError()
        self.assertTrue(error.Fail())
        self.assertEqual(error.GetType(), lldb.eErrorTypeExpression)
        data = error.GetErrorData()
        version = data.GetValueForKey("version")
        self.assertEqual(version.GetIntegerValue(), 1)
        err_ty = data.GetValueForKey("type")
        self.assertEqual(err_ty.GetIntegerValue(), lldb.eErrorTypeExpression)
        diags = data.GetValueForKey("errors").GetItemAtIndex(0)
        check_error(diags)

    def assert_has_file_diagnostic(self, error_msg, file=None):
        if file is None:
            file = self.main_source_diag_path
        # The SourceLocation for the file should be printed.
        self.assertIn(file + ":", error_msg)
        # Never show the dummy source code. This is the dummy content string
        # used by TypeSystemClang.
        self.assertNotIn("<LLDB-generated contents of", error_msg)
        self.assertNotIn(file + ">", error_msg)

    def assert_has_not_file_diagnostic(self, error_msg, file=None):
        if file is None:
            file = self.main_source_diag_path
        # The SourceLocation should not be printed.
        self.assertNotIn(file + ":", error_msg)
        # Never show the dummy source code. This is the dummy content string
        # used by TypeSystemClang.
        self.assertNotIn("<LLDB-generated contents of", error_msg)
        self.assertNotIn(file + ">", error_msg)

    def test_source_locations_from_debug_information(self):
        """Test that the source locations from debug information are correct"""
        self.build()

        (target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(self,
                                          '// Break here', self.main_source_spec)
        frame = thread.GetFrameAtIndex(0)
        top_level_opts = lldb.SBExpressionOptions();
        top_level_opts.SetTopLevel(True)

        # Test source locations of functions.
        value = frame.EvaluateExpression("foo(1, 2)")
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assertIn(":1:1: no matching function for call to 'foo'\n    1 | foo(1, 2)\n      | ^~~",
                      value.GetError().GetCString())
        self.assert_has_file_diagnostic(value.GetError().GetCString())

        # Test source locations of records.
        value = frame.EvaluateExpression("struct FooBar { double x; };", top_level_opts)
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assertIn(":1:8: redefinition of 'FooBar'\n    1 | struct FooBar { double x; };\n",
                      value.GetError().GetCString())
        self.assert_has_file_diagnostic(value.GetError().GetCString())

        # Test source locations of enums.
        value = frame.EvaluateExpression("enum class EnumInSource {};", top_level_opts)
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assert_has_file_diagnostic(value.GetError().GetCString())

        # Test source locations in headers.
        value = frame.EvaluateExpression("headerFunction(1)")
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assert_has_file_diagnostic(value.GetError().GetCString(),
                                        "header.h")

    @skipIf(debug_info=no_match("dsym"),
            bugnumber="Template function decl can only be found via dsym")
    def test_source_locations_from_debug_information_templates(self):
        """Test that the source locations from debug information are correct
        for template functions"""
        self.build()

        (target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(self,
                                          '// Break here', self.main_source_spec)
        frame = thread.GetFrameAtIndex(0)

        # Test source locations of template functions.
        value = frame.EvaluateExpression("TemplateFunc<int>(1)")
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assert_has_file_diagnostic(value.GetError().GetCString())

    def test_disabled_source_locations(self):
        """Test that disabling source locations with use-source-locations is
        actually disabling the creation of valid source locations"""
        self.build()
        # Disable source locations.
        self.runCmd("settings set plugin.symbol-file.dwarf.use-source-locations false")

        (target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(self,
                                          '// Break here', self.main_source_spec)
        frame = thread.GetFrameAtIndex(0)
        top_level_opts = lldb.SBExpressionOptions();
        top_level_opts.SetTopLevel(True)

        # Functions shouldn't have source locations now.
        value = frame.EvaluateExpression("foo(1, 2)")
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assertIn(":1:1: no matching function for call to 'foo'\n    1 | foo(1, 2)\n      | ^~~",
                      value.GetError().GetCString())
        self.assert_has_not_file_diagnostic(value.GetError().GetCString())

        # Records shouldn't have source locations now.
        value = frame.EvaluateExpression("struct FooBar { double x; };", top_level_opts)
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assertIn(":1:8: redefinition of 'FooBar'\n    1 | struct FooBar { double x; };\n",
                      value.GetError().GetCString())
        self.assert_has_not_file_diagnostic(value.GetError().GetCString())

        # Enums shouldn't have source locations now.
        value = frame.EvaluateExpression("enum class EnumInSource {};", top_level_opts)
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assert_has_not_file_diagnostic(value.GetError().GetCString())

        # Template functions shouldn't have source locations now.
        value = frame.EvaluateExpression("TemplateFunc<int>(1)")
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assert_has_not_file_diagnostic(value.GetError().GetCString())

        # Test source locations in headers are disabled.
        value = frame.EvaluateExpression("headerFunction(1)")
        self.assertFalse(value.GetError().Success(), value.GetError().GetCString())
        self.assert_has_not_file_diagnostic(value.GetError().GetCString(),
                                            "header.h")
