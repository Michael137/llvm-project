"""
Python LLDB data formatter for libc++ std::__wrap_iter (vector iterator)

Converted from LibCxx.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Format for std::__wrap_iter<T*>:
(lldb) fr var ibeg --raw --ptr-depth 1 -T
(std::__1::__wrap_iter<int *>) ibeg = {
  (std::__1::__wrap_iter<int *>::iterator_type) __i = 0x00000001001037a0 {
    (int) *__i = 1
  }
}
"""

import lldb


class VectorIteratorSyntheticFrontEnd:
    """
    Synthetic children frontend for libc++ vector iterator (__wrap_iter).

    This is a generic iterator formatter that wraps a pointer.
    """

    def __init__(self, valobj, internal_dict, item_names=None):
        self.valobj = valobj
        self.item_names = item_names or ["__i_", "__i"]
        self.item = None
        self.update()

    def num_children(self):
        """Return 1 child (the pointed-to item)."""
        return 1 if self.item else 0

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if name == "item":
            return 0
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if index == 0 and self.item:
            return self.item
        return None

    def update(self):
        """Update the cached state."""
        self.item = None

        # Try to find the iterator member with alternative names
        item_ptr = None
        for name in self.item_names:
            item_sp = self.valobj.GetChildMemberWithName(name)
            if item_sp and item_sp.IsValid():
                item_ptr = item_sp
                break

        if not item_ptr:
            return False

        # Dereference the pointer and name it "item"
        deref = item_ptr.Dereference()
        if deref and deref.IsValid():
            self.item = deref.Clone("item")

        return True

    def has_children(self):
        """Check if this object has children."""
        return True

def __lldb_init_module(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::__wrap_iter<.+>$" -l lldb.formatters.cpp.libcxx_vector_iterator_formatter.VectorIteratorSyntheticFrontEnd -w "cplusplus-py"')
