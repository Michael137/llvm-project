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
    # TODO: should the individual formatters register themselves?
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::bitset<.+>$" -l lldb.formatters.cpp.libcxx_bitset_formatter.LibcxxBitsetSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::vector<.+>$" -l lldb.formatters.cpp.libcxx_vector_formatter.libcxx_std_vector_synthetic_frontend_creator -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::valarray<.+>$" -l lldb.formatters.cpp.libcxx_valarray_formatter.LibcxxStdValarraySyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::slice_array<.+>$" -l lldb.formatters.cpp.libcxx_slice_array_formatter.LibcxxStdSliceArraySyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::(gslice|mask|indirect)_array<.+>$" -l lldb.formatters.cpp.libcxx_proxy_array_formatter.LibcxxStdProxyArraySyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::forward_list<.+>$" -l lldb.formatters.cpp.libcxx_forward_list_formatter.LibcxxForwardListSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::list<.+>$" -l lldb.formatters.cpp.libcxx_list_formatter.LibcxxListSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::map<.+> >$" -l lldb.formatters.cpp.libcxx_map_formatter.LibcxxStdMapSyntheticProvider -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::set<.+> >$" -l lldb.formatters.cpp.libcxx_map_formatter.LibcxxStdMapSyntheticProvider -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::multiset<.+> >$" -l lldb.formatters.cpp.libcxx_map_formatter.LibcxxStdMapSyntheticProvider -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::multimap<.+> >$" -l lldb.formatters.cpp.libcxx_map_formatter.LibcxxStdMapSyntheticProvider -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::tuple<.*>$" -l lldb.formatters.cpp.libcxx_tuple_formatter.TupleFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::unordered_(multi)?(map|set)<.+> >$" -l lldb.formatters.cpp.libcxx_unordered_map_formatter.LibcxxStdUnorderedMapSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::queue<.+>$" -l lldb.formatters.cpp.libcxx_queue_formatter.QueueFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::optional<.+>$" -l lldb.formatters.cpp.libcxx_optional_formatter.LibcxxOptionalSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::variant<.+>$" -l lldb.formatters.cpp.libcxx_variant_formatter.VariantFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::atomic<.+>$" -l lldb.formatters.cpp.libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::span<.+>$" -l lldb.formatters.cpp.libcxx_span_formatter.LibcxxStdSpanSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::ranges::ref_view<.+>$" -l lldb.formatters.cpp.libcxx_ranges_ref_view_formatter.LibcxxStdRangesRefViewSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::deque<.+>$" -l lldb.formatters.cpp.libcxx_deque_formatter.stddeque_SynthProvider -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::shared_ptr<.+>$" -l lldb.formatters.cpp.libcxx_shared_ptr_formatter.LibcxxSharedPtrSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::unique_ptr<.+>$" -l lldb.formatters.cpp.libcxx_unique_ptr_formatter.LibcxxUniquePtrSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::weak_ptr<.+>$" -l lldb.formatters.cpp.libcxx_shared_ptr_formatter.LibcxxSharedPtrSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::coroutine_handle<.+>$" -l lldb.formatters.cpp.libcxx_coroutine_handle_formatter.StdlibCoroutineHandleSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::__wrap_iter<.+>$" -l lldb.formatters.cpp.libcxx_vector_iterator_formatter.VectorIteratorSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::__map_(const_)?iterator<.+>$" -l lldb.formatters.cpp.libcxx_map_formatter.LibCxxMapIteratorSyntheticProvider -w "cplusplus-py"')
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::__hash_map_(const_)?iterator<.+>$" -l lldb.formatters.cpp.libcxx_unordered_map_formatter.LibCxxUnorderedMapIteratorSyntheticFrontEnd -w "cplusplus-py"')
    debugger.HandleCommand(f'type category enable cplusplus-py')
