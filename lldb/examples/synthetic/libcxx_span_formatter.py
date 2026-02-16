"""
Python LLDB data formatter for libc++ std::span

Converted from LibCxxSpan.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

std::span can either be instantiated with a compile-time known
extent or a std::dynamic_extent (this is the default if only the
type template argument is provided). The layout of std::span
depends on whether the extent is dynamic or not.

For static extents (e.g., std::span<int, 9>):

(std::__1::span<const int, 9>) s = {
  __data = 0x000000016fdff494
}

For dynamic extents, e.g., std::span<int>, the layout is:

(std::__1::span<const int, 18446744073709551615>) s = {
  __data = 0x000000016fdff494
  __size = 6
}
"""

import lldb


def get_child_member_with_name(valobj, *names):
    """Try to get a child member with one of the given names."""
    for name in names:
        child = valobj.GetChildMemberWithName(name)
        if child and child.IsValid():
            return child
    return None


def libcxx_span_summary_provider(valobj, internal_dict):
    """Summary provider for std::span."""
    valobj = valobj.GetNonSyntheticValue()
    if not valobj or not valobj.IsValid():
        return None

    # Try to get __size member (dynamic extent)
    size_sp = get_child_member_with_name(valobj, "__size_", "__size")
    if size_sp and size_sp.IsValid():
        num_elements = size_sp.GetValueAsUnsigned(0)
        return "size=%d" % num_elements

    # Static extent: get size from template argument
    valobj_type = valobj.GetType()
    if valobj_type.IsValid():
        type_name = valobj_type.GetName()
        num_elements = _extract_extent_from_type_name(type_name)
        return "size=%d" % num_elements

    return None


def _extract_extent_from_type_name(type_name):
    """
    Extract the extent value from a span type name.

    E.g., "std::span<int, 9>" -> 9
          "std::__1::span<int, 9>" -> 9
    """
    try:
        # Handle nested templates by finding the matching angle brackets
        depth = 0
        last_comma_pos = -1
        last_angle_pos = -1

        for i in range(len(type_name) - 1, -1, -1):
            c = type_name[i]
            if c == '>':
                if last_angle_pos == -1:
                    last_angle_pos = i
                depth += 1
            elif c == '<':
                depth -= 1
            elif c == ',' and depth == 1:
                last_comma_pos = i
                break

        if last_comma_pos != -1 and last_angle_pos != -1:
            extent_str = type_name[last_comma_pos + 1:last_angle_pos].strip()
            extent = int(extent_str)
            # Check for dynamic_extent (usually max value of size_t)
            # 18446744073709551615 is std::dynamic_extent (size_t(-1))
            if extent == 18446744073709551615:
                return 0
            return extent
    except (ValueError, IndexError):
        pass

    return 0


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
        """Return the number of children (elements in the span)."""
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

        if index < 0 or index >= self.m_num_elements:
            return None

        offset = index * self.m_element_size
        address = offset + self.m_start.GetValueAsUnsigned(0)

        return self.valobj.CreateValueFromAddress(
            "[%d]" % index, address, self.m_element_type
        )

    def update(self):
        """Update the cached state."""
        self.m_start = None
        self.m_element_type = None
        self.m_num_elements = 0
        self.m_element_size = 0

        # Get __data member (try both __data_ and __data for ABI compatibility)
        data_sp = get_child_member_with_name(self.valobj, "__data_", "__data")
        if not data_sp or not data_sp.IsValid():
            return False

        # Get element type from pointer type
        data_type = data_sp.GetType()
        if not data_type.IsValid():
            return False

        self.m_element_type = data_type.GetPointeeType()
        if not self.m_element_type.IsValid():
            return False

        # Get element size
        self.m_element_size = self.m_element_type.GetByteSize()
        if self.m_element_size == 0:
            return False

        self.m_start = data_sp

        # Get number of elements
        # First, try to get __size member (dynamic extent)
        size_sp = get_child_member_with_name(self.valobj, "__size_", "__size")
        if size_sp and size_sp.IsValid():
            self.m_num_elements = size_sp.GetValueAsUnsigned(0)
        else:
            # Static extent: get size from template argument
            valobj_type = self.valobj.GetType()
            if valobj_type.IsValid():
                type_name = valobj_type.GetName()
                self.m_num_elements = _extract_extent_from_type_name(type_name)

        return True

    def has_children(self):
        """Check if this object has children."""
        return self.m_num_elements > 0
