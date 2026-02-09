from enum import Enum
from sys import stderr
import sys
import lldb
import lldb.formatters.Logger

# libcxx STL formatters for LLDB
# These formatters are based upon the implementation of libc++ that
# ships with current releases of OS X - They will not work for other implementations
# of the standard C++ library - and they are bound to use the
# libc++-specific namespace

# the std::string summary is just an example for your convenience
# the actual summary that LLDB uses is C++ code inside the debugger's own core

# this could probably be made more efficient but since it only reads a handful of bytes at a time
# we probably don't need to worry too much about this for the time being


def make_string(F, L):
    strval = ""
    G = F.GetData().uint8
    for X in range(L):
        V = G[X]
        if V == 0:
            break
        strval = strval + chr(V % 256)
    return '"' + strval + '"'


# if we ever care about big-endian, these two functions might need to change


def is_short_string(value):
    return True if (value & 1) == 0 else False


def extract_short_size(value):
    return (value >> 1) % 256


# some of the members of libc++ std::string are anonymous or have internal names that convey
# no external significance - we access them by index since this saves a name lookup that would add
# no information for readers of the code, but when possible try to use
# meaningful variable names


def stdstring_SummaryProvider(valobj, dict):
    logger = lldb.formatters.Logger.Logger()
    r = valobj.GetChildAtIndex(0)
    B = r.GetChildAtIndex(0)
    first = B.GetChildAtIndex(0)
    D = first.GetChildAtIndex(0)
    l = D.GetChildAtIndex(0)
    s = D.GetChildAtIndex(1)
    D20 = s.GetChildAtIndex(0)
    size_mode = D20.GetChildAtIndex(0).GetValueAsUnsigned(0)
    if is_short_string(size_mode):
        size = extract_short_size(size_mode)
        return make_string(s.GetChildAtIndex(1), size)
    else:
        data_ptr = l.GetChildAtIndex(2)
        size_vo = l.GetChildAtIndex(1)
        # the NULL terminator must be accounted for
        size = size_vo.GetValueAsUnsigned(0) + 1
        if size <= 1 or size is None:  # should never be the case
            return '""'
        try:
            data = data_ptr.GetPointeeData(0, size)
        except:
            return '""'
        error = lldb.SBError()
        strval = data.GetString(error, 0)
        if error.Fail():
            return "<error:" + error.GetCString() + ">"
        else:
            return '"' + strval + '"'


def get_buffer_end(buffer, begin):
    """
    Returns a pointer to where the next element would be pushed.

    For libc++'s stable ABI and unstable < LLVM 22, returns `__end_`.
    For libc++'s unstable ABI, returns `__begin_ + __size_`.
    """
    map_end = buffer.GetChildMemberWithName("__end_")
    if map_end.IsValid():
        return map_end.GetValueAsUnsigned(0)
    map_size = buffer.GetChildMemberWithName("__size_").GetValueAsUnsigned(0)
    return begin + map_size


def get_buffer_endcap(parent, buffer, begin, has_compressed_pair_layout, is_size_based):
    """
    Returns a pointer to the end of the buffer.

    For libc++'s stable ABI and unstable < LLVM 22, returns:
        * `__end_cap_`, if `__compressed_pair` is being used
        * `__cap_`, otherwise
    For libc++'s unstable ABI, returns `__begin_ + __cap_`.
    """
    if has_compressed_pair_layout:
        map_endcap = parent._get_value_of_compressed_pair(
            buffer.GetChildMemberWithName("__end_cap_")
        )
    elif buffer.GetType().GetNumberOfDirectBaseClasses() == 1:
        # LLVM 22's __split_buffer is derived from a base class that describes its layout. When the
        # compressed pair ABI is required, we also use an anonymous struct. Per [#158131], LLDB
        # is unable to access members of an anonymous struct to a base class, through the derived
        # class. This means that in order to access the compressed pair's pointer, we need to first
        # get to its base class.
        #
        # [#158131]: https://github.com/llvm/llvm-project/issues/158131
        buffer = buffer.GetChildAtIndex(0)
        if is_size_based:
            map_endcap = buffer.GetChildMemberWithName("__cap_")
        else:
            map_endcap = buffer.GetChildMemberWithName("__back_cap_")
        map_endcap = map_endcap.GetValueAsUnsigned(0)
    else:
        map_endcap = buffer.GetChildMemberWithName("__cap_")
        if not map_endcap.IsValid():
            map_endcap = buffer.GetChildMemberWithName("__end_cap_")
        map_endcap = map_endcap.GetValueAsUnsigned(0)

    if is_size_based:
        return begin + map_endcap

    return map_endcap


