"""
Python LLDB data formatter for std::coroutine_handle

Converted from Coroutines.cpp

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""

import lldb
import struct

LLDB_INVALID_ADDRESS = 0xFFFFFFFFFFFFFFFF


def get_coro_frame_ptr_from_handle(valobj):
    """
    Extract the coroutine frame pointer from a coroutine_handle.

    We expect a single pointer in the `coroutine_handle` class.
    """
    if not valobj or not valobj.IsValid():
        return LLDB_INVALID_ADDRESS

    # We expect a single pointer in the coroutine_handle class
    if valobj.GetNumChildren() != 1:
        return LLDB_INVALID_ADDRESS

    ptr_sp = valobj.GetChildAtIndex(0)
    if not ptr_sp or not ptr_sp.IsValid():
        return LLDB_INVALID_ADDRESS

    if not ptr_sp.GetType().IsPointerType():
        return LLDB_INVALID_ADDRESS

    frame_ptr_addr = ptr_sp.GetValueAsUnsigned(LLDB_INVALID_ADDRESS)
    if frame_ptr_addr == 0 or frame_ptr_addr == LLDB_INVALID_ADDRESS:
        return LLDB_INVALID_ADDRESS

    return frame_ptr_addr


def extract_destroy_function(target, frame_ptr_addr):
    """
    Extract the destroy function from the coroutine frame.

    The coroutine frame layout is: [resume_ptr, destroy_ptr, ...]
    """
    process = target.GetProcess()
    if not process or not process.IsValid():
        return None

    ptr_size = process.GetAddressByteSize()
    destroy_func_ptr_addr = frame_ptr_addr + ptr_size

    error = lldb.SBError()
    destroy_func_addr = process.ReadPointerFromMemory(destroy_func_ptr_addr, error)
    if error.Fail():
        return None

    destroy_func_address = target.ResolveLoadAddress(destroy_func_addr)
    if not destroy_func_address or not destroy_func_address.IsValid():
        return None

    return destroy_func_address.GetFunction()


def infer_artificial_coro_type(destroy_func, var_name, target):
    """
    Infer coroutine type from artificial variables in the destroy function.

    Clang generates artificial `__promise` and `__coro_frame` variables
    inside the destroy function.
    """
    if not destroy_func or not destroy_func.IsValid():
        return None

    if not target or not target.IsValid():
        return None

    block = destroy_func.GetBlock()
    if not block or not block.IsValid():
        return None

    # Get all variables in the function block
    # SBBlock.GetVariables(SBTarget, arguments, locals, statics)
    variables = block.GetVariables(target, False, True, False)

    if not variables or not variables.IsValid():
        return None

    # Look for the artificial variable
    for i in range(variables.GetSize()):
        var = variables.GetValueAtIndex(i)
        if var and var.GetName() == var_name:
            var_type = var.GetType()
            if var_type and var_type.IsValid():
                # For pointer types, get the pointee type
                if var_type.IsPointerType():
                    return var_type.GetPointeeType()
                return var_type

    return None


def stdlib_coroutine_handle_summary_provider(valobj, _internal_dict):
    """Summary provider for std::coroutine_handle."""
    valobj_sp = valobj.GetNonSyntheticValue()
    frame_ptr_addr = get_coro_frame_ptr_from_handle(valobj_sp)

    if frame_ptr_addr == LLDB_INVALID_ADDRESS:
        return None

    if frame_ptr_addr == 0:
        return "nullptr"
    else:
        return "coro frame = 0x%x" % frame_ptr_addr


class StdlibCoroutineHandleSyntheticFrontEnd:
    """Synthetic children frontend for std::coroutine_handle."""

    def __init__(self, valobj, _internal_dict):
        self.valobj = valobj
        self.m_children = []
        self.update()

    def num_children(self):
        """Return number of children."""
        return len(self.m_children)

    def get_child_index(self, name):
        """Get the index of a child with the given name."""
        for idx, child in enumerate(self.m_children):
            if child and child.GetName() == name:
                return idx
        return None

    def get_child_at_index(self, idx):
        """Get the child at the given index."""
        if idx < len(self.m_children):
            return self.m_children[idx]
        return None

    def update(self):
        """Update the cached state."""
        self.m_children = []

        valobj_sp = self.valobj.GetNonSyntheticValue()
        if not valobj_sp or not valobj_sp.IsValid():
            return False

        frame_ptr_addr = get_coro_frame_ptr_from_handle(valobj_sp)
        if frame_ptr_addr == 0 or frame_ptr_addr == LLDB_INVALID_ADDRESS:
            return False

        target = self.valobj.GetTarget()
        if not target or not target.IsValid():
            return False

        process = target.GetProcess()
        if not process or not process.IsValid():
            return False

        ptr_size = process.GetAddressByteSize()

        # Get void type as fallback
        void_type = target.GetBasicType(lldb.eBasicTypeVoid)

        # Try to get promise type from template argument
        compiler_type = valobj_sp.GetType()
        promise_type = None
        if compiler_type.GetNumberOfTemplateArguments() > 0:
            promise_type = compiler_type.GetTemplateArgumentType(0)

        # Try to infer types from artificial variables in destroy function
        destroy_func = extract_destroy_function(target, frame_ptr_addr)

        # If promise type is void, try to infer it
        if (
            not promise_type
            or not promise_type.IsValid()
            or promise_type.GetName() == "void"
        ):
            inferred_promise = infer_artificial_coro_type(
                destroy_func, "__promise", target
            )
            if inferred_promise and inferred_promise.IsValid():
                promise_type = inferred_promise

        if not promise_type or not promise_type.IsValid():
            promise_type = void_type

        # Try to infer coro_frame type
        coro_frame_type = infer_artificial_coro_type(
            destroy_func, "__coro_frame", target
        )
        if not coro_frame_type or not coro_frame_type.IsValid():
            coro_frame_type = void_type

        # Create function pointer type for resume/destroy
        # Try to get a proper function pointer type from the destroy function
        func_ptr_type = None
        if destroy_func and destroy_func.IsValid():
            func_type = destroy_func.GetType()
            if func_type and func_type.IsValid():
                func_ptr_type = func_type.GetPointerType()

        # Fall back to void* if we couldn't get a function pointer type
        if not func_ptr_type or not func_ptr_type.IsValid():
            func_ptr_type = void_type.GetPointerType()
        if not func_ptr_type or not func_ptr_type.IsValid():
            return False

        # Get byte order info for creating pointer values
        byte_order = process.GetByteOrder()
        if byte_order == lldb.eByteOrderLittle:
            fmt = "<Q" if ptr_size == 8 else "<I"
        else:
            fmt = ">Q" if ptr_size == 8 else ">I"

        # Helper to create SBAddress from load address
        def make_addr(addr):
            sb_addr = lldb.SBAddress()
            sb_addr.SetLoadAddress(addr, target)
            return sb_addr

        # Helper to create a pointer value with a specific address as its value
        # (not reading from that address, but the pointer itself equals that address)
        def make_pointer_value(name, addr_value, ptr_type):
            addr_bytes = struct.pack(fmt, addr_value)
            error = lldb.SBError()
            data = lldb.SBData()
            data.SetData(error, addr_bytes, byte_order, ptr_size)
            if error.Fail():
                return None
            return target.CreateValueFromData(name, data, ptr_type)

        # Create the `resume` child (function pointer at frame_ptr_addr)
        resume_addr = make_addr(frame_ptr_addr)
        resume_sp = target.CreateValueFromAddress("resume", resume_addr, func_ptr_type)
        if resume_sp and resume_sp.IsValid():
            self.m_children.append(resume_sp)

        # Create the `destroy` child (function pointer at frame_ptr_addr + ptr_size)
        destroy_addr = make_addr(frame_ptr_addr + ptr_size)
        destroy_sp = target.CreateValueFromAddress(
            "destroy", destroy_addr, func_ptr_type
        )
        if destroy_sp and destroy_sp.IsValid():
            self.m_children.append(destroy_sp)

        # Create the `promise` child (at frame_ptr_addr + 2 * ptr_size)
        # We add it as a pointer type to avoid deep recursion with coroutine cycles.
        # We want the pointer VALUE to be frame_ptr_addr + 2 * ptr_size,
        # not read a pointer from that address.
        promise_ptr_type = promise_type.GetPointerType()
        if promise_ptr_type and promise_ptr_type.IsValid():
            promise_ptr_addr = frame_ptr_addr + 2 * ptr_size
            promise_sp = make_pointer_value(
                "promise", promise_ptr_addr, promise_ptr_type
            )
            if promise_sp and promise_sp.IsValid():
                self.m_children.append(promise_sp)

        # Create the `coro_frame` child (pointer to the frame itself)
        # We want the pointer VALUE to be frame_ptr_addr,
        # not read a pointer from that address.
        coro_frame_ptr_type = coro_frame_type.GetPointerType()
        if coro_frame_ptr_type and coro_frame_ptr_type.IsValid():
            coro_frame_sp = make_pointer_value(
                "coro_frame", frame_ptr_addr, coro_frame_ptr_type
            )
            if coro_frame_sp and coro_frame_sp.IsValid():
                self.m_children.append(coro_frame_sp)

        return len(self.m_children) > 0

    def has_children(self):
        """Check if this object has children."""
        return True

def init_formatter(debugger, dict):
    debugger.HandleCommand(f'type synthetic add -x "^std::__[[:alnum:]]+::coroutine_handle<.+>$" -l lldb.formatters.cpp.formatter_impls.libcxx_coroutine_handle_formatter.StdlibCoroutineHandleSyntheticFrontEnd -w "cplusplus-py"')
