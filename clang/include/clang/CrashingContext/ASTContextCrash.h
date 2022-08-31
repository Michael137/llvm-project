#ifndef LLVM_CLANG_CRASHINGCONTEXT_ASTCONTEXTCRASH_H
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Frontend/PrecompiledPreamble.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Serialization/ASTWriter.h"
#include "clang/Serialization/InMemoryModuleCache.h"
namespace clang {
namespace Crashing {
enum class CaptureDiagsKind { None, All, AllWithoutNonErrorsFromIncludes };
class ASTUnit {
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
  std::shared_ptr<LangOptions> LangOpts;
  IntrusiveRefCntPtr<DiagnosticsEngine> Diagnostics;
  IntrusiveRefCntPtr<FileManager> FileMgr;
  IntrusiveRefCntPtr<SourceManager> SourceMgr;
  IntrusiveRefCntPtr<InMemoryModuleCache> ModuleCache;
  std::unique_ptr<HeaderSearch> HeaderInfo;
  IntrusiveRefCntPtr<TargetInfo> Target;
  std::shared_ptr<Preprocessor> PP;
  IntrusiveRefCntPtr<ASTContext> Ctx;
  std::shared_ptr<TargetOptions> TargetOpts;
  std::shared_ptr<HeaderSearchOptions> HSOpts;
  std::shared_ptr<PreprocessorOptions> PPOpts;
  IntrusiveRefCntPtr<ASTReader> Reader;
  bool HadModuleLoaderFatalFailure = false;

  struct ASTWriterData {
    SmallString<128> Buffer;
    llvm::BitstreamWriter Stream;
    clang::ASTWriter Writer;
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
  std::vector<Decl *> TopLevelDecls;

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

  /// The number of stored diagnostics that come from the driver
  /// itself.
  ///
  /// Diagnostics that come from the driver are retained from one parse to
  /// the next.
  unsigned NumStoredDiagnosticsFromDriver = 0;

  /// Counter that determines when we want to try building a
  /// precompiled preamble.
  ///
  /// If zero, we will never build a precompiled preamble. Otherwise,
  /// it's treated as a counter that decrements each time we reparse
  /// without the benefit of a precompiled preamble. When it hits 1,
  /// we'll attempt to rebuild the precompiled header. This way, if
  /// building the precompiled preamble fails, we won't try again for
  /// some number of calls.
  unsigned PreambleRebuildCountdown = 0;

  /// Counter indicating how often the preamble was build in total.
  unsigned PreambleCounter = 0;

  /// Cache pairs "filename - source location"
  ///
  /// Cache contains only source locations from preamble so it is
  /// guaranteed that they stay valid when the SourceManager is recreated.
  /// This cache is used when loading preamble to increase performance
  /// of that loading. It must be cleared when preamble is recreated.
  llvm::StringMap<SourceLocation> PreambleSrcLocCache;

  /// The contents of the preamble.
  llvm::Optional<PrecompiledPreamble> Preamble;

  /// When non-NULL, this is the buffer used to store the contents of
  /// the main file when it has been padded for use with the precompiled
  /// preamble.
  std::unique_ptr<llvm::MemoryBuffer> SavedMainFileBuffer;

  /// The number of warnings that occurred while parsing the preamble.
  ///
  /// This value will be used to restore the state of the \c DiagnosticsEngine
  /// object when re-using the precompiled preamble. Note that only the
  /// number of warnings matters, since we will not save the preamble
  /// when any errors are present.
  unsigned NumWarningsInPreamble = 0;

  /// A list of the serialization ID numbers for each of the top-level
  /// declarations parsed within the precompiled preamble.
  std::vector<serialization::DeclID> TopLevelDeclsInPreamble;

  /// Whether we should be caching code-completion results.
  bool ShouldCacheCodeCompletionResults : 1;

  /// Whether to include brief documentation within the set of code
  /// completions cached.
  bool IncludeBriefCommentsInCodeCompletion : 1;

  /// True if non-system source files should be treated as volatile
  /// (likely to change while trying to use them).
  bool UserFilesAreVolatile : 1;

  static void ConfigureDiags(IntrusiveRefCntPtr<DiagnosticsEngine> Diags,
                             ASTUnit &AST, CaptureDiagsKind CaptureDiagnostics);

  void
  TranslateStoredDiagnostics(FileManager &FileMgr, SourceManager &SrcMan,
                             const SmallVectorImpl<StandaloneDiagnostic> &Diags,
                             SmallVectorImpl<StoredDiagnostic> &Out);

  void clearFileLevelDecls();

public:
  class ConcurrencyState {
    void *Mutex; // a std::recursive_mutex in debug;

  public:
    ConcurrencyState();
    ~ConcurrencyState() = default;

    void start();
    void finish();
  };
  ConcurrencyState ConcurrencyCheckValue;
  static std::unique_ptr<ASTUnit> create(std::shared_ptr<CompilerInvocation> CI,
                                         CaptureDiagsKind CaptureDiagnostics,
                                         bool UserFilesAreVolatile);
};
} // namespace Crashing
} // namespace clang
#endif // _H
