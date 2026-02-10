"""
Python LLDB data formatter for libc++ std::span

Converted from LibCxxSpan.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


class LibcxxStdSpanSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::span."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_start = None
        self.m_element_type = None
        self.m_num_elements = 0
        self.m_element_size = 0
        self.update()

    def num_children(self):
        """Calculate the number of children."""
        return self.m_num_elements

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if not self.m_start:
            return None
        try:
            if name.startswith('[') and name.endswith(']'):
                return int(name[1:-1])
        except ValueError:
            pass
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if not self.m_start:
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
        """
        Update the cached state.

        std::span can either be instantiated with a compile-time known
        extent or a std::dynamic_extent (this is the default if only the
        type template argument is provided). The layout of std::span
        depends on whether the extent is dynamic or not. For static
        extents (e.g., std::span<int, 9>):

        (std::__1::span<const int, 9>) s = {
          __data = 0x000000016fdff494
        }

        For dynamic extents, e.g., std::span<int>, the layout is:

        (std::__1::span<const int, 18446744073709551615>) s = {
          __data = 0x000000016fdff494
          __size = 6
        }

        This function checks for a '__size' member to determine the number
        of elements in the span. If no such member exists, we get the size
        from the only other place it can be: the template argument.
        """
        # Get element type
        data_type_finder_sp = self.valobj.GetChildMemberWithName("__data_")
        if not data_type_finder_sp or not data_type_finder_sp.IsValid():
            data_type_finder_sp = self.valobj.GetChildMemberWithName("__data")

        if not data_type_finder_sp or not data_type_finder_sp.IsValid():
            return False

        self.m_element_type = data_type_finder_sp.GetType().GetPointeeType()

        # Get element size
        if self.m_element_type.IsValid():
            size = self.m_element_type.GetByteSize()
            if size and size > 0:
                self.m_element_size = size
                self.m_start = data_type_finder_sp
            else:
                return False
        else:
            return False

        # Get number of elements
        size_sp = self.valobj.GetChildMemberWithName("__size_")
        if not size_sp or not size_sp.IsValid():
            size_sp = self.valobj.GetChildMemberWithName("__size")

        if size_sp and size_sp.IsValid():
            self.m_num_elements = size_sp.GetValueAsUnsigned(0)
        else:
            # Try to get from template argument
            compiler_type = self.valobj.GetType()
            if compiler_type.GetNumberOfTemplateArguments() >= 2:
                template_arg_type = compiler_type.GetTemplateArgumentType(1)
                if template_arg_type.IsValid():
                    # For integral template arguments, we need to extract the value
                    # This is a simplification - the C++ version uses GetIntegralTemplateArgument
                    # In Python we might need to parse the type name
                    type_name = compiler_type.GetName()
                    # Try to extract extent from type name like "std::span<int, 5>"
                    try:
                        import re
                        match = re.search(r',\s*(\d+)\s*>', type_name)
                        if match:
                            self.m_num_elements = int(match.group(1))
                    except:
                        pass

        return True

    def has_children(self):
        """Check if this object has children."""
        return True
