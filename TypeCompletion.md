- [Notes](#notes)
- [When is a type complete?](#when-is-a-type-complete-)
- [Type completion rework](#type-completion-rework)
- [When do SBAPIs Trigger Type Completion?](#when-do-sbapis-trigger-type-completion-)
- [Decl Origin Tracking and AST Kinds](#decl-origin-tracking-and-ast-kinds)
  * [What do we track origins for?](#what-do-we-track-origins-for-)
    + [APIs that care about decl origins](#apis-that-care-about-decl-origins)
  * [Minimal Import](#minimal-import)
  * [Origin Tracking Structures](#origin-tracking-structures)
  * [TypeSystemClang](#typesystemclang)
    + [ScratchTypeSystemClang](#scratchtypesystemclang)
    + [SpecializedScratchAST](#specializedscratchast)
  * [AST Import Flow](#ast-import-flow)
  * [Who owns ClangASTImporters and TypeSystemClangs (i.e., ASTContexts)](#who-owns-clangastimporters-and-typesystemclangs--ie--astcontexts-)
- [Example Completion Call-chain](#example-completion-call-chain)
- [Glossary](#glossary)
- [Completion APIs](#completion-apis)
  * [clang::ExternalASTSource](#clang--externalastsource)
  * [SymbolFileDWARF](#symbolfiledwarf)
  * [lldb_private::ClangASTSource](#lldb-private--clangastsource)
  * [lldb_private::ClangASTSource::ClangASTSourceProxy](#lldb-private--clangastsource--clangastsourceproxy)
  * [lldb_private::ClangExternalASTSourceCallbacks](#lldb-private--clangexternalastsourcecallbacks)
  * [lldb_private::ClangASTImporter](#lldb-private--clangastimporter)
  * [lldb_private::ExternalASTSourceWrapper](#lldb-private--externalastsourcewrapper)
  * [lldb_private::TypeSystemClang](#lldb-private--typesystemclang)
  * [clang::ASTImporter](#clang--astimporter)
  * [DWARFASTParserClang](#dwarfastparserclang)
  * [lldb_private::Type](#lldb-private--type)
  * [lldb_private::CompilerType](#lldb-private--compilertype)
  * [clang::Sema](#clang--sema)
  * [clang::Decl](#clang--decl)
  * [clang::Type](#clang--type)
- [TODO](#todo)
- [References](#references)

<small><i><a href='http://ecotrust-canada.github.io/markdown-toc/'>Table of contents generated with markdown-toc</a></i></small>

# Notes

`ExternalASTSource` is the interface into which AST producers can hook
and which Clang will consult when it deems necessary (when exactly clang calls back into
the source is not well-defined). `ExternalASTSource` maintains a generation count
which is be used to signal whether new redeclarations for a decl have become available.
Crucially the interface declares following methods:
* `LazyPtr`: this is a mechanism for storing pointers to decls which
             get retrieved from the external source only on use. E.g.,
             redeclarations are LazyPtrs.
* `FindExternalVisibleDeclsByName`
* `FindExternalLexicalDecls`
* `CompleteType`: Gives external AST source opportunity to complete a type.
                  However, this is not consistently called and there is no
                  mechanism for LLDB to control when clang calls it.
                  Gets called by clang::Sema::RequireCompleteType. More
                  often than not this gets called from within LLDB's
                  ClangExpressionDeclMap component and not clang::Sema
* `CompleteRedeclChain`: Gives external AST source opportunity to complete
                         redeclaration chain for a declaration. Called
                         whenever "the most recent declaration" (!) is
                         requested after a generation count incerement.
                         Note how via the generation count LLDB has
                         better control over when this API is called.
                         Called by clang via LazyGenerationalUpdatePtr.
* `layoutRecordType`: Clang uses this to give LLDB a chance to correct
                      type layout information

Below is a table that helps understand how the language-independent LLDB abstractions
map to types in libClang:

| LLDB                | Clang       |
| ------------------- | ----------- |
| CompilerDecl        | Decl        |
| CompilerDeclContext | DeclContext |
| CompilerType        | (Qual)Type  |
| TypeSystemClang     | ASTContext  |

An important distinction between the `CompleteType` APIs is that they operate
on either:
1. A `TagDecl`: this API is dictated by Clang
2. An `lldb_private::CompilerType`: this allows LLDB to work with more than just `TagDecl`s
                                    via the `ASTImporter`

`DWARFASTParserClang` operates on `CompilerType`'s (which are opaque types that are
language independent) and whenever it needs to operate on `Decl`s it asks `TypeSystemClang`
(via `m_ast`) to do it. `DWARFASTParserClang`'s usage of `CompilerType` is necessitated
by the `DWARFASTParser` interface and, more generally, the `SymbolFile` plugin, both of
which are supposed to be language-independent. The `ClangASTImporter` is designed to operate
on both Clang and LLDB types (e.g., `TagDecl`, `QualType`, `CompilerType`, etc.).

Additionally, `SymbolFileDWARF` and `DWARFASTParserClang` work with types backed by `DWARFDIE`s.
This is usually where the `DWARFASTParserClang::ParseXXX` come into play, i.e., we have a
type encoded in DWARF backed by a `DWARFDIE` and we want to create an AST decl from it.

`lldb_private::Type` represents a type in the eyes of `SymbolFileDWARF`; it contains information
about which symbol file this type is declared in but also what `CompilerType` it corresponds to and
what extent it has been resolved. The `CompilerType` on a `Type` is resolved lazily via, e.g., `GetFullCompilerType`
or `GetForwardCompilerType`.  A `Type` has an associated `lldb::user_id_t` (see `Type::m_encoding_uid`) which,
if valid, will get resolved into an "encoding type" (of type `Type`) via `SymbolFileDWARF::ResolveTypeUID`; this
triggers the actual parsing of a DWARF file. The backing `CompilerType`, is then constructed from
the associated `m_encoding_type` by adding the appropriate type modifiers and completing it if
necessary. (Note: why the indirection via `m_encoding_type`?)

`SymbolFileDWARF` stores a mapping from DIE to `lldb_private::Type`
in a map (see `SymbolFileDWARF::m_die_to_type`). Such types have three states:
1. `nullptr`: Type hasn't been parsed/resolved yet
2. `DIE_IS_BEING_PARSED`: Type is in the process of being parsed already
3. Otherwise: Type has been parsed and cached in `m_die_to_type`

# When is a type complete?
In LLDB this is dictated via `TypeSystemClang::IsTypeComplete`; this API will
itself attempt to complete the type if it hasn't been yet. According to the source
comments around this API, a type is considered complete (from the perspective of LLDB)
when it has a definition and layout information (e.g., byte-size, etc.). "Has a definition" here
means that all fields possibly queried by LLDB's execution unit or `clang::Sema` have
been set; this includes `DefinitionData` (for information about decls) and `TypeInfo` (for layout information).

From `clang::Decl`s perspective, a decl is deemed complete when the necessary `DeclContext::TagDeclBitfields`
are set (i.e., `TagDeclBitfields::IsCompleteDefinition`).

`clang::Sema`s perspective on whether a type is completed (i.e., `clang::Sema::isCompleteType`)
is more complicated but essentially is based on whether a reachable definition exists for a decl
and often falls back on `clang::Decl`s understanding of a completed type above.

LLDB will fall back on either `clang::Decl`s or `clang::Sema`s opinions regarding a type's
completeness (see `TypeSystemClang::IsTypeComplete`).

# Type completion rework
Currently LLDB constructs `CXXRecordDecl`s in multiple steps which aren't compatible with
Clang's way of constructing such decls. For record types we start a definition without completing it in time
("in time" here means `clang::Sema` doesn't call `CompleteType` consistently before reading out definition data).
Such incomplete definitions are a source of subtle bugs during expression evaluation. LLDB does this because (1)
it doesn't want to complete types if it doesn't absolutely have to, and (2) we don't have a definition available
and pretend to have a complete type to progress with completion of its members.

The main idea behind the refactor is to let LLDB construct a redeclaration chain for a
type (via `getMostRecentDecl`/`getCanonicalDecl`/`CompleteRedeclChain`), let the `ASTImporter`
pull in the definition for a type as soon as possible (instead of pretending to have completed a type) 
and when asked to complete a type, look through the declaration chain for the definition (via `getDefinition`).

In code this means there are no more `StartTagDeclarationDefinition`/`CompleteTagDeclarationDefinition` pair mismatches;
instead, every `StartTagDeclarationDefinition` is matched with a `CompleteTagDeclarationDefinition` within the same function.

# When do SBAPIs Trigger Type Completion?

TBD

# Decl Origin Tracking and AST Kinds

`ClangASTImporter` is the glue between LLDB and `clang::ASTImporter` that tracks information about where decls originate from and handles importing types into the
expression evaluation context. It does so by intercepting `ASTImporter::Import` calls through the `ClangASTImporter::ASTImporterDelegate` (particularly the
overriden `ClangASTImporter::ASTImporterDelegate::ImportImpl`). `ClangASTImporter` itself manages `clang::ASTImporter` instances per `clang::ASTContext` and does decl
origin tracking (described below). LLDB maintains several `clang::ASTContext`s and copies decls between them as needed. E.g., there's an AST for decls parsed out of
DWARF (via `DWARFASTParserClang`), an AST for decls from Clang modules, etc. LLDB does this to prevent mismatching decls for the same program entity to corrupt the
final ASTContext.

A UserExpression creates a single top-level `ClangASTImporter`; this importer is then called from components such as `ClangExpressionDeclMap` and `DWARFASTParserClang`
for importing types. The `ClangASTImporter` creates a delegate which forwards the import request to an actual `clang::ASTImporter`. Each source/destination `clang::ASTContext`
combination gets its own `ASTImporterDelegate` which is stored as `ASTContextMetadata` in `ClangASTImporter::m_metadata_map`; this means the same source AST that gets imported
into two separate destination AST will create two delegates. Similarly, importing into the same destination context from two different source ASTs will also create two delegates.

## What do we track origins for?

In several completion/lookup APIs (e.g., `ClangASTSource::CompleteTagDecl`), we want
to complete the origin decl before importing, so we have an actual definition to import.
In other words, the main reason we want to track decls is because LLDB performs type completion
lazily. So whenever LLDB feels like completing a type, it needs to be able to find the
decl/ASTContext/definition to import from; this is faciliated by caching the origin alongside
the destination context/decl (see [Origin Tracking Structures](#origin-tracking-structures))

### APIs that care about decl origins
* `CompleteTagDeclsScope`
* `ClangASTSource::layoutRecordType`
* `ClangASTSource::CompleteType`
* `ClangASTSource::FindExternalLexicalDecls`
* `ClangASTImporter::CanImport`
* `ClangASTImporter::Import`
* `ClangASTImporter::CompleteTagDecl`
* `ClangASTImporter::CompleteAndFetchChildren`
* `ClangASTImporter::GetDeclMetadata`
* `ClangASTImporter::ASTImporterDelegate::ImportImpl`

## Minimal Import

By default, all `ASTImporter` instances used by LLDB (created by `ASTImporterDelegate`) import using `clang::ASTImporter`'s "minimal import" mode.

## Origin Tracking Structures

* `DeclOrigin`
  - Most fundamental unit of origin tracking
  - Contains a `clang::Decl*` and the `clang::ASTContext*` that owns it

* `NamespaceMap`
  - Alias for `std::vector<std::pair<lldb::ModuleSP, CompilerDeclContext>>`
  - List of all LLDB modules that contain declarations for some namespace
  - Each `NamespaceDecl` is mapped to such a list via `NamespaceMetaMap`
  - When a `ClangASTSource` (e.g., `ClangExpressionDeclMap`) resolves a namespaced type
    (e.g., via `ClangASTSource::FindCompleteType`), it will search each LLDB module
    that knows of the namespace `DeclContext` in question for the type's name (this search
    is done in (`FindTypesInNamespace`, which requires both pieces of information we store
    in a `NamespaceMap` entry, aka `NamespaceMapItem`).

* `ClangASTMap` TODO
* `ClangASTMetadata` TODO
* `TypeSystemMap` TODO

* `ContextMetadataMap`
  - `map<clang::ASTContext*, ASTContextMetadata*>`
  - Stores metadata for a *destination* `ASTContext`
  - Owned by `ClangASTImporter`

* `NamespaceMetaMap`
  - Alias for `DenseMap<clang::NamespaceDecl*, NamespaceMap*>`
  - See `NamespaceMap`
  - Owned by `ASTImporterDelegate`

* `DelegateMap`
  - Alias for `DenseMap<clang::ASTContext*, ImporterDelegate*>`
  - Stores each **source** `ASTContext`'s `ASTImporterDelegate`
  - The `ClangASTImporter` APIs are used by the expression evaluator
  - Owned by `ASTImporterDelegate`

* `MapCompleter`
  - This is a protocol implemented by a `ClangASTSource` to populate the `NamespaceMetaMap`
    for a freshly imported `NamespaceDecl`
  - Owned by `ASTImporterDelegate`

* `OriginMap`
  - `map<clang::Decl*, DeclOrigin>`
  - Tracks the owning *source* decl and its owning `ASTContext` for a *destination* decl
  - Owned by `ASTImporterDelegate`

* `ASTContextMetadata`:
  - Main structure responsible for tracking decl origin information per `ASTContext`
  - An instance of this object tracks metadata for a single **destination** `ASTContext`
  - Keeps track of:
    - Unused `m_dst_ctx` (!) *<<<*
    - The source `ASTContext`s' `ASTImporterDelegate`s (tracked via `DelegateMap`)
      - Note, a single destination context can have multiple source contexts
    - Which `lldb::Module`s contain declarations whose `DeclContext`s are some given `NamespaceDecl`
      (maintained by `NamespaceMetaMap`)
    - A `MapCompleter` to populate the above `NamespaceMetaMap`
    - Which *from* decl (and *from* `ASTContext`) a *to* decl was imported from (via `OriginMap`)

* `ClangASTImporter::GetContextMetadata`/`ClangASTImporter::MaybeGetContextMetadata`
  - Returns the `ASTContextMetadataSP` for a given *destination* context
  - The `Maybe` variant will not create an `ASTContextMetadataSP` if one doesn't already
    exist

* `ASTImporterDelegate::Imported`
  - TODO

* `ASTImporterDelegate::setOrigin`
  - Primarily used in `ASTImporterDelegate::Imported` to adjust origins
    according for a newly imported decl

* `ASTImporterDelegate::getOrigin`/`ClangASTImporter::GetDeclOrigin`
  - Used in `CompleteTagDeclsScope` to complete specifically decls from a source context
  - `ClangASTSource::layoutRecordType`
  - See [APIs that care about decl origins](#apis-that-care-about-decl-origins)

```
ClangASTImporter
`- map<clang::ASTContext*, ASTContextMetadata*> m_metadata_map
   |- AST_1 : ...
   |- AST_2 : ASTContextMetadata_2
   `- ...     `- map<clang::Decl*, DeclOrigin> OriginMap
                 |- dst_decl1 : DeclOrigin_1
                 |              `- { clang::Decl* : nullptr,   clang::ASTContext* :  nullptr } // Invalid origin
                 |- dst_decl2 : DeclOrigin_2
                 |              `- { clang::Decl* : src_decl2, clang::ASTContext* : src_ctx2 } // Valid origin
                 |- dst_decl3 : DeclOrigin_3
                 |              `- { clang::Decl* : src_decl3, clang::ASTContext* :  nullptr } // Valid origin
                 |- dst_decl4 : DeclOrigin_4
                 `- ...         `- { clang::Decl* : nullptr,   clang::ASTContext* : src_ctx4 } // Valid origin
```

* `ClangASTImporter::DeclOrigin ClangASTImporter::GetDeclOrigin(const clang::Decl *decl)`

* `void ClangASTImporter::SetDeclOrigin(const clang::Decl *decl)`
  - Overwrites decl origin (only used for Objective-C support)

* `ClangASTImporter::NewDeclListener`
  - E.g., `CompleteTagDeclsScope`

## TypeSystemClang

The `lldb_private::TypeSystem` interface specifies APIs to create and
query language independent types (via `lldb::opaque_compiler_type_t`).
Its only member is a pointer to the `SymbolFile` which backs the `TypeSystem`
instance, which allows the `TypeSystem` to get conrete information about
a type from debug-info (e.g., during type completion).

`TypeSystemClang` implements the `TypeSystem` interface for the `C++` language plugin.
It owns all objects necessary for parsing and evaluating an expression including a
`clang::ASTContext`, `clang::FileManager`, `clang::DiagnosticsEngine`, `clang::IdentifierTable`,
`DWARFASTParserClang`, etc. Note that, `TypeSystemClang` *conditionally* owns the `ASTContext`; it will only
own the `ASTContext` after an explicit call to `TypeSystemClang::CreateContext`! It also
maintains metadata structures such as `DeclMetadataMap`/`TypeMetadataMap` (which keeps track of object-related
information about `clang::Decl`s/`clang::Type`s), `CXXRecordDeclAccessMap` (which keeps track of
a `CXXRecordDecl`s access specification. Finally, it also keeps a weak pointer to the `clang::Sema`
which parses and creates the associated `ASTContext`.

LLDB has two kinds of `TypeSystemClang`s:
1. `ScratchTypeSystemClang`
2. `SpecializedScratchAST`

### ScratchTypeSystemClang

A target owns a single main scratch AST into which expression evaluation
imports; a target can have further sub-ASTs which are separated from the
main scratch AST, e.g., when refining decl definitions with ones from modules.

A `ScratchTypeSystemClang` is a `TypeSystemClang` that also owns:
* A `ClangASTSource` for type completion
* Set of sub-ASTs (`map<IsolatedASTKind, TypeSystemClang*>`)
* `ClangPersistentVariables`: a structure keeping track of a target's persistent variables

Used for storing the final result variable.

### SpecializedScratchAST

TODO

## AST Import Flow

This section describes how decls are imported into various ASTs.

## Who owns ClangASTImporters and TypeSystemClangs (i.e., ASTContexts)

`TypeSystemClang` conditionally owns the `ASTContext` it wraps.

# AST Sources

There are several kinds of AST sources (and AST source wrappers) to be aware of:
* `ClangASTSource`:
  - Implements core lookup interface of `clang::ExternalASTSource`
  - Crucially, it implements `FindExternalVisibleDeclsByName`, which `clang::Sema` will consistently call during
    name resolution
  - Called into when TODO
* `ClangExpressionDeclMap`
  - Derives from `ClangASTSource` and handles book-keeping for things like persistent variables, Objective-C
    lookup, JIT execution, etc.
  - Called into when TODO
* `ClangExternalASTSourceCallbacks`
  - Alternative deriver of `clang::ExternalASTSource` (the other being `ClangASTSource`)
  - Default external AST source when creating a new *owning* `TypeSystemClang` (e.g., when creating
    default `TypeSystemClang` for a language plugin). However, when we start parsing an expression 
    we explicitly install a `ClangExpressionDeclMap`.
  - Implements `FindExternalVisibleDeclsByName` but handles Objective-C only!
  - Mostly keeps `ExternalASTSource` as no-ops but implements the completion APIs, e.g., `CompleteType`,
    which just forward to the completion APIs of the underlying `TypeSystemClang`
  - Called into when TODO
* `ClangASTSourceProxy`
  - A wrapper around `ClangASTSource` which just forwards to the underlying source
  - Ensures that the underlying `clang::ASTContext` (via `TypeSystemClang`) doesn't own the
    the AST source book-keeping structures. If the `clang::ASTContext` lifetime ends, it
    doesn't tear down any of the `ClangASTSource` since the installed AST source was the stateless
    proxy
  - Both the `ClangASTSource` and the `ClangExpressionDeclMap` get installed via this proxy when
    a new source is created. A `TypeSystemClang` will not directly own a `ClangASTSource`, instead it
    owns the proxy only. The proxy is *not* used for `ClangExternalASTSourceCallbacks` however; the
    `TypeSystemClang` owns an instance of this object (via the underyling `clang:ASTContext`
* `SemaSourceWithPriorities`
  - Implements the `clang::ExternalSemaSource` interface (which is an `clang::ExternalASTSource` that
    can provide information for semantic analysis)
  - TODO
* `ExternalASTSourceWrapper`
  - TODO

# Example Completion Call-chain

```
Process 1203 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = breakpoint 1.1
    frame #0: 0x0000000100003f2c a.out main at test.cpp:9:5
   6    int main() {
   7        lib::Foo<double> foo;
   8        foo.func(lib::EnumValues::FIRST);
-> 9        return 0;
   10   }
Process 1203 launched: '/Users/michaelbuch/Git/llvm-project/a.out' (arm64)
(lldb) p foo.func(lib::EnumValues::FIRST)

lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): main
~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): main
~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): main
~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): int
~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): int
~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): int
~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $__lldb_arg
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $__lldb_arg

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $__lldb_expr
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $__lldb_expr

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $__lldb_local_vars
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $__lldb_local_vars

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): foo
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): foo
~> ~> bool lldb_private::ClangExpressionDeclMap::LookupLocalVariable(lldb_private::NameSearchContext &, lldb_private::ConstString, lldb_private::SymbolContext &, const lldb_private::CompilerDeclContext &): foo
~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): Foo<double>
~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): Foo<double>
~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): Foo<double>
~> ~> ~> ~> ~> ~> lldb::TypeSP DWARFASTParserClang::ParseStructureLikeDIE(const lldb_private::SymbolContext &, const DWARFDIE &, ParsedDWARFTypeAttributes &): Foo<double>
~> ~> ~> ~> ~> ~> ~> void PrepareContextToReceiveMembers(lldb_private::TypeSystemClang &, lldb_private::ClangASTImporter &, clang::DeclContext *, DWARFDIE, const char *): Foo<double>
~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): double
~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): double
~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): double
~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> static bool lldb_private::TypeSystemClang::StartTagDeclarationDefinition(const lldb_private::CompilerType &)
~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): Foo<double>
~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetFullCompilerType()
~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> virtual bool SymbolFileDWARF::CompleteType(lldb_private::CompilerType &):
               RecordType 0x11f4a16e0 'class lib::Foo<double>'
               |-ClassTemplateSpecialization 0x11f4a15e8 'Foo'
~> ~> ~> ~> ~> ~> virtual bool DWARFASTParserClang::CompleteTypeFromDWARF(const DWARFDIE &, lldb_private::Type *, lldb_private::CompilerType &): Foo<double>
~> ~> ~> ~> ~> ~> ~> bool DWARFASTParserClang::CompleteRecordType(const DWARFDIE &, lldb_private::Type *, lldb_private::CompilerType &): Foo<double>
~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP DWARFASTParserClang::ParseStructureLikeDIE(const lldb_private::SymbolContext &, const DWARFDIE &, ParsedDWARFTypeAttributes &): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> void PrepareContextToReceiveMembers(lldb_private::TypeSystemClang &, lldb_private::ClangASTImporter &, clang::DeclContext *, DWARFDIE, const char *): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> static bool lldb_private::TypeSystemClang::StartTagDeclarationDefinition(const lldb_private::CompilerType &)
~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetLayoutCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual bool SymbolFileDWARF::CompleteType(lldb_private::CompilerType &):
                              RecordType 0x11f4a1890 'struct lib::Bar'
                              |-CXXRecord 0x11f4a17e0 'Bar'
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual bool DWARFASTParserClang::CompleteTypeFromDWARF(const DWARFDIE &, lldb_private::Type *, lldb_private::CompilerType &): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool DWARFASTParserClang::CompleteRecordType(const DWARFDIE &, lldb_private::Type *, lldb_private::CompilerType &): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): int
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetLayoutCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool GetCompleteQualType(clang::ASTContext *, clang::QualType, bool)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> void RequireCompleteType(lldb_private::CompilerType)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): barfunc
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): barfunc
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): barfunc
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): int
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): Bar
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> static bool lldb_private::TypeSystemClang::CompleteTagDeclarationDefinition(const lldb_private::CompilerType &)
~> ~> ~> ~> ~> ~> ~> ~> bool GetCompleteQualType(clang::ASTContext *, clang::QualType, bool)
~> ~> ~> ~> ~> ~> ~> ~> void RequireCompleteType(lldb_private::CompilerType)
~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::CompilerType::GetCompleteType() const
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual bool lldb_private::TypeSystemClang::GetCompleteType(lldb::opaque_compiler_type_t)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool GetCompleteQualType(clang::ASTContext *, clang::QualType, bool)
~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): double
~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetLayoutCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> bool GetCompleteQualType(clang::ASTContext *, clang::QualType, bool)
~> ~> ~> ~> ~> ~> ~> ~> void RequireCompleteType(lldb_private::CompilerType)
~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): func
~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): func
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): func
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *):
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): Foo<double>
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): EnumValues
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): EnumValues
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): EnumValues
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): unsigned long long
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb::TypeSP SymbolFileDWARF::ParseType(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): unsigned long long
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> virtual lldb::TypeSP DWARFASTParserClang::ParseTypeFromDWARF(const lldb_private::SymbolContext &, const DWARFDIE &, bool *): unsigned long long
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetFullCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> static bool lldb_private::TypeSystemClang::StartTagDeclarationDefinition(const lldb_private::CompilerType &)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> static bool lldb_private::TypeSystemClang::CompleteTagDeclarationDefinition(const lldb_private::CompilerType &)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): unsigned long long
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): Foo<double>
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> ~> ~> ~> ~> ~> static bool lldb_private::TypeSystemClang::CompleteTagDeclarationDefinition(const lldb_private::CompilerType &)
~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> virtual void lldb_private::ClangASTSource::CompleteType(clang::TagDecl *):
         ClassTemplateSpecializationDecl 0x11f4a0340 <<invalid sloc>> <invalid sloc> <undeserialized declarations> class Foo definition
         |-DefinitionData pass_in_registers standard_layout trivially_copyable trivial literal
         | |-DefaultConstructor exists trivial needs_implicit
         | |-CopyConstructor simple trivial has_const_param needs_implicit implicit_has_const_param
         | |-MoveConstructor exists simple trivial needs_implicit
         | |-CopyAssignment simple trivial has_const_param needs_implicit implicit_has_const_param
         | |-MoveAssignment exists simple trivial needs_implicit
         | |-Destructor simple irrelevant trivial
          needs_implicit
         |-TemplateArgument type 'double'
         | |-BuiltinType 0x11f46c630 'double'
         |-FieldDecl 0x11f4a8218 <<invalid sloc>> <invalid sloc> bar 'lib::Bar'
         |-FieldDecl 0x11f4a8440 <<invalid sloc>> <invalid sloc> foomem 'double'
         |-AccessSpecDecl 0x11f4a8490 <<invalid sloc>> <invalid sloc> public
         |-CXXMethodDecl 0x11f4a8650 <<invalid sloc>> <invalid sloc> func 'void (lib::EnumValues)'
           |-ParmVarDecl 0x11f4a85e8 <<invalid sloc>> <invalid sloc> 'lib::EnumValues'
           |-AsmLabelAttr 0x11f4a86f8 <<invalid sloc>> Implicit "_ZN3lib3FooIdE4funcENS_10EnumValuesE"
~> ~> ~> ~> virtual void lldb_private::ClangExternalASTSourceCallbacks::CompleteType(clang::TagDecl *)
~> ~> ~> ~> ~> void lldb_private::TypeSystemClang::CompleteTagDecl(clang::TagDecl *)
~> ~> ~> ~> virtual void lldb_private::ClangExternalASTSourceCallbacks::CompleteType(clang::TagDecl *)
~> ~> ~> ~> ~> void lldb_private::TypeSystemClang::CompleteTagDecl(clang::TagDecl *)
~> ~> ~> ~> bool lldb_private::ClangASTImporter::RequireCompleteType(clang::QualType)
~> ~> ~> ~> bool lldb_private::ClangASTImporter::RequireCompleteType(clang::QualType)
~> ~> ~> ~> bool lldb_private::ClangASTImporter::RequireCompleteType(clang::QualType)
~> ~> ~> ~> bool lldb_private::ClangASTImporter::CompleteTagDecl(clang::TagDecl *)
~> ~> ~> ~> ~> static bool lldb_private::TypeSystemClang::GetCompleteDecl(clang::ASTContext *, clang::Decl *)
~> ~> ~> ~> ~> void lldb_private::ClangASTImporter::ASTImporterDelegate::ImportDefinitionTo(clang::Decl *, clang::Decl *)
~> virtual bool lldb_private::TypeSystemClang::GetCompleteType(lldb::opaque_compiler_type_t)
~> ~> bool GetCompleteQualType(clang::ASTContext *, clang::QualType, bool)
~> virtual bool lldb_private::TypeSystemClang::GetCompleteType(lldb::opaque_compiler_type_t)
~> ~> bool GetCompleteQualType(clang::ASTContext *, clang::QualType, bool)
virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): foo
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): foo
~> ~> bool lldb_private::ClangExpressionDeclMap::LookupLocalVariable(lldb_private::NameSearchContext &, lldb_private::ConstString, lldb_private::SymbolContext &, const lldb_private::CompilerDeclContext &): foo
~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetFullCompilerType()
~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> lldb_private::CompilerType lldb_private::Type::GetForwardCompilerType()
~> ~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> virtual void lldb_private::ClangASTSource::CompleteType(clang::TagDecl *):
         ClassTemplateSpecializationDecl 0x11f4a0340 <<invalid sloc>> <invalid sloc> class Foo definition
         |-DefinitionData pass_in_registers standard_layout trivially_copyable trivial literal
         | |-DefaultConstructor exists trivial needs_implicit
         | |-CopyConstructor simple trivial has_const_param needs_implicit implicit_has_const_param
         | |-MoveConstructor exists simple trivial needs_implicit
         | |-CopyAssignment simple trivial has_const_param needs_implicit implicit_has_const_param
         | |-MoveAssignment exists simple trivial needs_implicit
         | |-Destructor simple irrelevant trivial needs_implicit
         |-TemplateArgument type 'double'
         | |-BuiltinType 0x11f46c630 'double'
         |-AccessSpecDecl 0x11f4a8490 <<invalid sloc>> <invalid sloc> public
         |-CXXMethodDecl 0x11f4a8650 <<invalid sloc>> <invalid sloc> func 'void (lib::EnumValues)'
         | |-ParmVarDecl 0x11f4a85e8 <<invalid sloc>> <invalid sloc> 'lib::EnumValues'
         | |-AsmLabelAttr 0x11f4a86f8 <<invalid sloc>> Implicit "_ZN3lib3FooIdE4funcENS_10EnumValuesE"
         |-FieldDecl 0x11f4a8218 <<invalid sloc>> <invalid sloc> bar 'lib::Bar'
         |-FieldDecl 0x11f4a8440 <<invalid sloc>> <invalid sloc> foomem 'double'
~> ~> ~> ~> bool lldb_private::ClangASTImporter::CompleteTagDecl(clang::TagDecl *)
~> ~> ~> ~> ~> static bool lldb_private::TypeSystemClang::GetCompleteDecl(clang::ASTContext *, clang::Decl *)
~> ~> ~> ~> ~> void lldb_private::ClangASTImporter::ASTImporterDelegate::ImportDefinitionTo(clang::Decl *, clang::Decl *)

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): lib
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): lib
~> ~> bool lldb_private::ClangExpressionDeclMap::LookupLocalVariable(lldb_private::NameSearchContext &, lldb_private::ConstString, lldb_private::SymbolContext &, const lldb_private::CompilerDeclContext &): lib
~> ~> void lldb_private::ClangExpressionDeclMap::LookupFunction(lldb_private::NameSearchContext &, lldb::ModuleSP, lldb_private::ConstString, const lldb_private::CompilerDeclContext &): lib

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): EnumValues
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): EnumValues
~> ~> void lldb_private::ClangExpressionDeclMap::LookupFunction(lldb_private::NameSearchContext &, lldb::ModuleSP, lldb_private::ConstString, const lldb_private::CompilerDeclContext &): EnumValues
~> lldb_private::Type *SymbolFileDWARF::ResolveType(const DWARFDIE &, bool, bool): EnumValues
~> lldb_private::CompilerType lldb_private::Type::GetFullCompilerType()
~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)
~> ~> ~> bool lldb_private::Type::ResolveCompilerType(lldb_private::Type::ResolveState)

virtual void lldb_private::ClangExternalASTSourceCallbacks::CompleteType(clang::TagDecl *)
~> void lldb_private::TypeSystemClang::CompleteTagDecl(clang::TagDecl *)

bool lldb_private::ClangASTImporter::LayoutRecordType(...)
bool lldb_private::ClangASTImporter::LayoutRecordType(...)

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $__lldb_arg_ptr
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $__lldb_arg_ptr

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): _$__lldb_valid_pointer_check
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): _$__lldb_valid_pointer_check

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $__lldb_local_val
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $__lldb_local_val

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $__lldb_arg_obj
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $__lldb_arg_obj

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $__lldb_arg_selector
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $__lldb_arg_selector

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $__lldb_objc_object_check
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $__lldb_objc_object_check

virtual void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &): $responds
~> void lldb_private::ClangExpressionDeclMap::FindExternalVisibleDecls(lldb_private::NameSearchContext &, lldb::ModuleSP, const lldb_private::CompilerDeclContext &): $responds

...
(lldb)
```

# Glossary
* Lines marked with *<<<* are targets for the type completion refactor (see [D101950](https://reviews.llvm.org/D101950)
  and rdar://75170305

# Completion APIs
## clang::ExternalASTSource
* `virtual void clang::ExternalASTSource::CompleteType(TagDecl *Tag);`
  - noop by default
  - "Give opportunity for external source to complete type"
  - Called from `clang::Sema::RequireCompleteType`
    More often than not this gets called from within LLDB's
    `ClangExpressionDeclMap` component and not `clang::Sema`

## SymbolFileDWARF
* `virtual bool SymbolFileDWARF::CompleteType(CompilerType &compiler_type) override;`
  - Called from `TypeSystemClang::CompleteTagDecl`
  - Performs following steps:
    1. Complete type via `ClangASTImporter::CompleteType` if enum or record (!) and the decl has a known origin
    2. If not possible, use `DWARFASTParserClang::CompleteTypeFromDWARF` 
      - Uses `m_forward_decl_clang_type_to_die` to determine whether a type has
        already been resolved. (!)

* `Type *SymbolFileDWARF::ResolveType(const DWARFDIE &die, bool, bool)`
  - Called from various places in `Plugins/SymbolFile` whenever we need to
    complete a type represented by a `DWARFDIE`
  - Calls `SymbolFileDWARF::ParseType`

* `TypeSP SymbolFileDWARF::ParseType(const SymbolContext &sc, const DWARFDIE &die, bool *type_is_new_ptr)`
  - Calls `ParseTypeFromDWARF` and `ParseTypes`
  - Called from `ResolveType`
  - Following steps:
    1. `ParseTypeFromDWARF`
    2. Adds parsed type into SymbolContext `TypeList`
    3. If the specified die has a `DW_TAG_subprogram` then insert parsed type into
       `m_function_scope_qualified_name_map`

## lldb_private::ClangASTSource
* `void ClangASTSource::CompleteType(TagDecl *tag_decl)`
  - Calls `CompleteTagDecl`
  - Falls back to `FindCompleteType`+`CompleteTagDeclWithOrigin`
  - Called from `ClangExpressionDeclMap`, `ASTImporter`, `RecordLayoutBuilder`, `TypeSystemClang`,
    `clang::Sema::RequireCompleteType`, `ClangASTSource` itself

## lldb_private::ClangASTSource::ClangASTSourceProxy
* `void ClangASTSourceProxy::CompleteType(clang::TagDecl *Tag) override`
  - Forwards to to `ClangASTSource::CompleteType`

## lldb_private::ClangExternalASTSourceCallbacks
* `void ClangExternalASTSourceCallbacks::CompleteType(clang::TagDecl *tag_decl)`
  - Calls `TypeSystemClang::CompleteTagDecl`

## lldb_private::ClangASTImporter
* `bool ClangASTImporter::CompleteType(const CompilerType &compiler_type)`
  - Called from `SymbolFileDWARF::CompleteType`
  - Checks whether type is an enum or record type (via `CanImport`). If so,
    calls `ClangASTImporter::Import` and on success with call `CompleteTagDeclarationDefinition`
  - Calls `SetHasExternalStorage(false)` on failure (TODO: why?)

* `bool ClangASTImporter::CompleteTagDecl(clang::TagDecl *decl)`
  - Will use `ASTImporter::ImportDefinition`
  - Called from `ClangASTSource::CompleteType`, `ClangASTImporter::RequireCompleteType`
    and `ClangASTImporter.cpp:MaybCompleteReturnType`

* `bool ClangASTImporter::CompleteTagDeclWithOrigin(clang::TagDecl *decl, clang::TagDecl *origin_decl)`
  - Called from `ClangASTSource` as a fall-back for when the regular `CompleteType` fails.
    In such ases we try to find an alternate definition somewhere which could allow us to
    complete the decl. The alternate definition is looked up via `FindCompleteType`
  - Uses `TypeSystemClang::GetCompleteDecl` and `ASTImporter::ImportDefinition` for
    type completion.

* `bool ClangASTImporter::RequireCompleteType(clang::QualType type)`
  - Tries to find definition for type (including in redeclaration chain, via `TagDecl::getDefinition`
  - If definition hasn't been pulled into the `TagDecl` (or it's redecl chain) yet, then
    try to find and import definition `ClangASTImporter::CompleteTagDecl`

## lldb_private::ExternalASTSourceWrapper
* `void ExternalASTSourceWrapper::CompleteType(clang::TagDecl *Tag) override`

## lldb_private::TypeSystemClang
* `void TypeSystemClang::CompleteTagDecl(clang::TagDecl *decl)`
  - Callers ask `TypeSystem` plugin to complete a `TagDecl` (why only `TagDecl`)?
  - Calls `CompleteType` on current symbolfile (which calls `ClangASTImporter::CompleteType`
    and `DWARFASTParserClang::CompleteTypeFromDWARF`
  - Called via `ClangExternalASTSourceCallbacks`

* `bool TypeSystemClang::GetCompleteType(lldb::opaque_compiler_type_t type)`

* `bool GetCompleteQualType(clang::ASTContext *ast, clang::QualType qual_type, bool allow_completion = true`

* `bool GetCompleteDecl(clang::Decl *decl)`

* `bool TypeSystemClang::StartTagDeclaration(const CompilerType &type)`
  - Used to build definition for a `clang::TagDecl`
  - Calls `TagType::getDecl` (which will walk redecl chain to find definition)
  - Then calls `TagDecl::startDefinition`
  - Called from:
    - `CreateStructForIdentifier` (which is used throughout LLDB's formatting component)
    - `ParseEnumType`, `CompleteEnumType`, `ParseStructureLikeDIE`, `ForcefullyCompleteType`

* `bool TypeSystemClang::CompletedTagDefinition(const CompilerType& type)`
  - Used to finalize the definition of a `clang::TagDecl`
  - If the tagdecl definition bits haven't been set yet (via `TagDecl::setCompleteDefinition`)
    then will call `CXXRecordDecl::completeDefinition` (which calls `RecordDecl::completeDefinition`/`TagDecl::completeDefinition`)
    to set said bits and account for any C++ method overrides
  - Called from:
    - `CreateStructForIdentifier`
    - `ClangASTImporter::CompleteType` after importing a type (!)
    - `ParseEnumType`, `CompleteEnumType`, `CompleteRecordType`,
      `ParseStructureLikeDIE`, `ForcefullyCompleteType`
    - Note how this list doesn't exactly match that of `StartTagDeclaration` *<<<*

## clang::ASTImporter
* `ASTImporter::CompleteDecl`
  * Called within ASTImporter to fill in definition data for Enum/Objective-C decls
  * For TagDecls (currently just called for Enums) fill in the redeclaration chain
    with definitions from the main TagDecl's DefinitionData. I.e., will allocate and
    copy DefinitionData for all decls in a redeclaration chain

## DWARFASTParserClang
* Reads types from DWARF and completes them by creating decls via `TypeSystemClang`, exposing them in
  LLDB's AST

* `bool DWARFASTParserClang::CompleteEnumType(const DWARFDIE &die, lldb_private::Type *type, CompilerType &clang_type)`
  - Parses enumerator children from DWARF and then adds them as EnumConstantDecls
    to the AST under the appropriate EnumType node
  - Calls `StartTagDeclarationDefinition/CompleteTagDeclarationDefinition`
    which for enums will simply copy DefinitionData from the decl associated
    with the specified `clang_type` to all decls in the redeclaration chain
  - Called from `CompleteTypeFromDWARF` for enum types
  - What counts as completion?
    - when all it's enum value children have been read from DWARF and exposed in the AST

* `bool DWARFASTParserClang::CompleteRecordType(const DWARFDIE &die, lldb_private::Type *type, CompilerType &clang_type)`
  - This function expects a definition for `clang_type` to have already
    been started (via `StartTagDeclarationDefinition`)! *<<<*
  - Called from `CompleteTypeFromDWARF` for structure/union/class types
  - Following steps:
    1. Parses members of record type from DWARF
    2. Calls `ResolveType` for each member
    3. Calls `RequireCompleteType` for each base class (NOTE: silently ignores bases for
       which `getTypeSourceInfo() == nullptr` while comment claims that leaving base types
       as forward declarations leads to crashes!!)
    4. Add overriden methods to `clang_type`'s decl
    5. `BuildIndirectFields`
    6. `CompleteTagDeclarationDefinition` (without prior `StartTagDeclarationDefinition` in this function!) *<<<*
      - The corresponding `StartTagDeclarationDefinition` is most likely started in `ParseStructureLikeDIE`
    7. `SetRecordLayout`

* `CompleteTypeFromDWARF(const DWARFDIE &die, lldb_private::Type *type, CompilerType &clang_type)`
  - Called from `SymbolFileDWARF::ParseType` (via `SymbolFileDWARF::ResolveType`)
  - Following steps:
    1. Set `DIE_IS_BEING_PARSED` bit *<<<*
    2. Dispatch to `ParseXXX` function based on DIE tag
    3. UpdateSymbolContextScopeForType(parsed_type)

* `void RequireCompleteType(CompilerType type)`
  - Called whenever C++ rules require a type to be complete
    (e.g., base classes, members, etc.)
  - Tries to force complete a type and if that's not possible
    will mark it as forcefully completed (via `ForcefullyCompleteType`) *<<<*

* `void PrepareContextToReceiveMembers(TypeSystemClang &ast, ClangASTImporter &ast_importer, clang::DeclContext *decl_ctx,
                                       DWARFDIE die, const char *type_name_cstr`
  - Similar to `RequireCompleteType` but doesn't force complete the type;
    instead this function merely prepares the type to be completed later. *<<<*
  - If the type was imported from an external AST, will pull in definition. Otherwise
    marks type as forcefully completed. *<<<*.
  - The main difference to `RequireCompleteType` is that we don't call `CompleteType`.
  - Called from `ParseStructureLikeDIE` (on the declcontext of the parsed DIE) and
    `ParseTypeModifier` (for `DW_TAG_typedef`) since we tend to construct half completed
    records to be able to complete the children

* `void ForcefullyCompleteType(CompilerType type)`
  - Called from `RequireCompleteType`
  - Calls `StartTagDeclarationDefinition/CompleteTagDeclarationDefinition`
  - This function essentially can leave record types with incomplete definitions.
    We allocate but don't fully set a record's `DefinitionData`. *<<<*
  - Sets `IsForcefullyCompleted` flag on `TypeSystemClang` metadata
    - This flag is used ... TODO

* `TypeSP ParseTypeFromDWARF(const SymbolContext &sc, const DWARFDIE &die, bool *type_is_new_ptr)`:
  - If it's the first time that `DWARFASTParserClang` sees this DIE, begin parsing:
    1. Set `DIE_IS_BEING_PARSED` in `m_die_to_type` for the specified 'die'
    2. Parse the DIE's attributes
    3. Based on the DIE's DW_TAG, call the appropriate `DWARFASTParserClang::ParseXXX` method
    4. Update `m_die_to_type`

## lldb_private::Type
* `bool Type::ResolveCompilerType(ResolveState compiler_type_resolve_state)`
  - Responsible for setting the `CompilerType` backing this `Type` object
  - If the underlying `CompilerType` hasn't been resolved yet, resolve the
    type from DWARF via `SymbolFileDWARF::ResolveTypeUID` (which calls `SymbolFileDWARF::ResolveType`)
    as a forward declaration (i.e., don't call `CompleteType`)
  - If the `compiler_type_resolve_state` isn't a `Forward` (i.e., the caller didn't request a full
    CompilerType), call `SymbolFileDWARF::CompleteType`

* `CompilerType Type::GetFullCompilerType()`
  - Reads type for backing DIE from DWARF if necessary and completes the
    underlying `CompilerType` of this objet
  - Calls `ResolveType(ResolveState::Full)`
  - Notably called from:
    1. `ClangASTSource::FindCompleteType` (called from `ClangASTSource::CompleteType`)
    2. `ClangExpressionDeclMap` APIs which copy types into the scratch AST
    3. some `DWARFASTParserClang::ParseXXX` APIs before creating nodes in the AST *<<<*

* `CompilerType Type::GetForwardCompilerType()`
  - Reads type for backing DIE from DWARF if necessary, sets the
    underlying `CompilerType` of this object *without* completing it
  - Calls `ResolveType(ResolveState::Forward)`
  - Called whenever we need information about `CompilerType` that doesn't
    a complete type. E.g., getting the type name, encoding.
    More crucially, this is used in the `gmodules` support when resolving
    types from `.pcm` files (see `DWARFASTParserClang::ParseTypeFromClangModule`)

## lldb_private::CompilerType

* `bool GetCompleteType() const`

## clang::Sema
* `Sema::RequireCompleteType`

## clang::Decl
* `XXXDecl *XXXDecl::getDefinition() const`
  - Depending on the kind of decl will return the definition associated with the declaration if available.
    Most interestingly, for `TagDecl`s (such as classes/enums/unions/structs), `FunctionDecl`s and `VarDecl`s
    this will walk through the redeclaration chain to look for a definition, if necessary.

* `RecordDecl *RecordDecl::getDefinition() const`

* `CXXRecordDecl *CXXRecordDecl::getDefinition() const`

* `TagDecl *TagDecl::getDefinition() const`

* `void TagDecl::startDefinition()`
  - allocates `CXXRecordDecl::DefinitionData` and propagates it to all decls on the redecl chain
  - After this function the DefinitionData can be mutated and completed with a call to `TagDecl::completeDefinition`
  - Used by LLDB to create definitions for decls (see `StartTagDeclarationDefinition`)

* `void TagDecl::completeDefinition()`

## clang::Type

* `TagDecl *TagType::getDecl() const`
  - Returns a `Type`s definition decl if possible (!)
  - Walks through the decls redeclaration chain and returns the definition if found (note, it can return a definition which is in
    progress, i.e., `isBeingDefined() == true` (!)). If no definition exists, returns decl associated with the `Type`. *<<<*
  - Called from various places in `ClangASTImporter` and `TypeSystemClang`. Most notably called
    when completing a `TagType` via `ClangASTImporter::RequireCompleteType` or `ClangASTImporter::CompleteAndFetchChildren`
  - *Note*: Does not consult external sources or perform lookups

* `TagDecl *Type::getAsTagDecl() const`
  - Utility function that forwards to `TagType::getDecl` if we're dealing with `TagType`s. Returns `nullptr` otherwise.
  - Used throughout LLDBs expression evaluation components (via `ClangUtil::GetAsTagDecl`)
  - *Note*: Does not consult external sources or perform lookups

# TODO
* ForgetSource/ForgetDestination
* CompleteRedeclChain
* ExternalVisibleDecls
* ExternalLexicalDecls
* LazyLocalLexicalDecls
* LazyExternalLexicalDecls

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
* ParseStructureLikeDIE
* ParseTypeFromClangModule
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

# References

- LLDB source
- Phabricator
- Raphael's master's thesis
