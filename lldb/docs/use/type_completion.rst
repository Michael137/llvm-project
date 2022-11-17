LLDB Type Completion
====================

.. contents::
  :local:

Introduction
------------

``ExternalASTSource`` is the interface into which AST producers can hook
and which Clang will consult when it deems necessary (when exactly clang calls back into
the source is not well-defined). ``ExternalASTSource`` maintains a generation count
which is be used to signal whether new redeclarations for a decl have become available.
Crucially the interface declares following methods:

* ``LazyPtr``: this is a mechanism for storing pointers to decls which
             get retrieved from the external source only on use. E.g.,
             redeclarations are LazyPtrs.
* ``FindExternalVisibleDeclsByName``
* ``FindExternalLexicalDecls``
* ``CompleteType``: Gives external AST source opportunity to complete a type.
                  However, this is not consistently called and there is no
                  mechanism for LLDB to control when clang calls it.
                  Gets called by clang::Sema::RequireCompleteType. More
                  often than not this gets called from within LLDB's
                  ClangExpressionDeclMap component and not clang::Sema
* ``CompleteRedeclChain``: Gives external AST source opportunity to complete
                         redeclaration chain for a declaration. Called
                         whenever "the most recent declaration" (!) is
                         requested after a generation count incerement.
                         Note how via the generation count LLDB has
                         better control over when this API is called.
                         Called by clang via LazyGenerationalUpdatePtr.
* ``layoutRecordType``: Clang uses this to give LLDB a chance to correct
                      type layout information

Below is a table that helps understand how the language-independent LLDB abstractions
map to types in libClang:

+---------------------+-------------+
| LLDB                | Clang       |
+=====================+=============+
| CompilerDecl        | Decl        |
+---------------------+-------------+
| CompilerDeclContext | DeclContext |
+---------------------+-------------+
| CompilerType        | (Qual)Type  |
+---------------------+-------------+
| TypeSystemClang     | ASTContext  |
+---------------------+-------------+

An important distinction between the ``CompleteType`` APIs is that they operate
on either:

1. A ``TagDecl``: this API is dictated by Clang
2. An ``lldb_private::CompilerType``: this allows LLDB to work with more than just ``TagDecl``s via the ``ASTImporter``

``DWARFASTParserClang`` operates on ``CompilerType``'s (which are opaque types that are
language independent) and whenever it needs to operate on ``Decl``s it asks ``TypeSystemClang``
(via ``m_ast``) to do it. ``DWARFASTParserClang``'s usage of ``CompilerType`` is necessitated
by the ``DWARFASTParser`` interface and, more generally, the ``SymbolFile`` plugin, both of
which are supposed to be language-independent. The ``ClangASTImporter`` is designed to operate
on both Clang and LLDB types (e.g., ``TagDecl``, ``QualType``, ``CompilerType``, etc.).

Additionally, ``SymbolFileDWARF`` and ``DWARFASTParserClang`` work with types backed by ``DWARFDIE``s.
This is usually where the ``DWARFASTParserClang::ParseXXX`` come into play, i.e., we have a
type encoded in DWARF backed by a ``DWARFDIE`` and we want to create an AST decl from it.

``lldb_private::Type`` represents a type in the eyes of ``SymbolFileDWARF``; it contains information
about which symbol file this type is declared in but also what ``CompilerType`` it corresponds to and
what extent it has been resolved. The ``CompilerType`` on a ``Type`` is resolved lazily via, e.g., ``GetFullCompilerType``
or ``GetForwardCompilerType``.  A ``Type`` has an associated ``lldb::user_id_t`` (see ``Type::m_encoding_uid``) which,
if valid, will get resolved into an "encoding type" (of type ``Type``) via ``SymbolFileDWARF::ResolveTypeUID``; this
triggers the actual parsing of a DWARF file. The backing ``CompilerType``, is then constructed from
the associated ``m_encoding_type`` by adding the appropriate type modifiers and completing it if
necessary. (Note: why the indirection via ``m_encoding_type``?)

``SymbolFileDWARF`` stores a mapping from DIE to ``lldb_private::Type``
in a map (see ``SymbolFileDWARF::m_die_to_type``). Such types have three states:
1. ``nullptr``: Type hasn't been parsed/resolved yet
2. ``DIE_IS_BEING_PARSED``: Type is in the process of being parsed already
3. Otherwise: Type has been parsed and cached in ``m_die_to_type``

When is a type complete?
------------------------
In LLDB this is dictated via ``TypeSystemClang::IsTypeComplete``; this API will
itself attempt to complete the type if it hasn't been yet. According to the source
comments around this API, a type is considered complete (from the perspective of LLDB)
when it has a definition and layout information (e.g., byte-size, etc.). "Has a definition" here
means that all fields possibly queried by LLDB's execution unit or ``clang::Sema`` have
been set; this includes ``DefinitionData`` (for information about decls) and ``TypeInfo`` (for layout information).

From ``clang::Decl``s perspective, a decl is deemed complete when the necessary ``DeclContext::TagDeclBitfields``
are set (i.e., ``TagDeclBitfields::IsCompleteDefinition``).

``clang::Sema``s perspective on whether a type is completed (i.e., ``clang::Sema::isCompleteType``) is more complicated but essentially is based on whether a reachable definition exists for a decl and often falls back on ``clang::Decl``s understanding of a completed type above.

LLDB will fall back on either ``clang::Decl``s or ``clang::Sema``s opinions regarding a type's
completeness (see ``TypeSystemClang::IsTypeComplete``).

