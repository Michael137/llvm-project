#include "DWARFASTParserLite.h"

#include "DWARFUnit.h"
#include "LogChannelDWARF.h"
#include "SymbolFileDWARF.h"

// FIXME: only needed for ParsedDWARFTypeAttributes
#include "DWARFASTParserClang.h"

#include "lldb/Symbol/CompileUnit.h"
#include "lldb/Symbol/Type.h"
#include "llvm/Support/ErrorHandling.h"

using namespace lldb_private;
using namespace lldb_private::plugin::dwarf;
using namespace llvm::dwarf;

lldb::TypeSP DWARFASTParserLite::ParseTypeFromDWARF(const SymbolContext &sc,
                                                    const DWARFDIE &die,
                                                    bool *type_is_new_ptr) {
  if (type_is_new_ptr)
    *type_is_new_ptr = false;

  if (!die)
    return nullptr;

  SymbolFileDWARF *dwarf = die.GetDWARF();

  ParsedDWARFTypeAttributes attrs(die);

  lldb::TypeSP type_sp;
  if (type_is_new_ptr)
    *type_is_new_ptr = true;

  const dw_tag_t tag = die.Tag();

  switch (tag) {
  case DW_TAG_typedef:
  case DW_TAG_template_alias:
  case DW_TAG_base_type:
  case DW_TAG_pointer_type:
  case DW_TAG_reference_type:
  case DW_TAG_rvalue_reference_type:
  case DW_TAG_const_type:
  case DW_TAG_restrict_type:
  case DW_TAG_volatile_type:
  case DW_TAG_LLVM_ptrauth_type:
  case DW_TAG_atomic_type:
  case DW_TAG_unspecified_type:
    type_sp = ParseTypeModifier(sc, die, attrs);
    break;
  case DW_TAG_structure_type:
  case DW_TAG_union_type:
  case DW_TAG_class_type:
    type_sp = ParseStructureLikeDIE(sc, die, attrs);
    break;
  case DW_TAG_enumeration_type:
  case DW_TAG_inlined_subroutine:
  case DW_TAG_subprogram:
  case DW_TAG_subroutine_type:
  case DW_TAG_array_type:
  case DW_TAG_ptr_to_member_type:
  default:
    dwarf->GetObjectFile()->GetModule()->ReportError(
        "[{0:x16}]: unhandled type tag {1:x4} ({2}), "
        "please file a bug and "
        "attach the file at the start of this error message",
        die.GetOffset(), tag, DW_TAG_value_to_name(tag));
    break;
  }

  if (type_sp) {
    dwarf->GetDIEToType()[die.GetDIE()] = type_sp.get();
  }
  return type_sp;
}

