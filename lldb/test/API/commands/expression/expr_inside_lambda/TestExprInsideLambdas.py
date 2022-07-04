"""
Test that lldb persistent variables works correctly.

./bin/llvm-lit -sv --time-tests tools/lldb/test/API/commands/expression/expr_inside_lambda
"""


import lldb
from lldbsuite.test.lldbtest import *


class ExprInsideLambdaTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    def test_expr_inside_lambda(self):
        """Test that lldb evaluating expressions inside lambda expressions works correctly."""
        self.build()
        (target, process, thread, bkpt) = \
                lldbutil.run_to_source_breakpoint(self, "break here", lldb.SBFileSpec("main.cpp"))

        self.expect_expr("class_var", result_type="int", result_value="109")
        self.expect_expr("this->class_var", result_type="int", result_value="109")
        self.expect_expr("shadowed", result_type="int", result_value="5")
        self.expect_expr("local_var", result_type="int", result_value="137")
        self.expect_expr("base_var", result_type="int", result_value="14")
        self.expect_expr("base_base_var", result_type="int", result_value="11")
        self.expect_expr("global_var", result_type="int", result_value="-5")

        self.expect_expr("baz_virt()", result_type="int", result_value="2")
        self.expect_expr("base_var", result_type="int", result_value="14")
        self.expect_expr("this->shadowed", result_type="int", result_value="-1")
        # TODO: test address of 'this'

        lldbutil.continue_to_breakpoint(process, bkpt)

        # Inside nested_lambda
        self.expect_expr("lambda_local_var", result_type="int", result_value="5")
        self.expect_expr("class_var", result_type="int", result_value="109")
        # self.expect_expr("local_var_copy", result_type="int", result_value="137") # TODO: expect error since not captured
        # TODO: test address of 'this'

        lldbutil.continue_to_breakpoint(process, bkpt)

        # By-ref mutates source
        self.expect_expr("lambda_local_var", result_type="int", result_value="0")

        # By-value doesn't mutate source
        self.expect_expr("local_var_copy", result_type="int", result_value="136")
        self.expect_expr("local_var", result_type="int", result_value="137")

        lldbutil.continue_to_breakpoint(process, bkpt)

        # TODO: test address of 'this'
        #       inside LocalLambdaClass
        self.expect_expr("lambda_class_local", result_type="int", result_value="-12345")
        self.expect_expr("this->lambda_class_local", result_type="int", result_value="-12345")
        self.expect_expr("outer_ptr->class_var", result_type="int", result_value="109")
        # self.expect_expr("class_var", result_type="int", result_value="218") # TODO: expect error referencing from nested type
        # self.expect_expr("base_var", result_type="int", result_value="137") # TODO: expect error referencing from nested type

        lldbutil.continue_to_breakpoint(process, bkpt)

        self.expect_expr("class_var", result_type="int", result_value="218")
        self.expect_expr("lambda_local_var", result_type="int", result_value="0")
        
        lldbutil.continue_to_breakpoint(process, bkpt)
        # self.expect_expr("f", result_type="int", result_value="0")
