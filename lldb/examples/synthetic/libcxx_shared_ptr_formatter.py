"""
Python LLDB data formatter for libc++ std::shared_ptr

Converted from LibCxx.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


def libcxx_shared_ptr_summary_provider(valobj, internal_dict):
    """Summary provider for std::shared_ptr."""
    valobj_sp = valobj.GetNonSyntheticValue()
    if not valobj_sp or not valobj_sp.IsValid():
        return None

    ptr_sp = valobj_sp.GetChildMemberWithName("__ptr_")
    ctrl_sp = valobj_sp.GetChildMemberWithName("__cntrl_")

    if not ctrl_sp or not ptr_sp:
        return None

    # Get pointer value
    ptr_value = ptr_sp.GetValueAsUnsigned(0)
    summary = "0x%x" % ptr_value if ptr_value != 0 else "nullptr"

    # Get control block value
    ctrl_addr = ctrl_sp.GetValueAsUnsigned(0)

    # Empty control field
    if ctrl_addr == 0:
        return summary

    # Get strong count (__shared_owners_)
    count_sp = ctrl_sp.GetChildMemberWithName("__shared_owners_")
    if count_sp and count_sp.IsValid():
        count = count_sp.GetValueAsUnsigned(0)
        # std::shared_ptr releases when __shared_owners_ hits -1
        # So __shared_owners_ == 0 means 1 owner. Add +1 here.
        summary += " strong=%d" % (count + 1)

    # Get weak count (__shared_weak_owners_)
    weak_count_sp = ctrl_sp.GetChildMemberWithName("__shared_weak_owners_")
    if weak_count_sp and weak_count_sp.IsValid():
        weak_count = weak_count_sp.GetValueAsUnsigned(0)
        # __shared_weak_owners_ indicates exact weak_ptr count
        summary += " weak=%d" % weak_count

    return summary


class LibcxxSharedPtrSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::shared_ptr."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_cntrl = None
        self.m_ptr_obj = None
        self.update()

    def num_children(self):
        """Return 1 if we have a valid control block, 0 otherwise."""
        return 1 if self.m_cntrl else 0

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if name == "pointer":
            return 0
        if name in ("object", "$$dereference$$"):
            return 1
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if not self.m_cntrl or not self.m_ptr_obj:
            return None

        if index == 0:
            return self.m_ptr_obj

        if index == 1:
            error = lldb.SBError()
            value_sp = self.m_ptr_obj.Dereference()
            if value_sp and value_sp.IsValid():
                return value_sp

        return None

    def update(self):
        """Update the cached state."""
        self.m_cntrl = None
        self.m_ptr_obj = None

        if not self.valobj.IsValid():
            return False

        target = self.valobj.GetTarget()
        if not target or not target.IsValid():
            return False

        ptr_obj_sp = self.valobj.GetChildMemberWithName("__ptr_")
        if not ptr_obj_sp or not ptr_obj_sp.IsValid():
            return False

        # Clone with user-friendly name
        self.m_ptr_obj = ptr_obj_sp.Clone("pointer")

        cntrl_sp = self.valobj.GetChildMemberWithName("__cntrl_")
        self.m_cntrl = cntrl_sp

        return True

    def has_children(self):
        """Check if this object has children."""
        return True
