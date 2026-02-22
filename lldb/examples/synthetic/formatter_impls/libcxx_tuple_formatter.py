"""
Python LLDB data formatter for libc++ std::tuple

Converted from LibCxxTuple.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


class TupleFrontEnd:
    """Synthetic children frontend for libc++ std::tuple."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_elements = []
        self.m_base = None
        self.update()

    def num_children(self):
        """Return the number of tuple elements."""
        return len(self.m_elements)

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        try:
            if name.startswith("[") and name.endswith("]"):
                return int(name[1:-1])
        except ValueError:
            pass
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if index >= len(self.m_elements):
            return None

        if not self.m_base:
            return None

        if self.m_elements[index]:
            return self.m_elements[index]

        holder_type = self.m_base.GetType().GetDirectBaseClassAtIndex(index)
        if not holder_type or not holder_type.IsValid():
            return None

        holder_sp = self.m_base.GetChildAtIndex(index)
        if not holder_sp or not holder_sp.IsValid():
            return None

        elem_sp = holder_sp.GetChildAtIndex(0)
        if elem_sp and elem_sp.IsValid():
            cloned = elem_sp.Clone("[%d]" % index)
            if cloned and cloned.IsValid():
                self.m_elements[index] = cloned
                return cloned

        return None

    def update(self):
        """Update the cached state."""
        self.m_elements = []
        self.m_base = None

        base_sp = self.valobj.GetChildMemberWithName("__base_")
        if not base_sp or not base_sp.IsValid():
            # Pre r304382 name of the base element
            base_sp = self.valobj.GetChildMemberWithName("base_")

        if not base_sp or not base_sp.IsValid():
            return False

        self.m_base = base_sp
        num_bases = base_sp.GetType().GetNumberOfDirectBaseClasses()
        self.m_elements = [None] * num_bases

        return True

    def has_children(self):
        """Check if this object has children."""
        return True

def init_formatter(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::tuple<.*>$" -l lldb.formatters.cpp.formatter_impls.libcxx_tuple_formatter.TupleFrontEnd -w "cplusplus-py"')