class stdvector_SynthProvider:
    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            start_val = self.start.GetValueAsUnsigned(0)
            finish_val = self.finish.GetValueAsUnsigned(0)
            # Before a vector has been constructed, it will contain bad values
            # so we really need to be careful about the length we return since
            # uninitialized data can cause us to return a huge number. We need
            # to also check for any of the start, finish or end of storage values
            # being zero (NULL). If any are, then this vector has not been
            # initialized yet and we should return zero

            # Make sure nothing is NULL
            if start_val == 0 or finish_val == 0:
                return 0
            # Make sure start is less than finish
            if start_val >= finish_val:
                return 0

            num_children = finish_val - start_val
            if (num_children % self.data_size) != 0:
                return 0
            else:
                num_children = num_children / self.data_size
            return num_children
        except:
            return 0

    def get_child_index(self, name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return int(name.lstrip("[").rstrip("]"))
        except:
            return -1

    def get_child_at_index(self, index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Retrieving child " + str(index)
        if index < 0:
            return None
        if index >= self.num_children():
            return None
        try:
            offset = index * self.data_size
            return self.start.CreateChildAtOffset(
                "[" + str(index) + "]", offset, self.data_type
            )
        except:
            return None

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.start = self.valobj.GetChildMemberWithName("__begin_")
            self.finish = self.valobj.GetChildMemberWithName("__end_")
            # the purpose of this field is unclear, but it is the only field whose type is clearly T* for a vector<T>
            # if this ends up not being correct, we can use the APIs to get at
            # template arguments
            data_type_finder = self.valobj.GetChildMemberWithName(
                "__end_cap_"
            ).GetChildMemberWithName("__first_")
            self.data_type = data_type_finder.GetType().GetPointeeType()
            self.data_size = self.data_type.GetByteSize()
        except:
            pass

    def has_children(self):
        return True


# Just an example: the actual summary is produced by a summary string:
# size=${svar%#}


def stdvector_SummaryProvider(valobj, dict):
    prov = stdvector_SynthProvider(valobj, None)
    return "size=" + str(prov.num_children())


class stdlist_entry:
    def __init__(self, entry):
        logger = lldb.formatters.Logger.Logger()
        self.entry = entry

    def _next_impl(self):
        logger = lldb.formatters.Logger.Logger()
        return stdlist_entry(self.entry.GetChildMemberWithName("__next_"))

    def _prev_impl(self):
        logger = lldb.formatters.Logger.Logger()
        return stdlist_entry(self.entry.GetChildMemberWithName("__prev_"))

    def _value_impl(self):
        logger = lldb.formatters.Logger.Logger()
        return self.entry.GetValueAsUnsigned(0)

    def _isnull_impl(self):
        logger = lldb.formatters.Logger.Logger()
        return self._value_impl() == 0

    def _sbvalue_impl(self):
        logger = lldb.formatters.Logger.Logger()
        return self.entry

    next = property(_next_impl, None)
    value = property(_value_impl, None)
    is_null = property(_isnull_impl, None)
    sbvalue = property(_sbvalue_impl, None)


class stdlist_iterator:
    def increment_node(self, node):
        logger = lldb.formatters.Logger.Logger()
        if node.is_null:
            return None
        return node.next

    def __init__(self, node):
        logger = lldb.formatters.Logger.Logger()
        # we convert the SBValue to an internal node object on entry
        self.node = stdlist_entry(node)

    def value(self):
        logger = lldb.formatters.Logger.Logger()
        return self.node.sbvalue  # and return the SBValue back on exit

    def next(self):
        logger = lldb.formatters.Logger.Logger()
        node = self.increment_node(self.node)
        if node is not None and node.sbvalue.IsValid() and not (node.is_null):
            self.node = node
            return self.value()
        else:
            return None

    def advance(self, N):
        logger = lldb.formatters.Logger.Logger()
        if N < 0:
            return None
        if N == 0:
            return self.value()
        if N == 1:
            return self.next()
        while N > 0:
            self.next()
            N = N - 1
        return self.value()


class stdlist_SynthProvider:
    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj
        self.count = None

    def next_node(self, node):
        logger = lldb.formatters.Logger.Logger()
        return node.GetChildMemberWithName("__next_")

    def value(self, node):
        logger = lldb.formatters.Logger.Logger()
        return node.GetValueAsUnsigned()

    # Floyd's cycle-finding algorithm
    # try to detect if this list has a loop
    def has_loop(self):
        global _list_uses_loop_detector
        logger = lldb.formatters.Logger.Logger()
        if not _list_uses_loop_detector:
            logger >> "Asked not to use loop detection"
            return False
        slow = stdlist_entry(self.head)
        fast1 = stdlist_entry(self.head)
        fast2 = stdlist_entry(self.head)
        while slow.next.value != self.node_address:
            slow_value = slow.value
            fast1 = fast2.next
            fast2 = fast1.next
            if fast1.value == slow_value or fast2.value == slow_value:
                return True
            slow = slow.next
        return False

    def num_children(self):
        global _list_capping_size
        logger = lldb.formatters.Logger.Logger()
        if self.count is None:
            self.count = self.num_children_impl()
            if self.count > _list_capping_size:
                self.count = _list_capping_size
        return self.count

    def num_children_impl(self):
        global _list_capping_size
        logger = lldb.formatters.Logger.Logger()
        try:
            next_val = self.head.GetValueAsUnsigned(0)
            prev_val = self.tail.GetValueAsUnsigned(0)
            # After a std::list has been initialized, both next and prev will
            # be non-NULL
            if next_val == 0 or prev_val == 0:
                return 0
            if next_val == self.node_address:
                return 0
            if next_val == prev_val:
                return 1
            if self.has_loop():
                return 0
            size = 2
            current = stdlist_entry(self.head)
            while current.next.value != self.node_address:
                size = size + 1
                current = current.next
                if size > _list_capping_size:
                    return _list_capping_size
            return size - 1
        except:
            return 0

    def get_child_index(self, name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return int(name.lstrip("[").rstrip("]"))
        except:
            return -1

    def get_child_at_index(self, index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Fetching child " + str(index)
        if index < 0:
            return None
        if index >= self.num_children():
            return None
        try:
            current = stdlist_iterator(self.head)
            current = current.advance(index)
            # we do not return __value_ because then all our children would be named __value_
            # we need to make a copy of __value__ with the right name -
            # unfortunate
            obj = current.GetChildMemberWithName("__value_")
            obj_data = obj.GetData()
            return self.valobj.CreateValueFromData(
                "[" + str(index) + "]", obj_data, self.data_type
            )
        except:
            return None

    def extract_type(self):
        logger = lldb.formatters.Logger.Logger()
        list_type = self.valobj.GetType().GetUnqualifiedType()
        if list_type.IsReferenceType():
            list_type = list_type.GetDereferencedType()
        if list_type.GetNumberOfTemplateArguments() > 0:
            data_type = list_type.GetTemplateArgumentType(0)
        else:
            data_type = None
        return data_type

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        self.count = None
        try:
            impl = self.valobj.GetChildMemberWithName("__end_")
            self.node_address = self.valobj.AddressOf().GetValueAsUnsigned(0)
            self.head = impl.GetChildMemberWithName("__next_")
            self.tail = impl.GetChildMemberWithName("__prev_")
            self.data_type = self.extract_type()
            self.data_size = self.data_type.GetByteSize()
        except:
            pass

    def has_children(self):
        return True


# Just an example: the actual summary is produced by a summary string:
# size=${svar%#}
def stdlist_SummaryProvider(valobj, dict):
    prov = stdlist_SynthProvider(valobj, None)
    return "size=" + str(prov.num_children())


class stdsharedptr_SynthProvider:
    def __init__(self, valobj, d):
        logger = lldb.formatters.Logger.Logger()
        logger.write("init")
        self.valobj = valobj
        # self.element_ptr_type = self.valobj.GetType().GetTemplateArgumentType(0).GetPointerType()
        self.ptr = None
        self.cntrl = None
        process = valobj.GetProcess()
        self.endianness = process.GetByteOrder()
        self.pointer_size = process.GetAddressByteSize()
        self.count_type = valobj.GetType().GetBasicType(lldb.eBasicTypeUnsignedLong)

    def num_children(self):
        return 1

    def has_children(self):
        return True

    def get_child_index(self, name):
        if name == "__ptr_":
            return 0
        if name == "count":
            return 1
        if name == "weak_count":
            return 2
        return -1

    def get_child_at_index(self, index):
        if index == 0:
            return self.ptr
        if index == 1:
            if self.cntrl is None:
                count = 0
            else:
                count = (
                    1
                    + self.cntrl.GetChildMemberWithName(
                        "__shared_owners_"
                    ).GetValueAsSigned()
                )
            return self.valobj.CreateValueFromData(
                "count",
                lldb.SBData.CreateDataFromUInt64Array(
                    self.endianness, self.pointer_size, [count]
                ),
                self.count_type,
            )
        if index == 2:
            if self.cntrl is None:
                count = 0
            else:
                count = (
                    1
                    + self.cntrl.GetChildMemberWithName(
                        "__shared_weak_owners_"
                    ).GetValueAsSigned()
                )
            return self.valobj.CreateValueFromData(
                "weak_count",
                lldb.SBData.CreateDataFromUInt64Array(
                    self.endianness, self.pointer_size, [count]
                ),
                self.count_type,
            )
        return None

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        self.ptr = self.valobj.GetChildMemberWithName(
            "__ptr_"
        )  # .Cast(self.element_ptr_type)
        cntrl = self.valobj.GetChildMemberWithName("__cntrl_")
        if cntrl.GetValueAsUnsigned(0):
            self.cntrl = cntrl.Dereference()
        else:
            self.cntrl = None

class MapEntry:
    """Wrapper around an LLDB ValueObject representing a tree node entry."""

    def __init__(self, entry_sp=None):
        self.m_entry_sp = entry_sp

    def left(self):
        """Get the left child pointer."""
        if not self.m_entry_sp:
            return None
        return self.m_entry_sp.CreateChildAtOffset(
            "left", 0, self.m_entry_sp.GetType()
        )

    def right(self):
        """Get the right child pointer."""
        if not self.m_entry_sp:
            return None
        addr_size = self.m_entry_sp.GetProcess().GetAddressByteSize()
        return self.m_entry_sp.CreateChildAtOffset(
            "right", addr_size, self.m_entry_sp.GetType()
        )

    def parent(self):
        """Get the parent pointer."""
        if not self.m_entry_sp:
            return None
        addr_size = self.m_entry_sp.GetProcess().GetAddressByteSize()
        return self.m_entry_sp.CreateChildAtOffset(
            "parent", 2 * addr_size, self.m_entry_sp.GetType()
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
        self.m_node_ptr_type = self.m_tree.GetType().FindDirectNestedType("__node_pointer")

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

# we can use two different categories for old and new formatters - type names are different enough that we should make no confusion
# talking with libc++ developer: "std::__1::class_name is set in stone
# until we decide to change the ABI. That shouldn't happen within a 5 year
# time frame"


def __lldb_init_module(debugger, dict):
    debugger.HandleCommand(
        'type summary add -F libcxx.stdstring_SummaryProvider "std::__1::string" -w libcxx'
    )
    debugger.HandleCommand(
        'type summary add -F libcxx.stdstring_SummaryProvider "std::__1::basic_string<char, class std::__1::char_traits<char>, class std::__1::allocator<char> >" -w libcxx'
    )
    debugger.HandleCommand(
        'type synthetic add -l libcxx.stdvector_SynthProvider -x "^(std::__1::)vector<.+>$" -w libcxx'
    )
    debugger.HandleCommand(
        'type summary add -F libcxx.stdvector_SummaryProvider -e -x "^(std::__1::)vector<.+>$" -w libcxx'
    )
    debugger.HandleCommand(
        'type synthetic add -l libcxx.stdlist_SynthProvider -x "^(std::__1::)list<.+>$" -w libcxx'
    )
    debugger.HandleCommand(
        'type summary add -F libcxx.stdlist_SummaryProvider -e -x "^(std::__1::)list<.+>$" -w libcxx'
    )
    debugger.HandleCommand(
        'type summary add -F libcxx.stdmap_SummaryProvider -e -x "^(std::__1::)map<.+> >$" -w libcxx'
    )
    debugger.HandleCommand("type category enable libcxx")
    debugger.HandleCommand(
        'type synthetic add -l libcxx.stddeque_SynthProvider -x "^(std::__1::)deque<.+>$" -w libcxx'
    )
    debugger.HandleCommand(
        'type synthetic add -l libcxx.stdsharedptr_SynthProvider -x "^(std::__1::)shared_ptr<.+>$" -w libcxx'
    )
    # turns out the structs look the same, so weak_ptr can be handled the same!
    debugger.HandleCommand(
        'type synthetic add -l libcxx.stdsharedptr_SynthProvider -x "^(std::__1::)weak_ptr<.+>$" -w libcxx'
    )
    """Initialize the module by registering the synthetic providers."""

    # Register std::map formatter
    debugger.HandleCommand(
        'type synthetic add -l libcxx.LibcxxStdMapSyntheticFrontEnd '
        '-x "^std::__[[:alnum:]]+::map<.+> >(( )?&)?$" -w libcxx'
    )


_list_capping_size = 255
_list_uses_loop_detector = True
