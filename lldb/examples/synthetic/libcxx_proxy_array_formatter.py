"""
Python LLDB data formatter for libc++ std::gslice_array, std::mask_array, std::indirect_array

Converted from LibCxxProxyArray.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Data formatter for libc++'s std::proxy_array.

A proxy_array's are created by using:
  std::gslice_array   operator[](const std::gslice& gslicearr);
  std::mask_array     operator[](const std::valarray<bool>& boolarr);
  std::indirect_array operator[](const std::valarray<std::size_t>& indarr);

These arrays have the following members:
- __vp_ points to std::valarray::__begin_
- __1d_ an array of offsets of the elements from @a __vp_
"""

import lldb


class LibcxxStdProxyArraySyntheticFrontEnd:
    """Synthetic children frontend for libc++ proxy arrays."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_base = None
        self.m_element_type = None
        self.m_element_size = 0
        self.m_start = None
        self.m_finish = None
        self.m_element_type_size_t = None
        self.m_element_size_size_t = 0
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
        if num_children % self.m_element_size_size_t != 0:
            return 0

        return num_children // self.m_element_size_size_t

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if not self.m_base:
            return None
        try:
            if name.startswith('[') and name.endswith(']'):
                return int(name[1:-1])
        except ValueError:
            pass
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if not self.m_base:
            return None

        offset = index * self.m_element_size_size_t
        offset = offset + self.m_start.GetValueAsUnsigned(0)

        # Read the indirect index
        target = self.valobj.GetTarget()
        if not target or not target.IsValid():
            return None

        indirect_addr = lldb.SBAddress(offset, target)
        indirect = target.CreateValueFromAddress(
            "indirect", indirect_addr, self.m_element_type_size_t
        )

        if not indirect or not indirect.IsValid():
            return None

        indirect_value = indirect.GetValueAsUnsigned(0)
        if indirect_value == 0:
            return None

        # Calculate actual offset
        offset = indirect_value * self.m_element_size
        offset = offset + self.m_base.GetValueAsUnsigned(0)

        name = "[%d] -> [%d]" % (index, indirect_value)
        value_addr = lldb.SBAddress(offset, target)
        return target.CreateValueFromAddress(name, value_addr, self.m_element_type)

    def update(self):
        """Update the cached state."""
        self.m_base = None
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

        vector_sp = self.valobj.GetChildMemberWithName("__1d_")
        if not vector_sp or not vector_sp.IsValid():
            return False

        vector_type = vector_sp.GetType()
        if vector_type.GetNumberOfTemplateArguments() == 0:
            return False

        self.m_element_type_size_t = vector_type.GetTemplateArgumentType(0)
        if self.m_element_type_size_t.IsValid():
            size = self.m_element_type_size_t.GetByteSize()
            if size:
                self.m_element_size_size_t = size
            else:
                return False

        if self.m_element_size_size_t == 0:
            return False

        base_sp = self.valobj.GetChildMemberWithName("__vp_")
        start_sp = vector_sp.GetChildMemberWithName("__begin_")
        finish_sp = vector_sp.GetChildMemberWithName("__end_")

        if not base_sp or not start_sp or not finish_sp:
            return False

        self.m_base = base_sp
        self.m_start = start_sp
        self.m_finish = finish_sp

        return True

    def has_children(self):
        """Check if this object has children."""
        return True
