"""
Python LLDB data formatter for libc++ std::string and std::wstring

Converted from LibCxx.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Note: This is a simplified version of the C++ string formatter.
The full C++ implementation has complex logic for handling short string
optimization, different string layouts (CSD vs DSC), and reading string
data from process memory. This Python version provides basic functionality
but may not handle all edge cases.
"""

import lldb


def get_value_or_old_compressed_pair(obj, child_name, compressed_pair_name):
    """
    Helper to get value from either new or old compressed pair layout.

    Returns tuple: (ValueObjectSP, is_compressed_pair)
    """
    node_sp = obj.GetChildMemberWithName(child_name)
    if node_sp and node_sp.IsValid():
        type_name = node_sp.GetTypeName()
        is_compressed = "__compressed_pair" in type_name if type_name else False
        return (node_sp, is_compressed)

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

    value = pair.GetChildMemberWithName("__first_")
    return value


def extract_libcxx_string_data(valobj):
    """Extract the __rep_ member from a libc++ string."""
    valobj_r_sp, is_compressed_pair = get_value_or_old_compressed_pair(
        valobj, "__rep_", "__r_"
    )

    if not valobj_r_sp:
        return None

    if is_compressed_pair:
        return get_first_value_of_libcxx_compressed_pair(valobj_r_sp)

    return valobj_r_sp


def libcxx_string_summary_provider_ascii(valobj, internal_dict):
    """
    Summary provider for std::string (ASCII/UTF-8).

    This is a simplified implementation that attempts to read the string
    contents. The full C++ version handles short string optimization,
    different layouts, and memory reading more robustly.
    """
    try:
        valobj_rep_sp = extract_libcxx_string_data(valobj)
        if not valobj_rep_sp or not valobj_rep_sp.IsValid():
            return "Summary Unavailable"

        # Try to get the __s (short) member
        short_sp = valobj_rep_sp.GetChildMemberWithName("__s")
        if not short_sp or not short_sp.IsValid():
            return "Summary Unavailable"

        # Check if we're in short mode
        is_long_sp = short_sp.GetChildMemberWithName("__is_long_")
        size_sp = short_sp.GetChildMemberWithName("__size_")

        if not size_sp or not size_sp.IsValid():
            return "Summary Unavailable"

        short_mode = True
        size = 0

        if is_long_sp and is_long_sp.IsValid():
            # New layout with explicit __is_long_
            short_mode = not is_long_sp.GetValueAsUnsigned(0)
            size = size_sp.GetValueAsUnsigned(0)
        else:
            # Old layout with encoded mode in size
            size_mode_value = size_sp.GetValueAsUnsigned(0)
            # Simplified: assume DSC layout and check high bit
            short_mode = (size_mode_value & 0x80) == 0
            size = size_mode_value if short_mode else 0

        if short_mode:
            # Short string - data is inline
            location_sp = short_sp.GetChildMemberWithName("__data_")
            if not location_sp or not location_sp.IsValid():
                return "Summary Unavailable"

            # Read the string data
            error = lldb.SBError()
            data = location_sp.GetData()
            if not data or not data.IsValid():
                return "Summary Unavailable"

            # Try to read as a C string
            string_data = location_sp.GetSummary()
            if string_data:
                return string_data

            # Fallback: try to read bytes
            try:
                bytes_list = []
                for i in range(min(size, 23)):  # Max short string size
                    byte = data.GetUnsignedInt8(error, i)
                    if error.Fail():
                        break
                    if byte == 0:
                        break
                    bytes_list.append(byte)

                if bytes_list:
                    string = bytes(bytes_list).decode('utf-8', errors='replace')
                    return '"%s"' % string
            except:
                pass

            return "Summary Unavailable"
        else:
            # Long string - data is in heap
            l_sp = valobj_rep_sp.GetChildMemberWithName("__l")
            if not l_sp or not l_sp.IsValid():
                return "Summary Unavailable"

            data_sp = l_sp.GetChildMemberWithName("__data_")
            size_vo = l_sp.GetChildMemberWithName("__size_")

            if not data_sp or not size_vo:
                return "Summary Unavailable"

            size = size_vo.GetValueAsUnsigned(0)
            if size > 1024:  # Sanity check
                return '"<string too long>"'

            # Try to get the summary
            summary = data_sp.GetSummary()
            if summary:
                return summary

            return "Summary Unavailable"

    except Exception as e:
        return "Summary Unavailable"


def libcxx_wstring_summary_provider(valobj, internal_dict):
    """
    Summary provider for std::wstring.

    This is a simplified implementation.
    """
    # Similar to ASCII but would need to handle wide characters
    return libcxx_string_summary_provider_ascii(valobj, internal_dict)


def libcxx_string_summary_provider_utf16(valobj, internal_dict):
    """Summary provider for std::u16string."""
    return libcxx_string_summary_provider_ascii(valobj, internal_dict)


def libcxx_string_summary_provider_utf32(valobj, internal_dict):
    """Summary provider for std::u32string."""
    return libcxx_string_summary_provider_ascii(valobj, internal_dict)
