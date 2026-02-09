"""
Python LLDB data formatter for libc++ std::function

Converted from LibCxx.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


def libcxx_function_summary_provider(valobj, internal_dict):
    """
    Summary provider for std::function.

    Note: This is a simplified version. The C++ version uses CPPLanguageRuntime
    to find callable information (lambda location, function pointer, etc.).
    The Python API doesn't have direct access to CPPLanguageRuntime, so this
    provides a basic implementation that shows the __f_ pointer value.
    """
    valobj_sp = valobj.GetNonSyntheticValue()
    if not valobj_sp or not valobj_sp.IsValid():
        return None

    # Try to get the __f_ member which stores the function pointer
    f_sp = valobj_sp.GetChildMemberWithName("__f_")
    if not f_sp or not f_sp.IsValid():
        return None

    f_value = f_sp.GetValueAsUnsigned(0)

    # In the C++ implementation, this would use CPPLanguageRuntime to determine
    # if this is a lambda, callable object, or free/member function.
    # Since we can't easily access that from Python, we just show the pointer.
    if f_value == 0:
        return "Empty"

    return "__f_ = 0x%x" % f_value


def __lldb_init_module(debugger, internal_dict):
    """Initialize the module by registering the summary provider."""
    debugger.HandleCommand(
        'type summary add -F libcxx_function_formatter.libcxx_function_summary_provider '
        '-x "^std::__[[:alnum:]]+::function<.+>$" -w libcxx'
    )
