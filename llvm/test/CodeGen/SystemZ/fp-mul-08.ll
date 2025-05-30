; RUN: llc < %s -mtriple=s390x-linux-gnu -mcpu=z10 \
; RUN:   | FileCheck -check-prefix=CHECK -check-prefix=CHECK-SCALAR %s
; RUN: llc < %s -mtriple=s390x-linux-gnu -mcpu=z14 \
; RUN:   | FileCheck -check-prefix=CHECK -check-prefix=CHECK-VECTOR %s

declare half @llvm.fma.f16(half %f1, half %f2, half %f3)
declare float @llvm.fma.f32(float %f1, float %f2, float %f3)

define half @f0(half %f1, half %f2, half %acc) {
; CHECK-LABEL: f0:
; CHECK-NOT: brasl
; CHECK: lcdfr %f{{[0-9]+}}, %f4
; CHECK: brasl %r14, __extendhfsf2@PLT
; CHECK: brasl %r14, __extendhfsf2@PLT
; CHECK: brasl %r14, __extendhfsf2@PLT
; CHECK-SCALAR: maebr %f0, %f8, %f10
; CHECK-VECTOR: wfmasb %f0, %f0, %f8, %f10
; CHECK: brasl %r14, __truncsfhf2@PLT
; CHECK: br %r14
  %negacc = fneg half %acc
  %res = call half @llvm.fma.f16 (half %f1, half %f2, half %negacc)
  ret half %res
}

define float @f1(float %f1, float %f2, float %acc) {
; CHECK-LABEL: f1:
; CHECK-SCALAR: msebr %f4, %f0, %f2
; CHECK-SCALAR: ler %f0, %f4
; CHECK-VECTOR: wfmssb %f0, %f0, %f2, %f4
; CHECK: br %r14
  %negacc = fneg float %acc
  %res = call float @llvm.fma.f32 (float %f1, float %f2, float %negacc)
  ret float %res
}

define float @f2(float %f1, ptr %ptr, float %acc) {
; CHECK-LABEL: f2:
; CHECK: mseb %f2, %f0, 0(%r2)
; CHECK-SCALAR: ler %f0, %f2
; CHECK-VECTOR: ldr %f0, %f2
; CHECK: br %r14
  %f2 = load float, ptr %ptr
  %negacc = fneg float %acc
  %res = call float @llvm.fma.f32 (float %f1, float %f2, float %negacc)
  ret float %res
}

define float @f3(float %f1, ptr %base, float %acc) {
; CHECK-LABEL: f3:
; CHECK: mseb %f2, %f0, 4092(%r2)
; CHECK-SCALAR: ler %f0, %f2
; CHECK-VECTOR: ldr %f0, %f2
; CHECK: br %r14
  %ptr = getelementptr float, ptr %base, i64 1023
  %f2 = load float, ptr %ptr
  %negacc = fneg float %acc
  %res = call float @llvm.fma.f32 (float %f1, float %f2, float %negacc)
  ret float %res
}

define float @f4(float %f1, ptr %base, float %acc) {
; The important thing here is that we don't generate an out-of-range
; displacement.  Other sequences besides this one would be OK.
;
; CHECK-LABEL: f4:
; CHECK: aghi %r2, 4096
; CHECK: mseb %f2, %f0, 0(%r2)
; CHECK-SCALAR: ler %f0, %f2
; CHECK-VECTOR: ldr %f0, %f2
; CHECK: br %r14
  %ptr = getelementptr float, ptr %base, i64 1024
  %f2 = load float, ptr %ptr
  %negacc = fneg float %acc
  %res = call float @llvm.fma.f32 (float %f1, float %f2, float %negacc)
  ret float %res
}

define float @f5(float %f1, ptr %base, float %acc) {
; Here too the important thing is that we don't generate an out-of-range
; displacement.  Other sequences besides this one would be OK.
;
; CHECK-LABEL: f5:
; CHECK: aghi %r2, -4
; CHECK: mseb %f2, %f0, 0(%r2)
; CHECK-SCALAR: ler %f0, %f2
; CHECK-VECTOR: ldr %f0, %f2
; CHECK: br %r14
  %ptr = getelementptr float, ptr %base, i64 -1
  %f2 = load float, ptr %ptr
  %negacc = fneg float %acc
  %res = call float @llvm.fma.f32 (float %f1, float %f2, float %negacc)
  ret float %res
}

define float @f6(float %f1, ptr %base, i64 %index, float %acc) {
; CHECK-LABEL: f6:
; CHECK: sllg %r1, %r3, 2
; CHECK: mseb %f2, %f0, 0(%r1,%r2)
; CHECK-SCALAR: ler %f0, %f2
; CHECK-VECTOR: ldr %f0, %f2
; CHECK: br %r14
  %ptr = getelementptr float, ptr %base, i64 %index
  %f2 = load float, ptr %ptr
  %negacc = fneg float %acc
  %res = call float @llvm.fma.f32 (float %f1, float %f2, float %negacc)
  ret float %res
}

define float @f7(float %f1, ptr %base, i64 %index, float %acc) {
; CHECK-LABEL: f7:
; CHECK: sllg %r1, %r3, 2
; CHECK: mseb %f2, %f0, 4092({{%r1,%r2|%r2,%r1}})
; CHECK-SCALAR: ler %f0, %f2
; CHECK-VECTOR: ldr %f0, %f2
; CHECK: br %r14
  %index2 = add i64 %index, 1023
  %ptr = getelementptr float, ptr %base, i64 %index2
  %f2 = load float, ptr %ptr
  %negacc = fneg float %acc
  %res = call float @llvm.fma.f32 (float %f1, float %f2, float %negacc)
  ret float %res
}

define float @f8(float %f1, ptr %base, i64 %index, float %acc) {
; CHECK-LABEL: f8:
; CHECK: sllg %r1, %r3, 2
; CHECK: lay %r1, 4096({{%r1,%r2|%r2,%r1}})
; CHECK: mseb %f2, %f0, 0(%r1)
; CHECK-SCALAR: ler %f0, %f2
; CHECK-VECTOR: ldr %f0, %f2
; CHECK: br %r14
  %index2 = add i64 %index, 1024
  %ptr = getelementptr float, ptr %base, i64 %index2
  %f2 = load float, ptr %ptr
  %negacc = fneg float %acc
  %res = call float @llvm.fma.f32 (float %f1, float %f2, float %negacc)
  ret float %res
}
