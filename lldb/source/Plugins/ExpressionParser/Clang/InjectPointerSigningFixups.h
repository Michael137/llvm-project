//===-- InjectPointerSigningFixups.h ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_EXPRESSIONPARSER_CLANG_INJECTPOINTERSIGNINGFIXUPS_H
#define LLDB_SOURCE_PLUGINS_EXPRESSIONPARSER_CLANG_INJECTPOINTERSIGNINGFIXUPS_H

#include "lldb/Host/Config.h"
#include "lldb/lldb-private-enumerations.h"
#include "llvm/Support/Error.h"

namespace llvm {
class Function;
class Module;
} // namespace llvm

namespace lldb_private {

llvm::Error InjectPointerSigningFixupCode(llvm::Module &M,
                                          ExecutionPolicy execution_policy);

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_EXPRESSIONPARSER_CLANG_INJECTPOINTERSIGNINGFIXUPS_H
