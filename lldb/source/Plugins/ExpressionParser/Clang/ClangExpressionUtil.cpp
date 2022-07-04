//===-- ClangExpressionUtil.cpp ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ClangExpressionUtil.h"

#include "lldb/Core/ValueObject.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Utility/ConstString.h"

namespace lldb_private {
namespace ClangExpressionUtil {
lldb::ValueObjectSP GetLambdaValueObject(StackFrame *frame) {
  assert(frame);

  if (auto thisValSP = frame->FindVariable(ConstString("this")))
    if (thisValSP->GetChildMemberWithName(ConstString("this"), true))
      return thisValSP;

  return nullptr;
}
} // namespace ClangExpressionUtil
} // namespace lldb_private
