"""
Python LLDB data formatter for libc++ std::string_view and std::wstring_view

Converted from LibCxx.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb


def libcxx_extract_string_view_data(valobj):
    """
    Extract __data_ and __size_ from a string_view.

    Returns tuple: (success, dataobj, size)
    """
    # Try to get __data_ (try both __data_ and __data for compatibility)
    dataobj = valobj.GetChildMemberWithName("__data_")
    if not dataobj or not dataobj.IsValid():
        dataobj = valobj.GetChildMemberWithName("__data")

    # Try to get __size_ (try both __size_ and __size for compatibility)
    sizeobj = valobj.GetChildMemberWithName("__size_")
    if not sizeobj or not sizeobj.IsValid():
        sizeobj = valobj.GetChildMemberWithName("__size")

    if not dataobj or not sizeobj:
        return (False, None, 0)

    if not dataobj.IsValid() or not sizeobj.IsValid():
        return (False, None, 0)

    size = sizeobj.GetValueAsUnsigned(0)
    return (True, dataobj, size)


def libcxx_string_view_summary_provider_ascii(valobj, internal_dict):
    """Summary provider for std::string_view (ASCII/UTF-8)."""
    try:
        success, dataobj, size = libcxx_extract_string_view_data(valobj)

        if not success:
            return "Summary Unavailable"

        if size == 0:
            return '""'

        if size > 1024:  # Sanity check
            return '"<string_view too long>"'

        # Try to get the summary from the data pointer
        summary = dataobj.GetSummary()
        if summary:
            return summary

        # Try to read the string data from memory
        data_addr = dataobj.GetValueAsUnsigned(0)
        if data_addr == 0:
            return '"<null>"'

        process = valobj.GetProcess()
        if not process or not process.IsValid():
            return "Summary Unavailable"

        error = lldb.SBError()
        data_bytes = process.ReadMemory(data_addr, size, error)

        if error.Fail() or not data_bytes:
            return "Summary Unavailable"

        try:
            string = data_bytes.decode("utf-8", errors="replace")
            return '"%s"' % string
        except:
            return "Summary Unavailable"

    except Exception as e:
        return "Summary Unavailable"


def libcxx_string_view_summary_provider_utf16(valobj, internal_dict):
    """Summary provider for std::u16string_view."""
    try:
        success, dataobj, size = libcxx_extract_string_view_data(valobj)

        if not success:
            return "Summary Unavailable"

        if size == 0:
            return 'u""'

        # For UTF-16, size is in characters, need to multiply by 2 for bytes
        byte_size = size * 2

        if byte_size > 2048:  # Sanity check
            return 'u"<string_view too long>"'

        data_addr = dataobj.GetValueAsUnsigned(0)
        if data_addr == 0:
            return 'u"<null>"'

        process = valobj.GetProcess()
        if not process or not process.IsValid():
            return "Summary Unavailable"

        error = lldb.SBError()
        data_bytes = process.ReadMemory(data_addr, byte_size, error)

        if error.Fail() or not data_bytes:
            return "Summary Unavailable"

        try:
            string = data_bytes.decode("utf-16", errors="replace")
            return 'u"%s"' % string
        except:
            return "Summary Unavailable"

    except Exception as e:
        return "Summary Unavailable"


def libcxx_string_view_summary_provider_utf32(valobj, internal_dict):
    """Summary provider for std::u32string_view."""
    try:
        success, dataobj, size = libcxx_extract_string_view_data(valobj)

        if not success:
            return "Summary Unavailable"

        if size == 0:
            return 'U""'

        # For UTF-32, size is in characters, need to multiply by 4 for bytes
        byte_size = size * 4

        if byte_size > 4096:  # Sanity check
            return 'U"<string_view too long>"'

        data_addr = dataobj.GetValueAsUnsigned(0)
        if data_addr == 0:
            return 'U"<null>"'

        process = valobj.GetProcess()
        if not process or not process.IsValid():
            return "Summary Unavailable"

        error = lldb.SBError()
        data_bytes = process.ReadMemory(data_addr, byte_size, error)

        if error.Fail() or not data_bytes:
            return "Summary Unavailable"

        try:
            string = data_bytes.decode("utf-32", errors="replace")
            return 'U"%s"' % string
        except:
            return "Summary Unavailable"

    except Exception as e:
        return "Summary Unavailable"


def libcxx_wstring_view_summary_provider(valobj, internal_dict):
    """Summary provider for std::wstring_view."""
    # wchar_t size varies by platform (2 bytes on Windows, 4 bytes on Unix)
    # Try to determine the size
    try:
        success, dataobj, size = libcxx_extract_string_view_data(valobj)

        if not success:
            return "Summary Unavailable"

        if size == 0:
            return 'L""'

        # Assume 4-byte wchar_t (most common on Unix systems)
        wchar_size = 4
        byte_size = size * wchar_size

        if byte_size > 4096:  # Sanity check
            return 'L"<string_view too long>"'

        data_addr = dataobj.GetValueAsUnsigned(0)
        if data_addr == 0:
            return 'L"<null>"'

        process = valobj.GetProcess()
        if not process or not process.IsValid():
            return "Summary Unavailable"

        error = lldb.SBError()
        data_bytes = process.ReadMemory(data_addr, byte_size, error)

        if error.Fail() or not data_bytes:
            return "Summary Unavailable"

        try:
            string = data_bytes.decode("utf-32", errors="replace")
            return 'L"%s"' % string
        except:
            # Try UTF-16 if UTF-32 fails (might be Windows)
            try:
                byte_size = size * 2
                data_bytes = process.ReadMemory(data_addr, byte_size, error)
                if not error.Fail() and data_bytes:
                    string = data_bytes.decode("utf-16", errors="replace")
                    return 'L"%s"' % string
            except:
                pass
            return "Summary Unavailable"

    except Exception as e:
        return "Summary Unavailable"
