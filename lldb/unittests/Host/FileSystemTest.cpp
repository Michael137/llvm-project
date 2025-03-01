//===-- FileSystemTest.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "TestingSupport/Host/DummyFilesystem.h"

#include "lldb/Host/FileSystem.h"

extern const char *TestMainArgv0;

using namespace llvm;
using namespace lldb_private;

TEST(FileSystemTest, FileAndDirectoryComponents) {
  using namespace std::chrono;
  FileSystem fs;

#ifdef _WIN32
  FileSpec fs1("C:\\FILE\\THAT\\DOES\\NOT\\EXIST.TXT");
#else
  FileSpec fs1("/file/that/does/not/exist.txt");
#endif
  FileSpec fs2(TestMainArgv0);

  fs.Resolve(fs2);

  EXPECT_EQ(system_clock::time_point(), fs.GetModificationTime(fs1));
  EXPECT_LT(system_clock::time_point() + hours(24 * 365 * 20),
            fs.GetModificationTime(fs2));
}

static IntrusiveRefCntPtr<DummyFileSystem> GetSimpleDummyFS() {
  IntrusiveRefCntPtr<DummyFileSystem> D(new DummyFileSystem());
  D->addRegularFile("/foo");
  D->addDirectory("/bar");
  D->addSymlink("/baz");
  D->addBrokenSymlink("/lux");
  D->addRegularFile("/qux", ~sys::fs::perms::all_read);
  D->setCurrentWorkingDirectory("/");
  return D;
}

TEST(FileSystemTest, Exists) {
  FileSystem fs(GetSimpleDummyFS());

  EXPECT_TRUE(fs.Exists("/foo"));
  EXPECT_TRUE(fs.Exists(FileSpec("/foo", FileSpec::Style::posix)));
}

TEST(FileSystemTest, Readable) {
  FileSystem fs(GetSimpleDummyFS());

  EXPECT_TRUE(fs.Readable("/foo"));
  EXPECT_TRUE(fs.Readable(FileSpec("/foo", FileSpec::Style::posix)));

  EXPECT_FALSE(fs.Readable("/qux"));
  EXPECT_FALSE(fs.Readable(FileSpec("/qux", FileSpec::Style::posix)));
}

TEST(FileSystemTest, GetByteSize) {
  FileSystem fs(GetSimpleDummyFS());

  EXPECT_EQ((uint64_t)1024, fs.GetByteSize("/foo"));
  EXPECT_EQ((uint64_t)1024,
            fs.GetByteSize(FileSpec("/foo", FileSpec::Style::posix)));
}

TEST(FileSystemTest, GetPermissions) {
  FileSystem fs(GetSimpleDummyFS());

  EXPECT_EQ(sys::fs::all_all, fs.GetPermissions("/foo"));
  EXPECT_EQ(sys::fs::all_all,
            fs.GetPermissions(FileSpec("/foo", FileSpec::Style::posix)));
}

TEST(FileSystemTest, MakeAbsolute) {
  FileSystem fs(GetSimpleDummyFS());

  {
    StringRef foo_relative = "foo";
    SmallString<16> foo(foo_relative);
    auto EC = fs.MakeAbsolute(foo);
    EXPECT_FALSE(EC);
    EXPECT_TRUE(foo.equals("/foo"));
  }

  {
    FileSpec file_spec("foo");
    auto EC = fs.MakeAbsolute(file_spec);
    EXPECT_FALSE(EC);
    EXPECT_EQ(FileSpec("/foo"), file_spec);
  }
}

TEST(FileSystemTest, Resolve) {
  FileSystem fs(GetSimpleDummyFS());

  {
    StringRef foo_relative = "foo";
    SmallString<16> foo(foo_relative);
    fs.Resolve(foo);
    EXPECT_TRUE(foo.equals("/foo"));
  }

  {
    FileSpec file_spec("foo");
    fs.Resolve(file_spec);
    EXPECT_EQ(FileSpec("/foo"), file_spec);
  }

  {
    StringRef foo_relative = "bogus";
    SmallString<16> foo(foo_relative);
    fs.Resolve(foo);
    EXPECT_TRUE(foo.equals("bogus"));
  }

  {
    FileSpec file_spec("bogus");
    fs.Resolve(file_spec);
    EXPECT_EQ(FileSpec("bogus"), file_spec);
  }
}

FileSystem::EnumerateDirectoryResult
VFSCallback(void *baton, llvm::sys::fs::file_type file_type,
            llvm::StringRef path) {
  auto visited = static_cast<std::vector<std::string> *>(baton);
  visited->push_back(path.str());
  return FileSystem::eEnumerateDirectoryResultNext;
}

TEST(FileSystemTest, EnumerateDirectory) {
  FileSystem fs(GetSimpleDummyFS());

  std::vector<std::string> visited;

  constexpr bool find_directories = true;
  constexpr bool find_files = true;
  constexpr bool find_other = true;

  fs.EnumerateDirectory("/", find_directories, find_files, find_other,
                        VFSCallback, &visited);

  EXPECT_THAT(visited,
              testing::UnorderedElementsAre("/foo", "/bar", "/baz", "/qux"));
}

TEST(FileSystemTest, OpenErrno) {
#ifdef _WIN32
  FileSpec spec("C:\\FILE\\THAT\\DOES\\NOT\\EXIST.TXT");
#else
  FileSpec spec("/file/that/does/not/exist.txt");
#endif
  FileSystem fs;
  auto file = fs.Open(spec, File::eOpenOptionReadOnly, 0, true);
  ASSERT_FALSE(file);
  std::error_code code = errorToErrorCode(file.takeError());
  EXPECT_EQ(code.category(), std::system_category());
  EXPECT_EQ(code.value(), ENOENT);
}

TEST(FileSystemTest, EmptyTest) {
  FileSpec spec;
  FileSystem fs;

  {
    std::error_code ec;
    fs.DirBegin(spec, ec);
    EXPECT_EQ(ec.category(), std::system_category());
    EXPECT_EQ(ec.value(), ENOENT);
  }

  {
    llvm::ErrorOr<vfs::Status> status = fs.GetStatus(spec);
    ASSERT_FALSE(status);
    EXPECT_EQ(status.getError().category(), std::system_category());
    EXPECT_EQ(status.getError().value(), ENOENT);
  }

  EXPECT_EQ(sys::TimePoint<>(), fs.GetModificationTime(spec));
  EXPECT_EQ(static_cast<uint64_t>(0), fs.GetByteSize(spec));
  EXPECT_EQ(llvm::sys::fs::perms::perms_not_known, fs.GetPermissions(spec));
  EXPECT_FALSE(fs.Exists(spec));
  EXPECT_FALSE(fs.Readable(spec));
  EXPECT_FALSE(fs.IsDirectory(spec));
  EXPECT_FALSE(fs.IsLocal(spec));
}
