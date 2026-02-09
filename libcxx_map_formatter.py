"""
Python LLDB data formatter for libc++ std::map

Converted from LibCxxMap.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


# The flattened layout of the std::__tree_iterator::__ptr_ looks
# as follows:
#
# The following shows the contiguous block of memory:
#
#        +-----------------------------+ class __tree_end_node
# __ptr_ | pointer __left_;            |
#        +-----------------------------+ class __tree_node_base
#        | pointer __right_;           |
#        | __parent_pointer __parent_; |
#        | bool __is_black_;           |
#        +-----------------------------+ class __tree_node
#        | __node_value_type __value_; | <<< our key/value pair
#        +-----------------------------+
#
# where __ptr_ has type __iter_pointer.


class MapEntry:
    """Wrapper around an LLDB ValueObject representing a tree node entry."""

    def __init__(self, entry_sp=None):
        self.m_entry_sp = entry_sp

    def left(self):
        """Get the left child pointer."""
        if not self.m_entry_sp:
            return None
        return self.m_entry_sp.GetSyntheticChildAtOffset(
            0, self.m_entry_sp.GetType(), True
        )

    def right(self):
        """Get the right child pointer."""
        if not self.m_entry_sp:
            return None
        addr_size = self.m_entry_sp.GetProcess().GetAddressByteSize()
        return self.m_entry_sp.GetSyntheticChildAtOffset(
            addr_size, self.m_entry_sp.GetType(), True
        )

    def parent(self):
        """Get the parent pointer."""
        if not self.m_entry_sp:
            return None
        addr_size = self.m_entry_sp.GetProcess().GetAddressByteSize()
        return self.m_entry_sp.GetSyntheticChildAtOffset(
            2 * addr_size, self.m_entry_sp.GetType(), True
        )

    def value(self):
        """Get the unsigned integer value of the entry."""
        if not self.m_entry_sp:
            return 0
        return self.m_entry_sp.GetValueAsUnsigned(0)

    def error(self):
        """Check if the entry has an error."""
        if not self.m_entry_sp:
            return True
        return self.m_entry_sp.GetError().Fail()

    def null(self):
        """Check if the entry is null."""
        return self.value() == 0

    def get_entry(self):
        """Get the underlying ValueObject."""
        return self.m_entry_sp

    def set_entry(self, entry):
        """Set the underlying ValueObject."""
        self.m_entry_sp = entry

    def __eq__(self, other):
        if not isinstance(other, MapEntry):
            return False
        if self.m_entry_sp is None and other.m_entry_sp is None:
            return True
        if self.m_entry_sp is None or other.m_entry_sp is None:
            return False
        return self.m_entry_sp.GetValueAsUnsigned() == other.m_entry_sp.GetValueAsUnsigned()


class MapIterator:
    """Iterator for traversing the red-black tree backing std::map."""

    def __init__(self, entry=None, depth=0):
        self.m_entry = MapEntry(entry) if entry else MapEntry()
        self.m_max_depth = depth
        self.m_error = False

    def value(self):
        """Get the current entry."""
        return self.m_entry.get_entry()

    def advance(self, count):
        """Advance the iterator by count steps."""
        if self.m_error:
            return None

        steps = 0
        while count > 0:
            self._next()
            count -= 1
            steps += 1
            if self.m_error or self.m_entry.null() or (steps > self.m_max_depth):
                return None

        return self.m_entry.get_entry()

    def _next(self):
        """
        Mimics libc++'s __tree_next algorithm, which libc++ uses
        in its __tree_iterator::operator++.
        """
        if self.m_entry.null():
            return

        right = MapEntry(self.m_entry.right())
        if not right.null():
            self.m_entry = self._tree_min(right)
            return

        steps = 0
        while not self._is_left_child(self.m_entry):
            if self.m_entry.error():
                self.m_error = True
                return
            self.m_entry.set_entry(self.m_entry.parent())
            steps += 1
            if steps > self.m_max_depth:
                self.m_entry = MapEntry()
                return

        self.m_entry = MapEntry(self.m_entry.parent())

    def _tree_min(self, x):
        """Mimics libc++'s __tree_min algorithm."""
        if x.null():
            return MapEntry()

        left = MapEntry(x.left())
        steps = 0
        while not left.null():
            if left.error():
                self.m_error = True
                return MapEntry()
            x = left
            left.set_entry(x.left())
            steps += 1
            if steps > self.m_max_depth:
                return MapEntry()

        return x

    def _is_left_child(self, x):
        """Check if x is a left child of its parent."""
        if x.null():
            return False
        rhs = MapEntry(x.parent())
        rhs.set_entry(rhs.left())
        return x.value() == rhs.value()


class LibcxxStdMapSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::map."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_tree = None
        self.m_root_node = None
        self.m_node_ptr_type = None
        self.m_count = None
        self.m_iterators = {}
        self.update()

    def num_children(self):
        """Calculate the number of children (map size)."""
        if self.m_count is not None:
            return self.m_count

        if self.m_tree is None:
            return 0

        # Try the new layout first (__size_)
        size_sp = self.m_tree.GetChildMemberWithName("__size_")
        if size_sp and size_sp.IsValid():
            self.m_count = size_sp.GetValueAsUnsigned(0)
            return self.m_count

        # Try old compressed pair layout (__pair3_)
        pair3_sp = self.m_tree.GetChildMemberWithName("__pair3_")
        if pair3_sp and pair3_sp.IsValid():
            return self._calculate_num_children_for_old_compressed_pair_layout(pair3_sp)

        return 0

    def _calculate_num_children_for_old_compressed_pair_layout(self, pair):
        """Handle old libc++ compressed pair layout."""
        # Try to get the first value from the compressed pair
        node_sp = pair.GetChildMemberWithName("__value_")
        if not node_sp or not node_sp.IsValid():
            # Alternative: try __first_
            node_sp = pair.GetChildMemberWithName("__first_")

        if not node_sp or not node_sp.IsValid():
            return 0

        self.m_count = node_sp.GetValueAsUnsigned(0)
        return self.m_count

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        try:
            # Names are in the format [0], [1], etc.
            if name.startswith('[') and name.endswith(']'):
                return int(name[1:-1])
        except ValueError:
            pass
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        num_children = self.num_children()
        if index >= num_children:
            return None

        if self.m_tree is None or self.m_root_node is None:
            return None

        key_val_sp = self._get_key_value_pair(index, num_children)
        if not key_val_sp:
            # This will stop all future searches until an Update() happens
            self.m_tree = None
            return None

        # Create a synthetic child with the appropriate name
        name = "[%d]" % index
        potential_child_sp = key_val_sp.Clone(name)

        if potential_child_sp and potential_child_sp.IsValid():
            num_child_children = potential_child_sp.GetNumChildren()

            # Handle __cc_ or __cc wrapper
            if num_child_children == 1:
                child0_sp = potential_child_sp.GetChildAtIndex(0)
                child_name = child0_sp.GetName() if child0_sp else ""
                if child_name in ("__cc_", "__cc"):
                    potential_child_sp = child0_sp.Clone(name)

            # Handle __cc_ and __nc wrapper
            elif num_child_children == 2:
                child0_sp = potential_child_sp.GetChildAtIndex(0)
                child1_sp = potential_child_sp.GetChildAtIndex(1)
                child0_name = child0_sp.GetName() if child0_sp else ""
                child1_name = child1_sp.GetName() if child1_sp else ""
                if child0_name in ("__cc_", "__cc") and child1_name == "__nc":
                    potential_child_sp = child0_sp.Clone(name)

        return potential_child_sp

    def update(self):
        """Update the cached state."""
        self.m_count = None
        self.m_tree = None
        self.m_root_node = None
        self.m_iterators.clear()

        self.m_tree = self.valobj.GetChildMemberWithName("__tree_")
        if not self.m_tree or not self.m_tree.IsValid():
            return False

        self.m_root_node = self.m_tree.GetChildMemberWithName("__begin_node_")
        if not self.m_root_node or not self.m_root_node.IsValid():
            return False

        # Get the __node_pointer type
        tree_type = self.m_tree.GetType()
        if tree_type.IsValid():
            self.m_node_ptr_type = tree_type.GetTypedefedType().GetTemplateArgumentType(1)
            if not self.m_node_ptr_type.IsValid():
                # Try alternative approach
                for i in range(tree_type.GetNumberOfTemplateArguments()):
                    arg_type = tree_type.GetTemplateArgumentType(i)
                    type_name = arg_type.GetName() if arg_type.IsValid() else ""
                    if "node_pointer" in type_name or "pointer" in type_name:
                        self.m_node_ptr_type = arg_type
                        break

        return True

    def has_children(self):
        """Check if this object has children."""
        return True

    def _get_key_value_pair(self, idx, max_depth):
        """
        Returns the ValueObject for the __tree_node type that
        holds the key/value pair of the node at index idx.
        """
        iterator = MapIterator(self.m_root_node, max_depth)

        advance_by = idx
        if idx > 0:
            # If we have already created the iterator for the previous
            # index, we can start from there and advance by 1.
            if idx - 1 in self.m_iterators:
                iterator = self.m_iterators[idx - 1]
                advance_by = 1

        iterated_sp = iterator.advance(advance_by)
        if not iterated_sp:
            # This tree is garbage - stop
            return None

        if not self.m_node_ptr_type or not self.m_node_ptr_type.IsValid():
            return None

        # iterated_sp is a __iter_pointer at this point.
        # We can cast it to a __node_pointer (which is what libc++ does).
        value_type_sp = iterated_sp.Cast(self.m_node_ptr_type)
        if not value_type_sp or not value_type_sp.IsValid():
            return None

        # Finally, get the key/value pair.
        value_type_sp = value_type_sp.GetChildMemberWithName("__value_")
        if not value_type_sp or not value_type_sp.IsValid():
            return None

        self.m_iterators[idx] = iterator

        return value_type_sp


class LibCxxMapIteratorSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::map::iterator."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_pair_sp = None
        self.update()

    def num_children(self):
        """Map iterators always have 2 children (first and second)."""
        return 2

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if not self.m_pair_sp:
            return None
        return self.m_pair_sp.GetIndexOfChildWithName(name)

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if not self.m_pair_sp:
            return None
        return self.m_pair_sp.GetChildAtIndex(index)

    def update(self):
        """Update the cached state."""
        self.m_pair_sp = None

        if not self.valobj.IsValid():
            return False

        target = self.valobj.GetTarget()
        if not target or not target.IsValid():
            return False

        # m_backend is a std::map::iterator
        # ...which is a __map_iterator<__tree_iterator<..., __node_pointer, ...>>
        #
        # Then, __map_iterator::__i_ is a __tree_iterator
        tree_iter_sp = self.valobj.GetChildMemberWithName("__i_")
        if not tree_iter_sp or not tree_iter_sp.IsValid():
            return False

        # Type is __tree_iterator::__node_pointer
        # (We could alternatively also get this from the template argument)
        tree_iter_type = tree_iter_sp.GetType()
        node_pointer_type = None
        if tree_iter_type.IsValid():
            node_pointer_type = tree_iter_type.GetTypedefedType().GetTemplateArgumentType(1)

        if not node_pointer_type or not node_pointer_type.IsValid():
            return False

        # __ptr_ is a __tree_iterator::__iter_pointer
        iter_pointer_sp = tree_iter_sp.GetChildMemberWithName("__ptr_")
        if not iter_pointer_sp or not iter_pointer_sp.IsValid():
            return False

        # Cast the __iter_pointer to a __node_pointer (which stores our key/value pair)
        node_pointer_sp = iter_pointer_sp.Cast(node_pointer_type)
        if not node_pointer_sp or not node_pointer_sp.IsValid():
            return False

        key_value_sp = node_pointer_sp.GetChildMemberWithName("__value_")
        if not key_value_sp or not key_value_sp.IsValid():
            return False

        # Create the synthetic child, which is a pair where the key and value can be
        # retrieved by querying the synthetic frontend for
        # GetIndexOfChildWithName("first") and GetIndexOfChildWithName("second")
        # respectively.
        #
        # std::map stores the actual key/value pair in value_type::__cc_ (or
        # previously __cc).
        key_value_sp = key_value_sp.Clone("pair")
        if key_value_sp.GetNumChildren() == 1:
            child0_sp = key_value_sp.GetChildAtIndex(0)
            child_name = child0_sp.GetName() if child0_sp else ""
            if child_name in ("__cc_", "__cc"):
                key_value_sp = child0_sp.Clone("pair")

        self.m_pair_sp = key_value_sp
        return True

    def has_children(self):
        """Check if this object has children."""
        return True


def __lldb_init_module(debugger, internal_dict):
    """Initialize the module by registering the synthetic providers."""
    # Register std::map formatter
    debugger.HandleCommand(
        'type synthetic add -l libcxx_map_formatter.LibcxxStdMapSyntheticFrontEnd '
        '-x "^std::__[[:alnum:]]+::map<.+> >(( )?&)?$" -w libcxx'
    )
    debugger.HandleCommand(
        'type synthetic add -l libcxx_map_formatter.LibcxxStdMapSyntheticFrontEnd '
        '-x "^std::__[[:alnum:]]+::multimap<.+> >(( )?&)?$" -w libcxx'
    )

    # Register std::map::iterator formatter
    debugger.HandleCommand(
        'type synthetic add -l libcxx_map_formatter.LibCxxMapIteratorSyntheticFrontEnd '
        '-x "^std::__[[:alnum:]]+::__map_iterator<.+>$" -w libcxx'
    )
    debugger.HandleCommand(
        'type synthetic add -l libcxx_map_formatter.LibCxxMapIteratorSyntheticFrontEnd '
        '-x "^std::__[[:alnum:]]+::__map_const_iterator<.+>$" -w libcxx'
    )
