; RUN: llc < %s -filetype=obj -o %t
; RUN: llvm-dwarfdump -v %t | FileCheck %s

; C++ source to regenerate:
; struct defaulted {
;   // inline defaulted
;   defaulted() = default;
; 
;   // out-of-line defaulted (inline keyword
;   // shouldn't change that)
;   inline ~defaulted();
; 
;   // These shouldn't produce a defaulted-ness DI flag
;   // (though technically they are DW_DEFAULTED_no)
;   defaulted& operator=(defaulted const&) { return *this; }
;   defaulted& operator=(defaulted &&);
; 
;   bool operator==(defaulted const&) const = default;
; };
; 
; defaulted::~defaulted() = default;
; defaulted& defaulted::operator=(defaulted &&) { return *this; }
; 
; void foo() {
;   defaulted d;
; }
; $ clang++ -O0 -g -gdwarf-5 debug-info-defaulted.cpp -c

; CHECK: .debug_info contents:

; CHECK:  DW_TAG_structure_type
; CHECK:    DW_AT_name [DW_FORM_strx1]	(indexed ({{.*}}) string = "defaulted")

; CHECK:    DW_TAG_subprogram [5]
; CHECK:      DW_AT_name [DW_FORM_strx1]	(indexed ({{.*}}) string = "defaulted")
; CHECK:      DW_AT_defaulted [DW_FORM_data1]	(DW_DEFAULTED_in_class)
; CHECK:      NULL

; CHECK:    DW_TAG_subprogram [5]
; CHECK:      DW_AT_name [DW_FORM_strx1]	(indexed ({{.*}}) string = "~defaulted")
; CHECK:      DW_AT_defaulted [DW_FORM_data1]	(DW_DEFAULTED_out_of_class)
; CHECK:      NULL

; CHECK:    DW_TAG_subprogram [7]
; CHECK:      DW_AT_name [DW_FORM_strx1]	(indexed ({{.*}}) string = "operator=")
; CHECK-NOT:  DW_AT_defaulted
; CHECK:      NULL

; CHECK:    DW_TAG_subprogram [7]
; CHECK:      DW_AT_name [DW_FORM_strx1]	(indexed ({{.*}}) string = "operator=")
; CHECK-NOT:  DW_AT_defaulted
; CHECK:      NULL

; CHECK:    DW_TAG_subprogram [9]
; CHECK:      DW_AT_name [DW_FORM_strx1]	(indexed ({{.*}}) string = "operator==")
; CHECK:      DW_AT_defaulted [DW_FORM_data1]	(DW_DEFAULTED_in_class)
; CHECK:      NULL

; CHECK:    NULL

%struct.defaulted = type { i8 }

declare void @llvm.dbg.declare(metadata, metadata, metadata)

define void @_Z3foov() !dbg !39 {
entry:
  %d = alloca %struct.defaulted, align 1
  call void @llvm.dbg.declare(metadata ptr %d, metadata !42, metadata !DIExpression()), !dbg !43
  ret void, !dbg !47
}

