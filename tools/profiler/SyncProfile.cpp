/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SyncProfile.h"
#include "UnwinderThread2.h"

SyncProfile::SyncProfile(const char* aName, int aEntrySize, PseudoStack *aStack,
                         Thread::tid_t aThreadId, bool aIsMainThread)
  : ThreadProfile(aName, aEntrySize, aStack, aThreadId,
                 Sampler::AllocPlatformData(aThreadId), aIsMainThread,
                 tlsStackTop.get()),
    mOwnerState(REFERENCED),
    mUtb(nullptr)
{
}

SyncProfile::~SyncProfile()
{
  if (mUtb) {
    utb__release_sync_buffer(mUtb);
  }
  Sampler::FreePlatformData(GetPlatformData());
}

bool
SyncProfile::SetUWTBuffer(LinkedUWTBuffer* aBuff)
{
  MOZ_ASSERT(aBuff);
  mUtb = aBuff;
  return true;
}

bool
SyncProfile::ShouldDestroy()
{
  GetMutex()->AssertNotCurrentThreadOwns();
  mozilla::MutexAutoLock lock(*GetMutex());
  if (mOwnerState == OWNED) {
    mOwnerState = OWNER_DESTROYING;
    return true;
  }
  mOwnerState = ORPHANED;
  return false;
}

void
SyncProfile::EndUnwind()
{
  // Mutex must be held when this is called
  GetMutex()->AssertCurrentThreadOwns();
  if (mOwnerState != ORPHANED) {
    flush();
    mOwnerState = OWNED;
  }
  // Save mOwnerState before we release the mutex
  OwnerState ownerState = mOwnerState;
  ThreadProfile::EndUnwind();
  if (ownerState == ORPHANED) {
    delete this;
  }
}

