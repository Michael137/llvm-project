//===-- TypeSystemClang.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_TYPESYSTEM_CLANG_TYPESYSTEMLITE_H
#define LLDB_SOURCE_PLUGINS_TYPESYSTEM_CLANG_TYPESYSTEMLITE_H

#include "lldb/Symbol/TypeSystem.h"

#include "clang/Basic/Specifiers.h"
#include "clang/AST/TypeBase.h"
#include "clang/AST/Attr.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/BinaryFormat/Dwarf.h"

#include <memory>

namespace lldb_private {

class DWARFASTParserLite;

struct LiteType;

struct LiteDecl {
  std::string name;
  const LiteType *type;
};

struct LiteType {
  clang::Type::TypeClass type_class;
  bool is_artificial = false;
  bool is_complete_objc_class = false;
  bool is_explicit = false;
  bool is_forward_declaration = false;
  bool is_inline = false;
  bool is_scoped_enum = false;
  bool is_vector = false;
  bool is_virtual = false;
  bool is_objc_direct_call = false;
  bool exports_symbols = false;
  clang::StorageClass storage = clang::SC_None;
  const char *mangled_name = nullptr;
  lldb_private::ConstString name;
  lldb_private::Declaration decl;
  //lldb_private::plugin::dwarf::DWARFFormValue abstract_origin;
  //lldb_private::plugin::dwarf::DWARFFormValue containing_type;
  //lldb_private::plugin::dwarf::DWARFFormValue signature;
  //lldb_private::plugin::dwarf::DWARFFormValue specification;
  //lldb_private::plugin::dwarf::DWARFFormValue type;
  lldb::LanguageType class_language = lldb::eLanguageTypeUnknown;
  std::optional<uint64_t> byte_size;
  std::optional<uint64_t> data_bit_size;
  std::optional<uint64_t> alignment;
  size_t calling_convention = llvm::dwarf::DW_CC_normal;
  uint32_t bit_stride = 0;
  uint32_t byte_stride = 0;
  uint32_t encoding = 0;

  ///< Indicates ref-qualifier of C++ member function if present.
  ///< Is RQ_None otherwise.
  clang::RefQualifierKind ref_qual = clang::RQ_None;

  ///< Has a value if this DIE represents an enum that was declared
  ///< with enum_extensibility.
  std::optional<clang::EnumExtensibilityAttr::Kind> enum_kind;

  llvm::SmallVector<LiteType *, 4> children;
  llvm::SmallVector<LiteDecl *, 4> child_decls;

  LLVM_DUMP_METHOD void dump() const {
      llvm::errs() << llvm::formatv("\n[name={0}, children=[", name);
      llvm::interleaveComma(child_decls, llvm::errs(), [](const LiteDecl *decl) {
        llvm::errs() << decl->name;
      });
      llvm::errs() << "]\n";
  }
};

class TypeSystemLite : public TypeSystem {
  // LLVM RTTI support
  static char ID;

public:
  static void Initialize();
  static void Terminate();
  static llvm::StringRef GetPluginNameStatic() { return "lite"; }
  static LanguageSet GetSupportedLanguagesForTypes();
  static LanguageSet GetSupportedLanguagesForExpressions();
  static lldb::TypeSystemSP CreateInstance(lldb::LanguageType language,
                                           lldb_private::Module *module,
                                           Target *target);

  CompilerType
  CreateRecordType(ConstString name, int kind,
                   lldb::LanguageType language) {
    auto *type = new LiteType;
    type->name = std::move(name);
    type->class_language = language;

    return CompilerType(weak_from_this(), type);
  }

  TypeSystemLite();
  ~TypeSystemLite() override;
  void Finalize() override {}

  // explicit TypeSystemLite(llvm::StringRef name, llvm::Triple triple);

  bool SupportsLanguage(lldb::LanguageType language) override {
#ifdef LITE
    return SupportsLanguageStatic(language);
#else
    return false;
#endif
  }

  static bool SupportsLanguageStatic(lldb::LanguageType language) {
#ifdef LITE
    return true;
#else
    return false;
#endif
  }

  // LLVM RTTI support
  bool isA(const void *ClassID) const override { return ClassID == &ID; }
  static bool classof(const TypeSystem *ts) { return ts->isA(&ID); }

