"""
Python LLDB data formatter for libc++ std::ranges::ref_view

Converted from LibCxxRangesRefView.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


class LibcxxStdRangesRefViewSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::ranges::ref_view."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_range_sp = None
        self.update()

    def num_children(self):
        """ref_view has a single child: the dereferenced __range_."""
        return 1

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        # We only have a single child
        return 0

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        # Since we only have a single child, return it
        if index == 0:
            return self.m_range_sp
        return None

    def update(self):
        """Update the cached state."""
        range_ptr = self.valobj.GetChildMemberWithName("__range_")
        if not range_ptr or not range_ptr.IsValid():
            return False

        error = lldb.SBError()
        self.m_range_sp = range_ptr.Dereference()

        return self.m_range_sp and self.m_range_sp.IsValid()

    def has_children(self):
        """Check if this object has children."""
        return True


def __lldb_init_module(debugger, internal_dict):
    """Initialize the module by registering the synthetic provider."""
    debugger.HandleCommand(
        'type synthetic add -l libcxx_ranges_ref_view_formatter.LibcxxStdRangesRefViewSyntheticFrontEnd '
        '-x "^std::__[[:alnum:]]+::ranges::ref_view<.+>$" -w libcxx'
    )
