"""
Python LLDB data formatter for libc++ std::unique_ptr

Converted from LibCxx.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


def get_value_or_old_compressed_pair(obj, child_name, compressed_pair_name):
    """
    Helper to get value from either new or old compressed pair layout.

    Returns tuple: (ValueObjectSP, is_compressed_pair)
    """
    # Try new layout first
    node_sp = obj.GetChildMemberWithName(child_name)
    if node_sp and node_sp.IsValid():
        # Check if it's a compressed pair
        type_name = node_sp.GetTypeName()
        is_compressed = "__compressed_pair" in type_name if type_name else False
        return (node_sp, is_compressed)

    # Try old compressed pair layout
    if compressed_pair_name:
        node_sp = obj.GetChildMemberWithName(compressed_pair_name)
        if node_sp and node_sp.IsValid():
            type_name = node_sp.GetTypeName()
            is_compressed = "__compressed_pair" in type_name if type_name else False
            return (node_sp, is_compressed)

    return (None, False)


def get_first_value_of_libcxx_compressed_pair(pair):
    """Get the first value from a libc++ compressed pair."""
    first_child = pair.GetChildAtIndex(0)
    if first_child and first_child.IsValid():
        value = first_child.GetChildMemberWithName("__value_")
        if value and value.IsValid():
            return value

    # pre-c88580c member name
    value = pair.GetChildMemberWithName("__first_")
    return value


def get_second_value_of_libcxx_compressed_pair(pair):
    """Get the second value from a libc++ compressed pair."""
    if pair.GetNumChildren() > 1:
        second_child = pair.GetChildAtIndex(1)
        if second_child and second_child.IsValid():
            value = second_child.GetChildMemberWithName("__value_")
            if value and value.IsValid():
                return value

    # pre-c88580c member name
    value = pair.GetChildMemberWithName("__second_")
    return value


def libcxx_unique_ptr_summary_provider(valobj, internal_dict):
    """Summary provider for std::unique_ptr."""
    valobj_sp = valobj.GetNonSyntheticValue()
    if not valobj_sp or not valobj_sp.IsValid():
        return None

    ptr_sp, is_compressed_pair = get_value_or_old_compressed_pair(
        valobj_sp, "__ptr_", "__ptr_"
    )

    if not ptr_sp:
        return None

    if is_compressed_pair:
        ptr_sp = get_first_value_of_libcxx_compressed_pair(ptr_sp)

    if not ptr_sp or not ptr_sp.IsValid():
        return None

    ptr_value = ptr_sp.GetValueAsUnsigned(0)
    return "0x%x" % ptr_value if ptr_value != 0 else "nullptr"


class LibcxxUniquePtrSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::unique_ptr."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_value_ptr_sp = None
        self.m_deleter_sp = None
        self.update()

    def num_children(self):
        """Return number of children."""
        if self.m_value_ptr_sp:
            return 2 if self.m_deleter_sp else 1
        return 0

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if name == "pointer":
            return 0
        if name == "deleter":
            return 1
        if name in ("obj", "object", "$$dereference$$"):
            return 2
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if not self.m_value_ptr_sp:
            return None

        if index == 0:
            return self.m_value_ptr_sp

        if index == 1:
            return self.m_deleter_sp

        if index == 2:
            error = lldb.SBError()
            value_sp = self.m_value_ptr_sp.Dereference()
            if value_sp and value_sp.IsValid():
                return value_sp

        return None

    def update(self):
        """Update the cached state."""
        if not self.valobj.IsValid():
            return False

        ptr_sp, is_compressed_pair = get_value_or_old_compressed_pair(
            self.valobj, "__ptr_", "__ptr_"
        )

        if not ptr_sp:
            return False

        # Retrieve the actual pointer and the deleter
        if is_compressed_pair:
            value_pointer_sp = get_first_value_of_libcxx_compressed_pair(ptr_sp)
            if value_pointer_sp and value_pointer_sp.IsValid():
                self.m_value_ptr_sp = value_pointer_sp.Clone("pointer")

            deleter_sp = get_second_value_of_libcxx_compressed_pair(ptr_sp)
            if deleter_sp and deleter_sp.IsValid():
                self.m_deleter_sp = deleter_sp.Clone("deleter")
        else:
            self.m_value_ptr_sp = ptr_sp.Clone("pointer")

            deleter_sp = self.valobj.GetChildMemberWithName("__deleter_")
            if deleter_sp and deleter_sp.IsValid():
                if deleter_sp.GetNumChildren() > 0:
                    self.m_deleter_sp = deleter_sp.Clone("deleter")

        return True

    def has_children(self):
        """Check if this object has children."""
        return True

def init_formatter(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::unique_ptr<.+>$" -l lldb.formatters.cpp.formatter_impls.libcxx_unique_ptr_formatter.LibcxxUniquePtrSyntheticFrontEnd -w "cplusplus-py"')
