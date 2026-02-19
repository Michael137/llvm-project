"""
Python LLDB data formatter for libc++ std::atomic

Converted from LibCxxAtomic.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


def get_libcxx_atomic_value(valobj):
    """
    We are supporting two versions of libc++ std::atomic

    Given std::atomic<int> i;

    The previous version of std::atomic was laid out like this:

    (lldb) frame var -L -R i
    0x00007ffeefbff9a0: (std::__1::atomic<int>) i = {
    0x00007ffeefbff9a0:   std::__1::__atomic_base<int, true> = {
    0x00007ffeefbff9a0:     std::__1::__atomic_base<int, false> = {
    0x00007ffeefbff9a0:       __a_ = 5
           }
       }
    }

    In this case we need to obtain __a_ and the current version is laid out as:

    (lldb) frame var -L -R i
    0x00007ffeefbff9b0: (std::__1::atomic<int>) i = {
    0x00007ffeefbff9b0:   std::__1::__atomic_base<int, true> = {
    0x00007ffeefbff9b0:     std::__1::__atomic_base<int, false> = {
    0x00007ffeefbff9b0:       __a_ = {
    0x00007ffeefbff9b0:         std::__1::__cxx_atomic_base_impl<int> = {
    0x00007ffeefbff9b0:           __a_value = 5
                   }
             }
          }
       }
    }

    In this case we need to obtain __a_value

    The below method covers both cases and returns the relevant member.
    """
    non_synthetic = valobj.GetNonSyntheticValue()
    if not non_synthetic or not non_synthetic.IsValid():
        return None

    member_a = non_synthetic.GetChildMemberWithName("__a_")
    if not member_a or not member_a.IsValid():
        return None

    member_a_value = member_a.GetChildMemberWithName("__a_value")
    if not member_a_value or not member_a_value.IsValid():
        return member_a

    return member_a_value


def libcxx_atomic_summary_provider(valobj, internal_dict):
    """Summary provider for std::atomic."""
    atomic_value = get_libcxx_atomic_value(valobj)
    if atomic_value and atomic_value.IsValid():
        summary = atomic_value.GetSummary()
        if summary:
            return summary
    return None


def is_libcxx_atomic(valobj):
    """Check if valobj is a libc++ std::atomic."""
    if valobj and valobj.IsValid():
        non_synthetic = valobj.GetNonSyntheticValue()
        if non_synthetic and non_synthetic.IsValid():
            return non_synthetic.GetChildMemberWithName("__a_") is not None
    return False


class LibcxxStdAtomicSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::atomic."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_real_child = None
        self.update()

    def num_children(self):
        """Return 1 if we have a valid child, 0 otherwise."""
        return 1 if self.m_real_child else 0

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if name == "Value":
            return 0
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if index == 0 and self.m_real_child:
            return self.m_real_child.Clone("Value")
        return None

    def update(self):
        """Update the cached state."""
        atomic_value = get_libcxx_atomic_value(self.valobj)
        if atomic_value and atomic_value.IsValid():
            self.m_real_child = atomic_value
        return True

    def has_children(self):
        """Check if this object has children."""
        return True

def __lldb_init_module(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::atomic<.+>$" -l lldb.formatters.cpp.libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd -w "cplusplus-py"')
