"""
Python LLDB data formatter for libc++ std::bitset

Converted from GenericBitset.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


class LibcxxBitsetSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::bitset."""

    def __init__(self, valobj, _internal_dict):
        self.valobj = valobj
        self.m_elements = []
        self.m_first = None
        self.m_bool_type = None
        self.m_size = 0

        if valobj.IsValid():
            target = valobj.GetTarget()
            if target and target.IsValid():
                self.m_bool_type = target.GetBasicType(lldb.eBasicTypeBool)

        self.update()

    def num_children(self):
        """Calculate the number of children."""
        return self.m_size

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        try:
            if name.startswith("[") and name.endswith("]"):
                idx = int(name[1:-1])
                if idx >= self.m_size:
                    return None
                return idx
        except ValueError:
            pass
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if index >= self.m_size or not self.m_first:
            return None

        if self.m_elements[index]:
            return self.m_elements[index]

        if not self.m_bool_type or not self.m_bool_type.IsValid():
            return None

        # For small bitsets __first_ is not an array, but a plain size_t.
        first_type = self.m_first.GetType()
        if first_type.IsArrayType():
            element_type = first_type.GetArrayElementType()
            if not element_type.IsValid():
                return None
            bit_size = element_type.GetByteSize() * 8
            if bit_size == 0:
                return None
            chunk = self.m_first.GetChildAtIndex(index // bit_size)
        else:
            bit_size = first_type.GetByteSize() * 8
            if bit_size == 0:
                return None
            chunk = self.m_first

        if not chunk or not chunk.IsValid():
            return None

        chunk_idx = index % bit_size
        chunk_val = chunk.GetValueAsUnsigned(0)
        bit_set = (chunk_val & (1 << chunk_idx)) != 0

        process = self.valobj.GetProcess()
        if not process or not process.IsValid():
            return None

        error = lldb.SBError()
        data = lldb.SBData()
        data.SetData(
            error,
            bytes([1 if bit_set else 0]),
            process.GetByteOrder(),
            process.GetAddressByteSize(),
        )

        name = "[%d]" % index
        target = self.valobj.GetTarget()
        if not target or not target.IsValid():
            return None

        child = target.CreateValueFromData(name, data, self.m_bool_type)
        if child and child.IsValid():
            self.m_elements[index] = child

        return child

    def update(self):
        """Update the cached state."""
        self.m_elements = []
        self.m_first = None
        self.m_size = 0

        if not self.valobj.IsValid():
            return False

        # Get the size from the template argument
        compiler_type = self.valobj.GetType()
        if not compiler_type.IsValid():
            return False

        # GetNonReferenceType to handle references to bitset
        compiler_type = compiler_type.GetNonReferenceType()

        num_template_args = compiler_type.GetNumberOfTemplateArguments()
        if num_template_args == 0:
            return False

        # Template argument 0 is the size (it's an integral value, not a type).
        # For integral template arguments, we extract from the type name.
        type_name = compiler_type.GetName()
        # Parse size from type name like "std::bitset<32>"
        try:
            start = type_name.find("<")
            end = type_name.find(">")
            if start != -1 and end != -1:
                size_str = type_name[start + 1 : end].strip()
                self.m_size = int(size_str)
        except (ValueError, IndexError):
            return False

        self.m_elements = [None] * self.m_size

        # Get the data container member
        self.m_first = self.valobj.GetChildMemberWithName("__first_")
        if not self.m_first or not self.m_first.IsValid():
            return False

        return True

    def has_children(self):
        """Check if this object has children."""
        return True

def init_formatter(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::bitset<.+>$" -l lldb.formatters.cpp.formatter_impls.libcxx_bitset_formatter.LibcxxBitsetSyntheticFrontEnd -w "cplusplus-py" -D true')
