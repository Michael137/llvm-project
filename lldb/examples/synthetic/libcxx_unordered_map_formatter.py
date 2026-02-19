"""
Python LLDB data formatter for libc++ std::unordered_map and std::unordered_multimap

Converted from LibCxxUnorderedMap.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb
import re


def is_std_template(type_name, template_name):
    """Check if type_name is a std template with the given name."""
    if not type_name:
        return False
    pattern = r"std::__[^:]+::" + re.escape(template_name) + r"<"
    return re.search(pattern, type_name) is not None


def is_unordered_map(type_name):
    """Check if type is an unordered_map or unordered_multimap."""
    return is_std_template(type_name, "unordered_map") or is_std_template(
        type_name, "unordered_multimap"
    )


class LibcxxStdUnorderedMapSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::unordered_map."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_element_type = None
        self.m_node_type = None
        self.m_tree = None
        self.m_num_elements = 0
        self.m_next_element = None
        self.m_elements_cache = []
        self.update()

    def num_children(self):
        """Calculate the number of children."""
        return self.m_num_elements

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
        if index >= self.m_num_elements:
            return None
        if self.m_tree is None:
            return None

        while index >= len(self.m_elements_cache):
            if self.m_next_element is None:
                return None

            error = lldb.SBError()
            node_sp = self.m_next_element.Dereference()
            if not node_sp or not node_sp.IsValid():
                return None

            value_sp = node_sp.GetChildMemberWithName("__value_")
            hash_sp = node_sp.GetChildMemberWithName("__hash_")

            if not hash_sp or not value_sp:
                # Try casting to the node type
                if self.m_node_type and self.m_node_type.IsValid():
                    node_ptr_type = self.m_node_type.GetPointerType()
                    node_sp = self.m_next_element.Cast(node_ptr_type).Dereference()
                    if not node_sp or not node_sp.IsValid():
                        return None

                    hash_sp = node_sp.GetChildMemberWithName("__hash_")
                    if not hash_sp or not hash_sp.IsValid():
                        return None

                    value_sp = node_sp.GetChildMemberWithName("__value_")
                    if not value_sp or not value_sp.IsValid():
                        # Since D101206, libc++ wraps __value_ in an anonymous union
                        # Child 0: __hash_node_base base class
                        # Child 1: __hash_
                        # Child 2: anonymous union
                        anon_union_sp = node_sp.GetChildAtIndex(2)
                        if not anon_union_sp or not anon_union_sp.IsValid():
                            return None

                        value_sp = anon_union_sp.GetChildMemberWithName("__value_")
                        if not value_sp or not value_sp.IsValid():
                            return None
                else:
                    # m_node_type is invalid and we couldn't get value directly
                    return None

            self.m_elements_cache.append(value_sp)
            next_sp = node_sp.GetChildMemberWithName("__next_")
            if next_sp and next_sp.IsValid() and next_sp.GetValueAsUnsigned(0) != 0:
                self.m_next_element = next_sp
            else:
                self.m_next_element = None

        val_hash = self.m_elements_cache[index]
        if not val_hash or not val_hash.IsValid():
            return None

        name = "[%d]" % index
        data = val_hash.GetData()
        if not data or not data.IsValid():
            return None

        target = self.valobj.GetTarget()
        if not target or not target.IsValid():
            return None

        return target.CreateValueFromData(name, data, self.m_element_type)

    def get_element_type(self, table_type):
        """Get the element type (key/value pair type)."""
        # Get value_type from the table type (like C++ does)
        element_type = table_type.FindDirectNestedType("value_type")
        if element_type and element_type.IsValid():
            element_type = element_type.GetTypedefedType()
        else:
            # Fall back to the old approach
            element_type = table_type.GetTypedefedType()
            for i in range(element_type.GetNumberOfDirectBaseClasses()):
                base = element_type.GetDirectBaseClassAtIndex(i)
                if base and "value_type" in (base.GetName() or ""):
                    element_type = base.GetTypedefedType()
                    break

        # In newer layouts, element type is directly a std::pair
        if element_type and is_std_template(element_type.GetName(), "pair"):
            return element_type

        # For older unordered_map layouts with __hash_value_type wrapper
        backend_type = self.valobj.GetType()
        if backend_type.IsPointerType() or backend_type.IsReferenceType():
            backend_type = backend_type.GetPointeeType()

        if is_unordered_map(backend_type.GetCanonicalType().GetName()):
            if element_type and element_type.GetNumberOfFields() > 0:
                field_type = element_type.GetFieldAtIndex(0).GetType()
                actual_type = field_type.GetTypedefedType()
                if is_std_template(actual_type.GetName(), "pair"):
                    return actual_type

        return element_type

    def get_node_type(self):
        """Get the node type."""
        table_sp = self.valobj.GetChildMemberWithName("__table_")
        if not table_sp or not table_sp.IsValid():
            return None

        # Try new layout
        node_sp = table_sp.GetChildMemberWithName("__first_node_")
        if not node_sp or not node_sp.IsValid():
            # Try old compressed pair layout
            p1_sp = table_sp.GetChildMemberWithName("__p1_")
            if p1_sp and p1_sp.IsValid():
                # Get first value of compressed pair
                node_sp = p1_sp.GetChildMemberWithName("__value_")
                if not node_sp or not node_sp.IsValid():
                    node_sp = p1_sp.GetChildMemberWithName("__first_")

        if not node_sp or not node_sp.IsValid():
            return None

        node_type = node_sp.GetType()
        if node_type.GetNumberOfTemplateArguments() > 0:
            template_arg = node_type.GetTemplateArgumentType(0)
            if template_arg.IsValid():
                return template_arg.GetPointeeType()

        return None

    def calculate_num_children_impl(self, table):
        """Calculate number of children from the table."""
        # Try new layout
        size_sp = table.GetChildMemberWithName("__size_")
        if size_sp and size_sp.IsValid():
            return size_sp.GetValueAsUnsigned(0)

        # Try old compressed pair layout
        p2_sp = table.GetChildMemberWithName("__p2_")
        if p2_sp and p2_sp.IsValid():
            num_elements_sp = p2_sp.GetChildMemberWithName("__value_")
            if not num_elements_sp or not num_elements_sp.IsValid():
                num_elements_sp = p2_sp.GetChildMemberWithName("__first_")

            if num_elements_sp and num_elements_sp.IsValid():
                return num_elements_sp.GetValueAsUnsigned(0)

        return 0

    def get_tree_pointer(self, table):
        """Get the tree pointer from the table."""
        # Try new layout
        tree_sp = table.GetChildMemberWithName("__first_node_")
        if not tree_sp or not tree_sp.IsValid():
            # Try old compressed pair layout
            p1_sp = table.GetChildMemberWithName("__p1_")
            if p1_sp and p1_sp.IsValid():
                tree_sp = p1_sp.GetChildMemberWithName("__value_")
                if not tree_sp or not tree_sp.IsValid():
                    tree_sp = p1_sp.GetChildMemberWithName("__first_")

        if not tree_sp or not tree_sp.IsValid():
            return None

        return tree_sp.GetChildMemberWithName("__next_")

    def update(self):
        """Update the cached state."""
        self.m_num_elements = 0
        self.m_next_element = None
        self.m_elements_cache = []

        table_sp = self.valobj.GetChildMemberWithName("__table_")
        if not table_sp or not table_sp.IsValid():
            return False

        self.m_node_type = self.get_node_type()
        if not self.m_node_type:
            return False

        table_type = table_sp.GetType()
        self.m_element_type = self.get_element_type(table_type)
        if not self.m_element_type or not self.m_element_type.IsValid():
            return False

        tree_sp = self.get_tree_pointer(table_sp)
        if not tree_sp or not tree_sp.IsValid():
            return False

        self.m_tree = tree_sp

        self.m_num_elements = self.calculate_num_children_impl(table_sp)

        if self.m_num_elements > 0:
            self.m_next_element = self.m_tree

        return True

    def has_children(self):
        """Check if this object has children."""
        return True


class LibCxxUnorderedMapIteratorSyntheticFrontEnd:
    """Synthetic children frontend for libc++ std::unordered_map::iterator."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.m_pair_sp = None
        self.update()

    def num_children(self):
        """Iterator always has 2 children (first and second)."""
        return 2

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        if name == "first":
            return 0
        if name == "second":
            return 1
        return None

    def get_child_at_index(self, index):
        """Get the child at the given index."""
        if self.m_pair_sp and self.m_pair_sp.IsValid():
            child = self.m_pair_sp.GetChildAtIndex(index)
            if child and child.IsValid():
                # Get the proper element type from the pair's type template arguments
                pair_type = self.m_pair_sp.GetType()
                if pair_type.GetNumberOfTemplateArguments() > index:
                    elem_type = pair_type.GetTemplateArgumentType(index)
                    if elem_type and elem_type.IsValid():
                        return child.Cast(elem_type)
            return child
        return None

    def update(self):
        """Update the cached state."""
        self.m_pair_sp = None

        if not self.valobj.IsValid():
            return False

        target = self.valobj.GetTarget()
        if not target or not target.IsValid():
            return False

        # Get the hash iterator
        # m_backend is an 'unordered_map::iterator', aka a
        # '__hash_map_iterator<__hash_table::iterator>'
        #
        # __hash_map_iterator::__i_ is a __hash_table::iterator (aka
        # __hash_iterator<__node_pointer>)
        hash_iter_sp = self.valobj.GetChildMemberWithName("__i_")
        if not hash_iter_sp or not hash_iter_sp.IsValid():
            return False

        # Type is '__hash_iterator<__node_pointer>'
        hash_iter_type = hash_iter_sp.GetType()
        if not hash_iter_type.IsValid():
            return False

        # Type is '__node_pointer'
        node_pointer_type = hash_iter_type.GetTemplateArgumentType(0)
        if not node_pointer_type or not node_pointer_type.IsValid():
            return False

        # Cast the __hash_iterator to a __node_pointer (which stores our key/value pair)
        hash_node_sp = hash_iter_sp.Cast(node_pointer_type)
        if not hash_node_sp or not hash_node_sp.IsValid():
            return False

        key_value_sp = hash_node_sp.GetChildMemberWithName("__value_")
        if not key_value_sp or not key_value_sp.IsValid():
            # Since D101206, libc++ wraps __value_ in an anonymous union
            # Child 0: __hash_node_base base class
            # Child 1: __hash_
            # Child 2: anonymous union
            anon_union_sp = hash_node_sp.GetChildAtIndex(2)
            if not anon_union_sp or not anon_union_sp.IsValid():
                return False

            key_value_sp = anon_union_sp.GetChildMemberWithName("__value_")
            if not key_value_sp or not key_value_sp.IsValid():
                return False

        # Create the synthetic child, which is a pair where the key and value can be
        # retrieved by querying the synthetic frontend for
        # get_child_index("first") and get_child_index("second") respectively.
        #
        # std::unordered_map stores the actual key/value pair in
        # __hash_value_type::__cc_ (or previously __cc).
        potential_child_sp = key_value_sp.Clone("pair")
        if potential_child_sp and potential_child_sp.IsValid():
            if potential_child_sp.GetNumChildren() == 1:
                child0_sp = potential_child_sp.GetChildAtIndex(0)
                if child0_sp and child0_sp.IsValid():
                    child_name = child0_sp.GetName()
                    if child_name in ("__cc_", "__cc"):
                        potential_child_sp = child0_sp.Clone("pair")

        self.m_pair_sp = potential_child_sp
        return True

    def has_children(self):
        """Check if this object has children."""
        return True

def __lldb_init_module(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::unordered_(multi)?(map|set)<.+> >$" -l lldb.formatters.cpp.libcxx_unordered_map_formatter.LibcxxStdUnorderedMapSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::__hash_map_(const_)?iterator<.+>$" -l lldb.formatters.cpp.libcxx_unordered_map_formatter.LibCxxUnorderedMapIteratorSyntheticFrontEnd -w "cplusplus-py"')