// Required for ParseFunction...required for setting m_sc.block...required to
// get in-scope local variables
Function *DWARFASTParserLite::ParseFunctionFromDWARF(
    CompileUnit &comp_unit, const DWARFDIE &die, AddressRanges func_ranges) {
  llvm::DWARFAddressRangesVector unused_func_ranges;
  const char *name = nullptr;
  const char *mangled = nullptr;
  std::optional<int> decl_file;
  std::optional<int> decl_line;
  std::optional<int> decl_column;
  std::optional<int> call_file;
  std::optional<int> call_line;
  std::optional<int> call_column;
  DWARFExpressionList frame_base;

  const dw_tag_t tag = die.Tag();

  if (tag != DW_TAG_subprogram)
    return nullptr;

  if (die.GetDIENamesAndRanges(name, mangled, unused_func_ranges, decl_file,
                               decl_line, decl_column, call_file, call_line,
                               call_column, &frame_base)) {
    Mangled func_name;
    if (mangled)
      func_name.SetValue(ConstString(mangled));
    else if ((die.GetParent().Tag() == DW_TAG_compile_unit ||
              die.GetParent().Tag() == DW_TAG_partial_unit) &&
             Language::LanguageIsCPlusPlus(
                 SymbolFileDWARF::GetLanguage(*die.GetCU())) &&
             !Language::LanguageIsObjC(
                 SymbolFileDWARF::GetLanguage(*die.GetCU())) &&
             name && strcmp(name, "main") != 0) {
      // If the mangled name is not present in the DWARF, generate the
      // demangled name using the decl context. We skip if the function is
      // "main" as its name is never mangled.
      func_name.SetDemangledName(ConstructDemangledNameFromDWARF(die));
      // Ensure symbol is preserved (as the mangled name).
      func_name.SetMangledName(ConstString(name));
    } else
      func_name.SetValue(ConstString(name));

    lldb::FunctionSP func_sp;
    std::unique_ptr<Declaration> decl_up;
    if (decl_file || decl_line || decl_column)
      decl_up = std::make_unique<Declaration>(
          die.GetCU()->GetFile(decl_file.value_or(0)), decl_line.value_or(0),
          decl_column.value_or(0));

    SymbolFileDWARF *dwarf = die.GetDWARF();
    // Supply the type _only_ if it has already been parsed
    Type *func_type = dwarf->GetDIEToType().lookup(die.GetDIE());

    assert(func_type == nullptr || func_type != DIE_IS_BEING_PARSED);

    const lldb::user_id_t func_user_id = die.GetID();

    // The base address of the scope for any of the debugging information
    // entries listed above is given by either the DW_AT_low_pc attribute or the
    // first address in the first range entry in the list of ranges given by the
    // DW_AT_ranges attribute.
    //   -- DWARFv5, Section 2.17 Code Addresses, Ranges and Base Addresses
    //
    // If no DW_AT_entry_pc attribute is present, then the entry address is
    // assumed to be the same as the base address of the containing scope.
    //   -- DWARFv5, Section 2.18 Entry Address
    //
    // We currently don't support Debug Info Entries with
    // DW_AT_low_pc/DW_AT_entry_pc and DW_AT_ranges attributes (the latter
    // attributes are ignored even though they should be used for the address of
    // the function), but compilers also don't emit that kind of information. If
    // this becomes a problem we need to plumb these attributes separately.
    Address func_addr = func_ranges[0].GetBaseAddress();

    func_sp = std::make_shared<Function>(
        &comp_unit,
        func_user_id, // UserID is the DIE offset
        func_user_id, func_name, func_type, std::move(func_addr),
        std::move(func_ranges));

    if (func_sp.get() != nullptr) {
      if (frame_base.IsValid())
        func_sp->GetFrameBaseExpression() = frame_base;
      comp_unit.AddFunction(func_sp);
      return func_sp.get();
    }
  }
  return nullptr;
}

