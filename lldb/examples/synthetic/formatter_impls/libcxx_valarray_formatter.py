"""
Python LLDB data formatter for libc++ std::valarray

Converted from LibCxxValarray.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


class LibcxxStdValarraySyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::valarray."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_start = None
        self.m_finish = None
        self.m_element_type = None
        self.m_element_size = 0
        self.update()

    def num_children(self):
        """Calculate the number of children."""
        if not self.m_start or not self.m_finish:
            return 0

        start_val = self.m_start.GetValueAsUnsigned(0)
        finish_val = self.m_finish.GetValueAsUnsigned(0)

        if start_val == 0 or finish_val == 0:
            return 0

        if start_val >= finish_val:
            return 0

        num_children = finish_val - start_val
        if num_children % self.m_element_size != 0:
            return 0

        return num_children // self.m_element_size

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if not self.m_start or not self.m_finish:
            return None
        try:
            if name.startswith("[") and name.endswith("]"):
                return int(name[1:-1])
        except ValueError:
            pass
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if not self.m_start or not self.m_finish:
            return None

        offset = index * self.m_element_size
        offset = offset + self.m_start.GetValueAsUnsigned(0)

        name = "[%d]" % index
        target = self.valobj.GetTarget()
        if not target or not target.IsValid():
            return None

        addr = lldb.SBAddress(offset, target)
        return target.CreateValueFromAddress(name, addr, self.m_element_type)

    def update(self):
        """Update the cached state."""
        self.m_start = None
        self.m_finish = None

        compiler_type = self.valobj.GetType()
        if compiler_type.GetNumberOfTemplateArguments() == 0:
            return False

        self.m_element_type = compiler_type.GetTemplateArgumentType(0)
        if self.m_element_type.IsValid():
            size = self.m_element_type.GetByteSize()
            if size:
                self.m_element_size = size
            else:
                return False

        if self.m_element_size == 0:
            return False

        start_sp = self.valobj.GetChildMemberWithName("__begin_")
        finish_sp = self.valobj.GetChildMemberWithName("__end_")

        if not start_sp or not finish_sp:
            return False

        self.m_start = start_sp
        self.m_finish = finish_sp

        return True

    def has_children(self):
        """Check if this object has children."""
        return True

def init_formatter(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::valarray<.+>$" -l lldb.formatters.cpp.formatter_impls.libcxx_valarray_formatter.LibcxxStdValarraySyntheticFrontEnd -w "cplusplus-py"')
