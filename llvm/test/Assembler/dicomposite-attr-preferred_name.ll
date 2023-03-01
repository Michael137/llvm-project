; RUN: llvm-as < %s | llvm-dis | llvm-as | llvm-dis | FileCheck %s
; RUN: verify-uselistorder %s

%struct.Foo = type { i8 }

@var = global %struct.Foo zeroinitializer, align 1, !dbg !0

!llvm.dbg.cu = !{!2}
!llvm.linker.options = !{}
!llvm.module.flags = !{!11, !12, !13, !14, !15, !16}
!llvm.ident = !{!17}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "var", scope: !2, file: !3, line: 10, type: !5, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 17.0.0", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !4, splitDebugInlining: false, nameTableKind: None, sysroot: "/")
!3 = !DIFile(filename: "pref.cpp", directory: "/tmp")
!4 = !{!0}
!5 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<int>", file: !3, line: 8, size: 8, flags: DIFlagTypePassByValue, elements: !6, templateParams: !7, identifier: "_ZTS3FooIiE", preferredName: !10)
!6 = !{}
!7 = !{!8}
!8 = !DITemplateTypeParameter(name: "T", type: !9)
!9 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!10 = !DIDerivedType(tag: DW_TAG_typedef, name: "Bar<int>", file: !3, line: 5, baseType: !5)

; CHECK:        ![[BASE_TY:[0-9]+]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<int>"
; CHECK-SAME:   preferredName: ![[NAME_TY:[0-9]+]]
; CHECK:        ![[NAME_TY]] = !DIDerivedType(tag: DW_TAG_typedef,
; CHECK-SAME:   baseType: ![[BASE_TY]]

!11 = !{i32 7, !"Dwarf Version", i32 4}
!12 = !{i32 2, !"Debug Info Version", i32 3}
!13 = !{i32 1, !"wchar_size", i32 4}
!14 = !{i32 8, !"PIC Level", i32 2}
!15 = !{i32 7, !"uwtable", i32 1}
!16 = !{i32 7, !"frame-pointer", i32 1}
!17 = !{!"clang version 17.0.0"}
