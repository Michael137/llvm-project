//===-- LibCxxMap.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LibCxx.h"

#include "clang/AST/DeclCXX.h"

#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/lldb-defines.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/lldb-forward.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace lldb_private {
namespace formatters {
class LibcxxStdFlatMapSyntheticFrontEnd : public SyntheticChildrenFrontEnd {
public:
  LibcxxStdFlatMapSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp);

  ~LibcxxStdFlatMapSyntheticFrontEnd() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

  lldb::ChildCacheState Update() override;

  llvm::Expected<size_t> GetIndexOfChildWithName(ConstString name) override;

private:
  bool UpdatePairType(CompilerType key_type, CompilerType value_type,
                      ExecutionContextScope *exe_scope);

  WritableDataBufferSP ReadKeyValueFromMemory(Process &process,
                                              ValueObject &key,
                                              ValueObject &value);

  std::unique_ptr<SyntheticChildrenFrontEnd> m_keys;
  std::unique_ptr<SyntheticChildrenFrontEnd> m_values;
  llvm::DenseMap<size_t, ValueObjectSP> m_elements;

  CompilerType m_pair_type;
  size_t m_key_size = 0;
  size_t m_value_size = 0;
};

} // namespace formatters
} // namespace lldb_private

lldb_private::formatters::LibcxxStdFlatMapSyntheticFrontEnd::
    LibcxxStdFlatMapSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp) {
  if (valobj_sp)
    Update();
}

llvm::Expected<uint32_t> lldb_private::formatters::
    LibcxxStdFlatMapSyntheticFrontEnd::CalculateNumChildren() {
  if (!m_keys)
    return 0;

  return m_keys->CalculateNumChildren();
}

// TODO: this needs to be unique across different template arguments!
static CompilerType GetLLDBPairType(TargetSP target_sp, CompilerType key_type,
                                    CompilerType value_type) {
  // Synthesize new type into key/value TypeSystemClang. Otherwise invariants of
  // other parts of LLDB are violated.
  TypeSystemClangSP scratch_ts_sp =
      key_type.GetTypeSystem().dyn_cast_or_null<TypeSystemClang>();
  if (!scratch_ts_sp)
    return {};

  static constexpr llvm::StringLiteral g_lldb_autogen_flat_map_pair(
      "__lldb_autogen_flat_map_pair");

  CompilerType compiler_type =
      scratch_ts_sp->GetTypeForIdentifier<clang::CXXRecordDecl>(
          g_lldb_autogen_flat_map_pair);

  if (!compiler_type) {
    // TODO: should create a ClassTemplateSpecializationDecl?
    compiler_type = scratch_ts_sp->CreateRecordType(
        nullptr, OptionalClangModuleID(), lldb::eAccessPublic,
        g_lldb_autogen_flat_map_pair,
        llvm::to_underlying(clang::TagTypeKind::Struct),
        lldb::eLanguageTypeC_plus_plus);

    if (compiler_type) {
      TypeSystemClang::StartTagDeclarationDefinition(compiler_type);
      TypeSystemClang::AddFieldToRecordType(compiler_type, "first", key_type,
                                            lldb::eAccessPublic, 0);
      TypeSystemClang::AddFieldToRecordType(compiler_type, "second", value_type,
                                            lldb::eAccessPublic, 0);
      TypeSystemClang::CompleteTagDeclarationDefinition(compiler_type);
    }
  }
  return compiler_type;
}

bool lldb_private::formatters::LibcxxStdFlatMapSyntheticFrontEnd::
    UpdatePairType(CompilerType key_type, CompilerType value_type,
                   ExecutionContextScope *exe_scope) {
  TargetSP target_sp(m_backend.GetTargetSP());
  if (!target_sp)
    return false;

  auto pair_type = GetLLDBPairType(target_sp, key_type, value_type);
  if (!pair_type.IsValid())
    return false;

  size_t key_size = 0;
  if (auto size_or_err = key_type.GetByteSize(exe_scope))
    key_size = *size_or_err;
  else {
    llvm::consumeError(size_or_err.takeError());
    return false;
  }

  size_t value_size = 0;
  if (auto size_or_err = value_type.GetByteSize(exe_scope))
    value_size = *size_or_err;
  else {
    llvm::consumeError(size_or_err.takeError());
    return false;
  }

  m_pair_type = pair_type;
  m_key_size = key_size;
  m_value_size = value_size;

  return true;
}

