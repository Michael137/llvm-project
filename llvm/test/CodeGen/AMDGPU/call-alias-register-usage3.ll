; RUN: llc -O0 -mtriple=amdgcn-amd-amdhsa -mcpu=gfx900 < %s | FileCheck %s

; CallGraphAnalysis, which CodeGenSCC order depends on, does not look
; through aliases. If GlobalOpt is never run, we do not see direct
; calls,

@alias3 = hidden alias void (), ptr @aliasee_vgpr256_sgpr102

; CHECK-LABEL: {{^}}kernel3:
; CHECK:      .amdhsa_next_free_vgpr max(totalnumvgprs(kernel3.num_agpr, kernel3.num_vgpr), 1, 0)
; CHECK-NEXT: .amdhsa_next_free_sgpr max(kernel3.numbered_sgpr+extrasgprs(kernel3.uses_vcc, kernel3.uses_flat_scratch, 1), 1, 0)-extrasgprs(kernel3.uses_vcc, kernel3.uses_flat_scratch, 1)

; CHECK:      .set kernel3.num_vgpr, max(41, .Laliasee_vgpr256_sgpr102.num_vgpr)
; CHECK-NEXT: .set kernel3.num_agpr, max(0, .Laliasee_vgpr256_sgpr102.num_agpr)
; CHECK-NEXT: .set kernel3.numbered_sgpr, max(33, .Laliasee_vgpr256_sgpr102.numbered_sgpr)
define amdgpu_kernel void @kernel3() #0 {
bb:
  call void @alias3() #2
  ret void
}

; CHECK:      .set .Laliasee_vgpr256_sgpr102.num_vgpr, 253
; CHECK-NEXT: .set .Laliasee_vgpr256_sgpr102.num_agpr, 0
; CHECK-NEXT: .set .Laliasee_vgpr256_sgpr102.numbered_sgpr, 0
define internal void @aliasee_vgpr256_sgpr102() #1 {
bb:
  call void asm sideeffect "; clobber v252 ", "~{v252}"()
  ret void
}

attributes #0 = { noinline norecurse nounwind optnone }
attributes #1 = { noinline norecurse nounwind readnone willreturn "amdgpu-flat-work-group-size"="1,256" "amdgpu-waves-per-eu"="1,1" }
attributes #2 = { nounwind readnone willreturn }

