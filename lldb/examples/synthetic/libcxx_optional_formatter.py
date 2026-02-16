"""
Python LLDB data formatter for libc++ std::optional

Converted from GenericOptional.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""


def libcxx_optional_summary_provider(valobj, _internal_dict):
    """Summary provider for std::optional."""
    valobj_sp = valobj.GetNonSyntheticValue()
    if not valobj_sp or not valobj_sp.IsValid():
        return None

    # Check __engaged_ directly instead of relying on synthetic provider
    engaged_sp = valobj_sp.GetChildMemberWithName("__engaged_")
    if not engaged_sp or not engaged_sp.IsValid():
        return None

    has_value = engaged_sp.GetValueAsUnsigned(0) != 0
    return " Has Value=%s " % ("true" if has_value else "false")


class LibcxxOptionalSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::optional."""

    def __init__(self, valobj, _internal_dict):
        self.valobj = valobj
        self.m_has_value = False
        self.m_value = None
        self.update()

    def num_children(self):
        """Return number of children (1 if has value, 0 otherwise)."""
        return 1 if self.m_has_value else 0

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if name == "$$dereference$$":
            return 0
        if name == "Value":
            return 0
        try:
            if name.startswith("[") and name.endswith("]"):
                return int(name[1:-1])
        except ValueError:
            pass
        return None

    def get_child_at_index(self, idx):
        """Get the child at the given index."""
        if not self.m_has_value:
            return None

        if idx != 0:
            return None

        return self.m_value

    def update(self):
        """Update the cached state."""
        self.m_has_value = False
        self.m_value = None

        if not self.valobj.IsValid():
            return False

        # __engaged_ is a bool flag that is true if the optional contains a value
        engaged_sp = self.valobj.GetChildMemberWithName("__engaged_")
        if not engaged_sp or not engaged_sp.IsValid():
            return False

        self.m_has_value = engaged_sp.GetValueAsUnsigned(0) != 0

        if not self.m_has_value:
            return True

        # Find __val_ through the parent of __engaged_
        parent = engaged_sp.GetParent()
        if not parent or not parent.IsValid():
            return False

        # Try the direct approach from C++ code first:
        # parent->GetChildAtIndex(0)->GetChildMemberWithName("__val_")
        first_child = parent.GetChildAtIndex(0)
        if first_child and first_child.IsValid():
            val_sp = first_child.GetChildMemberWithName("__val_")
            if val_sp and val_sp.IsValid():
                self.m_value = val_sp.Clone("Value")
                return True

        return False

    def has_children(self):
        """Check if this object has children."""
        return self.m_has_value
