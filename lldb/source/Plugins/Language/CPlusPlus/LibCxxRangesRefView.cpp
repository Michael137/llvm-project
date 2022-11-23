//===-- LibCxxRangesRefView.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LibCxx.h"

#include "lldb/Core/ValueObject.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Utility/ConstString.h"
#include "llvm/ADT/APSInt.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace lldb_private {
namespace formatters {

class LibcxxStdRangesRefViewSyntheticFrontEnd : public SyntheticChildrenFrontEnd {
public:
  LibcxxStdRangesRefViewSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp);

  ~LibcxxStdRangesRefViewSyntheticFrontEnd() override = default;

  size_t CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(size_t idx) override;

  bool Update() override;

  bool MightHaveChildren() override;

  size_t GetIndexOfChildWithName(ConstString name) override;

private:
  lldb::ValueObjectSP m_range_sp = nullptr;
};

lldb_private::formatters::LibcxxStdRangesRefViewSyntheticFrontEnd::
    LibcxxStdRangesRefViewSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp) {
  if (valobj_sp)
    Update();
}

size_t lldb_private::formatters::LibcxxStdRangesRefViewSyntheticFrontEnd::
    CalculateNumChildren() {
  return 1;
}

lldb::ValueObjectSP
lldb_private::formatters::LibcxxStdRangesRefViewSyntheticFrontEnd::GetChildAtIndex(
    size_t idx) {
  return m_range_sp;
}

bool lldb_private::formatters::LibcxxStdRangesRefViewSyntheticFrontEnd::Update() {
  // Get element type.
  ValueObjectSP range_ptr = GetChildMemberWithName(
      m_backend, {ConstString("__range_")});
  if (!range_ptr)
    return false;

  lldb_private::Status error;
  auto range_sp = range_ptr->Dereference(error);
  if (!error.Success())
    return false;

  m_range_sp = range_ptr->Dereference(error);

  return true;
}

bool lldb_private::formatters::LibcxxStdRangesRefViewSyntheticFrontEnd::
    MightHaveChildren() {
  return true;
}

size_t lldb_private::formatters::LibcxxStdRangesRefViewSyntheticFrontEnd::
    GetIndexOfChildWithName(ConstString name) {
  return 0;
}

lldb_private::SyntheticChildrenFrontEnd *
LibcxxStdRangesRefViewSyntheticFrontEndCreator(CXXSyntheticChildren *,
                                      lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp)
    return nullptr;
  CompilerType type = valobj_sp->GetCompilerType();
  if (!type.IsValid())
    return nullptr;
  return new LibcxxStdRangesRefViewSyntheticFrontEnd(valobj_sp);
}

} // namespace formatters
} // namespace lldb_private
