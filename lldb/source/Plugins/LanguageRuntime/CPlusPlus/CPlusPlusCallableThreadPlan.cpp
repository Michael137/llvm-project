//===-- CPlusPlusCallableThreadPlan.cpp -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CPlusPlusCallableThreadPlan.h"

#include "lldb/Target/ThreadPlanRunToAddress.h"
#include "lldb/Target/ThreadPlanStepUntil.h"
#include "lldb/Utility/LLDBLog.h"

using namespace lldb;
using namespace lldb_private;

uint32_t CPlusPlusCallableThreadPlan::s_default_flag_values = 0;

CPlusPlusCallableThreadPlan::CPlusPlusCallableThreadPlan(
    Thread &thread, Address callable_address, StackID return_stack_id,
    bool stop_others)
    : ThreadPlan(ThreadPlan::eKindGeneric, "Step through C++ callable glue",
                 thread, eVoteNoOpinion, eVoteNoOpinion),
      ThreadPlanShouldStopHere(this),
      m_callable_addr(std::move(callable_address)), m_stop_others(stop_others),
      m_func_sp(std::make_shared<ThreadPlanRunToAddress>(
          GetThread(), m_callable_addr, m_stop_others)),
      m_return_stack_id(std::move(return_stack_id)) {
  SetFlagsToDefault();

  ThreadPlanShouldStopHere::ThreadPlanShouldStopHereCallbacks callbacks(
      CPlusPlusCallableThreadPlan::DefaultShouldStopHereCallback, nullptr);
  SetShouldStopHereCallbacks(&callbacks, nullptr);
}

CPlusPlusCallableThreadPlan::~CPlusPlusCallableThreadPlan() = default;

bool CPlusPlusCallableThreadPlan::StopOthers() { return false; }

void CPlusPlusCallableThreadPlan::SetFlagsToDefault() {
  GetFlags().Set(CPlusPlusCallableThreadPlan::s_default_flag_values);
}

void CPlusPlusCallableThreadPlan::DidPush() {
  if (m_func_sp)
    PushPlan(m_func_sp);
}

static lldb::FrameComparison
CompareCurrentFrameToStartFrame(Thread &thread, StackID check_stack_id) {
  FrameComparison frame_order;
  StackID cur_frame_id = thread.GetStackFrameAtIndex(0)->GetStackID();

  if (cur_frame_id == check_stack_id) {
    frame_order = eFrameCompareEqual;
  } else if (cur_frame_id < check_stack_id) {
    frame_order = eFrameCompareYounger;
  } else {
    frame_order = eFrameCompareOlder;
  }
  return frame_order;
}

bool CPlusPlusCallableThreadPlan::IsBackAtCallsite(StackFrame *callsite_frame,
                                                   StackFrame *current_frame,
                                                   Thread &thread) {
  assert(callsite_frame != nullptr);
  assert(current_frame != nullptr);

  return current_frame->GetStackID() == callsite_frame->GetStackID();
}

bool CPlusPlusCallableThreadPlan::DefaultShouldStopHereCallback(
    ThreadPlan *current_plan, Flags &flags, lldb::FrameComparison operation,
    Status &status, void *baton) {
  StackFrame *frame = current_plan->GetThread().GetStackFrameAtIndex(0).get();
  Log *log = GetLog(LLDBLog::Step);
  bool should_stop_here = true;

  // First see if the ThreadPlanShouldStopHere default implementation thinks we
  // should get out of here (to handle [[nodebug]] attributes)
  if (!ThreadPlanShouldStopHere::DefaultShouldStopHereCallback(
          current_plan, flags, operation, status, baton))
    return false;

  // If frame is younger, allow stop
  if (operation == eFrameCompareYounger)
    return true;

  if (frame != nullptr && baton != nullptr) {
    StackID *return_stack_id = static_cast<StackID *>(baton);
    auto return_frame_sp =
        current_plan->GetThread().GetFrameWithStackID(*return_stack_id);
    // If we're back at our callsite, allow stop
    if (IsBackAtCallsite(return_frame_sp.get(), frame,
                         current_plan->GetThread()))
      return true;
  }

  return false;
}

bool CPlusPlusCallableThreadPlan::ShouldStop(Event *event_ptr) {
  // First stage: we are still handling the "run to an address"
  if (m_func_sp) {
    if (!m_func_sp->IsPlanComplete()) {
      return false;
    } else {
      if (!m_func_sp->PlanSucceeded()) {
        SetPlanComplete(false);
        return true;
      }

      // Throw away the run-to-address thread-plan but
      // don't mark the overall thread-plan as complete
      // yet since we want to get the caller back to
      // the callable's callsite.
      m_func_sp.reset();
    }
  }

  // Second stage, if all went well with the run-to-address,
  // stop the thread if we are still within the callable's
  // frame or deeper (e.g., if we stepped into a function
  // within the callable).
  //
  // If we stepped out of the frame, get us back to the
  // callsite (since we did all this work of hiding the
  // call-path from the user already).
  Log *log = GetLog(LLDBLog::Step);

  auto return_frame_sp = GetThread().GetFrameWithStackID(m_return_stack_id);
  auto curr_frame_sp = GetThread().GetStackFrameAtIndex(0);
  if (curr_frame_sp && return_frame_sp) {
    // If we're back at our callsite, allow stop
    if (IsBackAtCallsite(return_frame_sp.get(), curr_frame_sp.get(),
                         GetThread())) {
      SetPlanComplete();
      return true;
    }
  }

  m_return_plan_sp = CheckShouldStopHereAndQueueStepOut(
      CompareCurrentFrameToStartFrame(GetThread(), m_return_stack_id),
      m_status);

  return true;
}

void CPlusPlusCallableThreadPlan::GetDescription(Stream *s,
                                                 lldb::DescriptionLevel level) {
  assert(s != nullptr);
  if (level == lldb::eDescriptionLevelBrief)
    s->Printf("Step through");
  else {
    s->PutCString("Stepping through to callable address: ");

    if (level == eDescriptionLevelBrief)
      m_callable_addr.GetDescription(*s, GetTarget(), level);
  }
}

