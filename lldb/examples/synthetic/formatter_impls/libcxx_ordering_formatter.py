"""
Python LLDB data formatter for libc++ comparison ordering types

Converted from LibCxx.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Formatters for C++20 three-way comparison ordering types:
- std::partial_ordering
- std::weak_ordering
- std::strong_ordering
"""

import lldb


def libcxx_extract_ordering_value(valobj):
    """Extract the __value_ member from an ordering type."""
    value_sp = valobj.GetChildMemberWithName("__value_")
    if not value_sp or not value_sp.IsValid():
        return None

    value = value_sp.GetValueAsSigned(0)
    return value


def libcxx_partial_ordering_summary_provider(valobj, internal_dict):
    """Summary provider for std::partial_ordering."""
    value = libcxx_extract_ordering_value(valobj)
    if value is None:
        return None

    if value == -1:
        return "less"
    elif value == 0:
        return "equivalent"
    elif value == 1:
        return "greater"
    elif value == -127:
        return "unordered"
    else:
        return None


def libcxx_weak_ordering_summary_provider(valobj, internal_dict):
    """Summary provider for std::weak_ordering."""
    value = libcxx_extract_ordering_value(valobj)
    if value is None:
        return None

    if value == -1:
        return "less"
    elif value == 0:
        return "equivalent"
    elif value == 1:
        return "greater"
    else:
        return None


def libcxx_strong_ordering_summary_provider(valobj, internal_dict):
    """Summary provider for std::strong_ordering."""
    value = libcxx_extract_ordering_value(valobj)
    if value is None:
        return None

    if value == -1:
        return "less"
    elif value == 0:
        return "equal"
    elif value == 1:
        return "greater"
    else:
        return None
