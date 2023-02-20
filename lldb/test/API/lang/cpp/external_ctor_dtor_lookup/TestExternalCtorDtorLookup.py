"""
Test that we can constructors/destructors
without a linkage name because they are
marked DW_AT_external and the fallback
mangled-name-guesser in LLDB doesn't account
for ABI tags.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class ExternalCtorDtorLookupTestCase(TestBase):

    @skipIfWindows
    def test(self):
        self.build()
        lldbutil.run_to_source_breakpoint(self, 'b\.getWrapper\(\)',
                lldb.SBFileSpec('main.cpp', False))

        self.expect_expr('b.sinkWrapper(b.getWrapper())', result_type='int', result_value='-1')
        self.expect_expr('B{}.m_int', result_type='int', result_value='47')
        self.expect_expr('D{}.m_int', result_type='int', result_value='-1')
        self.filecheck("target module dump ast", __file__)
# CHECK: |-CXXRecordDecl {{.*}} struct B definition
# CHECK: | |-virtual public 'A'
# CHECK: | `-CXXConstructorDecl {{.*}} B 'void ()'
# CHECK: |   `-AbiTagAttr {{.*}} TagB
# CHECK: |-CXXRecordDecl {{.*}} struct D definition
# CHECK: | `-CXXConstructorDecl {{.*}} D 'void ()'
# CHECK: |   `-AbiTagAttr {{.*}} TagD
# CHECK: |-CXXRecordDecl {{.*}} struct A definition
# CHECK: | |-CXXConstructorDecl {{.*}} A 'void (int)'
# CHECK: | | |-ParmVarDecl {{.*}} 'int'
# CHECK: | | `-AbiTagAttr {{.*}} Ctor Int
# CHECK: | |-CXXConstructorDecl {{.*}} A 'void (float)'
# CHECK: | | |-ParmVarDecl {{.*}} 'float'
# CHECK: | | `-AbiTagAttr {{.*}} Ctor Float
# CHECK: | |-CXXConstructorDecl {{.*}} A 'void ()'
# CHECK: | | `-AbiTagAttr {{.*}} Ctor Default                    
# CHECK: | `-CXXDestructorDecl {{.*}} ~A 'void ()'
# CHECK: |   `-AbiTagAttr {{.*}}  Default Dtor
        
        # Confirm that we can call the C2 constructor for 'struct A'
        self.expect('expression --top-level -- class Derived : virtual A {};')
        self.expect_expr('Derived{}.m_int', result_type='int', result_value='-1')

        # Confirm that we can call the D2 destructor for 'struct A'
        self.expect('expression Derived* $derived_ptr = new Derived()')
        self.expect('expression delete $derived_ptr')
