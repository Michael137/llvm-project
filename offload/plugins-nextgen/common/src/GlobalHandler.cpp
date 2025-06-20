//===- GlobalHandler.cpp - Target independent global & env. var handling --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Target independent global handler and environment manager.
//
//===----------------------------------------------------------------------===//

#include "GlobalHandler.h"
#include "PluginInterface.h"
#include "Utils/ELF.h"

#include "Shared/Utils.h"

#include "llvm/ProfileData/InstrProfData.inc"
#include "llvm/Support/Error.h"

#include <cstring>
#include <string>

using namespace llvm;
using namespace omp;
using namespace target;
using namespace plugin;
using namespace error;

Expected<std::unique_ptr<ObjectFile>>
GenericGlobalHandlerTy::getELFObjectFile(DeviceImageTy &Image) {
  assert(utils::elf::isELF(Image.getMemoryBuffer().getBuffer()) &&
         "Input is not an ELF file");

  auto Expected =
      ELFObjectFileBase::createELFObjectFile(Image.getMemoryBuffer());
  if (!Expected) {
    return Plugin::error(ErrorCode::INVALID_BINARY, Expected.takeError(),
                         "error parsing binary");
  }
  return Expected;
}

Error GenericGlobalHandlerTy::moveGlobalBetweenDeviceAndHost(
    GenericDeviceTy &Device, DeviceImageTy &Image, const GlobalTy &HostGlobal,
    bool Device2Host) {

  GlobalTy DeviceGlobal(HostGlobal.getName(), HostGlobal.getSize());

  // Get the metadata from the global on the device.
  if (auto Err = getGlobalMetadataFromDevice(Device, Image, DeviceGlobal))
    return Err;

  // Perform the actual transfer.
  return moveGlobalBetweenDeviceAndHost(Device, HostGlobal, DeviceGlobal,
                                        Device2Host);
}

/// Actually move memory between host and device. See readGlobalFromDevice and
/// writeGlobalToDevice for the interface description.
Error GenericGlobalHandlerTy::moveGlobalBetweenDeviceAndHost(
    GenericDeviceTy &Device, const GlobalTy &HostGlobal,
    const GlobalTy &DeviceGlobal, bool Device2Host) {

  // Transfer the data from the source to the destination.
  if (Device2Host) {
    if (auto Err =
            Device.dataRetrieve(HostGlobal.getPtr(), DeviceGlobal.getPtr(),
                                HostGlobal.getSize(), nullptr))
      return Err;
  } else {
    if (auto Err = Device.dataSubmit(DeviceGlobal.getPtr(), HostGlobal.getPtr(),
                                     HostGlobal.getSize(), nullptr))
      return Err;
  }

  DP("Successfully %s %u bytes associated with global symbol '%s' %s the "
     "device "
     "(%p -> %p).\n",
     Device2Host ? "read" : "write", HostGlobal.getSize(),
     HostGlobal.getName().data(), Device2Host ? "from" : "to",
     DeviceGlobal.getPtr(), HostGlobal.getPtr());

  return Plugin::success();
}

bool GenericGlobalHandlerTy::isSymbolInImage(GenericDeviceTy &Device,
                                             DeviceImageTy &Image,
                                             StringRef SymName) {
  // Get the ELF object file for the image. Notice the ELF object may already
  // be created in previous calls, so we can reuse it. If this is unsuccessful
  // just return false as we couldn't find it.
  auto ELFObjOrErr = getELFObjectFile(Image);
  if (!ELFObjOrErr) {
    consumeError(ELFObjOrErr.takeError());
    return false;
  }

  // Search the ELF symbol using the symbol name.
  auto SymOrErr = utils::elf::getSymbol(**ELFObjOrErr, SymName);
  if (!SymOrErr) {
    consumeError(SymOrErr.takeError());
    return false;
  }

  return SymOrErr->has_value();
}