  ConstString DeclGetName(void *opaque_decl) override {
    return ConstString("Unimplemented: DeclGetName");
  }

  CompilerType GetTypeForDecl(void *opaque_decl) override {
    if (!opaque_decl)
      return CompilerType();

    return CompilerType();
  }

  plugin::dwarf::DWARFASTParser *GetDWARFParser() override;

  ConstString DeclContextGetName(void *opaque_decl_ctx) override {
    return ConstString("Unimplemented: DeclContextGetName");
  }

  ConstString DeclContextGetScopeQualifiedName(void *opaque_decl_ctx) override {
    return ConstString("Unimplemented: DeclContextGetScopeQualifiedName");
  }

  bool DeclContextIsClassMethod(void *opaque_decl_ctx) override {
    return false;
  }

  bool DeclContextIsContainedInLookup(void *opaque_decl_ctx,
                                      void *other_opaque_decl_ctx) override {
    return false;
  }

  lldb::LanguageType DeclContextGetLanguage(void *opaque_decl_ctx) override {
    return lldb::eLanguageTypeC_plus_plus;
  }

  // Tests
#ifndef NDEBUG
  /// Verify the integrity of the type to catch CompilerTypes that mix
  /// and match invalid TypeSystem/Opaque type pairs.
  bool Verify(lldb::opaque_compiler_type_t type) override { return true; }
#endif

  bool IsArrayType(lldb::opaque_compiler_type_t type,
                   CompilerType *element_type, uint64_t *size,
                   bool *is_incomplete) override {
    return false;
  }

  bool IsAggregateType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsAnonymousType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsCharType(lldb::opaque_compiler_type_t type) override { return false; }

  bool IsCompleteType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsDefined(lldb::opaque_compiler_type_t type) override { return false; }