!llvm.dbg.cu = !{!0}
!llvm.linker.options = !{}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7}
!llvm.ident = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 17.0.0", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: Apple, sysroot: "/")
!1 = !DIFile(filename: "../llvm-project/clang/test/CodeGenCXX/debug-info-defaulted.cpp", directory: "/tmp", checksumkind: CSK_MD5, checksum: "ee982c44dd268333101243e050103fb8")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 8, !"PIC Level", i32 2}
!6 = !{i32 7, !"uwtable", i32 1}
!7 = !{i32 7, !"frame-pointer", i32 1}
!8 = !{!"clang version 17.0.0"}
!9 = distinct !DISubprogram(name: "operator=", linkageName: "_ZN9defaultedaSEOS_", scope: !10, file: !1, line: 29, type: !24, scopeLine: 29, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, declaration: !23, retainedNodes: !32)
!10 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "defaulted", file: !1, line: 12, size: 8, flags: DIFlagTypePassByReference | DIFlagNonTrivial, elements: !11, identifier: "_ZTS9defaulted")
!11 = !{!12, !16, !17, !23, !27}
!12 = !DISubprogram(name: "defaulted", scope: !10, file: !1, line: 14, type: !13, scopeLine: 14, flags: DIFlagPrototyped, spFlags: DISPFlagDefaultedInClass)
!13 = !DISubroutineType(types: !14)
!14 = !{null, !15}
!15 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!16 = !DISubprogram(name: "~defaulted", scope: !10, file: !1, line: 18, type: !13, scopeLine: 18, flags: DIFlagPrototyped, spFlags: DISPFlagDefaultedOutOfClass)
!17 = !DISubprogram(name: "operator=", linkageName: "_ZN9defaultedaSERKS_", scope: !10, file: !1, line: 22, type: !18, scopeLine: 22, flags: DIFlagPrototyped, spFlags: 0)
!18 = !DISubroutineType(types: !19)
!19 = !{!20, !15, !21}
!20 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !10, size: 64)
!21 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !22, size: 64)
!22 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !10)
!23 = !DISubprogram(name: "operator=", linkageName: "_ZN9defaultedaSEOS_", scope: !10, file: !1, line: 23, type: !24, scopeLine: 23, flags: DIFlagPrototyped, spFlags: 0)
!24 = !DISubroutineType(types: !25)
!25 = !{!20, !15, !26}
!26 = !DIDerivedType(tag: DW_TAG_rvalue_reference_type, baseType: !10, size: 64)
!27 = !DISubprogram(name: "operator==", linkageName: "_ZNK9defaultedeqERKS_", scope: !10, file: !1, line: 25, type: !28, scopeLine: 25, flags: DIFlagPrototyped, spFlags: DISPFlagDefaultedInClass)
!28 = !DISubroutineType(types: !29)
!29 = !{!30, !31, !21}
!30 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!31 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !22, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!32 = !{}
!33 = !DILocalVariable(name: "this", arg: 1, scope: !9, type: !34, flags: DIFlagArtificial | DIFlagObjectPointer)
!34 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
!35 = !DILocation(line: 0, scope: !9)
!36 = !DILocalVariable(arg: 2, scope: !9, file: !1, line: 29, type: !26)
!37 = !DILocation(line: 29, column: 45, scope: !9)
!38 = !DILocation(line: 29, column: 49, scope: !9)
!39 = distinct !DISubprogram(name: "foo", linkageName: "_Z3foov", scope: !1, file: !1, line: 35, type: !40, scopeLine: 35, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !32)
!40 = !DISubroutineType(types: !41)
!41 = !{null}
!42 = !DILocalVariable(name: "d", scope: !39, file: !1, line: 36, type: !10)
!43 = !DILocation(line: 36, column: 13, scope: !39)
!46 = !DILocation(line: 37, column: 22, scope: !39)
!47 = !DILocation(line: 38, column: 1, scope: !39)
!48 = distinct !DISubprogram(name: "~defaulted", linkageName: "_ZN9defaultedD1Ev", scope: !10, file: !1, line: 28, type: !13, scopeLine: 28, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, declaration: !16, retainedNodes: !32)
!49 = !DILocalVariable(name: "this", arg: 1, scope: !48, type: !34, flags: DIFlagArtificial | DIFlagObjectPointer)
!50 = !DILocation(line: 0, scope: !48)
!51 = !DILocation(line: 28, column: 23, scope: !48)
!52 = distinct !DISubprogram(name: "~defaulted", linkageName: "_ZN9defaultedD2Ev", scope: !10, file: !1, line: 28, type: !13, scopeLine: 28, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, declaration: !16, retainedNodes: !32)
!53 = !DILocalVariable(name: "this", arg: 1, scope: !52, type: !34, flags: DIFlagArtificial | DIFlagObjectPointer)
!54 = !DILocation(line: 0, scope: !52)
!55 = !DILocation(line: 28, column: 23, scope: !52)