void DWARFASTParserLite::ParseSingleMember(plugin::dwarf::DWARFDIE die, plugin::dwarf::DWARFDIE parent_die, CompilerType class_clang_type) {
  // This function can only parse DW_TAG_member.
  assert(die.Tag() == DW_TAG_member);

  lldb::ModuleSP module_sp = parent_die.GetDWARF()->GetObjectFile()->GetModule();
  const dw_tag_t tag = die.Tag();
  // Get the parent byte size so we can verify any members will fit
  const uint64_t parent_byte_size =
      parent_die.GetAttributeValueAsUnsigned(DW_AT_byte_size, UINT64_MAX);
  const uint64_t parent_bit_size =
      parent_byte_size == UINT64_MAX ? UINT64_MAX : parent_byte_size * 8;

  const DWARFASTParserClang::MemberAttributes attrs(die, parent_die, module_sp);

  Type *member_type = die.ResolveTypeUID(attrs.encoding_form.Reference());
  if (!member_type) {
    if (attrs.name)
      module_sp->ReportError(
          "{0:x8}: DW_TAG_member '{1}' refers to type {2:x16}"
          " which was unable to be parsed",
          die.GetID(), attrs.name, attrs.encoding_form.Reference().GetOffset());
    else
      module_sp->ReportError("{0:x8}: DW_TAG_member refers to type {1:x16}"
                             " which was unable to be parsed",
                             die.GetID(),
                             attrs.encoding_form.Reference().GetOffset());
    return;
  }

  const uint64_t character_width = 8;
  CompilerType member_clang_type = member_type->GetLayoutCompilerType();

  uint64_t field_bit_offset = (attrs.member_byte_offset == UINT32_MAX
                                   ? 0
                                   : (attrs.member_byte_offset * 8ULL));

  DWARFASTParserClang::FieldInfo last_field_info;

  if (attrs.bit_size > 0) {
    DWARFASTParserClang::FieldInfo this_field_info;
    this_field_info.bit_offset = field_bit_offset;
    this_field_info.bit_size = attrs.bit_size;

    if (attrs.data_bit_offset != UINT64_MAX) {
      this_field_info.bit_offset = attrs.data_bit_offset;
    } else {
      auto byte_size = attrs.byte_size;
      if (!byte_size)
        byte_size = llvm::expectedToOptional(member_type->GetByteSize(nullptr));

      ObjectFile *objfile = die.GetDWARF()->GetObjectFile();
      if (objfile->GetByteOrder() == lldb::eByteOrderLittle) {
        this_field_info.bit_offset += byte_size.value_or(0) * 8;
        this_field_info.bit_offset -= (attrs.bit_offset + attrs.bit_size);
      } else {
        this_field_info.bit_offset += attrs.bit_offset;
      }
    }

    // Update the field bit offset we will report for layout
    field_bit_offset = this_field_info.bit_offset;

    last_field_info = this_field_info;
    last_field_info.SetIsBitfield(true);
  } else {
    DWARFASTParserClang::FieldInfo this_field_info;
    this_field_info.is_bitfield = false;
    this_field_info.bit_offset = field_bit_offset;

    // TODO: we shouldn't silently ignore the bit_size if we fail
    //       to GetByteSize.
    if (std::optional<uint64_t> clang_type_size =
            llvm::expectedToOptional(member_type->GetByteSize(nullptr))) {
      this_field_info.bit_size = *clang_type_size * character_width;
    }

    if (this_field_info.GetFieldEnd() <= last_field_info.GetEffectiveFieldEnd())
      this_field_info.SetEffectiveFieldEnd(
          last_field_info.GetEffectiveFieldEnd());

    last_field_info = this_field_info;
  }

  TypeSystemLite::AddFieldToRecordType(
      class_clang_type, attrs.name, member_clang_type);
}

bool DWARFASTParserLite::ParseChildMembers(
    DWARFDIE parent_die, CompilerType class_clang_type) {
  if (!parent_die)
    return false;

  lldb::ModuleSP module_sp = parent_die.GetDWARF()->GetObjectFile()->GetModule();
  auto ast = class_clang_type.GetTypeSystem<TypeSystemLite>();
  assert (ast);

  for (DWARFDIE die : parent_die.children()) {
    dw_tag_t tag = die.Tag();

    switch (tag) {
    case DW_TAG_member:
      ParseSingleMember(die, parent_die, class_clang_type);
      break;
    default:
      break;
    }
  }

  return true;
}

