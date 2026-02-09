"""
Python LLDB data formatter for libc++ std::slice_array

Converted from LibCxxSliceArray.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Data formatter for libc++'s std::slice_array.

A slice_array is created by using:
  operator[](std::slice slicearr);
and std::slice is created by:
  slice(std::size_t start, std::size_t size, std::size_t stride);
The std::slice_array has the following members:
- __vp_ points to std::valarray::__begin_ + @a start
- __size_ is @a size
- __stride_ is @a stride
"""

import lldb


def libcxx_std_slice_array_summary_provider(valobj, internal_dict):
    """Summary provider for std::slice_array."""
    obj = valobj.GetNonSyntheticValue()
    if not obj or not obj.IsValid():
        return None

    ptr_sp = obj.GetChildMemberWithName("__size_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None
    size = ptr_sp.GetValueAsUnsigned(0)

    ptr_sp = obj.GetChildMemberWithName("__stride_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None
    stride = ptr_sp.GetValueAsUnsigned(0)

    return "stride=%d size=%d" % (stride, size)


class LibcxxStdSliceArraySyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::slice_array."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_start = None
        self.m_size = 0
        self.m_stride = 0
        self.m_element_type = None
        self.m_element_size = 0
        self.update()

    def num_children(self):
        """Calculate the number of children."""
        return self.m_size

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

        offset = index * self.m_stride * self.m_element_size
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

        start_sp = self.valobj.GetChildMemberWithName("__vp_")
        size_sp = self.valobj.GetChildMemberWithName("__size_")
        stride_sp = self.valobj.GetChildMemberWithName("__stride_")

        if not start_sp or not size_sp or not stride_sp:
            return False

        self.m_start = start_sp
        self.m_size = size_sp.GetValueAsUnsigned(0)
        self.m_stride = stride_sp.GetValueAsUnsigned(0)

        return True

    def has_children(self):
        """Check if this object has children."""
        return True


def __lldb_init_module(debugger, internal_dict):
    """Initialize the module by registering the formatters."""
    # Register summary provider
    debugger.HandleCommand(
        'type summary add -F libcxx_slice_array_formatter.libcxx_std_slice_array_summary_provider '
        '-x "^std::__[[:alnum:]]+::slice_array<.+>$" -w libcxx'
    )

    # Register synthetic provider
    debugger.HandleCommand(
        'type synthetic add -l libcxx_slice_array_formatter.LibcxxStdSliceArraySyntheticFrontEnd '
        '-x "^std::__[[:alnum:]]+::slice_array<.+>$" -w libcxx'
    )
