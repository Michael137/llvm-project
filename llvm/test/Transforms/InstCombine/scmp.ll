; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --version 5
; RUN: opt < %s -passes=instcombine -S | FileCheck %s

declare void @use(i8 %value)

define i1 @scmp_eq_0(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_eq_0(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp eq i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp eq i8 %1, 0
  ret i1 %2
}

define i1 @scmp_ne_0(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_ne_0(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp ne i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp ne i8 %1, 0
  ret i1 %2
}

define i1 @scmp_eq_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_eq_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sgt i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp eq i8 %1, 1
  ret i1 %2
}

define i1 @scmp_ne_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_ne_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sle i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp ne i8 %1, 1
  ret i1 %2
}

define i1 @scmp_eq_negative_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_eq_negative_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp slt i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp eq i8 %1, -1
  ret i1 %2
}

define i1 @scmp_ne_negative_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_ne_negative_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sge i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp ne i8 %1, -1
  ret i1 %2
}

define i1 @scmp_sgt_0(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_sgt_0(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sgt i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp sgt i8 %1, 0
  ret i1 %2
}

define i1 @scmp_sgt_neg_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_sgt_neg_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sge i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp sgt i8 %1, -1
  ret i1 %2
}

define i1 @scmp_sge_0(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_sge_0(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sge i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp sge i8 %1, 0
  ret i1 %2
}

define i1 @scmp_sge_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_sge_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sgt i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp sge i8 %1, 1
  ret i1 %2
}

define i1 @scmp_slt_0(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_slt_0(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp slt i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp slt i8 %1, 0
  ret i1 %2
}

define i1 @scmp_slt_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_slt_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sle i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp slt i8 %1, 1
  ret i1 %2
}

define i1 @scmp_sle_0(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_sle_0(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp sle i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp sle i8 %1, 0
  ret i1 %2
}

define i1 @scmp_sle_neg_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_sle_neg_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp slt i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp sle i8 %1, -1
  ret i1 %2
}

; scmp(x, y) u< C => x s>= y when C u> 1 and C != -1
define i1 @scmp_ult_positive_const_gt_than_1_lt_than_umax(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @scmp_ult_positive_const_gt_than_1_lt_than_umax(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP1:%.*]] = icmp sge i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP1]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp ult i8 %1, 4
  ret i1 %2
}

; scmp(x, y) s> C => x s< y when C != 0 and C != -1
define i1 @ucmp_ugt_const_not_0_or_neg1(i32 %x, i32 %y) {
; CHECK-LABEL: define i1 @ucmp_ugt_const_not_0_or_neg1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = icmp slt i32 [[X]], [[Y]]
; CHECK-NEXT:    ret i1 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = icmp ugt i8 %1, 12
  ret i1 %2
}


; ========== Fold -scmp(x, y) => scmp(y, x) ==========
define i8 @scmp_negated(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_negated(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[Y]], i32 [[X]])
; CHECK-NEXT:    ret i8 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  %2 = sub i8 0, %1
  ret i8 %2
}

