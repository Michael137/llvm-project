; REQUIRES: object-emission
; RUN: llc -filetype=obj -o %t %s
; RUN: llvm-dwarfdump -debug-info %t | FileCheck %s
; Source:
;    template<typename T>
;    struct Foo;
;    
;    typedef Foo<int> BarInt;
;    typedef Foo<double> BarDouble;
;    typedef Foo<char> BarChar;
;    
;    template<typename T>
;    struct [[clang::preferred_name(BarInt),
;             clang::preferred_name(BarDouble)]] Foo {};
;    
;    Foo<int> varInt;
;    Foo<double> varDouble;
;    Foo<char> varChar;
;
; Compilation flag (on macOS):
;   clang++ -glldb -S -emit-llvm -std=c++2a pref.cpp

; ModuleID = 'pref.cpp'
source_filename = "pref.cpp"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.Foo = type { i8 }
%struct.Foo.0 = type { i8 }
%struct.Foo.1 = type { i8 }

@varInt = global %struct.Foo zeroinitializer, align 1, !dbg !0
@varDouble = global %struct.Foo.0 zeroinitializer, align 1, !dbg !5
@varChar = global %struct.Foo.1 zeroinitializer, align 1, !dbg !13

!llvm.module.flags = !{!24, !25, !26, !27, !28, !29, !30}
!llvm.dbg.cu = !{!2}
!llvm.linker.options = !{}
!llvm.ident = !{!31}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "varInt", scope: !2, file: !3, line: 12, type: !19, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 17.0.0", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !4, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "pref.cpp", directory: "/tmp")
!4 = !{!0, !5, !13}
!5 = !DIGlobalVariableExpression(var: !6, expr: !DIExpression())
!6 = distinct !DIGlobalVariable(name: "varDouble", scope: !2, file: !3, line: 13, type: !7, isLocal: false, isDefinition: true)
!7 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<double>", file: !3, line: 10, size: 8, flags: DIFlagTypePassByValue, elements: !8, templateParams: !9, identifier: "_ZTS3FooIdE", preferredName: !12)
!8 = !{}
!9 = !{!10}
!10 = !DITemplateTypeParameter(name: "T", type: !11)
!11 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!12 = !DIDerivedType(tag: DW_TAG_typedef, name: "BarDouble", file: !3, line: 5, baseType: !7)
!13 = !DIGlobalVariableExpression(var: !14, expr: !DIExpression())
!14 = distinct !DIGlobalVariable(name: "varChar", scope: !2, file: !3, line: 14, type: !15, isLocal: false, isDefinition: true)
!15 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<char>", file: !3, line: 10, size: 8, flags: DIFlagTypePassByValue, elements: !8, templateParams: !16, identifier: "_ZTS3FooIcE")
!16 = !{!17}
!17 = !DITemplateTypeParameter(name: "T", type: !18)
!18 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!19 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<int>", file: !3, line: 10, size: 8, flags: DIFlagTypePassByValue, elements: !8, templateParams: !20, identifier: "_ZTS3FooIiE", preferredName: !23)
!20 = !{!21}
!21 = !DITemplateTypeParameter(name: "T", type: !22)
!22 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!23 = !DIDerivedType(tag: DW_TAG_typedef, name: "BarInt", file: !3, line: 4, baseType: !19)

; CHECK:      0x[[FOO_INT:[0-9a-f]+]]: DW_TAG_structure_type
; CHECK-DAG:    DW_AT_LLVM_preferred_name	(0x[[FOO_INT_PREF:[0-9a-f]+]])
; CHECK-DAG:    DW_AT_name	("Foo<int>")
; CHECK-DAG:    NULL

; CHECK:      0x[[FOO_INT_PREF]]: DW_TAG_typedef
; CHECK-DAG:    DW_AT_type	(0x[[FOO_INT]] "Foo<int>")
; CHECK-DAG:    DW_AT_name	("BarInt")

; CHECK:      0x[[FOO_INT:[0-9a-f]+]]: DW_TAG_structure_type
; CHECK-DAG:    DW_AT_LLVM_preferred_name	(0x[[FOO_DOUBLE_PREF:[0-9a-f]+]])
; CHECK-DAG:    DW_AT_name	("Foo<double>")
; CHECK:        NULL

; CHECK:      0x[[FOO_DOUBLE_PREF]]: DW_TAG_typedef
; CHECK-DAG:    DW_AT_type	(0x[[FOO_INT]] "Foo<double>")
; CHECK-DAG:    DW_AT_name	("BarDouble")

!24 = !{i32 2, !"SDK Version", [2 x i32] [i32 13, i32 3]}
!25 = !{i32 7, !"Dwarf Version", i32 4}
!26 = !{i32 2, !"Debug Info Version", i32 3}
!27 = !{i32 1, !"wchar_size", i32 4}
!28 = !{i32 8, !"PIC Level", i32 2}
!29 = !{i32 7, !"uwtable", i32 1}
!30 = !{i32 7, !"frame-pointer", i32 1}
!31 = !{!"clang version 17.0.0"}