WritableDataBufferSP
lldb_private::formatters::LibcxxStdFlatMapSyntheticFrontEnd::
    ReadKeyValueFromMemory(Process &process, ValueObject &key,
                           ValueObject &value) {
  auto [key_addr, key_type] = key.GetAddressOf();
  auto [val_addr, val_type] = value.GetAddressOf();

  if (val_addr == LLDB_INVALID_ADDRESS || key_addr == LLDB_INVALID_ADDRESS)
    return nullptr;

  Status readmem_error;
  WritableDataBufferSP buffer_sp(
      new DataBufferHeap(m_value_size + m_key_size, 0));
  size_t bytes_read = process.ReadMemory(key_addr, buffer_sp->GetBytes(),
                                         m_key_size, readmem_error);
  if (!readmem_error.Success() || bytes_read == 0)
    return nullptr;

  bytes_read = process.ReadMemory(val_addr, buffer_sp->GetBytes() + m_key_size,
                                  m_value_size, readmem_error);
  if (!readmem_error.Success() || bytes_read == 0)
    return nullptr;

  return buffer_sp;
}

lldb::ValueObjectSP
lldb_private::formatters::LibcxxStdFlatMapSyntheticFrontEnd::GetChildAtIndex(
    uint32_t idx) {
  auto it = m_elements.find(idx);
  if (it != m_elements.end())
    return it->getSecond();

  if (!m_keys || !m_values)
    return nullptr;

  auto key_sp = m_keys->GetChildAtIndex(idx);
  if (!key_sp)
    return nullptr;

  auto value_sp = m_values->GetChildAtIndex(idx);
  if (!value_sp)
    return nullptr;

  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return nullptr;

  ExecutionContext exe_ctx(valobj_sp->GetExecutionContextRef());

  if (!m_pair_type.IsValid())
    if (!UpdatePairType(key_sp->GetCompilerType(), value_sp->GetCompilerType(),
                        exe_ctx.GetBestExecutionContextScope()))
      return nullptr;

  lldb::ProcessSP process_sp(valobj_sp->GetProcessSP());
  if (!process_sp)
    return nullptr;

  WritableDataBufferSP key_value_buffer_sp =
      ReadKeyValueFromMemory(*process_sp, *key_sp, *value_sp);

  StreamString idx_name;
  idx_name.Printf("[%" PRIu32 "]", idx);
  DataExtractor data(key_value_buffer_sp, process_sp->GetByteOrder(),
                     process_sp->GetAddressByteSize());

  auto pair_sp = CreateValueObjectFromData(idx_name.GetString(), data, exe_ctx,
                                           m_pair_type);
  if (!pair_sp)
    return nullptr;

  m_elements[idx] = pair_sp;

  return pair_sp;
}

lldb::ChildCacheState
lldb_private::formatters::LibcxxStdFlatMapSyntheticFrontEnd::Update() {
  m_keys = nullptr;
  m_values = nullptr;
  m_elements.clear();
  m_pair_type.Clear();
  m_key_size = 0;
  m_value_size = 0;

  auto containers_sp = m_backend.GetChildMemberWithName("__containers_");
  if (!containers_sp)
    return lldb::ChildCacheState::eRefetch;

  auto keys_sp = containers_sp->GetChildMemberWithName("keys");
  if (!keys_sp)
    return lldb::ChildCacheState::eRefetch;

  m_keys = std::unique_ptr<SyntheticChildrenFrontEnd>(
      LibcxxStdVectorSyntheticFrontEndCreator(nullptr, keys_sp));

  auto values_sp = containers_sp->GetChildMemberWithName("values");
  if (!values_sp)
    return lldb::ChildCacheState::eRefetch;

  m_values = std::unique_ptr<SyntheticChildrenFrontEnd>(
      LibcxxStdVectorSyntheticFrontEndCreator(nullptr, values_sp));

  return lldb::ChildCacheState::eRefetch;
}

llvm::Expected<size_t>
lldb_private::formatters::LibcxxStdFlatMapSyntheticFrontEnd::
    GetIndexOfChildWithName(ConstString name) {
  auto optional_idx = formatters::ExtractIndexFromString(name.GetCString());
  if (!optional_idx) {
    return llvm::createStringError("Type has no child named '%s'",
                                   name.AsCString());
  }
  return *optional_idx;
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::LibcxxStdFlatMapSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  return (valobj_sp ? new LibcxxStdFlatMapSyntheticFrontEnd(valobj_sp)
                    : nullptr);
}