Type completion rework
----------------------
Currently LLDB constructs ``CXXRecordDecl``s in multiple steps which aren't compatible with
Clang's way of constructing such decls. For record types we start a definition without completing it in time
("in time" here means ``clang::Sema`` doesn't call ``CompleteType`` consistently before reading out definition data).
Such incomplete definitions are a source of subtle bugs during expression evaluation. LLDB does this because (1)
it doesn't want to complete types if it doesn't absolutely have to, and (2) we don't have a definition available
and pretend to have a complete type to progress with completion of its members.

The main idea behind the refactor is to let LLDB construct a redeclaration chain for a
type (via ``getMostRecentDecl``/``getCanonicalDecl``/``CompleteRedeclChain``), let the ``ASTImporter``
pull in the definition for a type as soon as possible (instead of pretending to have completed a type) 
and when asked to complete a type, look through the declaration chain for the definition (via ``getDefinition``).

In code this means there are no more ``StartTagDeclarationDefinition``/``CompleteTagDeclarationDefinition`` pair mismatches;
instead, every ``StartTagDeclarationDefinition`` is matched with a ``CompleteTagDeclarationDefinition`` within the same function.

When do SBAPIs Trigger Type Completion?
---------------------------------------

TBD

Decl Origin Tracking and AST Kinds
----------------------------------

``ClangASTImporter`` is the glue between LLDB and ``clang::ASTImporter`` that tracks information about where decls originate from and handles importing types into the
expression evaluation context. It does so by intercepting ``ASTImporter::Import`` calls through the ``ClangASTImporter::ASTImporterDelegate`` (particularly the
overriden ``ClangASTImporter::ASTImporterDelegate::ImportImpl``). ``ClangASTImporter`` itself manages ``clang::ASTImporter`` instances per ``clang::ASTContext`` and does decl
origin tracking (described below). LLDB maintains several ``clang::ASTContext``s and copies decls between them as needed. E.g., there's an AST for decls parsed out of
DWARF (via ``DWARFASTParserClang``), an AST for decls from Clang modules, etc. LLDB does this to prevent mismatching decls for the same program entity to corrupt the
final ASTContext.

A UserExpression creates a single top-level ``ClangASTImporter``; this importer is then called from components such as ``ClangExpressionDeclMap`` and ``DWARFASTParserClang``
for importing types. The ``ClangASTImporter`` creates a delegate which forwards the import request to an actual ``clang::ASTImporter``. Each source/destination ``clang::ASTContext``
combination gets its own ``ASTImporterDelegate`` which is stored as ``ASTContextMetadata`` in ``ClangASTImporter::m_metadata_map``; this means the same source AST that gets imported
into two separate destination AST will create two delegates. Similarly, importing into the same destination context from two different source ASTs will also create two delegates.

What do we track origins for?
*****************************

In several completion/lookup APIs (e.g., ``ClangASTSource::CompleteTagDecl``), we want
to complete the origin decl before importing, so we have an actual definition to import.
In other words, the main reason we want to track decls is because LLDB performs type completion
lazily. So whenever LLDB feels like completing a type, it needs to be able to find the
decl/ASTContext/definition to import from; this is faciliated by caching the origin alongside
the destination context/decl (see [Origin Tracking Structures](#origin-tracking-structures))

APIs that care about decl origins
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* ``CompleteTagDeclsScope``
* ``ClangASTSource::layoutRecordType``
* ``ClangASTSource::CompleteType``
* ``ClangASTSource::FindExternalLexicalDecls``
* ``ClangASTImporter::CanImport``
* ``ClangASTImporter::Import``
* ``ClangASTImporter::CompleteTagDecl``
* ``ClangASTImporter::CompleteAndFetchChildren``
* ``ClangASTImporter::GetDeclMetadata``
* ``ClangASTImporter::ASTImporterDelegate::ImportImpl``

Minimal Import
**************

By default, all ``ASTImporter`` instances used by LLDB (created by ``ASTImporterDelegate``) import using ``clang::ASTImporter``'s "minimal import" mode.

Minimal import has following effects on the import process:
1. `ASTNodeImporter::ImportDeclContext`: now only imports the DeclContext decl, not necessarily the decls
                                         contained in the context
2. `addDeclToContexts`: calls `addDeclInternal` unconditionally
3. `shouldForceImportDeclContext`: *crucial*, `ImportDefinitionKind::IDK_Default` now
                                   doesn't import any part of a definition in minimal import
                                   case
4. `ImportDefinition`: doesn't call `setCompleteDefinition` in minimal import case
5. `getStructuralEquivalenceKind`: results in a much weaker equivalence check for records with
                                   external lexical storage
6. `VisitRecordDecl`: doesn't call `ImportImplicitMethods` in minimal import case

TypeSystemClang
***************

The ``lldb_private::TypeSystem`` interface specifies APIs to create and
query language independent types (via ``lldb::opaque_compiler_type_t``).
Its only member is a pointer to the ``SymbolFile`` which backs the ``TypeSystem``
instance, which allows the ``TypeSystem`` to get conrete information about
a type from debug-info (e.g., during type completion).

``TypeSystemClang`` implements the ``TypeSystem`` interface for the ``C++`` language plugin.
It owns all objects necessary for parsing and evaluating an expression including a
``clang::ASTContext``, ``clang::FileManager``, ``clang::DiagnosticsEngine``, ``clang::IdentifierTable``,
``DWARFASTParserClang``, etc. Note that, ``TypeSystemClang`` *conditionally* owns the ``ASTContext``; it will only
own the ``ASTContext`` after an explicit call to ``TypeSystemClang::CreateContext``! It also
maintains metadata structures such as ``DeclMetadataMap``/``TypeMetadataMap`` (which keeps track of object-related
information about ``clang::Decl``s/``clang::Type``s), ``CXXRecordDeclAccessMap`` (which keeps track of
a ``CXXRecordDecl``s access specification. Finally, it also keeps a weak pointer to the ``clang::Sema``
which parses and creates the associated ``ASTContext``.

LLDB has two kinds of ``TypeSystemClang``s:

1. ``ScratchTypeSystemClang``
2. ``SpecializedScratchAST``

ScratchTypeSystemClang
~~~~~~~~~~~~~~~~~~~~~~

A target owns a single main scratch AST into which expression evaluation
imports; a target can have further sub-ASTs which are separated from the
main scratch AST, e.g., when refining decl definitions with ones from modules.

A ``ScratchTypeSystemClang`` is a ``TypeSystemClang`` that also owns:

* A ``ClangASTSource`` for type completion
* Set of sub-ASTs (``map<IsolatedASTKind, TypeSystemClang*>``)
* ``ClangPersistentVariables``: a structure keeping track of a target's persistent variables

Used for storing the final result variable.

SpecializedScratchAST
~~~~~~~~~~~~~~~~~~~~~

TODO

AST Import Flow
***************

This section describes how decls are imported into various ASTs.

Who owns ClangASTImporters and TypeSystemClangs (i.e., ASTContexts)
*******************************************************************

``TypeSystemClang`` conditionally owns the ``ASTContext`` it wraps.

AST Sources
-----------

There are several kinds of AST sources (and AST source wrappers) to be aware of:

* ``ClangASTSource``:

  - Implements core lookup interface of ``clang::ExternalASTSource``
  - Crucially, it implements ``FindExternalVisibleDeclsByName``, which ``clang::Sema`` will consistently call during
    name resolution
  - Called into when TODO

* ``ClangExpressionDeclMap``

  - Derives from ``ClangASTSource`` and handles book-keeping for things like persistent variables, Objective-C
    lookup, JIT execution, etc.
  - Called into when TODO

* ``ClangExternalASTSourceCallbacks``

  - Alternative deriver of ``clang::ExternalASTSource`` (the other being ``ClangASTSource``)
  - Default external AST source when creating a new *owning* ``TypeSystemClang`` (e.g., when creating
    default ``TypeSystemClang`` for a language plugin). However, when we start parsing an expression 
    we explicitly install a ``ClangExpressionDeclMap``.
  - Implements ``FindExternalVisibleDeclsByName`` but handles Objective-C only!
  - Mostly keeps ``ExternalASTSource`` as no-ops but implements the completion APIs, e.g., ``CompleteType``,
    which just forward to the completion APIs of the underlying ``TypeSystemClang``
  - Called into when TODO

* ``ClangASTSourceProxy``

  - A wrapper around ``ClangASTSource`` which just forwards to the underlying source
  - Ensures that the underlying ``clang::ASTContext`` (via ``TypeSystemClang``) doesn't own the
    the AST source book-keeping structures. If the ``clang::ASTContext`` lifetime ends, it
    doesn't tear down any of the ``ClangASTSource`` since the installed AST source was the stateless
    proxy
  - Both the ``ClangASTSource`` and the ``ClangExpressionDeclMap`` get installed via this proxy when
    a new source is created. A ``TypeSystemClang`` will not directly own a ``ClangASTSource``, instead it
    owns the proxy only. The proxy is *not* used for ``ClangExternalASTSourceCallbacks`` however; the
    ``TypeSystemClang`` owns an instance of this object (via the underyling ``clang:ASTContext``

* ``SemaSourceWithPriorities``

  - Implements the ``clang::ExternalSemaSource`` interface (which is an ``clang::ExternalASTSource`` that
    can provide information for semantic analysis)
  - TODO

* ``ExternalASTSourceWrapper``

  - TODO

Example Completion Call-chain
-----------------------------

Single Record Type
******************

```
1. ClangExpressionParser::Parse
2. ClangExpressionParser::ParseInternal
3. clang::Sema::CppLookupName
4. CppNamespaceLookup
5. LookupDirect
6. clang::DeclContext::lookup
7. ClangASTSource::ClangASTSourceProxy::FindExternalVisibleDeclsByName(“f”)
8. ClangASTSource::FindExternalVisibleDeclsByName(“f”, decl_ctx)
    1. NameSearchContext context(“f”, decl_ctx)
    2. m_active_lookups.insert(“f”);
    3. ClangExpressionDeclMap::FindExternalVisibleDecls(context)
        1. ClangExpressionDeclMap::FindExternalVisibleDecls(context, lldb::ModuleSP(), namespace_decl);
            1. ClangExpressionDeclMap::LookupLocalVariable
                1. Variable::GetDecl
                    1. Variable::GetType
                        1. SymbolFileType::GetType
                            1. SymbolFileDWARF::ResolveTypeUID
                                1. DWARFDIE::ResolveType
                                    1. SymbolFileDWARF::ResolveType
                                        1. SymbolFileDWARF::GetTypeForDIE
                                            1. SymbolFileDWARF::ParseType
                                                1. DWARFASTParserClang::ParseTypeFromDWARF
                                                    1. DWARFASTParserClang::ParseStructureLikeDIE
                                                        1. TypeSystemClang::CreateRecordType
                                                        2. TypeSystemClang::StartTagDeclarationDefinition
                2. ClangExpressionDeclMap::AddOneVariable
                3. ClangExpressionDeclMap::GetVariableValue
                    1. Type::GetFullCompilerType
                        1. Type::ResolveCompilerType
                            1. SymbolFileDWARF::CompleteType(“struct Foo”)
                                1. CompleteTypeFromDWARF(“struct Foo”)
                                    1. m_ast.SetHasExternalStorage(clang_type)
                                    2. CompleteRecordType
                                        1. ParseChildMembers
                                            1. ParseSingleMember
                                                1. ResolveTypeUID
                                                2. RequireCompleteType(member_clang_type)
                                                3. TypeSystemClang::AddFieldToRecordType(“struct Foo”, field_decl)
                                        2. TypeSystemClang::CompleteTagDeclarationDefinition
                                        3. SetRecordLayout(record_decl, layout_info)
                    2. ClangASTImporter::CopyType(“struct Foo”)
                    3. ClangASTSource::GuardedCopyType(“struct Foo”)
    4. SetExternalVisibleDeclsForName(decl_ctx, “f”, name_decls); <<< Sets StoredDeclsMap
    5. m_active_lookups.erase(uniqued_const_decl_name);
```

Single Nested Record Type
*************************

```
LookupDirect
    clang::DeclContext::lookup
        lldb_private::ClangASTSource::ClangASTSourceProxy::FindExtern
            lldb_private::ClangASTSource::FindExternalVisibleDeclsByName
                lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls
                    lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecl
                        lldb_private::ClangExpressionDeclMap::LookupLocalVariable
                            lldb_private::Variable::GetDecl
                                lldb_private::Variable::GetType
                                    lldb_private::SymbolFileType::GetType
                                        SymbolFileDWARF::ResolveTypeUID
                                            DWARFDIE::ResolveType
                                                SymbolFileDWARF::ResolveType
                                                    SymbolFileDWARF::GetTypeForDIE
                                                        SymbolFileDWARF::ParseType
                                                            DWARFASTParserClang::ParseTypeFromDWARF
                                                                DWARFASTParserClang::ParseStructureLikeDIE
                                                                    CreateRecordType
                                                                    lldb_private::TypeSystemClang::StartTagDeclarationDefinition
                                                                    SetHasExternalStorage(clang_type.GetOpaqueQualType(), true)
                             lldb_private::ClangExpressionDeclMap::AddOneVariable
                                 lldb_private::ClangExpressionDeclMap::GetVariableValue
                                     lldb_private::Type::GetFullCompilerType
                                         lldb_private::Type::ResolveCompilerType
                                             SymbolFileDWARF::CompleteType
                                                 DWARFASTParserClang::CompleteTypeFromDWARF(“struct Foo”)
                                                     DWARFASTParserClang::CompleteRecordType(“struct Foo”)
                                                         DWARFASTParserClang::ParseChildMembers
                                                             DWARFASTParserClang::ParseSingleMember
                                                                 DWARFDIE::ResolveTypeUID
                                                                     SymbolFileDWARF::ResolveTypeUID
                                                                         SymbolFileDWARF::ResolveType
                                                                             SymbolFileDWARF::GetTypeForDIE
                                                                                 SymbolFileDWARF::ParseType
                                                                                     DWARFASTParserClang::ParseTypeFromDWARF
                                                                                        	DWARFASTParserClang::ParseStructureLikeDIE(“struct Bar”)
                                                                                         	    CreateRecordType
                                                                                         	    lldb_private::TypeSystemClang::StartTagDeclarationDefinition
                                                                                         	    SetHasExternalStorage(clang_type.GetOpaqueQualType(), true)
                                                                 lldb_private::Type::GetLayoutCompilerType
                                                                     lldb_private::Type::ResolveCompilerType
                                                                         SymbolFileDWARF::CompleteType(“strut Bar”)
                                                                             SymbolFileDWARF::CompleteType
                                                                                 DWARFASTParserClang::CompleteTypeFromDWARF
                                                                                     DWARFASTParserClang::CompleteRecordType
                                                                                         TypeSystemClang::CompleteTagDeclarationDefinition
                                                                                             cxx_record_decl->setHasLoadedFieldsFromExternalStorage(true);
                                                                                             cxx_record_decl->setHasExternalLexicalStorage(false);
                                                                                             cxx_record_decl->setHasExternalVisibleStorage(false);
                                                                                         GetClangASTImporter().SetRecordLayout(record_decl, layout_info);
                                                                 RequireCompleteType(“struct Foo::Bar”)
                                                                 TypeSystemClang::AddFieldToRecordType(“struct Foo”, “bar”, “struct Foo::Bar”)
                                                         TypeSystemClang::CompleteTagDeclarationDefinition
                                                             cxx_record_decl->setHasLoadedFieldsFromExternalStorage(true);
                                                             cxx_record_decl->setHasExternalLexicalStorage(false);
                                                             cxx_record_decl->setHasExternalVisibleStorage(false);
                                                         GetClangASTImporter().SetRecordLayout(record_decl, layout_info);
                                     lldb_private::ClangASTSource::GuardedCopyType
                                         lldb_private::ClangASTImporter::CopyType
                                             clang::ASTImporter::Import                        
                                                 clang::ASTNodeImporter::VisitRecordType
                                                     clang::ASTImporter::Import
                                                         lldb_private::ClangASTImporter::ASTImporterDelegate::ImportImpl
                                                             clang::ASTImporter::ImportImpl
                                                                 clang::ASTNodeImporter::VisitRecordDecl
                                     ClangExpressionDeclMap::AddExpressionVariable
                                         ClangASTSource::CompleteType
```

Single Derived Record Type
**************************

```
clang::Sema::BuildUsingDeclaration
    clang::Sema::LookupName
        clang::Sema::CppLookupName
            CppNamespaceLookup
                LookupDirect
                    clang::DeclContext::lookup
                        lldb_private::ClangASTSource::ClangASTSourceProxy::FindExternalVisibleDeclsByNam
                            lldb_private::ClangASTSource::FindExternalVisibleDeclsByName
                                lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls
                                    lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls
                                        lldb_private::ClangExpressionDeclMap::LookupLocalVariable(“f”)
                                            lldb_private::Variable::GetDecl
                                                lldb_private::Variable::GetType(“struct Foo”)
                                                    lldb_private::SymbolFileType::GetType
                                                        SymbolFileDWARF::ResolveTypeUID
                                                            DWARFDIE::ResolveType
                                                                SymbolFileDWARF::ResolveType
                                                                    SymbolFileDWARF::GetTypeForDIE
                                                                        SymbolFileDWARF::ParseType
                                                                            DWARFASTParserClang::ParseTypeFromDWARF
                                                                                DWARFASTParserClang::ParseStructureLikeDIE
                                                                                    lldb_private::TypeSystemClang::CreateRecordType(“struct Foo”)
                                                                                        LinkDeclContextToDIE
                                                                                        GetUniqueDWARFASTTypeMap().Insert(Type(Type::RsolveState::Forward))
                                                                                        TypeSystemClang::StartTagDeclarationDefinition
                                                                                        m_ast.SetHasExternalStorage
                                            m_ast.CreateVariableDeclaration(“f”)
                                            lldb_private::ClangExpressionDeclMap::AddOneVariable(“f”)
                                                lldb_private::ClangExpressionDeclMap::GetVariableValue
                                                    lldb_private::Type::GetFullCompilerType
                                                        lldb_private::Type::ResolveCompilerType
                                                            SymbolFileDWARF::CompleteType
                                                                auto die_it = GetForwardDeclClangTypeToDie
                                                                GetDIEToType().lookup(dwarf_die.GetDIE());
                                                                CompleteTypeFromDWARF
                                                                    DWARFASTParserClang::CompleteRecordType(“struct Foo”)
                                                                        DWARFASTParserClang::ParseChildMember
                                                                            DWARFASTParserClang::ParseInheritance(“struct Foo”)
                                                                                DWARFDIE::ResolveTypeUID
                                                                                    SymbolFileDWARF::ResolveTypeUID(“struct Bar”)
                                                                                        SymbolFileDWARF::ResolveType(“struct Bar”)
                                                                                            SymbolFileDWARF::GetTypeForDIE
                                                                                                SymbolFileDWARF::ParseType
                                                                                                    DWARFASTParserClang::ParseTypeFromDWARF
                                                                                                        DWARFASTParserClang::ParseStructureLikeDIE
                                                                                                            lldb_private::TypeSystemClang::CreateRecordType(“struct Foo”)
                                                                                                                LinkDeclContextToDIE
                                                                                                                GetUniqueDWARFASTTypeMap().Insert(Type(Type::RsolveState::Forward))
                                                                                                                TypeSystemClang::StartTagDeclarationDefinition
                                                                                                                m_ast.SetHasExternalStorage(true)
                                                                                CompilerType base_class_clang_type = base_class_type->GetFullCompilerType();
                                                                                    lldb_private::Type::ResolveCompilerType(“struct Bar”)
                                                                                        SymbolFileDWARF::CompleteType(“struct Bar”)
                                                                                            DWARFASTParserClang::CompleteTypeFromDWARF
                                                                                                DWARFASTParserClang::CompleteRecordType
                                                                                                    DWARFASTParserClang::ParseChildMembers
                                                                                                        DWARFASTParserClang::ParseSingleMember
                                                                                                            DWARFDIE::ResolveTypeUID
                                                                                                                SymbolFileDWARF::ResolveTypeUID
                                                                                                                    SymbolFileDWARF::ResolveType
                                                                                                    CompleteTagDeclarationDefinition(“struct Bar”)
                                                                                                    GetClangASTImporter().SetRecordLayout(record_decl, layout_info)
                                                                        CompleteTagDeclarationDefinition(“struct Foo”)
                                                                        GetClangASTImporter().SetRecordLayout(record_decl, layout_info)
                                             lldb_private::ClangExpressionDeclMap::GetVariableValue
                                                 lldb_private::ClangASTSource::GuardedCopyType
                                                     lldb_private::ClangASTImporter::CopyType
                                                         clang::ASTImporter::Import
                                                             clang::ASTNodeImporter::VisitRecordType
                                                                 clang::ASTImporter::Import
                                                                     lldb_private::ClangASTImporter::ASTImporterDelegate::ImportImp
                                                                         clang::ASTImporter::ImportImpl
                                                                             clang::ASTNodeImporter::VisitRecordDecl(“struct Foo”)
                                                                                 clang::ASTNodeImporter::ImportDefinition
                                                                                     clang::CXXRecordDecl::setBases
                                                                                         clang::RecordDecl::field_empty
                                                                                             clang::RecordDecl::field_begin
                                                                                                 clang::RecordDecl::LoadFieldsFromExternalStorage
                                                                                                     lldb_private::ClangASTSource::ClangASTSourceProxy::FindExternalLexicalDecls
                                                                                                         lldb_private::ClangASTSource::FindExternalLexicalDecls
                                                                                                             lldb_private::ClangExternalASTSourceCallbacks::CompleteType(“struct Bar”)
IRForTarget::runOnModule
    IRForTarget::CreateResultVariable
        lldb_private::ClangExpressionDeclMap::AddPersistentVariable
            lldb_private::ClangExpressionDeclMap::DeportType
                lldb_private::ClangASTImporter::DeportType
                    lldb_private::ClangASTImporter::CopyType
                        clang::ASTImporter::Import
                            clang::ASTImporter::Import
                                clang::TypeVisitor<clang::ASTNodeImporter, llvm::Expected<clang::QualType>
                                    clang::ASTNodeImporter::VisitRecordType
                                        clang::ASTImporter::Import
                                            lldb_private::ClangASTImporter::ASTImporterDelegate::ImportImpl
                                                lldb_private::ClangASTImporter::CopyDecl
                                                    clang::ASTImporter::Import
                                                        lldb_private::ClangASTImporter::ASTImporterDelegate::ImportImpl
                                                            clang::ASTImporter::ImportImpl
                                                                clang::ASTNodeImporter::VisitRecordDecl
                                                                    clang::ASTNodeImporter::ImportDefinition
                                                                        clang::CXXRecordDecl::setBases
                                                                            clang::CXXRecordDecl::addedClassSubobject
                                                                                clang::CXXRecordDecl::hasConstexprDestructor
                                                                                    clang::CXXRecordDecl::getDestructor
                                                                                        clang::DeclContext::lookup
                                                                                            clang::DeclContext::buildLookup
                                                                                                clang::DeclContext::LoadLexicalDeclsFromExternalStorage
                                                                                                    clang::ExternalASTSource::FindExternalLexicalDecls
                                                                                                         lldb_private::ClangASTSource::ClangASTSourceProxy::FindExternalLexicalDecls
```

Pointer to Record Type
**********************

gmodules
********

```
 clang::Sema::LookupName
     clang::Sema::CppLookupName
         CppNamespaceLookup
             LookupDirect
                 clang::DeclContext::lookup
                     lldb_private::ClangASTSource::ClangASTSourceProxy::FindExternalVisibleDeclsByName
                         lldb_private::ClangASTSource::FindExternalVisibleDeclsByName
                             lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls
                                 lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls
                                     lldb_private::ClangExpressionDeclMap::LookupLocalVariable
                                         lldb_private::Variable::GetDecl
                                             lldb_private::Variable::GetType
                                                 lldb_private::SymbolFileType::GetType
                                                     SymbolFileDWARF::ResolveTypeUID
                                                         DWARFDIE::ResolveType
                                                             SymbolFileDWARF::ResolveType
                                                                 SymbolFileDWARF::GetTypeForDIE
                                                                     SymbolFileDWARF::ParseType
                                                                         DWARFASTParserClang::ParseTypeFromDWARF
                                                                             DWARFASTParserClang::ParseStructureLikeDIE(“ClassInMod1”)
                                                                                 DWARFASTParserClang::ParseTypeFromClangModule
                                                                                     SymbolFileDWARF::FindTypes
                                                                                         lldb_private::AppleDWARFIndex::GetTypes
                                                                                             DWARFMappedHash::MemoryTable::FindByName
                                                                                                 DWARFMappedHash::ExtractDIEArray
                                                                                                     SymbolFileDWARF::FindTypes
                                                                                                         SymbolFileDWARF::ResolveType
                                                                                                             SymbolFileDWARF::GetTypeForDIE
                                                                                                                 SymbolFileDWARF::ParseType
                                                                                                                     DWARFASTParserClang::ParseTypeFromDWARF
                                                                                                                         DWARFASTParserClang::ParseStructureLikeDIE(“ClassInMod1”)
                                                                                                                             lldb_private::TypeSystemClang::StartTagDeclarationDefinition
                                                                                                                             dwarf->GetForwardDeclClangTypeToDie().try_emplace(clang_type)
                                                                                                                             m_ast.SetHasExternalStorage(clang_type.GetOpaqueQualType(), true);
                                                                                     lldb_private::ClangASTImporter::CopyType
                                                                                         clang::ASTImporter::Import
                                                                                              clang::ASTImporter::Import
                                                                                                  clang::ASTNodeImporter::VisitRecordType
                                                                                                      clang::ASTImporter::Import
                                                                                                          lldb_private::ClangASTImporter::ASTImporterDelegate::ImportImpl
                                                                                                              clang::ASTImporter::ImportImpl
                                                                                                                  clang::ASTNodeImporter::VisitRecordDecl
                                         lldb_private::ClangExpressionDeclMap::AddOneVariable
                                             lldb_private::ClangExpressionDeclMap::GetVariableValue
                                                 lldb_private::Type::GetFullCompilerType
                                                     lldb_private::Type::ResolveCompilerType
                                                         SymbolFileDWARF::CompleteType(“ClassInMod1”)
                                                            lldb_private::ClangASTImporter::CompleteType(“ClassInMod1”)
                                                                lldb_private::ClangASTImporter::Import
                                                                    lldb_private::ClangASTImporter::CompleteAndFetchChildren
                                                                        lldb_private::ClangASTImporter::RequireCompleteType
                                                                            lldb_private::ClangASTImporter::CompleteTagDecl
                                                                                lldb_private::TypeSystemClang::GetCompleteDecl
                                                                                    lldb_private::ClangExternalASTSourceCallbacks::CompleteType
                                                                                        lldb_private::TypeSystemClang::CompleteTagDecl
                                                                                            SymbolFileDWARF::CompleteType("ClassInMod1”)
                                                                                                DWARFASTParserClang::CompleteTypeFromDWARF
                                                                                                    DWARFASTParserClang::CompleteRecordType("ClassInMod1”)
                                                                                                        DWARFASTParserClang::ParseChildMembers
                                                                                                            DWARFASTParserClang::ParseSingleMember
                                                                                                                DWARFDIE::ResolveTypeUID
                                                                                                                    SymbolFileDWARF::ResolveTypeUID
                                                                                                                        SymbolFileDWARF::ResolveType
                                                                                                                            SymbolFileDWARF::GetTypeForDIE
                                                                                                                                SymbolFileDWARF::ParseType
                                                                                                                                    DWARFASTParserClang::ParseTypeFromDWARF("ClassInMod2”)
                                                                                                                                        DWARFASTParserClang::ParseStructureLikeDIE("ClassInMod2”)
                                                                                                                                            DWARFASTParserClang::ParseTypeFromClangModule("ClassInMod2”)
                                                                                                                                                SymbolFileDWARF::FindTypes
                                                                                                                                                    lldb_private::AppleDWARFIndex::GetTypes
                                                                                                                                                        DWARFMappedHash::MemoryTable::FindByName("ClassInMod2”)
                                                                                                                                                            DWARFMappedHash::ExtractDIEArray
                                                                                                                                                                SymbolFileDWARF::FindTypes
                                                                                                                                                                    SymbolFileDWARF::ResolveType
                                                                                                                                                                        SymbolFileDWARF::GetTypeForDIE
                                                                                                                                                                            SymbolFileDWARF::ParseType
                                                                                                                                                                                DWARFASTParserClang::ParseTypeFromDWARF("ClassInMod2”)
                                                                                                                                                                                    DWARFASTParserClang::ParseStructureLikeDIE("ClassInMod2”)
                                                                                                                                                                                        lldb_private::TypeSystemClang::StartTagDeclarationDefinition
                                                                                                                                                                                        dwarf->GetForwardDeclClangTypeToDie().try_emplace(clang_type)
                                                                                                                                                                                        m_ast.SetHasExternalStorage(clang_type.GetOpaqueQualType(), true);
                                                                                                                                                lldb_private::ClangASTImporter::CopyType(“ClassInMod2”)
                                                                                                                                                    clang::ASTImporter::Import
                                                                                                                                                        clang::ASTImporter::Import
                                                                                                                                                            clang::ASTNodeImporter::VisitRecordType
                                                                                                                                                                clang::ASTImporter::Import
                                                                                                                                                                    lldb_private::ClangASTImporter::ASTImporterDelegate::ImportImpl
                                                                                                                                                                        clang::ASTImporter::ImportImpl
                                                                                                                                                                            clang::ASTNodeImporter::VisitRecordDecl
                                                                                                                lldb_private::Type::GetLayoutCompilerType(“ClassInMod2”)
                                                                                                                    lldb_private::Type::ResolveCompilerType(“ClassInMod2”)
                                                                                                                        SymbolFileDWARF::CompleteType(“ClassInMod2”)
                                                                                                                            lldb_private::ClangASTImporter::CompleteType(“ClassInMod2”)
                                                                                                                                lldb_private::ClangASTImporter::Import
                                                                                                                                    lldb_private::ClangASTImporter::CompleteAndFetchChildren
                                                                                                                                        lldb_private::ClangASTImporter::RequireCompleteType
                                                                                                                                            lldb_private::ClangASTImporter::CompleteTagDecl
                                                                                                                                                lldb_private::TypeSystemClang::GetCompleteDecl
                                                                                                                                                    lldb_private::ClangExternalASTSourceCallbacks::CompleteType
                                                                                                                                                        lldb_private::TypeSystemClang::CompleteTagDecl
                                                                                                                                                            SymbolFileDWARF::CompleteType
                                                                                                                                                                DWARFASTParserClang::CompleteTypeFromDWARF
                                                                                                                                                                    DWARFASTParserClang::CompleteRecordType
                                                                                                                                                                        TypeSystemClang::CompleteTagDeclarationDefinition
                                                                                                        lldb_private::TypeSystemClang::CompleteTagDeclarationDefinition(“ClassInMod1”)
                                                                                lldb_private::ClangASTImporter::ASTImporterDelegate::ImportDefinitionTo(“ClassInMod1”)
                                                                                    clang::ASTImporter::ImportDefinition
                                                                                        clang::ASTNodeImporter::ImportDefinition
                                                                                            clang::ASTNodeImporter::ImportDeclContext
                                                                                                clang::ASTImporter::Import(“ClassInMod2”)
                                                                                                    lldb_private::ClangASTImporter::ASTImporterDelegate::ImportImpl
                                                                                                        clang::ASTImporter::ImportImpl
                                                                                                            clang::ASTNodeImporter::VisitFieldDecl
                                                                                                                clang::ASTNodeImporter::import<clang::QualType>
                                                                                                                    clang::ASTImporter::Import
                                                                                                                        clang::ASTImporter::Import
                                                                                                                            clang::ASTNodeImporter::VisitRecordType
                                                                                                                                clang::ASTImporter::Import
                                                                                                                                    lldb_private::ClangASTImporter::ASTImporterDelegate::ImportImpl
                                                                                                                                        clang::ASTImporter::ImportImpl
                                                                                                                                            clang::ASTNodeImporter::VisitRecordDecl(“ClassInMod2”)
                                                                                                                clang::DeclContext::addDeclInternal
                                                                                                                    clang::DeclContext::addHiddenDecl
                                                                                                                        clang::CXXRecordDecl::addedMember
                                                                                                                            clang::CXXRecordDecl::addedClassSubobject
                                                                                                                                clang::CXXRecordDecl::hasConstexprDestructor
                                                                                                                                    clang::CXXRecordDecl::getDestructor
                                                                                                                                        clang::DeclContext::lookup
                                                                                                                                            clang::DeclContext::buildLookup
                                                                                                                                                clang::DeclContext::LoadLexicalDeclsFromExternalStorage
                                                                                                                                                    clang::ExternalASTSource::FindExternalLexicalDecls
                                                                                                                                                        lldb_private::ClangExternalASTSourceCallbacks::CompleteType(“ClassInMod2”)
                                                                                                                                                            lldb_private::TypeSystemClang::CompleteTagDecl
                                                                                                                                                                lldb_private::ClangASTImporter::CompleteType
                                                                                                                                                                    lldb_private::TypeSystemClang::CompleteTagDeclarationDefinition(“ClassInMod2”)
                                                                lldb_private::TypeSystemClang::CompleteTagDeclarationDefinition(“ClassInMod1”)
```

Example Minimal Import
----------------------

Glossary
--------

* Lines marked with *<<<* are targets for the type completion refactor (see [D101950](https://reviews.llvm.org/D101950)
  and rdar://75170305

Completion APIs
---------------

clang::ExternalASTSource
************************

* ``virtual void clang::ExternalASTSource::CompleteType(TagDecl *Tag);``

  - noop by default
  - "Give opportunity for external source to complete type"
  - Called from ``clang::Sema::RequireCompleteType``
    More often than not this gets called from within LLDB's
    ``ClangExpressionDeclMap`` component and not ``clang::Sema``

SymbolFileDWARF
***************

* ``virtual bool SymbolFileDWARF::CompleteType(CompilerType &compiler_type) override;``

  - Called from ``TypeSystemClang::CompleteTagDecl``
  - Performs following steps:

    1. Complete type via ``ClangASTImporter::CompleteType`` if enum or record (!) and the decl has a known origin
    2. If not possible, use ``DWARFASTParserClang::CompleteTypeFromDWARF`` 

      - Uses ``m_forward_decl_clang_type_to_die`` to determine whether a type has
        already been resolved. (!)

* ``Type *SymbolFileDWARF::ResolveType(const DWARFDIE &die, bool, bool)``

  - Called from various places in ``Plugins/SymbolFile`` whenever we need to
    complete a type represented by a ``DWARFDIE``
  - Calls ``SymbolFileDWARF::ParseType``

* ``TypeSP SymbolFileDWARF::ParseType(const SymbolContext &sc, const DWARFDIE &die, bool *type_is_new_ptr)``

  - Calls ``ParseTypeFromDWARF`` and ``ParseTypes``
  - Called from ``ResolveType``
  - Following steps:

    1. ``ParseTypeFromDWARF``
    2. Adds parsed type into SymbolContext ``TypeList``
    3. If the specified die has a ``DW_TAG_subprogram`` then insert parsed type into
       ``m_function_scope_qualified_name_map``

lldb_private::ClangASTSource
****************************

* ``void ClangASTSource::CompleteType(TagDecl *tag_decl)``

  - Calls ``CompleteTagDecl``
  - Falls back to ``FindCompleteType``+``CompleteTagDeclWithOrigin``
  - Called from ``ClangExpressionDeclMap``, ``ASTImporter``, ``RecordLayoutBuilder``, ``TypeSystemClang``,
    ``clang::Sema::RequireCompleteType``, ``ClangASTSource`` itself

lldb_private::ClangASTSource::ClangASTSourceProxy
*************************************************

* ``void ClangASTSourceProxy::CompleteType(clang::TagDecl *Tag) override``

  - Forwards to to ``ClangASTSource::CompleteType``

lldb_private::ClangExternalASTSourceCallbacks
*********************************************

* ``void ClangExternalASTSourceCallbacks::CompleteType(clang::TagDecl *tag_decl)``

  - Calls ``TypeSystemClang::CompleteTagDecl``

lldb_private::ClangASTImporter
******************************

* ``bool ClangASTImporter::CompleteType(const CompilerType &compiler_type)``

  - Called from ``SymbolFileDWARF::CompleteType``
  - Checks whether type is an enum or record type (via ``CanImport``). If so,
    calls ``ClangASTImporter::Import`` and on success with call ``CompleteTagDeclarationDefinition``
  - Calls ``SetHasExternalStorage(false)`` on failure (TODO: why?)

* ``bool ClangASTImporter::CompleteTagDecl(clang::TagDecl *decl)``

  - Will use ``ASTImporter::ImportDefinition``
  - Called from ``ClangASTSource::CompleteType``, ``ClangASTImporter::RequireCompleteType``
    and ``ClangASTImporter.cpp:MaybCompleteReturnType``

* ``bool ClangASTImporter::CompleteTagDeclWithOrigin(clang::TagDecl *decl, clang::TagDecl *origin_decl)``

  - Called from ``ClangASTSource`` as a fall-back for when the regular ``CompleteType`` fails.
    In such ases we try to find an alternate definition somewhere which could allow us to
    complete the decl. The alternate definition is looked up via ``FindCompleteType``
  - Uses ``TypeSystemClang::GetCompleteDecl`` and ``ASTImporter::ImportDefinition`` for
    type completion.

* ``bool ClangASTImporter::RequireCompleteType(clang::QualType type)``

  - Tries to find definition for type (including in redeclaration chain, via ``TagDecl::getDefinition``
  - If definition hasn't been pulled into the ``TagDecl`` (or it's redecl chain) yet, then
    try to find and import definition ``ClangASTImporter::CompleteTagDecl``

lldb_private::ExternalASTSourceWrapper
**************************************

* ``void ExternalASTSourceWrapper::CompleteType(clang::TagDecl *Tag) override``

lldb_private::TypeSystemClang
*****************************

* ``void TypeSystemClang::CompleteTagDecl(clang::TagDecl *decl)``

  - Callers ask ``TypeSystem`` plugin to complete a ``TagDecl`` (why only ``TagDecl``)?
  - Calls ``CompleteType`` on current symbolfile (which calls ``ClangASTImporter::CompleteType``
    and ``DWARFASTParserClang::CompleteTypeFromDWARF``
  - Called via ``ClangExternalASTSourceCallbacks``

* ``bool TypeSystemClang::GetCompleteType(lldb::opaque_compiler_type_t type)``

* ``bool GetCompleteQualType(clang::ASTContext *ast, clang::QualType qual_type, bool allow_completion = true``

* ``bool GetCompleteDecl(clang::Decl *decl)``

* ``bool TypeSystemClang::StartTagDeclaration(const CompilerType &type)``

  - Used to build definition for a ``clang::TagDecl``
  - Calls ``TagType::getDecl`` (which will walk redecl chain to find definition)
  - Then calls ``TagDecl::startDefinition``
  - Called from:

    - ``CreateStructForIdentifier`` (which is used throughout LLDB's formatting component)
    - ``ParseEnumType``, ``CompleteEnumType``, ``ParseStructureLikeDIE``, ``ForcefullyCompleteType``

* ``bool TypeSystemClang::CompletedTagDefinition(const CompilerType& type)``

  - Used to finalize the definition of a ``clang::TagDecl``
  - If the tagdecl definition bits haven't been set yet (via ``TagDecl::setCompleteDefinition``)
    then will call ``CXXRecordDecl::completeDefinition`` (which calls ``RecordDecl::completeDefinition``/``TagDecl::completeDefinition``)
    to set said bits and account for any C++ method overrides
  - Called from:

    - ``CreateStructForIdentifier``
    - ``ClangASTImporter::CompleteType`` after importing a type (!)
    - ``ParseEnumType``, ``CompleteEnumType``, ``CompleteRecordType``,
      ``ParseStructureLikeDIE``, ``ForcefullyCompleteType``
    - Note how this list doesn't exactly match that of ``StartTagDeclaration`` *<<<*

clang::ASTImporter
******************

* ``ASTImporter::CompleteDecl``

  * Called within ASTImporter to fill in definition data for Enum/Objective-C decls
  * For TagDecls (currently just called for Enums) fill in the redeclaration chain
    with definitions from the main TagDecl's DefinitionData. I.e., will allocate and
    copy DefinitionData for all decls in a redeclaration chain

DWARFASTParserClang
*******************

* Reads types from DWARF and completes them by creating decls via ``TypeSystemClang``, exposing them in
  LLDB's AST

* ``bool DWARFASTParserClang::CompleteEnumType(const DWARFDIE &die, lldb_private::Type *type, CompilerType &clang_type)``

  - Parses enumerator children from DWARF and then adds them as EnumConstantDecls
    to the AST under the appropriate EnumType node
  - Calls ``StartTagDeclarationDefinition/CompleteTagDeclarationDefinition``
    which for enums will simply copy DefinitionData from the decl associated
    with the specified ``clang_type`` to all decls in the redeclaration chain
  - Called from ``CompleteTypeFromDWARF`` for enum types
  - What counts as completion?
    - when all it's enum value children have been read from DWARF and exposed in the AST

* ``bool DWARFASTParserClang::CompleteRecordType(const DWARFDIE &die, lldb_private::Type *type, CompilerType &clang_type)``

  - This function expects a definition for ``clang_type`` to have already
    been started (via ``StartTagDeclarationDefinition``)! *<<<*
  - Called from ``CompleteTypeFromDWARF`` for structure/union/class types
  - Following steps:

    1. Parses members of record type from DWARF
    2. Calls ``ResolveType`` for each member
    3. Calls ``RequireCompleteType`` for each base class (NOTE: silently ignores bases for
       which ``getTypeSourceInfo() == nullptr`` while comment claims that leaving base types
       as forward declarations leads to crashes!!)
    4. Add overriden methods to ``clang_type``'s decl
    5. ``BuildIndirectFields``
    6. ``CompleteTagDeclarationDefinition`` (without prior ``StartTagDeclarationDefinition`` in this function!) *<<<*

      - The corresponding ``StartTagDeclarationDefinition`` is most likely started in ``ParseStructureLikeDIE``

    7. ``SetRecordLayout``

* ``CompleteTypeFromDWARF(const DWARFDIE &die, lldb_private::Type *type, CompilerType &clang_type)``

  - Called from ``SymbolFileDWARF::ParseType`` (via ``SymbolFileDWARF::ResolveType``)
  - Following steps:
    1. Set ``DIE_IS_BEING_PARSED`` bit *<<<*
    2. Dispatch to ``ParseXXX`` function based on DIE tag
    3. UpdateSymbolContextScopeForType(parsed_type)

* ``void RequireCompleteType(CompilerType type)``

  - Called whenever C++ rules require a type to be complete
    (e.g., base classes, members, etc.)
  - Tries to force complete a type and if that's not possible
    will mark it as forcefully completed (via ``ForcefullyCompleteType``) *<<<*

* ``void PrepareContextToReceiveMembers(TypeSystemClang &ast, ClangASTImporter &ast_importer, clang::DeclContext *decl_ctx,
                                       DWARFDIE die, const char *type_name_cstr``

  - Similar to ``RequireCompleteType`` but doesn't force complete the type;
    instead this function merely prepares the type to be completed later. *<<<*
  - If the type was imported from an external AST, will pull in definition. Otherwise
    marks type as forcefully completed. *<<<*.
  - The main difference to ``RequireCompleteType`` is that we don't call ``CompleteType``.
  - Called from ``ParseStructureLikeDIE`` (on the declcontext of the parsed DIE) and
    ``ParseTypeModifier`` (for ``DW_TAG_typedef``) since we tend to construct half completed
    records to be able to complete the children

* ``void ForcefullyCompleteType(CompilerType type)``

  - Called from ``RequireCompleteType``
  - Calls ``StartTagDeclarationDefinition/CompleteTagDeclarationDefinition``
  - This function essentially can leave record types with incomplete definitions.
    We allocate but don't fully set a record's ``DefinitionData``. *<<<*
  - Sets ``IsForcefullyCompleted`` flag on ``TypeSystemClang`` metadata
    - This flag is used ... TODO

* ``TypeSP ParseTypeFromDWARF(const SymbolContext &sc, const DWARFDIE &die, bool *type_is_new_ptr)``:

  - If it's the first time that ``DWARFASTParserClang`` sees this DIE, begin parsing:
    1. Set ``DIE_IS_BEING_PARSED`` in ``m_die_to_type`` for the specified 'die'
    2. Parse the DIE's attributes
    3. Based on the DIE's DW_TAG, call the appropriate ``DWARFASTParserClang::ParseXXX`` method
    4. Update ``m_die_to_type``

lldb_private::Type
******************

* ``bool Type::ResolveCompilerType(ResolveState compiler_type_resolve_state)``

  - Responsible for setting the ``CompilerType`` backing this ``Type`` object
  - If the underlying ``CompilerType`` hasn't been resolved yet, resolve the
    type from DWARF via ``SymbolFileDWARF::ResolveTypeUID`` (which calls ``SymbolFileDWARF::ResolveType``)
    as a forward declaration (i.e., don't call ``CompleteType``)
  - If the ``compiler_type_resolve_state`` isn't a ``Forward`` (i.e., the caller didn't request a full
    CompilerType), call ``SymbolFileDWARF::CompleteType``

* ``CompilerType Type::GetFullCompilerType()``

  - Reads type for backing DIE from DWARF if necessary and completes the
    underlying ``CompilerType`` of this objet
  - Calls ``ResolveType(ResolveState::Full)``
  - Notably called from:
    1. ``ClangASTSource::FindCompleteType`` (called from ``ClangASTSource::CompleteType``)
    2. ``ClangExpressionDeclMap`` APIs which copy types into the scratch AST
    3. some ``DWARFASTParserClang::ParseXXX`` APIs before creating nodes in the AST *<<<*

* ``CompilerType Type::GetForwardCompilerType()``

  - Reads type for backing DIE from DWARF if necessary, sets the
    underlying ``CompilerType`` of this object *without* completing it
  - Calls ``ResolveType(ResolveState::Forward)``
  - Called whenever we need information about ``CompilerType`` that doesn't
    a complete type. E.g., getting the type name, encoding.
    More crucially, this is used in the ``gmodules`` support when resolving
    types from ``.pcm`` files (see ``DWARFASTParserClang::ParseTypeFromClangModule``)

lldb_private::CompilerType
**************************

* ``bool GetCompleteType() const``

clang::Sema
***********

* ``Sema::RequireCompleteType``

clang::Decl
***********

* ``XXXDecl *XXXDecl::getDefinition() const``

  - Depending on the kind of decl will return the definition associated with the declaration if available.
    Most interestingly, for ``TagDecl``s (such as classes/enums/unions/structs), ``FunctionDecl``s and ``VarDecl``s
    this will walk through the redeclaration chain to look for a definition, if necessary.

* ``RecordDecl *RecordDecl::getDefinition() const``

* ``CXXRecordDecl *CXXRecordDecl::getDefinition() const``

* ``TagDecl *TagDecl::getDefinition() const``

* ``void TagDecl::startDefinition()``

  - allocates ``CXXRecordDecl::DefinitionData`` and propagates it to all decls on the redecl chain
  - After this function the DefinitionData can be mutated and completed with a call to ``TagDecl::completeDefinition``
  - Used by LLDB to create definitions for decls (see ``StartTagDeclarationDefinition``)

* ``void TagDecl::completeDefinition()``

clang::Type
***********

* ``TagDecl *TagType::getDecl() const``

  - Returns a ``Type``s definition decl if possible (!)
  - Walks through the decls redeclaration chain and returns the definition if found (note, it can return a definition which is in
    progress, i.e., ``isBeingDefined() == true`` (!)). If no definition exists, returns decl associated with the ``Type``. *<<<*
  - Called from various places in ``ClangASTImporter`` and ``TypeSystemClang``. Most notably called
    when completing a ``TagType`` via ``ClangASTImporter::RequireCompleteType`` or ``ClangASTImporter::CompleteAndFetchChildren``
  - *Note*: Does not consult external sources or perform lookups

* ``TagDecl *Type::getAsTagDecl() const``

  - Utility function that forwards to ``TagType::getDecl`` if we're dealing with ``TagType``s. Returns ``nullptr`` otherwise.
  - Used throughout LLDBs expression evaluation components (via ``ClangUtil::GetAsTagDecl``)
  - *Note*: Does not consult external sources or perform lookups

TODO
----
* Single-step through example
  * single variable
    * ~record type~
    * ~nested record type~
    * ~derived record type~
    * reference to record
  * clang module
  * method call
  * member access
  * typedef
  * Templates/~Template specializations
  * namespaces
    * Multiple types in single namespace but print only a single type
* Move documentation of individual APIs to function contracts in source code
* APIs that track origins
* CompleteRedeclChain
* ParseStructureLikeDIE
* ParseTypeFromClangModule
* ExternalVisibleDecls
* ExternalLexicalDecls
* LazyLocalLexicalDecls
* LazyExternalLexicalDecls

* ForgetSource/ForgetDestination
* TypeSystemClang ownership
* TagDecl::completeDefinition
* setCompleteDefinition
* ParseSubroutine
* ParseInheritance
* getASTRecordLayout
* CompleteTagDeclsScope
* ParseSingleMember
* SBModule::FindTypes
* SBTarget::FindTypes
* Module::FindTypes
* SymbolFile::FindTypes
* ClangUtil::GetAsTagDecl
* TypeSystemClang::GetAsTagDecl
* ClangASTImporter::CompleteAndFetchChildren
* GetTypeForDIE
* ClangASTImporter
* Import
* ImportDefinitionTo
* GetLayoutCompilerType
* ImportDeclContext
* CopyDecl
* CopyType
* DeportType
* FindCompleteType
* GetDeclOrigin
* gmodules
* IsTypeComplete
* InjectedClassNameType
* layoutRecordType
* CxxModuleHandler
* ClangModulesDeclVendor
* ClangPersistentVariables

References
----------

- LLDB source
- Phabricator
- Raphael's master's thesis