lldb::TypeSP
DWARFASTParserLite::ParseStructureLikeDIE(const SymbolContext &sc,
                                          const DWARFDIE &die,
                                          ParsedDWARFTypeAttributes &attrs) {
  CompilerType clang_type;
  const dw_tag_t tag = die.Tag();
  SymbolFileDWARF *dwarf = die.GetDWARF();
  lldb::LanguageType cu_language = SymbolFileDWARF::GetLanguage(*die.GetCU());
  Log *log = GetLog(DWARFLog::TypeCompletion | DWARFLog::Lookups);

  ConstString unique_typename(attrs.name);
  Declaration unique_decl(attrs.decl);
  uint64_t byte_size = attrs.byte_size.value_or(0);

  int tag_decl_kind = -1;
  if (tag == DW_TAG_structure_type) {
    tag_decl_kind = llvm::to_underlying(clang::TagTypeKind::Struct);
  } else if (tag == DW_TAG_union_type) {
    tag_decl_kind = llvm::to_underlying(clang::TagTypeKind::Union);
  } else if (tag == DW_TAG_class_type) {
    tag_decl_kind = llvm::to_underlying(clang::TagTypeKind::Class);
  }

  assert(tag_decl_kind != -1);
  UNUSED_IF_ASSERT_DISABLED(tag_decl_kind);

  clang_type = m_ast.CreateRecordType(
      attrs.name, tag_decl_kind, attrs.class_language);

  ParseChildMembers(die, clang_type);

  lldb::TypeSP type_sp = dwarf->MakeType(
      die.GetID(), attrs.name, attrs.byte_size, nullptr, LLDB_INVALID_UID,
      Type::eEncodingIsUID, &attrs.decl, clang_type,
      Type::ResolveState::Forward,
      TypePayloadClang(OptionalClangModuleID(), attrs.is_complete_objc_class));

  // UniqueDWARFASTType is large, so don't create a local variables on the
  // stack, put it on the heap. This function is often called recursively and
  // clang isn't good at sharing the stack space for variables in different
  // blocks.
  auto unique_ast_entry_up = std::make_unique<UniqueDWARFASTType>();
  // Add our type to the unique type map so we don't end up creating many
  // copies of the same type over and over in the ASTContext for our
  // module
  unique_ast_entry_up->m_type_sp = type_sp;
  unique_ast_entry_up->m_die = die;
  unique_ast_entry_up->m_declaration = unique_decl;
  unique_ast_entry_up->m_byte_size = byte_size;
  unique_ast_entry_up->m_is_forward_declaration = attrs.is_forward_declaration;
  dwarf->GetUniqueDWARFASTTypeMap().Insert(unique_typename,
                                           *unique_ast_entry_up);

  // Leave this as a forward declaration until we need to know the
  // details of the type. lldb_private::Type will automatically call
  // the SymbolFile virtual function
  // "SymbolFileDWARF::CompleteType(Type *)" When the definition
  // needs to be defined.
  bool inserted =
      dwarf->GetForwardDeclCompilerTypeToDIE()
          .try_emplace(
              clang_type.GetOpaqueQualType(),
              *die.GetDIERef())
          .second;
  assert(inserted && "Type already in the forward declaration map!");
  (void)inserted;

  return type_sp;
}

lldb::TypeSP
DWARFASTParserLite::ParseTypeModifier(const SymbolContext &sc,
                                       const DWARFDIE &die,
                                       ParsedDWARFTypeAttributes &attrs) {
  Log *log = GetLog(DWARFLog::TypeCompletion | DWARFLog::Lookups);
  SymbolFileDWARF *dwarf = die.GetDWARF();
  const dw_tag_t tag = die.Tag();
  lldb::LanguageType cu_language = SymbolFileDWARF::GetLanguage(*die.GetCU());
  Type::ResolveState resolve_state = Type::ResolveState::Unresolved;
  Type::EncodingDataType encoding_data_type = Type::eEncodingIsUID;
  lldb::TypeSP type_sp;
  CompilerType clang_type;

  if (tag == DW_TAG_typedef || tag == DW_TAG_template_alias) {
    llvm_unreachable("Not implemented yet.");
  }

  switch (tag) {
  default:
    llvm_unreachable("Not implemented yet.");
    break;

  case DW_TAG_base_type: {
    resolve_state = Type::ResolveState::Full;
    // If a builtin type's size isn't a multiple of a byte, DWARF producers may
    // add a precise bit-size to the type. Use the most precise bit-size
    // possible.
    const uint64_t bit_size = attrs.data_bit_size
                                  ? *attrs.data_bit_size
                                  : attrs.byte_size.value_or(0) * 8;
    clang_type = m_ast.GetBuiltinTypeForDWARFEncodingAndBitSize(
        attrs.name.GetStringRef(), attrs.encoding, bit_size);
    break;
  }
  case DW_TAG_pointer_type:
    encoding_data_type = Type::eEncodingIsPointerUID;
    break;
  case DW_TAG_reference_type:
    encoding_data_type = Type::eEncodingIsLValueReferenceUID;
    break;
  case DW_TAG_const_type:
    encoding_data_type = Type::eEncodingIsConstUID;
    break;
  }

  return dwarf->MakeType(die.GetID(), attrs.name, attrs.byte_size, nullptr,
                         attrs.type.Reference().GetID(), encoding_data_type,
                         &attrs.decl, clang_type, resolve_state, {});
}
