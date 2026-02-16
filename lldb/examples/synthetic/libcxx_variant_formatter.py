"""
Python LLDB data formatter for libc++ std::variant

Converted from LibCxxVariant.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

libc++ variant implementation contains two members that we care about, both
are contained in the __impl member:
- __index which tells us which of the variadic template types is the active
  type for the variant
- __data is a variadic union which recursively contains itself as member
  which refers to the tailing variadic types.
  - __head which refers to the leading non pack type
    - __value refers to the actual value contained
  - __tail which refers to the remaining pack types
"""

import lldb


class LibcxxVariantIndexValidity:
    """Enum for variant index validity states."""

    VALID = 0
    INVALID = 1
    NPOS = 2


def variant_npos_value(index_byte_size):
    """Get the npos value for the given index byte size."""
    if index_byte_size == 1:
        return 0xFF
    elif index_byte_size == 2:
        return 0xFFFF
    elif index_byte_size == 4:
        return 0xFFFFFFFF
    else:
        # Fallback to stable ABI type
        return 0xFFFFFFFF


def libcxx_variant_get_index_validity(impl_sp):
    """Check if the variant index is valid."""
    if not impl_sp or not impl_sp.IsValid():
        return LibcxxVariantIndexValidity.INVALID

    index_sp = impl_sp.GetChildMemberWithName("__index")
    if not index_sp or not index_sp.IsValid():
        return LibcxxVariantIndexValidity.INVALID

    # In the stable ABI, the type of __index is just int.
    # In the unstable ABI with _LIBCPP_ABI_VARIANT_INDEX_TYPE_OPTIMIZATION,
    # the type can be unsigned char/short/int depending on variant size.
    index_type = index_sp.GetType()
    if not index_type.IsValid():
        return LibcxxVariantIndexValidity.INVALID

    index_type_bytes = index_type.GetByteSize()
    if not index_type_bytes:
        return LibcxxVariantIndexValidity.INVALID

    npos_value = variant_npos_value(index_type_bytes)
    index_value = index_sp.GetValueAsUnsigned(0)

    if index_value == npos_value:
        return LibcxxVariantIndexValidity.NPOS

    return LibcxxVariantIndexValidity.VALID


def libcxx_variant_index_value(impl_sp):
    """Get the variant index value."""
    if not impl_sp or not impl_sp.IsValid():
        return None

    index_sp = impl_sp.GetChildMemberWithName("__index")
    if not index_sp or not index_sp.IsValid():
        return None

    return index_sp.GetValueAsUnsigned(0)


def libcxx_variant_get_nth_head(impl_sp, index):
    """
    Get the Nth head from the variant's recursive __data union.

    - index 0: __data.__head.__value
    - index 1: __data.__tail.__head.__value
    - index 2: __data.__tail.__tail.__head.__value
    etc.
    """
    if not impl_sp or not impl_sp.IsValid():
        return None

    data_sp = impl_sp.GetChildMemberWithName("__data")
    if not data_sp or not data_sp.IsValid():
        return None

    current_level = data_sp
    for n in range(index):
        tail_sp = current_level.GetChildMemberWithName("__tail")
        if not tail_sp or not tail_sp.IsValid():
            return None
        current_level = tail_sp

    return current_level.GetChildMemberWithName("__head")


def libcxx_variant_summary_provider(valobj, internal_dict):
    """Summary provider for std::variant."""
    valobj_sp = valobj.GetNonSyntheticValue()
    if not valobj_sp or not valobj_sp.IsValid():
        return None

    impl_sp = valobj_sp.GetChildMemberWithName("__impl_")
    if not impl_sp or not impl_sp.IsValid():
        impl_sp = valobj_sp.GetChildMemberWithName("__impl")

    if not impl_sp or not impl_sp.IsValid():
        return None

    validity = libcxx_variant_get_index_validity(impl_sp)

    if validity == LibcxxVariantIndexValidity.INVALID:
        return None

    if validity == LibcxxVariantIndexValidity.NPOS:
        return " No Value"

    index_value = libcxx_variant_index_value(impl_sp)
    if index_value is None:
        return None

    nth_head = libcxx_variant_get_nth_head(impl_sp, index_value)
    if not nth_head or not nth_head.IsValid():
        return None

    head_type = nth_head.GetType()
    if not head_type.IsValid():
        return None

    if head_type.GetNumberOfTemplateArguments() < 2:
        return None

    template_type = head_type.GetTemplateArgumentType(1)
    if not template_type.IsValid():
        return None

    return " Active Type = %s " % template_type.GetName()


class VariantFrontEnd:
    """Synthetic children frontend for libc++ std::variant."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_size = 0
        self.update()

    def num_children(self):
        """Return the number of children."""
        return self.m_size

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        try:
            if name.startswith("[") and name.endswith("]"):
                return int(name[1:-1])
        except ValueError:
            pass
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if index >= self.m_size:
            return None

        impl_sp = self.valobj.GetChildMemberWithName("__impl_")
        if not impl_sp or not impl_sp.IsValid():
            impl_sp = self.valobj.GetChildMemberWithName("__impl")

        if not impl_sp or not impl_sp.IsValid():
            return None

        index_value = libcxx_variant_index_value(impl_sp)
        if index_value is None:
            return None

        nth_head = libcxx_variant_get_nth_head(impl_sp, index_value)
        if not nth_head or not nth_head.IsValid():
            return None

        head_type = nth_head.GetType()
        if not head_type.IsValid():
            return None

        if head_type.GetNumberOfTemplateArguments() < 2:
            return None

        template_type = head_type.GetTemplateArgumentType(1)
        if not template_type.IsValid():
            return None

        head_value = nth_head.GetChildMemberWithName("__value")
        if not head_value or not head_value.IsValid():
            return None

        return head_value.Clone("Value")

    def update(self):
        """Update the cached state."""
        self.m_size = 0

        impl_sp = self.valobj.GetChildMemberWithName("__impl_")
        if not impl_sp or not impl_sp.IsValid():
            impl_sp = self.valobj.GetChildMemberWithName("__impl")

        if not impl_sp or not impl_sp.IsValid():
            return False

        validity = libcxx_variant_get_index_validity(impl_sp)

        if validity == LibcxxVariantIndexValidity.INVALID:
            return False

        if validity == LibcxxVariantIndexValidity.NPOS:
            return True

        self.m_size = 1
        return True

    def has_children(self):
        """Check if this object has children."""
        return True
