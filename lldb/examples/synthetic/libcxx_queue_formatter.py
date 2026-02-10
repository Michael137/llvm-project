"""
Python LLDB data formatter for libc++ std::queue and std::priority_queue

Converted from LibCxxQueue.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


class QueueFrontEnd:
    """Synthetic children frontend for libc++ std::queue."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_container_sp = None
        self.update()

    def num_children(self):
        """Return the number of children."""
        if self.m_container_sp:
            return self.m_container_sp.GetNumChildren()
        return 0

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if self.m_container_sp:
            return self.m_container_sp.GetIndexOfChildWithName(name)
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if self.m_container_sp:
            return self.m_container_sp.GetChildAtIndex(index)
        return None

    def update(self):
        """Update the cached state."""
        self.m_container_sp = None

        c_sp = self.valobj.GetChildMemberWithName("c")
        if not c_sp or not c_sp.IsValid():
            return False

        # Get the synthetic value of the underlying container
        self.m_container_sp = c_sp.GetSyntheticValue()
        return True

    def has_children(self):
        """Check if this object has children."""
        return True
