"""
Python LLDB data formatter for libc++ std::list

Converted from GenericList.cpp

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


class ListEntry:
    """Represents a node in the list."""

    def __init__(self, entry_sp=None):
        self.m_entry_sp = entry_sp

    def value(self):
        """Get the address value of this entry."""
        if not self.m_entry_sp or not self.m_entry_sp.IsValid():
            return 0
        return self.m_entry_sp.GetValueAsUnsigned(0)

    def next(self):
        """Get the next entry in the list."""
        if not self.m_entry_sp or not self.m_entry_sp.IsValid():
            return ListEntry()
        next_sp = self.m_entry_sp.GetChildMemberWithName("__next_")
        return ListEntry(next_sp)

    def prev(self):
        """Get the previous entry in the list."""
        if not self.m_entry_sp or not self.m_entry_sp.IsValid():
            return ListEntry()
        prev_sp = self.m_entry_sp.GetChildMemberWithName("__prev_")
        return ListEntry(prev_sp)

    def null(self):
        """Check if this entry is null."""
        return self.value() == 0

    def __bool__(self):
        return (
            self.m_entry_sp is not None
            and self.m_entry_sp.IsValid()
            and not self.null()
        )

    def get_entry(self):
        """Get the underlying ValueObject."""
        return self.m_entry_sp

    def __eq__(self, other):
        return self.value() == other.value()

    def __ne__(self, other):
        return not self.__eq__(other)


class ListIterator:
    """Iterator for traversing the list."""

    def __init__(self, entry):
        if isinstance(entry, ListEntry):
            self.m_entry = entry
        else:
            self.m_entry = ListEntry(entry)

    def value(self):
        """Get the current entry's ValueObject."""
        return self.m_entry.get_entry()

    def advance(self, count):
        """Advance the iterator by count steps."""
        if count == 0:
            return self.m_entry.get_entry()
        if count == 1:
            self._next()
            return self.m_entry.get_entry()
        while count > 0:
            self._next()
            count -= 1
            if self.m_entry.null():
                return None
        return self.m_entry.get_entry()

    def _next(self):
        """Move to the next entry."""
        self.m_entry = self.m_entry.next()

    def __eq__(self, other):
        return self.m_entry == other.m_entry


class LibcxxListSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::list."""

    def __init__(self, valobj, _internal_dict):
        self.valobj = valobj
        self.m_count = 0xFFFFFFFF  # UINT32_MAX sentinel
        self.m_head = None
        self.m_tail = None
        self.m_node_address = 0
        self.m_element_type = None
        self.m_list_capping_size = 255

        # Loop detection state
        self.m_loop_detected = 0
        self.m_slow_runner = ListEntry()
        self.m_fast_runner = ListEntry()

        # Iterator cache for efficient traversal
        self.m_iterators = {}

        self.update()

    def num_children(self):
        """Calculate the number of children."""
        if self.m_count != 0xFFFFFFFF:
            return self.m_count

        if not self.m_head or not self.m_tail or self.m_node_address == 0:
            return 0

        # Try to get the size from __size_ or __size_alloc_
        size_node_sp, is_compressed_pair = get_value_or_old_compressed_pair(
            self.valobj, "__size_", "__size_alloc_"
        )
        if is_compressed_pair and size_node_sp:
            size_node_sp = get_first_value_of_libcxx_compressed_pair(size_node_sp)

        if size_node_sp and size_node_sp.IsValid():
            self.m_count = size_node_sp.GetValueAsUnsigned(0xFFFFFFFF)

        if self.m_count != 0xFFFFFFFF:
            return self.m_count

        # Fall back to counting nodes
        next_val = self.m_head.GetValueAsUnsigned(0)
        prev_val = self.m_tail.GetValueAsUnsigned(0)
        if next_val == 0 or prev_val == 0:
            return 0
        if next_val == self.m_node_address:
            return 0
        if next_val == prev_val:
            return 1

        size = 2
        current = ListEntry(self.m_head)
        while current.next() and current.next().value() != self.m_node_address:
            size += 1
            current = current.next()
            if size > self.m_list_capping_size:
                break

        self.m_count = size - 1
        return self.m_count

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        try:
            if name.startswith("[") and name.endswith("]"):
                return int(name[1:-1])
        except ValueError:
            pass
        return None

    def _has_loop(self, count):
        """Detect if there's a loop in the list using tortoise-hare algorithm."""
        # Don't bother checking for a loop if we won't actually need to jump nodes
        if self.m_count < 2:
            return False

        if self.m_loop_detected == 0:
            # First time setup
            self.m_slow_runner = ListEntry(self.m_head).next()
            self.m_fast_runner = self.m_slow_runner.next()
            self.m_loop_detected = 1

        # Run loop detection
        steps_to_run = min(count, self.m_count)
        while (
            self.m_loop_detected < steps_to_run
            and self.m_slow_runner
            and self.m_fast_runner
            and self.m_slow_runner != self.m_fast_runner
        ):
            self.m_slow_runner = self.m_slow_runner.next()
            self.m_fast_runner = self.m_fast_runner.next().next()
            self.m_loop_detected += 1

        if count <= self.m_loop_detected:
            return False
        if not self.m_slow_runner or not self.m_fast_runner:
            return False
        return self.m_slow_runner == self.m_fast_runner

    def _get_item(self, idx):
        """Get the node at the given index, using cached iterators."""
        advance = idx
        current = ListIterator(self.m_head)
        if idx > 0:
            cached = self.m_iterators.get(idx - 1)
            if cached is not None:
                current = cached
                advance = 1
        value_sp = current.advance(advance)
        self.m_iterators[idx] = current
        return value_sp

    def get_child_at_index(self, idx):
        """Get the child at the given index."""
        if idx >= self.num_children():
            return None

        if not self.m_head or not self.m_tail or self.m_node_address == 0:
            return None

        if self._has_loop(idx + 1):
            return None

        current_sp = self._get_item(idx)
        if not current_sp or not current_sp.IsValid():
            return None

        # current_sp is a __next_ pointer. Get the node address it points to.
        node_addr = current_sp.GetValueAsUnsigned(0)
        if node_addr == 0:
            return None

        # __value_ is at offset 2*pointer_size in the node (after __prev_ and __next_)
        process = self.valobj.GetProcess()
        if not process or not process.IsValid():
            return None

        ptr_size = process.GetAddressByteSize()
        value_addr = node_addr + 2 * ptr_size

        target = self.valobj.GetTarget()
        if not target or not target.IsValid() or not self.m_element_type:
            return None

        name = "[%d]" % idx
        sb_addr = lldb.SBAddress(value_addr, target)
        return target.CreateValueFromAddress(name, sb_addr, self.m_element_type)

    def update(self):
        """Update the cached state."""
        # Reset state
        self.m_loop_detected = 0
        self.m_count = 0xFFFFFFFF
        self.m_head = None
        self.m_tail = None
        self.m_node_address = 0
        self.m_list_capping_size = 255
        self.m_slow_runner = ListEntry()
        self.m_fast_runner = ListEntry()
        self.m_iterators = {}
        self.m_element_type = None

        if not self.valobj.IsValid():
            return False

        # Get target's max children setting
        target = self.valobj.GetTarget()
        if target and target.IsValid():
            max_children = target.GetMaximumNumberOfChildrenToDisplay()
            if max_children > 0:
                self.m_list_capping_size = max_children

        # Get element type from template argument
        list_type = self.valobj.GetType()
        if list_type.IsReferenceType():
            list_type = list_type.GetNonReferenceType()

        if list_type.GetNumberOfTemplateArguments() > 0:
            self.m_element_type = list_type.GetTemplateArgumentType(0)

        # Get the address of the backend (for detecting end of circular list)
        backend_addr = self.valobj.AddressOf()
        if not backend_addr or not backend_addr.IsValid():
            return False
        self.m_node_address = backend_addr.GetValueAsUnsigned(0)
        if self.m_node_address == 0 or self.m_node_address == 0xFFFFFFFFFFFFFFFF:
            return False

        # Get __end_ member
        impl_sp = self.valobj.GetChildMemberWithName("__end_")
        if not impl_sp or not impl_sp.IsValid():
            return False

        # Get head (__next_) and tail (__prev_) from __end_
        self.m_head = impl_sp.GetChildMemberWithName("__next_")
        self.m_tail = impl_sp.GetChildMemberWithName("__prev_")

        return True

    def has_children(self):
        """Check if this object has children."""
        return True