  bool IsFloatingPointType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsFunctionType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  size_t
  GetNumberOfFunctionArguments(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  CompilerType GetFunctionArgumentAtIndex(lldb::opaque_compiler_type_t type,
                                          const size_t index) override {
    return {};
  }

  bool IsFunctionPointerType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsMemberFunctionPointerType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsBlockPointerType(lldb::opaque_compiler_type_t type,
                          CompilerType *function_pointer_type_ptr) override {
    return false;
  }

  bool IsIntegerType(lldb::opaque_compiler_type_t type,
                     bool &is_signed) override {
    return false;
  }

  bool IsScopedEnumerationType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsPossibleDynamicType(lldb::opaque_compiler_type_t type,
                             CompilerType *target_type, // Can pass NULL
                             bool check_cplusplus, bool check_objc) override {
    return false;
  }

  bool IsPointerType(lldb::opaque_compiler_type_t type,
                     CompilerType *pointee_type) override {
    return false;
  }

  bool IsScalarType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsVoidType(lldb::opaque_compiler_type_t type) override { return false; }

  bool CanPassInRegisters(const CompilerType &type) override { return false; }

  // Type Completion

  bool GetCompleteType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsForcefullyCompleted(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  // AST related queries

  uint32_t GetPointerByteSize() override { return 8; }

  unsigned GetPtrAuthKey(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  unsigned GetPtrAuthDiscriminator(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  bool GetPtrAuthAddressDiversity(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  // Accessors

  ConstString GetTypeName(lldb::opaque_compiler_type_t type,
                          bool BaseOnly) override {
    assert (type);
    return ((LiteType*)type)->name;
  }

  ConstString GetDisplayTypeName(lldb::opaque_compiler_type_t type) override {
    return GetTypeName(type, /*BaseOnly=*/false);
  }

  uint32_t
  GetTypeInfo(lldb::opaque_compiler_type_t type,
              CompilerType *pointee_or_element_compiler_type) override {
    return 0;
  }

  lldb::LanguageType
  GetMinimumLanguage(lldb::opaque_compiler_type_t type) override {
    return lldb::eLanguageTypeC_plus_plus;
  }

  lldb::TypeClass GetTypeClass(lldb::opaque_compiler_type_t type) override {
    return lldb::eTypeClassInvalid;
  }

  // Creating related types

  CompilerType GetArrayElementType(lldb::opaque_compiler_type_t type,
                                   ExecutionContextScope *exe_scope) override {
    return CompilerType();
  }

  CompilerType GetArrayType(lldb::opaque_compiler_type_t type,
                            uint64_t size) override {
    return CompilerType();
  }

  CompilerType GetCanonicalType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType
  GetEnumerationIntegerType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  // Returns -1 if this isn't a function of if the function doesn't have a
  // prototype Returns a value >= 0 if there is a prototype.
  int GetFunctionArgumentCount(lldb::opaque_compiler_type_t type) override {
    return -1;
  }

  CompilerType GetFunctionArgumentTypeAtIndex(lldb::opaque_compiler_type_t type,
                                              size_t idx) override {
    return CompilerType();
  }

  CompilerType
  GetFunctionReturnType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  size_t GetNumMemberFunctions(lldb::opaque_compiler_type_t type) override {
      return 0;
  }

  TypeMemberFunctionImpl
  GetMemberFunctionAtIndex(lldb::opaque_compiler_type_t type,
                           size_t idx) override {
    return TypeMemberFunctionImpl();
  }

  CompilerType GetPointeeType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType GetPointerType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType
  GetLValueReferenceType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType
  GetRValueReferenceType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType GetAtomicType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType AddConstModifier(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType AddVolatileModifier(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType AddRestrictModifier(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType AddPtrAuthModifier(lldb::opaque_compiler_type_t type,
                                  uint32_t payload) override {
    return CompilerType();
  }

  /// \param opaque_payload      The m_payload field of Type, which may
  /// carry TypeSystem-specific extra information.
  CompilerType CreateTypedef(lldb::opaque_compiler_type_t type,
                             const char *name,
                             const CompilerDeclContext &decl_ctx,
                             uint32_t opaque_payload) override {
    return CompilerType();
  }

  // Exploring the type

  const llvm::fltSemantics &
  GetFloatTypeSemantics(size_t byte_size, lldb::Format format) override {
    return llvm::APFloat::IEEEdouble();
  }

  llvm::Expected<uint64_t>
  GetBitSize(lldb::opaque_compiler_type_t type,
             ExecutionContextScope *exe_scope) override {
    return 0;
  }

  lldb::Encoding GetEncoding(lldb::opaque_compiler_type_t type) override {
    return lldb::eEncodingInvalid;
  }

  lldb::Format GetFormat(lldb::opaque_compiler_type_t type) override {
    return lldb::eFormatDefault;
  }

  llvm::Expected<uint32_t>
  GetNumChildren(lldb::opaque_compiler_type_t type,
                 bool omit_empty_base_classes,
                 const ExecutionContext *exe_ctx) override {
    assert (type);
    return ((LiteType*)type)->child_decls.size();
  }

  CompilerType GetBuiltinTypeByName(ConstString name) override {
    return CompilerType();
  }

  lldb::BasicType
  GetBasicTypeEnumeration(lldb::opaque_compiler_type_t type) override {
    return lldb::eBasicTypeInvalid;
  }

  void ForEachEnumerator(
      lldb::opaque_compiler_type_t type,
      std::function<bool(const CompilerType &integer_type, ConstString name,
                         const llvm::APSInt &value)> const &callback) override {
  }

  uint32_t GetNumFields(lldb::opaque_compiler_type_t type) override {
    // TODO: this is not the same as "children"
    return llvm::cantFail(GetNumChildren(type, /*omit_empty_base_classes=*/true, nullptr));
  }

  CompilerType GetFieldAtIndex(lldb::opaque_compiler_type_t type, size_t idx,
                               std::string &name, uint64_t *bit_offset_ptr,
                               uint32_t *bitfield_bit_size_ptr,
                               bool *is_bitfield_ptr) override {
    return CompilerType();
  }

  uint32_t GetNumDirectBaseClasses(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  uint32_t
  GetNumVirtualBaseClasses(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  CompilerType GetDirectBaseClassAtIndex(lldb::opaque_compiler_type_t type,
                                         size_t idx,
                                         uint32_t *bit_offset_ptr) override {
    return CompilerType();
  }

  CompilerType GetVirtualBaseClassAtIndex(lldb::opaque_compiler_type_t type,
                                          size_t idx,
                                          uint32_t *bit_offset_ptr) override {
    return CompilerType();
  }

  CompilerDecl GetStaticFieldWithName(lldb::opaque_compiler_type_t type,
                                      llvm::StringRef name) override {
    return CompilerDecl();
  }

  llvm::Expected<CompilerType>
  GetDereferencedType(lldb::opaque_compiler_type_t type,
                      ExecutionContext *exe_ctx, std::string &deref_name,
                      uint32_t &deref_byte_size, int32_t &deref_byte_offset,
                      ValueObject *valobj, uint64_t &language_flags) override {
    return CompilerType();
  }

  llvm::Expected<CompilerType> GetChildCompilerTypeAtIndex(
      lldb::opaque_compiler_type_t type, ExecutionContext *exe_ctx, size_t idx,
      bool transparent_pointers, bool omit_empty_base_classes,
      bool ignore_array_bounds, std::string &child_name,
      uint32_t &child_byte_size, int32_t &child_byte_offset,
      uint32_t &child_bitfield_bit_size, uint32_t &child_bitfield_bit_offset,
      bool &child_is_base_class, bool &child_is_deref_of_parent,
      ValueObject *valobj, uint64_t &language_flags) override {
    assert (type);
    const auto &child_decls = ((LiteType*)type)->child_decls;
    const auto *child = child_decls[idx];

    child_name = child->name;
    child_byte_size = child->type->byte_size.value_or(0);
    child_bitfield_bit_size = child_byte_size * 8;

    // TODO:
    child_byte_offset = 0;
    child_bitfield_bit_offset = 0;
    child_is_base_class = false;
    child_is_deref_of_parent = false;
    language_flags = 0;

    return CompilerType(weak_from_this(), type);
  }

  // Lookup a child given a name. This function will match base class names and
  // member member names in "clang_type" only, not descendants.
  llvm::Expected<uint32_t>
  GetIndexOfChildWithName(lldb::opaque_compiler_type_t type,
                          llvm::StringRef name,
                          bool omit_empty_base_classes) override {
    assert (type);
    const auto &child_decls = ((LiteType*)type)->child_decls;
    return std::distance(child_decls.begin(), llvm::find_if(child_decls, [name](const LiteDecl *type) {
                    assert (type);
                    return type->name == name;
                }));
  }

  size_t
  GetIndexOfChildMemberWithName(lldb::opaque_compiler_type_t type,
                                llvm::StringRef name,
                                bool omit_empty_base_classes,
                                std::vector<uint32_t> &child_indexes) override {
    child_indexes.push_back(llvm::cantFail(GetIndexOfChildWithName(type, name, omit_empty_base_classes)));
    return child_indexes.size();
  }

  CompilerType GetDirectNestedTypeWithName(lldb::opaque_compiler_type_t type,
                                           llvm::StringRef name) override {
    return CompilerType();
  }

  bool IsTemplateType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  size_t GetNumTemplateArguments(lldb::opaque_compiler_type_t type,
                                 bool expand_pack) override {
    return 0;
  }

  lldb::TemplateArgumentKind
  GetTemplateArgumentKind(lldb::opaque_compiler_type_t type, size_t idx,
                          bool expand_pack) override {
    return lldb::eTemplateArgumentKindNull;
  }

  CompilerType GetTypeTemplateArgument(lldb::opaque_compiler_type_t type,
                                       size_t idx, bool expand_pack) override {
    return CompilerType();
  }

  std::optional<CompilerType::IntegralTemplateArgument>
  GetIntegralTemplateArgument(lldb::opaque_compiler_type_t type, size_t idx,
                              bool expand_pack) override {
    return std::nullopt;
  }

  // DIL

  /// Checks if the type is eligible for integral promotion.
  bool IsPromotableIntegerType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  /// Perform integral promotion on a given type.
  /// This promotes eligible types (boolean, integers, unscoped enumerations)
  /// to a larger integer type according to type system rules.
  /// \returns Promoted type.
  llvm::Expected<CompilerType>
  DoIntegralPromotion(CompilerType from,
                      ExecutionContextScope *exe_scope) override {
    return from;
  }

  // Dumping types

#ifndef NDEBUG
  /// Convenience LLVM-style dump method for use in the debugger only.
  LLVM_DUMP_METHOD void dump(lldb::opaque_compiler_type_t type) const override {
  }
#endif

  bool DumpTypeValue(lldb::opaque_compiler_type_t type, Stream &s,
                     lldb::Format format, const DataExtractor &data,
                     lldb::offset_t data_offset, size_t data_byte_size,
                     uint32_t bitfield_bit_size, uint32_t bitfield_bit_offset,
                     ExecutionContextScope *exe_scope) override {
    return false;
  }

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  /// Dump the type to stdout.
  void DumpTypeDescription(
      lldb::opaque_compiler_type_t type,
      lldb::DescriptionLevel level = lldb::eDescriptionLevelFull) override {}

  /// Print a description of the type to a stream. The exact implementation
  /// varies, but the expectation is that eDescriptionLevelFull returns a
  /// source-like representation of the type, whereas eDescriptionLevelVerbose
  /// does a dump of the underlying AST if applicable.
  void DumpTypeDescription(
      lldb::opaque_compiler_type_t type, Stream &s,
      lldb::DescriptionLevel level = lldb::eDescriptionLevelFull) override {}

  /// Dump a textual representation of the internal TypeSystem state to the
  /// given stream.
  ///
  /// This should not modify the state of the TypeSystem if possible.
  ///
  /// \param[out] output Stream to dup the AST into.
  /// \param[in] filter If empty, dump whole AST. If non-empty, will only
  /// dump decls whose names contain \c filter.
  /// \param[in] show_color If true, prints the AST color-highlighted.
  void Dump(llvm::raw_ostream &output, llvm::StringRef filter,
            bool show_color) override {}

  /// This is used by swift.
  bool IsRuntimeGeneratedType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  // TODO: Determine if these methods should move to TypeSystemClang.

  bool IsPointerOrReferenceType(lldb::opaque_compiler_type_t type,
                                CompilerType *pointee_type) override {
    return false;
  }

  unsigned GetTypeQualifiers(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  std::optional<size_t>
  GetTypeBitAlign(lldb::opaque_compiler_type_t type,
                  ExecutionContextScope *exe_scope) override {
    return std::nullopt;
  }

  CompilerType GetBasicTypeFromAST(lldb::BasicType basic_type) override {
    return CompilerType();
  }

  CompilerType CreateGenericFunctionPrototype() override {
    return CompilerType();
  }

  CompilerType GetBuiltinTypeForEncodingAndBitSize(lldb::Encoding encoding,
                                                   size_t bit_size) override {
    return CompilerType();
  }

  bool IsBeingDefined(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsConst(lldb::opaque_compiler_type_t type) override { return false; }

  uint32_t IsHomogeneousAggregate(lldb::opaque_compiler_type_t type,
                                  CompilerType *base_type_ptr) override {
    return 0;
  }

  bool IsPolymorphicClass(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsTypedefType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  // If the current object represents a typedef type, get the underlying type
  CompilerType GetTypedefedType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  bool IsVectorType(lldb::opaque_compiler_type_t type,
                    CompilerType *element_type, uint64_t *size) override {
    return false;
  }

  CompilerType
  GetFullyUnqualifiedType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  CompilerType GetNonReferenceType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  bool IsReferenceType(lldb::opaque_compiler_type_t type,
                       CompilerType *pointee_type, bool *is_rvalue) override {
    return false;
  }

  // TODO: does this need to live on TypeSystem?
  static void AddFieldToRecordType(
      CompilerType class_clang_type, llvm::StringRef name, CompilerType member_clang_type) {
    assert (class_clang_type);
    assert (member_clang_type);
    auto *record = ((LiteType*)class_clang_type.GetOpaqueQualType());

    auto * decl = new LiteDecl;
    decl->name = name.str();
    decl->type = static_cast<LiteType*>(member_clang_type.GetOpaqueQualType());
    record->child_decls.push_back(decl);
  }

  CompilerType GetBuiltinTypeForDWARFEncodingAndBitSize(
        llvm::StringRef name, uint32_t encoding, uint32_t bit_size);

private:
  std::unique_ptr<DWARFASTParserLite> m_dwarf_ast_parser_up;
};
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_TYPESYSTEM_CLANG_TYPESYSTEMLITE_H
