from lldb.formatters.cpp.libcxx_atomic_formatter import *
from lldb.formatters.cpp.libcxx_chrono_formatter import *
from lldb.formatters.cpp.libcxx_function_formatter import *
from lldb.formatters.cpp.libcxx_map_formatter import *
from lldb.formatters.cpp.libcxx_ordering_formatter import *
from lldb.formatters.cpp.libcxx_proxy_array_formatter import *
from lldb.formatters.cpp.libcxx_queue_formatter import *
from lldb.formatters.cpp.libcxx_ranges_ref_view_formatter import *
from lldb.formatters.cpp.libcxx_shared_ptr_formatter import *
from lldb.formatters.cpp.libcxx_slice_array_formatter import *
from lldb.formatters.cpp.libcxx_span_formatter import *
from lldb.formatters.cpp.libcxx_string_formatter import *
from lldb.formatters.cpp.libcxx_string_view_formatter import *
from lldb.formatters.cpp.libcxx_tuple_formatter import *
from lldb.formatters.cpp.libcxx_unique_ptr_formatter import *
from lldb.formatters.cpp.libcxx_unordered_map_formatter import *
from lldb.formatters.cpp.libcxx_valarray_formatter import *
from lldb.formatters.cpp.libcxx_variant_formatter import *
from lldb.formatters.cpp.libcxx_vector_formatter import *
from lldb.formatters.cpp.libcxx_vector_iterator_formatter import *
from lldb.formatters.cpp.libcxx_deque_formatter import *
from lldb.formatters.cpp.libcxx_optional_formatter import *
from lldb.formatters.cpp.libcxx_bitset_formatter import *
from lldb.formatters.cpp.libcxx_forward_list_formatter import *
from lldb.formatters.cpp.libcxx_list_formatter import *
from lldb.formatters.cpp.libcxx_coroutine_handle_formatter import *

def __lldb_init_module(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::bitset<.+>$" -l libcxx_bitset_formatter.LibcxxBitsetSyntheticFrontEnd -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::vector<.+>$" -l libcxx_vector_formatter.libcxx_std_vector_synthetic_frontend_creator -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::valarray<.+>$" -l libcxx_valarray_formatter.LibcxxStdValarraySyntheticFrontEnd -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::slice_array<.+>$" -l libcxx_slice_array_formatter.LibcxxStdSliceArraySyntheticFrontEnd -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::(gslice|mask|indirect)_array<.+>$" -l libcxx_proxy_array_formatter.LibcxxStdProxyArraySyntheticFrontEnd -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::forward_list<.+>$" -l libcxx_forward_list_formatter.LibcxxForwardListSyntheticFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::list<.+>$" -l libcxx_list_formatter.LibcxxListSyntheticFrontEnd -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::map<.+> >$" -l libcxx_map_formatter.LibcxxStdMapSyntheticProvider -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::set<.+> >$" -l libcxx_map_formatter.LibcxxStdMapSyntheticProvider -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::multiset<.+> >$" -l libcxx_map_formatter.LibcxxStdMapSyntheticProvider -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::multimap<.+> >$" -l libcxx_map_formatter.LibcxxStdMapSyntheticProvider -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::tuple<.*>$" -l libcxx_tuple_formatter.TupleFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::unordered_(multi)?(map|set)<.+> >$" -l libcxx_unordered_map_formatter.LibcxxStdUnorderedMapSyntheticFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::queue<.+>$" -l libcxx_queue_formatter.QueueFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::optional<.+>$" -l libcxx_optional_formatter.LibcxxOptionalSyntheticFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::variant<.+>$" -l libcxx_variant_formatter.VariantFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::atomic<.+>$" -l libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::span<.+>$" -l libcxx_span_formatter.LibcxxStdSpanSyntheticFrontEnd -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::ranges::ref_view<.+>$" -l libcxx_ranges_ref_view_formatter.LibcxxStdRangesRefViewSyntheticFrontEnd -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::deque<.+>$" -l libcxx_deque_formatter.stddeque_SynthProvider -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::shared_ptr<.+>$" -l libcxx_shared_ptr_formatter.LibcxxSharedPtrSyntheticFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::unique_ptr<.+>$" -l libcxx_unique_ptr_formatter.LibcxxUniquePtrSyntheticFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::weak_ptr<.+>$" -l libcxx_shared_ptr_formatter.LibcxxSharedPtrSyntheticFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::coroutine_handle<.+>$" -l libcxx_coroutine_handle_formatter.StdlibCoroutineHandleSyntheticFrontEnd -d -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::__wrap_iter<.+>$" -l libcxx_vector_iterator_formatter.VectorIteratorSyntheticFrontEnd -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::__map_(const_)?iterator<.+>$" -l libcxx_map_formatter.LibCxxMapIteratorSyntheticProvider -w "cplusplus"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::__hash_map_(const_)?iterator<.+>$" -l libcxx_unordered_map_formatter.LibCxxUnorderedMapIteratorSyntheticFrontEnd -w "cplusplus"')