; Negative test: do not fold if the original scmp result is already used
define i8 @scmp_negated_multiuse(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_negated_multiuse(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP1:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    call void @use(i8 [[TMP1]])
; CHECK-NEXT:    [[TMP2:%.*]] = sub nsw i8 0, [[TMP1]]
; CHECK-NEXT:    ret i8 [[TMP2]]
;
  %1 = call i8 @llvm.scmp(i32 %x, i32 %y)
  call void @use(i8 %1)
  %2 = sub i8 0, %1
  ret i8 %2
}

; Fold ((x s< y) ? -1 : (x != y)) into scmp(x, y)
define i8 @scmp_from_select_lt(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_lt(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %ne_bool = icmp ne i32 %x, %y
  %ne = zext i1 %ne_bool to i8
  %lt = icmp slt i32 %x, %y
  %r = select i1 %lt, i8 -1, i8 %ne
  ret i8 %r
}

; Fold (x s< y) ? -1 : zext(x s> y) into scmp(x, y)
define i8 @scmp_from_select_lt_and_gt(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_lt_and_gt(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %gt_bool = icmp sgt i32 %x, %y
  %gt = zext i1 %gt_bool to i8
  %lt = icmp slt i32 %x, %y
  %r = select i1 %lt, i8 -1, i8 %gt
  ret i8 %r
}

; Vector version
define <4 x i8> @scmp_from_select_vec_lt(<4 x i32> %x, <4 x i32> %y) {
; CHECK-LABEL: define <4 x i8> @scmp_from_select_vec_lt(
; CHECK-SAME: <4 x i32> [[X:%.*]], <4 x i32> [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call <4 x i8> @llvm.scmp.v4i8.v4i32(<4 x i32> [[X]], <4 x i32> [[Y]])
; CHECK-NEXT:    ret <4 x i8> [[R]]
;
  %ne_bool = icmp ne <4 x i32> %x, %y
  %ne = zext <4 x i1> %ne_bool to <4 x i8>
  %lt = icmp slt <4 x i32> %x, %y
  %r = select <4 x i1> %lt, <4 x i8> splat(i8 -1), <4 x i8> %ne
  ret <4 x i8> %r
}

; Fold (x s<= y) ? sext(x != y) : 1 into scmp(x, y)
define i8 @scmp_from_select_le(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_le(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %ne_bool = icmp ne i32 %x, %y
  %ne = sext i1 %ne_bool to i8
  %le = icmp sle i32 %x, %y
  %r = select i1 %le, i8 %ne, i8 1
  ret i8 %r
}

; Fold (x s>= y) ? zext(x != y) : -1 into scmp(x, y)
define i8 @scmp_from_select_ge(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_ge(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %ne_bool = icmp ne i32 %x, %y
  %ne = zext i1 %ne_bool to i8
  %ge = icmp sge i32 %x, %y
  %r = select i1 %ge, i8 %ne, i8 -1
  ret i8 %r
}

; Fold scmp(x nsw- y, 0) to scmp(x, y)
define i8 @scmp_of_sub_and_zero(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_of_sub_and_zero(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %diff = sub nsw i32 %x, %y
  %r = call i8 @llvm.scmp(i32 %diff, i32 0)
  ret i8 %r
}

; Negative test: no nsw
define i8 @scmp_of_sub_and_zero_neg_1(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_of_sub_and_zero_neg_1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[DIFF:%.*]] = sub i32 [[X]], [[Y]]
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[DIFF]], i32 0)
; CHECK-NEXT:    ret i8 [[R]]
;
  %diff = sub i32 %x, %y
  %r = call i8 @llvm.scmp(i32 %diff, i32 0)
  ret i8 %r
}

; Negative test: second argument of scmp is not 0
define i8 @scmp_of_sub_and_zero_neg2(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_of_sub_and_zero_neg2(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[DIFF:%.*]] = sub nsw i32 [[X]], [[Y]]
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[DIFF]], i32 15)
; CHECK-NEXT:    ret i8 [[R]]
;
  %diff = sub nsw i32 %x, %y
  %r = call i8 @llvm.scmp(i32 %diff, i32 15)
  ret i8 %r
}

; Negative test: calling ucmp instead of scmp
define i8 @scmp_of_sub_and_zero_neg3(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_of_sub_and_zero_neg3(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[DIFF:%.*]] = sub nsw i32 [[X]], [[Y]]
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.ucmp.i8.i32(i32 [[DIFF]], i32 0)
; CHECK-NEXT:    ret i8 [[R]]
;
  %diff = sub nsw i32 %x, %y
  %r = call i8 @llvm.ucmp(i32 %diff, i32 0)
  ret i8 %r
}

; Fold (x s> y) ? 1 : sext(x s< y)
define i8 @scmp_from_select_gt_and_lt(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_gt_and_lt(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %lt_bool = icmp slt i32 %x, %y
  %lt = sext i1 %lt_bool to i8
  %gt = icmp sgt i32 %x, %y
  %r = select i1 %gt, i8 1, i8 %lt
  ret i8 %r
}

; (x == y) ? 0 : (x s> y ? 1 : -1) into scmp(x, y)
define i8 @scmp_from_select_eq_and_gt(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_eq_and_gt(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %eq = icmp eq i32 %x, %y
  %gt = icmp sgt i32 %x, %y
  %sel1 = select i1 %gt, i8 1, i8 -1
  %r = select i1 %eq, i8 0, i8 %sel1
  ret i8 %r
}

define i8 @scmp_from_select_eq_and_gt_inverse(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_eq_and_gt_inverse(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %ne = icmp ne i32 %x, %y
  %gt = icmp sgt i32 %x, %y
  %sel1 = select i1 %gt, i8 1, i8 -1
  %r = select i1 %ne, i8 %sel1, i8 0
  ret i8 %r
}

define <4 x i8> @scmp_from_select_eq_and_gt_vec(<4 x i32> %x, <4 x i32> %y) {
; CHECK-LABEL: define <4 x i8> @scmp_from_select_eq_and_gt_vec(
; CHECK-SAME: <4 x i32> [[X:%.*]], <4 x i32> [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call <4 x i8> @llvm.scmp.v4i8.v4i32(<4 x i32> [[X]], <4 x i32> [[Y]])
; CHECK-NEXT:    ret <4 x i8> [[R]]
;
  %eq = icmp eq <4 x i32> %x, %y
  %gt = icmp sgt <4 x i32> %x, %y
  %sel1 = select <4 x i1> %gt, <4 x i8> splat(i8 1), <4 x i8> splat(i8 -1)
  %r = select <4 x i1> %eq, <4 x i8> splat(i8 0), <4 x i8> %sel1
  ret <4 x i8> %r
}

define i8 @scmp_from_select_eq_and_gt_commuted1(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_eq_and_gt_commuted1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[Y]], i32 [[X]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %eq = icmp eq i32 %x, %y
  %gt = icmp slt i32 %x, %y
  %sel1 = select i1 %gt, i8 1, i8 -1
  %r = select i1 %eq, i8 0, i8 %sel1
  ret i8 %r
}

define i8 @scmp_from_select_eq_and_gt_commuted2(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_eq_and_gt_commuted2(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[Y]], i32 [[X]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %eq = icmp eq i32 %x, %y
  %gt = icmp sgt i32 %x, %y
  %sel1 = select i1 %gt, i8 -1, i8 1
  %r = select i1 %eq, i8 0, i8 %sel1
  ret i8 %r
}

define i8 @scmp_from_select_eq_and_gt_commuted3(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_eq_and_gt_commuted3(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[R:%.*]] = call i8 @llvm.scmp.i8.i32(i32 [[X]], i32 [[Y]])
; CHECK-NEXT:    ret i8 [[R]]
;
  %eq = icmp eq i32 %x, %y
  %gt = icmp slt i32 %x, %y
  %sel1 = select i1 %gt, i8 -1, i8 1
  %r = select i1 %eq, i8 0, i8 %sel1
  ret i8 %r
}

define <3 x i2> @scmp_unary_shuffle_ops(<3 x i8> %x, <3 x i8> %y) {
; CHECK-LABEL: define <3 x i2> @scmp_unary_shuffle_ops(
; CHECK-SAME: <3 x i8> [[X:%.*]], <3 x i8> [[Y:%.*]]) {
; CHECK-NEXT:    [[TMP1:%.*]] = call <3 x i2> @llvm.scmp.v3i2.v3i8(<3 x i8> [[X]], <3 x i8> [[Y]])
; CHECK-NEXT:    [[R:%.*]] = shufflevector <3 x i2> [[TMP1]], <3 x i2> poison, <3 x i32> <i32 1, i32 0, i32 2>
; CHECK-NEXT:    ret <3 x i2> [[R]]
;
  %sx = shufflevector <3 x i8> %x, <3 x i8> poison, <3 x i32> <i32 1, i32 0, i32 2>
  %sy = shufflevector <3 x i8> %y, <3 x i8> poison, <3 x i32> <i32 1, i32 0, i32 2>
  %r = call <3 x i2> @llvm.scmp(<3 x i8> %sx, <3 x i8> %sy)
  ret <3 x i2> %r
}

; Negative test: true value of outer select is not zero
define i8 @scmp_from_select_eq_and_gt_neg1(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_eq_and_gt_neg1(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[EQ:%.*]] = icmp eq i32 [[X]], [[Y]]
; CHECK-NEXT:    [[GT:%.*]] = icmp sgt i32 [[X]], [[Y]]
; CHECK-NEXT:    [[SEL1:%.*]] = select i1 [[GT]], i8 1, i8 -1
; CHECK-NEXT:    [[R:%.*]] = select i1 [[EQ]], i8 5, i8 [[SEL1]]
; CHECK-NEXT:    ret i8 [[R]]
;
  %eq = icmp eq i32 %x, %y
  %gt = icmp sgt i32 %x, %y
  %sel1 = select i1 %gt, i8 1, i8 -1
  %r = select i1 %eq, i8 5, i8 %sel1
  ret i8 %r
}

; Negative test: true value of inner select is not 1 or -1
define i8 @scmp_from_select_eq_and_gt_neg2(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_eq_and_gt_neg2(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[EQ:%.*]] = icmp eq i32 [[X]], [[Y]]
; CHECK-NEXT:    [[GT:%.*]] = icmp sgt i32 [[X]], [[Y]]
; CHECK-NEXT:    [[SEL1:%.*]] = select i1 [[GT]], i8 2, i8 -1
; CHECK-NEXT:    [[R:%.*]] = select i1 [[EQ]], i8 0, i8 [[SEL1]]
; CHECK-NEXT:    ret i8 [[R]]
;
  %eq = icmp eq i32 %x, %y
  %gt = icmp sgt i32 %x, %y
  %sel1 = select i1 %gt, i8 2, i8 -1
  %r = select i1 %eq, i8 0, i8 %sel1
  ret i8 %r
}

; Negative test: false value of inner select is not 1 or -1
define i8 @scmp_from_select_eq_and_gt_neg3(i32 %x, i32 %y) {
; CHECK-LABEL: define i8 @scmp_from_select_eq_and_gt_neg3(
; CHECK-SAME: i32 [[X:%.*]], i32 [[Y:%.*]]) {
; CHECK-NEXT:    [[EQ:%.*]] = icmp eq i32 [[X]], [[Y]]
; CHECK-NEXT:    [[GT:%.*]] = icmp sgt i32 [[X]], [[Y]]
; CHECK-NEXT:    [[SEL1:%.*]] = select i1 [[GT]], i8 1, i8 22
; CHECK-NEXT:    [[R:%.*]] = select i1 [[EQ]], i8 0, i8 [[SEL1]]
; CHECK-NEXT:    ret i8 [[R]]
;
  %eq = icmp eq i32 %x, %y
  %gt = icmp sgt i32 %x, %y
  %sel1 = select i1 %gt, i8 1, i8 22
  %r = select i1 %eq, i8 0, i8 %sel1
  ret i8 %r
}
