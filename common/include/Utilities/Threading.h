/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <semaphore.h>
#include <errno.h> // EBUSY
#include <pthread.h>

#ifdef __APPLE__
#include <mach/semaphore.h>
#endif

#include "Pcsx2Defs.h"

#undef Yield // release the burden of windows.h global namespace spam.

// --------------------------------------------------------------------------------------
//  PCSX2_THREAD_LOCAL - Defines platform/operating system support for Thread Local Storage
// --------------------------------------------------------------------------------------
//
// TLS is enabled by default. It will be disabled at compile time for Linux plugin.
// If you link SPU2X/ZZOGL with a TLS library, you will consume a DVT slots. Slots
// are rather limited and it ends up to "impossible to dlopen the library"
// None of the above plugin uses TLS variable in a multithread context
#ifndef PCSX2_THREAD_LOCAL
#define PCSX2_THREAD_LOCAL 1
#endif

class wxTimeSpan;

namespace Threading
{
class pxThread;
class RwMutex;
}

namespace Exception
{
class BaseThreadError : public RuntimeError
{
    DEFINE_EXCEPTION_COPYTORS(BaseThreadError, RuntimeError)
    DEFINE_EXCEPTION_MESSAGES(BaseThreadError)

public:
    Threading::pxThread *m_thread;

protected:
    BaseThreadError()
    {
        m_thread = NULL;
    }

public:
    explicit BaseThreadError(Threading::pxThread *_thread)
    {
        m_thread = _thread;
        m_message_diag = L"An unspecified thread-related error occurred (thread=%s)";
    }

    explicit BaseThreadError(Threading::pxThread &_thread)
    {
        m_thread = &_thread;
        m_message_diag = L"An unspecified thread-related error occurred (thread=%s)";
    }

    virtual wxString FormatDiagnosticMessage() const;

    Threading::pxThread &Thread();
    const Threading::pxThread &Thread() const;
};

class ThreadCreationError : public BaseThreadError
{
    DEFINE_EXCEPTION_COPYTORS(ThreadCreationError, BaseThreadError)

public:
    explicit ThreadCreationError(Threading::pxThread *_thread)
    {
        m_thread = _thread;
        SetBothMsgs(L"Thread creation failure.  An unspecified error occurred while trying to create the %s thread.");
    }

    explicit ThreadCreationError(Threading::pxThread &_thread)
    {
        m_thread = &_thread;
        SetBothMsgs(L"Thread creation failure.  An unspecified error occurred while trying to create the %s thread.");
    }
};
}


namespace Threading
{
// --------------------------------------------------------------------------------------
//  Platform Specific External APIs
// --------------------------------------------------------------------------------------
// The following set of documented functions have Linux/Win32 specific implementations,
// which are found in WinThreads.cpp and LnxThreads.cpp

// For use in spin/wait loops.
extern void SpinWait();

// sleeps the current thread for the given number of milliseconds.
extern void Sleep(int ms);

class Semaphore
{
protected:
#ifdef __APPLE__
    semaphore_t m_sema;
    int m_counter;
#else
    sem_t m_sema;
#endif

public:
    Semaphore();
    virtual ~Semaphore();

    void Reset();
    void Post();
    void Post(int multiple);

    void WaitWithoutYield();
    bool WaitWithoutYield(const wxTimeSpan &timeout);
    void WaitNoCancel();
    void WaitNoCancel(const wxTimeSpan &timeout);
    bool TryWait();
    int Count();

    void Wait();
    bool Wait(const wxTimeSpan &timeout);
};

class Mutex
{
protected:
    pthread_mutex_t m_mutex;

public:
    Mutex();
    virtual ~Mutex();
    virtual bool IsRecursive() const { return false; }

    bool RecreateIfLocked();
    void Detach();

    void Acquire();
    bool Acquire(const wxTimeSpan &timeout);
    bool TryAcquire();
    void Release();

    void AcquireWithoutYield();
    bool AcquireWithoutYield(const wxTimeSpan &timeout);

    void Wait();
    bool Wait(const wxTimeSpan &timeout);
    void WaitWithoutYield();
    bool WaitWithoutYield(const wxTimeSpan &timeout);

protected:
    // empty constructor used by MutexLockRecursive
    Mutex(bool) {}
};

class MutexRecursive : public Mutex
{
public:
    MutexRecursive();
    virtual ~MutexRecursive();
    virtual bool IsRecursive() const { return true; }
};

// --------------------------------------------------------------------------------------
//  ScopedLock
// --------------------------------------------------------------------------------------
// Helper class for using Mutexes.  Using this class provides an exception-safe (and
// generally clean) method of locking code inside a function or conditional block.  The lock
// will be automatically released on any return or exit from the function.
//
// Const qualification note:
//  ScopedLock takes const instances of the mutex, even though the mutex is modified
//  by locking and unlocking.  Two rationales:
//
//  1) when designing classes with accessors (GetString, GetValue, etc) that need mutexes,
//     this class needs a const hack to allow those accessors to be const (which is typically
//     *very* important).
//
//  2) The state of the Mutex is guaranteed to be unchanged when the calling function or
//     scope exits, by any means.  Only via manual calls to Release or Acquire does that
//     change, and typically those are only used in very special circumstances of their own.
//
class ScopedLock
{
    DeclareNoncopyableObject(ScopedLock);

protected:
    Mutex *m_lock;
    bool m_IsLocked;

public:
    virtual ~ScopedLock();
    explicit ScopedLock(const Mutex *locker = NULL);
    explicit ScopedLock(const Mutex &locker);
    void AssignAndLock(const Mutex &locker);
    void AssignAndLock(const Mutex *locker);

    void Release();
    void Acquire();

    bool IsLocked() const { return m_IsLocked; }

protected:
    // Special constructor used by ScopedTryLock
    ScopedLock(const Mutex &locker, bool isTryLock);
};

class ScopedTryLock : public ScopedLock
{
public:
    ScopedTryLock(const Mutex &locker)
        : ScopedLock(locker, true)
    {
    }
    virtual ~ScopedTryLock() = default;
    bool Failed() const { return !m_IsLocked; }
};

// --------------------------------------------------------------------------------------
//  ScopedLockBool
// --------------------------------------------------------------------------------------
// A ScopedLock in which you specify an external bool to get updated on locks/unlocks.
// Note that the isLockedBool should only be used as an indicator for the locked status,
// and not actually depended on for thread synchronization...

struct ScopedLockBool
{
    ScopedLock m_lock;
    std::atomic<bool> &m_bool;

    ScopedLockBool(Mutex &mutexToLock, std::atomic<bool> &isLockedBool)
        : m_lock(mutexToLock)
        , m_bool(isLockedBool)
    {
        m_bool.store(m_lock.IsLocked(), std::memory_order_relaxed);
    }
    virtual ~ScopedLockBool()
    {
        m_bool.store(false, std::memory_order_relaxed);
    }
    void Acquire()
    {
        m_lock.Acquire();
        m_bool.store(m_lock.IsLocked(), std::memory_order_relaxed);
    }
    void Release()
    {
        m_bool.store(false, std::memory_order_relaxed);
        m_lock.Release();
    }
};
}
