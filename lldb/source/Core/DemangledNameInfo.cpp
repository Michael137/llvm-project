//===-- DemangledNameInfo.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/DemangledNameInfo.h"

using namespace llvm::itanium_demangle;

namespace lldb_private {

bool TrackingOutputBuffer::shouldTrack() const {
  if (!isPrintingTopLevelFunctionType())
    return false;

  if (isGtInsideTemplateArgs())
    return false;

  if (NameInfo.ArgumentsRange)
    return false;

  return true;
}

bool TrackingOutputBuffer::canFinalize() const {
  if (!isPrintingTopLevelFunctionType())
    return false;

  if (isGtInsideTemplateArgs())
    return false;

  if (!NameInfo.ArgumentsRange)
    return false;

  return true;
}

void TrackingOutputBuffer::updateBasenameEnd() {
  if (!shouldTrack())
    return;

  initBasename();

  NameInfo.BasenameRange->second = getCurrentPosition();
}

void TrackingOutputBuffer::updateScopeStart() {
  if (!shouldTrack())
    return;

  initScope();

  NameInfo.ScopeRange->first = getCurrentPosition();
}

void TrackingOutputBuffer::updateScopeEnd() {
  if (!shouldTrack())
    return;

  initScope();

  NameInfo.ScopeRange->second = getCurrentPosition();
}

void TrackingOutputBuffer::finalizeArgumentEnd() {
  if (!canFinalize())
    return;

  initArguments();

  NameInfo.ArgumentsRange->second = getCurrentPosition();
}

void TrackingOutputBuffer::finalizeQualifiersStart() {
  if (!canFinalize())
    return;

  initQualifiers();

  NameInfo.QualifiersRange->first = getCurrentPosition();
}

void TrackingOutputBuffer::finalizeQualifiersEnd() {
  if (!canFinalize())
    return;

  initQualifiers();

  NameInfo.QualifiersRange->second = getCurrentPosition();
}

void TrackingOutputBuffer::finalizeStart() {
  if (!shouldTrack())
    return;

  initArguments();
  initBasename();

  NameInfo.ArgumentsRange->first = getCurrentPosition();

  // If nothing has set the end of the basename yet (for example when
  // printing templates), then the beginning of the arguments is the end of
  // the basename.
  if (NameInfo.BasenameRange->second == 0)
    NameInfo.BasenameRange->second = getCurrentPosition();

  assert(!shouldTrack());
  assert(canFinalize());
}

void TrackingOutputBuffer::finalizeEnd() {
  if (!canFinalize())
    return;

  initScope();
  initBasename();

  if (NameInfo.ScopeRange->first > NameInfo.ScopeRange->second)
    NameInfo.ScopeRange->second = NameInfo.ScopeRange->first;
  NameInfo.BasenameRange->first = NameInfo.ScopeRange->second;
}

ScopedOverride<unsigned> TrackingOutputBuffer::enterFunctionTypePrinting() {
  return {FunctionPrintingDepth, FunctionPrintingDepth + 1};
}

bool TrackingOutputBuffer::isPrintingTopLevelFunctionType() const {
  return FunctionPrintingDepth == 1;
}

void TrackingOutputBuffer::printLeft(const Node &N) {
  switch (N.getKind()) {
  case Node::KFunctionType:
    printLeftImpl(static_cast<const FunctionType &>(N));
    break;
  case Node::KFunctionEncoding:
    printLeftImpl(static_cast<const FunctionEncoding &>(N));
    break;
  case Node::KNestedName:
    printLeftImpl(static_cast<const NestedName &>(N));
    break;
  case Node::KNameWithTemplateArgs:
    printLeftImpl(static_cast<const NameWithTemplateArgs &>(N));
    break;
  default:
    OutputBuffer::printLeft(N);
  }
}

void TrackingOutputBuffer::printRight(const Node &N) {
  switch (N.getKind()) {
  case Node::KFunctionType:
    printRightImpl(static_cast<const FunctionType &>(N));
    break;
  case Node::KFunctionEncoding:
    printRightImpl(static_cast<const FunctionEncoding &>(N));
    break;
  default:
    OutputBuffer::printRight(N);
  }
}

void TrackingOutputBuffer::printLeftImpl(const FunctionType &N) {
  auto Scoped = enterFunctionTypePrinting();
  OutputBuffer::printLeft(N);
}

void TrackingOutputBuffer::printRightImpl(const FunctionType &N) {
  auto Scoped = enterFunctionTypePrinting();
  OutputBuffer::printRight(N);
}

void TrackingOutputBuffer::printLeftImpl(const FunctionEncoding &N) {
  auto Scoped = enterFunctionTypePrinting();

  const Node *Ret = N.getReturnType();
  if (Ret) {
    printLeft(*Ret);
    if (!Ret->hasRHSComponent(*this))
      *this += " ";
  }

  updateScopeStart();

  N.getName()->print(*this);
}

void TrackingOutputBuffer::printRightImpl(const FunctionEncoding &N) {
  auto Scoped = enterFunctionTypePrinting();
  finalizeStart();

  printOpen();
  N.getParams().printWithComma(*this);
  printClose();

  finalizeArgumentEnd();

  const Node *Ret = N.getReturnType();

  if (Ret)
    printRight(*Ret);

  finalizeQualifiersStart();

  auto CVQuals = N.getCVQuals();
  auto RefQual = N.getRefQual();
  auto *Attrs = N.getAttrs();
  auto *Requires = N.getRequires();

  if (CVQuals & QualConst)
    *this += " const";
  if (CVQuals & QualVolatile)
    *this += " volatile";
  if (CVQuals & QualRestrict)
    *this += " restrict";
  if (RefQual == FrefQualLValue)
    *this += " &";
  else if (RefQual == FrefQualRValue)
    *this += " &&";
  if (Attrs != nullptr)
    Attrs->print(*this);
  if (Requires != nullptr) {
    *this += " requires ";
    Requires->print(*this);
  }

  finalizeQualifiersEnd();
  finalizeEnd();
}

void TrackingOutputBuffer::printLeftImpl(const NestedName &N) {
  N.Qual->print(*this);
  *this += "::";
  updateScopeEnd();
  N.Name->print(*this);
  updateBasenameEnd();
}

void TrackingOutputBuffer::printLeftImpl(const NameWithTemplateArgs &N) {
  N.Name->print(*this);
  updateBasenameEnd();
  N.TemplateArgs->print(*this);
}

void TrackingOutputBuffer::notifyInsertion(size_t Position, size_t Count) {
  auto update = [&](std::optional<std::pair<size_t, size_t>> &range) {
    if (!range)
      return;

    if (range->first >= Position)
      range->first += Count;

    if (range->second >= Position)
      range->second += Count;
  };

  update(NameInfo.BasenameRange);
  update(NameInfo.ScopeRange);
  update(NameInfo.ArgumentsRange);
  update(NameInfo.QualifiersRange);
}

void TrackingOutputBuffer::notifyDeletion(size_t Old, size_t New) {
  // We only handle truncation.
  if (New >= Old)
    return;

  const auto diff = Old - New;

  auto update = [&](std::optional<std::pair<size_t, size_t>> &range) {
    if (!range)
      return;

    if (range->first >= New)
      range->first -= diff;

    if (range->second >= New)
      range->second -= diff;
  };

  update(NameInfo.BasenameRange);
  update(NameInfo.ScopeRange);
  update(NameInfo.ArgumentsRange);
  update(NameInfo.QualifiersRange);
}

void TrackingOutputBuffer::initBasename() {
  if (!NameInfo.BasenameRange)
    NameInfo.BasenameRange = {0, 0};
}

void TrackingOutputBuffer::initScope() {
  if (!NameInfo.ScopeRange)
    NameInfo.ScopeRange = {0, 0};
}

void TrackingOutputBuffer::initArguments() {
  if (!NameInfo.ArgumentsRange)
    NameInfo.ArgumentsRange = {0, 0};
}

void TrackingOutputBuffer::initQualifiers() {
  if (!NameInfo.QualifiersRange)
    NameInfo.QualifiersRange = {0, 0};
}

} // namespace lldb_private
