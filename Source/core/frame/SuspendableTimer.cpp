/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/frame/SuspendableTimer.h"

namespace blink {

namespace {
// The lowest value returned by TimerBase::nextUnalignedFireInterval is 0.0
const double kNextFireIntervalInvalid = -1.0;
}

SuspendableTimer::SuspendableTimer(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_nextFireInterval(kNextFireIntervalInvalid)
    , m_repeatInterval(0)
#if ENABLE(ASSERT)
    , m_suspended(false)
#endif
{
}

SuspendableTimer::~SuspendableTimer()
{
}

bool SuspendableTimer::hasPendingActivity() const
{
    return isActive();
}

void SuspendableTimer::stop()
{
    m_nextFireInterval = kNextFireIntervalInvalid;
    EventRacerTimerBase::stop();
}

void SuspendableTimer::suspend()
{
#if ENABLE(ASSERT)
    ASSERT(!m_suspended);
    m_suspended = true;
#endif
    if (isActive()) {
        m_nextFireInterval = nextUnalignedFireInterval();
        ASSERT(m_nextFireInterval >= 0.0);
        m_repeatInterval = repeatInterval();
        stop();
    }
}

void SuspendableTimer::resume()
{
#if ENABLE(ASSERT)
    ASSERT(m_suspended);
    m_suspended = false;
#endif
    if (m_nextFireInterval >= 0.0) {
        // start() was called before, therefore location() is already set.
        // m_nextFireInterval is only set in suspend() if the Timer was active.
        start(m_nextFireInterval, m_repeatInterval, location());
        m_nextFireInterval = kNextFireIntervalInvalid;
    }
}

} // namespace blink
