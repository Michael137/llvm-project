from enum import Enum
from sys import stderr
import sys

import lldb
import lldb.formatters.Logger


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


class stddeque_SynthProvider:
    def __init__(self, valobj, d):
        logger = lldb.formatters.Logger.Logger()
        logger.write("init")
        self.valobj = valobj
        self.pointer_size = self.valobj.GetProcess().GetAddressByteSize()
        self.count = None
        try:
            self.find_block_size()
        except:
            self.block_size = -1
            self.element_size = -1
        logger.write(
            "block_size=%d, element_size=%d" % (self.block_size, self.element_size)
        )

    def find_block_size(self):
        # in order to use the deque we must have the block size, or else
        # it's impossible to know what memory addresses are valid
        obj_type = self.valobj.GetType()
        if obj_type.IsReferenceType():
            obj_type = obj_type.GetDereferencedType()
        elif obj_type.IsPointerType():
            obj_type = obj_type.GetPointeeType()
        self.element_type = obj_type.GetTemplateArgumentType(0)
        self.element_size = self.element_type.GetByteSize()
        # The code says this, but there must be a better way:
        # template <class _Tp, class _Allocator>
        # class __deque_base {
        #    static const difference_type __block_size = sizeof(value_type) < 256 ? 4096 / sizeof(value_type) : 16;
        # }
        if self.element_size < 256:
            self.block_size = 4096 // self.element_size
        else:
            self.block_size = 16

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        if self.count is None:
            return 0
        return self.count

    def has_children(self):
        return True

    def get_child_index(self, name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return int(name.lstrip("[").rstrip("]"))
        except:
            return -1

    @staticmethod
    def _subscript(ptr: lldb.SBValue, idx: int, name: str) -> lldb.SBValue:
        """Access a pointer value as if it was an array. Returns ptr[idx]."""
        deref_t = ptr.GetType().GetPointeeType()
        offset = idx * deref_t.GetByteSize()
        return ptr.CreateChildAtOffset(name, offset, deref_t)

    def get_child_at_index(self, index):
        logger = lldb.formatters.Logger.Logger()
        logger.write("Fetching child " + str(index))
        if index < 0 or self.count is None:
            return None
        if index >= self.num_children():
            return None
        try:
            i, j = divmod(self.start + index, self.block_size)
            val = stddeque_SynthProvider._subscript(self.map_begin, i, "")
            return stddeque_SynthProvider._subscript(val, j, f"[{index}]")
        except:
            return None

    def _get_value_of_compressed_pair(self, pair):
        value = pair.GetChildMemberWithName("__value_")
        if not value.IsValid():
            # pre-r300140 member name
            value = pair.GetChildMemberWithName("__first_")
        return value.GetValueAsUnsigned(0)

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            has_compressed_pair_layout = True
            alloc_valobj = self.valobj.GetChildMemberWithName("__alloc_")
            size_valobj = self.valobj.GetChildMemberWithName("__size_")
            if alloc_valobj.IsValid() and size_valobj.IsValid():
                has_compressed_pair_layout = False

            # A deque is effectively a two-dim array, with fixed width.
            # 'map' contains pointers to the rows of this array. The
            # full memory area allocated by the deque is delimited
            # by 'first' and 'end_cap'. However, only a subset of this
            # memory contains valid data since a deque may have some slack
            # at the front and back in order to have O(1) insertion at
            # both ends. The rows in active use are delimited by
            # 'begin' and 'end'.
            #
            # To find the elements that are actually constructed, the 'start'
            # variable tells which element in this NxM array is the 0th
            # one, and the 'size' element gives the number of elements
            # in the deque.
            if has_compressed_pair_layout:
                count = self._get_value_of_compressed_pair(
                    self.valobj.GetChildMemberWithName("__size_")
                )
            else:
                count = size_valobj.GetValueAsUnsigned(0)

            # give up now if we cant access memory reliably
            if self.block_size < 0:
                logger.write("block_size < 0")
                return
            start = self.valobj.GetChildMemberWithName("__start_").GetValueAsUnsigned(0)

            map_ = self.valobj.GetChildMemberWithName("__map_")
            is_size_based = map_.GetChildMemberWithName("__size_").IsValid()
            first = map_.GetChildMemberWithName("__first_")
            # LLVM 22 renames __map_.__begin_ to __map_.__front_cap_
            if not first:
                first = map_.GetChildMemberWithName("__front_cap_")
            map_first = first.GetValueAsUnsigned(0)
            self.map_begin = map_.GetChildMemberWithName("__begin_")
            map_begin = self.map_begin.GetValueAsUnsigned(0)
            map_end = get_buffer_end(map_, map_begin)
            map_endcap = get_buffer_endcap(
                self, map_, map_begin, has_compressed_pair_layout, is_size_based
            )

            # check consistency
            if not map_first <= map_begin <= map_end <= map_endcap:
                logger.write("map pointers are not monotonic")
                return
            total_rows, junk = divmod(map_endcap - map_first, self.pointer_size)
            if junk:
                logger.write("endcap-first doesnt align correctly")
                return
            active_rows, junk = divmod(map_end - map_begin, self.pointer_size)
            if junk:
                logger.write("end-begin doesnt align correctly")
                return
            start_row, junk = divmod(map_begin - map_first, self.pointer_size)
            if junk:
                logger.write("begin-first doesnt align correctly")
                return

            logger.write(
                "update success: count=%r, start=%r, first=%r" % (count, start, first)
            )
            # if consistent, save all we really need:
            self.count = count
            self.start = start
            self.first = first
        except:
            self.count = None
            self.start = None
            self.map_first = None
            self.map_begin = None
        return False
