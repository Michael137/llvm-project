; RUN: llc < %s | FileCheck %s
; RUN: %if ptxas %{ llc < %s | %ptxas-verify %}

target datalayout = "e-i64:64-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

; Check that we can handle global variables of large integer and fp128 type.

; (lsb) 0x0102'0304'0506...0F10 (msb)
@gv = addrspace(1) externally_initialized global i128 21345817372864405881847059188222722561, align 16
; CHECK: .visible .global .align 16 .b8 gv[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

@gv_fp128 = addrspace(1) externally_initialized global fp128 0xL0807060504030201100F0E0D0C0B0A09, align 16
; CHECK: .visible .global .align 16 .b8 gv_fp128[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

; Make sure that we do not overflow on large number of elements.
; CHECK: .visible .global .align 1 .b8 large_data[4831838208];
@large_data = global [4831838208 x i8] zeroinitializer
