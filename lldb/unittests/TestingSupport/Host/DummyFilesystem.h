//===-- DummyFileSystem.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_UNITTESTS_TESTINGSUPPORT_HOST_DUMMYFILESYSTEM_H
#define LLDB_UNITTESTS_TESTINGSUPPORT_HOST_DUMMYFILESYSTEM_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/VirtualFileSystem.h"

#include <map>

namespace lldb_private {

// Modified from llvm/unittests/Support/VirtualFileSystemTest.cpp
struct DummyFile : public llvm::vfs::File {
  llvm::vfs::Status S;
  explicit DummyFile(llvm::vfs::Status S) : S(S) {}
  llvm::ErrorOr<llvm::vfs::Status> status() override { return S; }
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>>
  getBuffer(const llvm::Twine &Name, int64_t FileSize,
            bool RequiresNullTerminator, bool IsVolatile) override {
    llvm_unreachable("unimplemented");
  }
  std::error_code close() override { return std::error_code(); }
};

class DummyFileSystem : public llvm::vfs::FileSystem {
  int FSID;       // used to produce UniqueIDs
  int FileID = 0; // used to produce UniqueIDs
  std::string cwd;
  std::map<std::string, llvm::vfs::Status> FilesAndDirs;

  static int getNextFSID() {
    static int Count = 0;
    return Count++;
  }

public:
  DummyFileSystem() : FSID(getNextFSID()) {}

  llvm::ErrorOr<llvm::vfs::Status> status(const llvm::Twine &Path) override {
    std::map<std::string, llvm::vfs::Status>::iterator I =
        FilesAndDirs.find(Path.str());
    if (I == FilesAndDirs.end())
      return make_error_code(llvm::errc::no_such_file_or_directory);
    // Simulate a broken symlink, where it points to a file/dir that
    // does not exist.
    if (I->second.isSymlink() &&
        I->second.getPermissions() == llvm::sys::fs::perms::no_perms)
      return std::error_code(ENOENT, std::generic_category());
    return I->second;
  }
  llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>>
  openFileForRead(const llvm::Twine &Path) override {
    auto S = status(Path);
    if (S)
      return std::unique_ptr<llvm::vfs::File>(new DummyFile{*S});
    return S.getError();
  }
  llvm::ErrorOr<std::string> getCurrentWorkingDirectory() const override {
    return cwd;
  }
  std::error_code setCurrentWorkingDirectory(const llvm::Twine &Path) override {
    cwd = Path.str();
    return std::error_code();
  }
  // Map any symlink to "/symlink".
  std::error_code getRealPath(const llvm::Twine &Path,
                              llvm::SmallVectorImpl<char> &Output) override {
    auto I = FilesAndDirs.find(Path.str());
    if (I == FilesAndDirs.end())
      return make_error_code(llvm::errc::no_such_file_or_directory);
    if (I->second.isSymlink()) {
      Output.clear();
      llvm::Twine("/symlink").toVector(Output);
      return std::error_code();
    }
    Output.clear();
    Path.toVector(Output);
    return std::error_code();
  }

  struct DirIterImpl : public llvm::vfs::detail::DirIterImpl {
    std::map<std::string, llvm::vfs::Status> &FilesAndDirs;
    std::map<std::string, llvm::vfs::Status>::iterator I;
    std::string Path;
    bool isInPath(llvm::StringRef S) {
      if (Path.size() < S.size() && S.starts_with(Path)) {
        auto LastSep = S.find_last_of('/');
        if (LastSep == Path.size() || LastSep == Path.size() - 1)
          return true;
      }
      return false;
    }
    DirIterImpl(std::map<std::string, llvm::vfs::Status> &FilesAndDirs,
                const llvm::Twine &_Path)
        : FilesAndDirs(FilesAndDirs), I(FilesAndDirs.begin()),
          Path(_Path.str()) {
      for (; I != FilesAndDirs.end(); ++I) {
        if (isInPath(I->first)) {
          CurrentEntry = llvm::vfs::directory_entry(
              std::string(I->second.getName()), I->second.getType());
          break;
        }
      }
    }
    std::error_code increment() override {
      ++I;
      for (; I != FilesAndDirs.end(); ++I) {
        if (isInPath(I->first)) {
          CurrentEntry = llvm::vfs::directory_entry(
              std::string(I->second.getName()), I->second.getType());
          break;
        }
      }
      if (I == FilesAndDirs.end())
        CurrentEntry = llvm::vfs::directory_entry();
      return std::error_code();
    }
  };

  llvm::vfs::directory_iterator dir_begin(const llvm::Twine &Dir,
                                          std::error_code &EC) override {
    return llvm::vfs::directory_iterator(
        std::make_shared<DirIterImpl>(FilesAndDirs, Dir));
  }

  void addEntry(llvm::StringRef Path, const llvm::vfs::Status &Status) {
    FilesAndDirs[std::string(Path)] = Status;
  }

  void addRegularFile(llvm::StringRef Path,
                      llvm::sys::fs::perms Perms = llvm::sys::fs::all_all) {
    llvm::vfs::Status S(Path, llvm::sys::fs::UniqueID(FSID, FileID++),
                        std::chrono::system_clock::now(), 0, 0, 1024,
                        llvm::sys::fs::file_type::regular_file, Perms);
    addEntry(Path, S);
  }

  void addDirectory(llvm::StringRef Path,
                    llvm::sys::fs::perms Perms = llvm::sys::fs::all_all) {
    llvm::vfs::Status S(Path, llvm::sys::fs::UniqueID(FSID, FileID++),
                        std::chrono::system_clock::now(), 0, 0, 0,
                        llvm::sys::fs::file_type::directory_file, Perms);
    addEntry(Path, S);
  }

  void addSymlink(llvm::StringRef Path) {
    llvm::vfs::Status S(Path, llvm::sys::fs::UniqueID(FSID, FileID++),
                        std::chrono::system_clock::now(), 0, 0, 0,
                        llvm::sys::fs::file_type::symlink_file,
                        llvm::sys::fs::all_all);
    addEntry(Path, S);
  }

  void addBrokenSymlink(llvm::StringRef Path) {
    llvm::vfs::Status S(Path, llvm::sys::fs::UniqueID(FSID, FileID++),
                        std::chrono::system_clock::now(), 0, 0, 0,
                        llvm::sys::fs::file_type::symlink_file,
                        llvm::sys::fs::no_perms);
    addEntry(Path, S);
  }
};
} // namespace lldb_private

#endif // LLDB_UNITTESTS_TESTINGSUPPORT_HOST_DUMMYFILESYSTEM_H
