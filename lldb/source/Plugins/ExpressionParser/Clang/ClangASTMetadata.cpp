//===-- ClangASTMetadata.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/ExpressionParser/Clang/ClangASTMetadata.h"
#include "lldb/Target/Language.h"
#include "lldb/Utility/Stream.h"

using namespace lldb_private;

std::optional<bool> ClangASTMetadata::GetIsDynamicCXXType() const {
  switch (m_is_dynamic_cxx) {
  case 0:
    return std::nullopt;
  case 1:
    return false;
  case 2:
    return true;
  }
  llvm_unreachable("Invalid m_is_dynamic_cxx value");
}

void ClangASTMetadata::SetIsDynamicCXXType(std::optional<bool> b) {
  m_is_dynamic_cxx = b ? *b + 1 : 0;
}

void ClangASTMetadata::Dump(Stream *s) {
  lldb::user_id_t uid = GetUserID();

  if (uid != LLDB_INVALID_UID) {
    s->Printf("uid=0x%" PRIx64, uid);
  }

  uint64_t isa_ptr = GetISAPtr();
  if (isa_ptr != 0) {
    s->Printf("isa_ptr=0x%" PRIx64, isa_ptr);
  }

  if (m_is_dynamic_cxx) {
    s->Printf("is_dynamic_cxx=%i ", m_is_dynamic_cxx);
  }
  s->EOL();
}

void ClangASTMetadata::SetObjectPtrLanguage(lldb::LanguageType lang) {
  if (Language::LanguageIsObjC(lang))
    m_object_ptr_language = 1;
  else if (Language::LanguageIsCPlusPlus(lang))
    m_object_ptr_language = 2;
  else
    m_object_ptr_language = 0;
}

lldb::LanguageType ClangASTMetadata::GetObjectPtrLanguage() const {
  switch (m_object_ptr_language) {
    case 0: return lldb::LanguageType::eLanguageTypeUnknown;
    case 1: return lldb::LanguageType::eLanguageTypeObjC;
    case 2: return lldb::LanguageType::eLanguageTypeC_plus_plus;
  }

  llvm_unreachable("Invalid m_object_ptr_language value");
}
