; RUN: llvm-as < %s | llvm-dis | llvm-as | llvm-dis | FileCheck %s
; RUN: verify-uselistorder %s

!named = !{!0, !1, !2}

!0 = !DIBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
!1 = !DIBasicType(name: "double", size: 64, align: 64, encoding: DW_ATE_float)

; Template type parameter pack: DIPackNode wrapping two type params.
; CHECK: !2 = !DIPackNode(elementTag: DW_TAG_template_type_parameter, name: "Ts", elements: !3)
; CHECK-NEXT: !3 = !{!4, !5}
; CHECK-NEXT: !4 = !DITemplateTypeParameter(type: !0)
; CHECK-NEXT: !5 = !DITemplateTypeParameter(type: !1)
!2 = !DIPackNode(elementTag: DW_TAG_template_type_parameter, name: "Ts",
                 elements: !{!DITemplateTypeParameter(type: !0),
                             !DITemplateTypeParameter(type: !1)})