Error GenericGlobalHandlerTy::getGlobalMetadataFromImage(
    GenericDeviceTy &Device, DeviceImageTy &Image, GlobalTy &ImageGlobal) {

  // Get the ELF object file for the image. Notice the ELF object may already
  // be created in previous calls, so we can reuse it.
  auto ELFObj = getELFObjectFile(Image);
  if (!ELFObj)
    return ELFObj.takeError();

  // Search the ELF symbol using the symbol name.
  auto SymOrErr = utils::elf::getSymbol(**ELFObj, ImageGlobal.getName());
  if (!SymOrErr)
    return Plugin::error(
        ErrorCode::NOT_FOUND, "failed ELF lookup of global '%s': %s",
        ImageGlobal.getName().data(), toString(SymOrErr.takeError()).data());

  if (!SymOrErr->has_value())
    return Plugin::error(ErrorCode::NOT_FOUND,
                         "failed to find global symbol '%s' in the ELF image",
                         ImageGlobal.getName().data());

  auto AddrOrErr = utils::elf::getSymbolAddress(**SymOrErr);
  // Get the section to which the symbol belongs.
  if (!AddrOrErr)
    return Plugin::error(
        ErrorCode::NOT_FOUND, "failed to get ELF symbol from global '%s': %s",
        ImageGlobal.getName().data(), toString(AddrOrErr.takeError()).data());

  // Setup the global symbol's address and size.
  ImageGlobal.setPtr(const_cast<void *>(*AddrOrErr));
  ImageGlobal.setSize((*SymOrErr)->getSize());

  return Plugin::success();
}

Error GenericGlobalHandlerTy::readGlobalFromImage(GenericDeviceTy &Device,
                                                  DeviceImageTy &Image,
                                                  const GlobalTy &HostGlobal) {

  GlobalTy ImageGlobal(HostGlobal.getName(), -1);
  if (auto Err = getGlobalMetadataFromImage(Device, Image, ImageGlobal))
    return Err;

  if (ImageGlobal.getSize() != HostGlobal.getSize())
    return Plugin::error(ErrorCode::INVALID_BINARY,
                         "transfer failed because global symbol '%s' has "
                         "%u bytes in the ELF image but %u bytes on the host",
                         HostGlobal.getName().data(), ImageGlobal.getSize(),
                         HostGlobal.getSize());

  DP("Global symbol '%s' was found in the ELF image and %u bytes will copied "
     "from %p to %p.\n",
     HostGlobal.getName().data(), HostGlobal.getSize(), ImageGlobal.getPtr(),
     HostGlobal.getPtr());

  assert(Image.getStart() <= ImageGlobal.getPtr() &&
         utils::advancePtr(ImageGlobal.getPtr(), ImageGlobal.getSize()) <
             utils::advancePtr(Image.getStart(), Image.getSize()) &&
         "Attempting to read outside the image!");

  // Perform the copy from the image to the host memory.
  std::memcpy(HostGlobal.getPtr(), ImageGlobal.getPtr(), HostGlobal.getSize());

  return Plugin::success();
}

Expected<GPUProfGlobals>
GenericGlobalHandlerTy::readProfilingGlobals(GenericDeviceTy &Device,
                                             DeviceImageTy &Image) {
  GPUProfGlobals DeviceProfileData;
  auto ObjFile = getELFObjectFile(Image);
  if (!ObjFile)
    return ObjFile.takeError();

  std::unique_ptr<ELFObjectFileBase> ELFObj(
      static_cast<ELFObjectFileBase *>(ObjFile->release()));
  DeviceProfileData.TargetTriple = ELFObj->makeTriple();

  // Iterate through elf symbols
  for (auto &Sym : ELFObj->symbols()) {
    auto NameOrErr = Sym.getName();
    if (!NameOrErr)
      return NameOrErr.takeError();

    // Check if given current global is a profiling global based
    // on name
    if (*NameOrErr == getInstrProfNamesVarName()) {
      // Read in profiled function names from ELF
      auto SectionOrErr = Sym.getSection();
      if (!SectionOrErr)
        return SectionOrErr.takeError();

      auto ContentsOrErr = (*SectionOrErr)->getContents();
      if (!ContentsOrErr)
        return ContentsOrErr.takeError();

      SmallVector<uint8_t> NameBytes(ContentsOrErr->bytes());
      DeviceProfileData.NamesData = NameBytes;
    } else if (NameOrErr->starts_with(getInstrProfCountersVarPrefix())) {
      // Read global variable profiling counts
      SmallVector<int64_t> Counts(Sym.getSize() / sizeof(int64_t), 0);
      GlobalTy CountGlobal(NameOrErr->str(), Sym.getSize(), Counts.data());
      if (auto Err = readGlobalFromDevice(Device, Image, CountGlobal))
        return Err;
      DeviceProfileData.Counts.append(std::move(Counts));
    } else if (NameOrErr->starts_with(getInstrProfDataVarPrefix())) {
      // Read profiling data for this global variable
      __llvm_profile_data Data{};
      GlobalTy DataGlobal(NameOrErr->str(), Sym.getSize(), &Data);
      if (auto Err = readGlobalFromDevice(Device, Image, DataGlobal))
        return Err;
      DeviceProfileData.Data.push_back(std::move(Data));
    } else if (*NameOrErr == INSTR_PROF_QUOTE(INSTR_PROF_RAW_VERSION_VAR)) {
      uint64_t RawVersionData;
      GlobalTy RawVersionGlobal(NameOrErr->str(), Sym.getSize(),
                                &RawVersionData);
      if (auto Err = readGlobalFromDevice(Device, Image, RawVersionGlobal))
        return Err;
      DeviceProfileData.Version = RawVersionData;
    }
  }
  return DeviceProfileData;
}

