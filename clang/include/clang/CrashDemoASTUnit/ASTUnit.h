#ifndef LLVM_CLANG_CRASHDEMO_ASTUNIT_H
#define LLVM_CLANG_CRASHDEMO_ASTUNIT_H
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/PrecompiledPreamble.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Serialization/ASTWriter.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Serialization/InMemoryModuleCache.h"
#include "clang/Sema/CodeCompleteConsumer.h"
#include <utility>
namespace clang {

class ASTContext;
class ASTDeserializationListener;
class ASTMutationListener;
class ASTReader;
class CompilerInstance;
class CompilerInvocation;
class Decl;
class FileEntry;
class FileManager;
class FrontendAction;
class HeaderSearch;
class InputKind;
class InMemoryModuleCache;
class PCHContainerOperations;
class PCHContainerReader;
class Preprocessor;
class PreprocessorOptions;
class Sema;
class TargetInfo;

namespace crashing {

/// \brief Enumerates the available scopes for skipping function bodies.
enum class SkipFunctionBodiesScope { None, Preamble, PreambleAndMainFile };

/// \brief Enumerates the available kinds for capturing diagnostics.
enum class CaptureDiagsKind { None, All, AllWithoutNonErrorsFromIncludes };

/// Utility class for loading a ASTContext from an AST file.
class ASTUnit {
public:
  struct StandaloneFixIt {
    std::pair<unsigned, unsigned> RemoveRange;
    std::pair<unsigned, unsigned> InsertFromRange;
    std::string CodeToInsert;
    bool BeforePreviousInsertions;
  };

  struct StandaloneDiagnostic {
    unsigned ID;
    DiagnosticsEngine::Level Level;
    std::string Message;
    std::string Filename;
    unsigned LocOffset;
    std::vector<std::pair<unsigned, unsigned>> Ranges;
    std::vector<StandaloneFixIt> FixIts;
  };

private:
  std::shared_ptr<LangOptions>            LangOpts;
  IntrusiveRefCntPtr<DiagnosticsEngine>   Diagnostics;
  IntrusiveRefCntPtr<FileManager>         FileMgr;
  IntrusiveRefCntPtr<SourceManager>       SourceMgr;
  IntrusiveRefCntPtr<InMemoryModuleCache> ModuleCache;
  std::unique_ptr<HeaderSearch>           HeaderInfo;
  IntrusiveRefCntPtr<TargetInfo>          Target;
  std::shared_ptr<Preprocessor>           PP;
  IntrusiveRefCntPtr<ASTContext>          Ctx;

  struct ASTWriterData {
    SmallString<128> Buffer;
    llvm::BitstreamWriter Stream;
    ASTWriter Writer;
  
    ASTWriterData(InMemoryModuleCache &ModuleCache)
        : Stream(Buffer), Writer(Stream, Buffer, ModuleCache, {}) {}
  };
  std::unique_ptr<ASTWriterData> WriterData;

  FileSystemOptions FileSystemOpts;

  /// The AST consumer that received information about the translation
  /// unit as it was parsed or loaded.
  std::unique_ptr<ASTConsumer> Consumer;

  /// The semantic analysis object used to type-check the translation
  /// unit.
  std::unique_ptr<Sema> TheSema;

  /// Optional owned invocation, just used to make the invocation used in
  /// LoadFromCommandLine available.
  std::shared_ptr<CompilerInvocation> Invocation;

  /// Fake module loader: the AST unit doesn't need to load any modules.
  TrivialModuleLoader ModuleLoader;

  // OnlyLocalDecls - when true, walking this AST should only visit declarations
  // that come from the AST itself, not from included precompiled headers.
  // FIXME: This is temporary; eventually, CIndex will always do this.
  bool OnlyLocalDecls = false;

  /// Whether to capture any diagnostics produced.
  CaptureDiagsKind CaptureDiagnostics = CaptureDiagsKind::None;

  /// Track whether the main file was loaded from an AST or not.
  bool MainFileIsAST;

  /// What kind of translation unit this AST represents.
  TranslationUnitKind TUKind = TU_Complete;

  /// Whether we should time each operation.
  bool WantTiming;

  /// Whether the ASTUnit should delete the remapped buffers.
  bool OwnsRemappedFileBuffers = true;

  /// Track the top-level decls which appeared in an ASTUnit which was loaded
  /// from a source file.
  //
  // FIXME: This is just an optimization hack to avoid deserializing large parts
  // of a PCH file when using the Index library on an ASTUnit loaded from
  // source. In the long term we should make the Index library use efficient and
  // more scalable search mechanisms.
  std::vector<Decl*> TopLevelDecls;

  /// Sorted (by file offset) vector of pairs of file offset/Decl.
  using LocDeclsTy = SmallVector<std::pair<unsigned, Decl *>, 64>;
  using FileDeclsTy = llvm::DenseMap<FileID, std::unique_ptr<LocDeclsTy>>;

  /// Map from FileID to the file-level declarations that it contains.
  /// The files and decls are only local (and non-preamble) ones.
  FileDeclsTy FileDecls;

  /// The name of the original source file used to generate this ASTUnit.
  std::string OriginalSourceFile;

  /// The set of diagnostics produced when creating the preamble.
  SmallVector<StandaloneDiagnostic, 4> PreambleDiagnostics;

  /// The set of diagnostics produced when creating this
  /// translation unit.
  SmallVector<StoredDiagnostic, 4> StoredDiagnostics;

  /// The set of diagnostics produced when failing to parse, e.g. due
  /// to failure to load the PCH.
  SmallVector<StoredDiagnostic, 4> FailedParseDiagnostics;
};
}
} // namespace clang
#endif // LLVM_CLANG_FRONTEND_ASTUNIT_H
