"""
Python LLDB data formatter for libc++ std::chrono types

Converted from LibCxx.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Formatters for C++20 chrono types:
- sys_seconds / sys_time<seconds>
- local_seconds / local_time<seconds>
- sys_days / sys_time<days>
- local_days / local_time<days>
- month
- weekday
- year_month_day
"""

import lldb
import time


def libcxx_chrono_sys_seconds_summary_provider(valobj, internal_dict):
    """Summary provider for std::chrono::sys_seconds."""
    ptr_sp = valobj.GetChildMemberWithName("__d_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    ptr_sp = ptr_sp.GetChildMemberWithName("__rep_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    seconds = ptr_sp.GetValueAsSigned(0)

    # chrono library valid range: [-32767-01-01, 32767-12-31]
    chrono_timestamp_min = -1_096_193_779_200  # -32767-01-01T00:00:00Z
    chrono_timestamp_max = 971_890_963_199     # 32767-12-31T23:59:59Z

    if seconds < chrono_timestamp_min or seconds > chrono_timestamp_max:
        return "timestamp=%d s" % seconds

    try:
        # Format as ISO 8601 UTC timestamp
        dt = time.gmtime(seconds)
        formatted = time.strftime("%Y-%m-%dT%H:%M:%SZ", dt)
        return "date/time=%s timestamp=%d s" % (formatted, seconds)
    except:
        return "timestamp=%d s" % seconds


def libcxx_chrono_local_seconds_summary_provider(valobj, internal_dict):
    """Summary provider for std::chrono::local_seconds."""
    ptr_sp = valobj.GetChildMemberWithName("__d_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    ptr_sp = ptr_sp.GetChildMemberWithName("__rep_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    seconds = ptr_sp.GetValueAsSigned(0)

    chrono_timestamp_min = -1_096_193_779_200
    chrono_timestamp_max = 971_890_963_199

    if seconds < chrono_timestamp_min or seconds > chrono_timestamp_max:
        return "timestamp=%d s" % seconds

    try:
        # Format as ISO 8601 local timestamp (no Z suffix)
        dt = time.gmtime(seconds)
        formatted = time.strftime("%Y-%m-%dT%H:%M:%S", dt)
        return "date/time=%s timestamp=%d s" % (formatted, seconds)
    except:
        return "timestamp=%d s" % seconds


def libcxx_chrono_sys_days_summary_provider(valobj, internal_dict):
    """Summary provider for std::chrono::sys_days."""
    ptr_sp = valobj.GetChildMemberWithName("__d_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    ptr_sp = ptr_sp.GetChildMemberWithName("__rep_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    days = ptr_sp.GetValueAsSigned(0)

    chrono_timestamp_min = -12_687_428  # -32767-01-01Z
    chrono_timestamp_max = 11_248_737   # 32767-12-31Z

    if days < chrono_timestamp_min or days > chrono_timestamp_max:
        return "timestamp=%d days" % days

    try:
        seconds = days * 86400
        dt = time.gmtime(seconds)
        formatted = time.strftime("%Y-%m-%dZ", dt)
        return "date=%s timestamp=%d days" % (formatted, days)
    except:
        return "timestamp=%d days" % days


def libcxx_chrono_local_days_summary_provider(valobj, internal_dict):
    """Summary provider for std::chrono::local_days."""
    ptr_sp = valobj.GetChildMemberWithName("__d_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    ptr_sp = ptr_sp.GetChildMemberWithName("__rep_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    days = ptr_sp.GetValueAsSigned(0)

    chrono_timestamp_min = -12_687_428
    chrono_timestamp_max = 11_248_737

    if days < chrono_timestamp_min or days > chrono_timestamp_max:
        return "timestamp=%d days" % days

    try:
        seconds = days * 86400
        dt = time.gmtime(seconds)
        formatted = time.strftime("%Y-%m-%d", dt)
        return "date=%s timestamp=%d days" % (formatted, days)
    except:
        return "timestamp=%d days" % days


def libcxx_chrono_month_summary_provider(valobj, internal_dict):
    """Summary provider for std::chrono::month."""
    months = [
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    ]

    ptr_sp = valobj.GetChildMemberWithName("__m_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    month = ptr_sp.GetValueAsUnsigned(0)

    if 1 <= month <= 12:
        return "month=%s" % months[month - 1]
    else:
        return "month=%u" % month


def libcxx_chrono_weekday_summary_provider(valobj, internal_dict):
    """Summary provider for std::chrono::weekday."""
    weekdays = [
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    ]

    ptr_sp = valobj.GetChildMemberWithName("__wd_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    weekday = ptr_sp.GetValueAsUnsigned(0)

    if weekday < 7:
        return "weekday=%s" % weekdays[weekday]
    else:
        return "weekday=%u" % weekday


def libcxx_chrono_year_month_day_summary_provider(valobj, internal_dict):
    """Summary provider for std::chrono::year_month_day."""
    # Get year
    ptr_sp = valobj.GetChildMemberWithName("__y_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    ptr_sp = ptr_sp.GetChildMemberWithName("__y_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    year = ptr_sp.GetValueAsSigned(0)

    # Get month
    ptr_sp = valobj.GetChildMemberWithName("__m_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    ptr_sp = ptr_sp.GetChildMemberWithName("__m_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    month = ptr_sp.GetValueAsUnsigned(0)

    # Get day
    ptr_sp = valobj.GetChildMemberWithName("__d_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    ptr_sp = ptr_sp.GetChildMemberWithName("__d_")
    if not ptr_sp or not ptr_sp.IsValid():
        return None

    day = ptr_sp.GetValueAsUnsigned(0)

    # Format as YYYY-MM-DD
    if year < 0:
        return "date=-%04d-%02u-%02u" % (-year, month, day)
    else:
        return "date=%04d-%02u-%02u" % (year, month, day)


def __lldb_init_module(debugger, internal_dict):
    """Initialize the module by registering the summary providers."""
    # Register sys_seconds / sys_time<seconds>
    debugger.HandleCommand(
        'type summary add -F libcxx_chrono_formatter.libcxx_chrono_sys_seconds_summary_provider '
        '-x "^std::__[[:alnum:]]+::chrono::sys_time<std::__[[:alnum:]]+::chrono::duration<.+, std::__[[:alnum:]]+::ratio<1(L){0,2}, 1(L){0,2}> > >$" '
        '-w libcxx'
    )

    # Register local_seconds / local_time<seconds>
    debugger.HandleCommand(
        'type summary add -F libcxx_chrono_formatter.libcxx_chrono_local_seconds_summary_provider '
        '-x "^std::__[[:alnum:]]+::chrono::local_time<std::__[[:alnum:]]+::chrono::duration<.+, std::__[[:alnum:]]+::ratio<1(L){0,2}, 1(L){0,2}> > >$" '
        '-w libcxx'
    )

    # Register sys_days / sys_time<days>
    debugger.HandleCommand(
        'type summary add -F libcxx_chrono_formatter.libcxx_chrono_sys_days_summary_provider '
        '-x "^std::__[[:alnum:]]+::chrono::sys_time<std::__[[:alnum:]]+::chrono::duration<.+, std::__[[:alnum:]]+::ratio<86400(L){0,2}, 1(L){0,2}> > >$" '
        '-w libcxx'
    )

    # Register local_days / local_time<days>
    debugger.HandleCommand(
        'type summary add -F libcxx_chrono_formatter.libcxx_chrono_local_days_summary_provider '
        '-x "^std::__[[:alnum:]]+::chrono::local_time<std::__[[:alnum:]]+::chrono::duration<.+, std::__[[:alnum:]]+::ratio<86400(L){0,2}, 1(L){0,2}> > >$" '
        '-w libcxx'
    )

    # Register month
    debugger.HandleCommand(
        'type summary add -F libcxx_chrono_formatter.libcxx_chrono_month_summary_provider '
        '-x "^std::__[[:alnum:]]+::chrono::month$" -w libcxx'
    )

    # Register weekday
    debugger.HandleCommand(
        'type summary add -F libcxx_chrono_formatter.libcxx_chrono_weekday_summary_provider '
        '-x "^std::__[[:alnum:]]+::chrono::weekday$" -w libcxx'
    )

    # Register year_month_day
    debugger.HandleCommand(
        'type summary add -F libcxx_chrono_formatter.libcxx_chrono_year_month_day_summary_provider '
        '-x "^std::__[[:alnum:]]+::chrono::year_month_day$" -w libcxx'
    )