void GPUProfGlobals::dump() const {
  outs() << "======= GPU Profile =======\nTarget: " << TargetTriple.str()
         << "\n";

  outs() << "======== Counters =========\n";
  for (size_t i = 0; i < Counts.size(); i++) {
    if (i > 0 && i % 10 == 0)
      outs() << "\n";
    else if (i != 0)
      outs() << " ";
    outs() << Counts[i];
  }
  outs() << "\n";

  outs() << "========== Data ===========\n";
  for (const auto &ProfData : Data) {
    outs() << "{ ";
// The ProfData.Name maybe array, eg: NumValueSites[IPVK_Last+1] .
// If we print out it directly, we are accessing out of bound data.
// Skip dumping the array for now.
#define INSTR_PROF_DATA(Type, LLVMType, Name, Initializer)                     \
  if (sizeof(#Name) > 2 && #Name[sizeof(#Name) - 2] == ']') {                  \
    outs() << "[...] ";                                                        \
  } else {                                                                     \
    outs() << ProfData.Name << " ";                                            \
  }
#include "llvm/ProfileData/InstrProfData.inc"
    outs() << "}\n";
  }

  outs() << "======== Functions ========\n";
  std::string s;
  s.reserve(NamesData.size());
  for (uint8_t Name : NamesData) {
    s.push_back((char)Name);
  }

  InstrProfSymtab Symtab;
  if (Error Err = Symtab.create(StringRef(s))) {
    consumeError(std::move(Err));
  }
  Symtab.dumpNames(outs());
  outs() << "===========================\n";
}

Error GPUProfGlobals::write() const {
  if (!__llvm_write_custom_profile)
    return Plugin::error(ErrorCode::INVALID_BINARY,
                         "could not find symbol __llvm_write_custom_profile. "
                         "The compiler-rt profiling library must be linked for "
                         "GPU PGO to work.");

  size_t DataSize = Data.size() * sizeof(__llvm_profile_data),
         CountsSize = Counts.size() * sizeof(int64_t);
  __llvm_profile_data *DataBegin, *DataEnd;
  char *CountersBegin, *CountersEnd, *NamesBegin, *NamesEnd;

  // Initialize array of contiguous data. We need to make sure each section is
  // contiguous so that the PGO library can compute deltas properly
  SmallVector<uint8_t> ContiguousData(NamesData.size() + DataSize + CountsSize);

  // Compute region pointers
  DataBegin = (__llvm_profile_data *)(ContiguousData.data() + CountsSize);
  DataEnd =
      (__llvm_profile_data *)(ContiguousData.data() + CountsSize + DataSize);
  CountersBegin = (char *)ContiguousData.data();
  CountersEnd = (char *)(ContiguousData.data() + CountsSize);
  NamesBegin = (char *)(ContiguousData.data() + CountsSize + DataSize);
  NamesEnd = (char *)(ContiguousData.data() + CountsSize + DataSize +
                      NamesData.size());

  // Copy data to contiguous buffer
  memcpy(DataBegin, Data.data(), DataSize);
  memcpy(CountersBegin, Counts.data(), CountsSize);
  memcpy(NamesBegin, NamesData.data(), NamesData.size());

  // Invoke compiler-rt entrypoint
  int result = __llvm_write_custom_profile(
      TargetTriple.str().c_str(), DataBegin, DataEnd, CountersBegin,
      CountersEnd, NamesBegin, NamesEnd, &Version);
  if (result != 0)
    return Plugin::error(ErrorCode::HOST_IO,
                         "error writing GPU PGO data to file");

  return Plugin::success();
}

bool GPUProfGlobals::empty() const {
  return Counts.empty() && Data.empty() && NamesData.empty();
}
