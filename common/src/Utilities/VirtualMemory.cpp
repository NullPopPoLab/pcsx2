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

#include "PrecompiledHeader.h"
#include "PageFaultSource.h"

#ifndef __WXMSW__
#include <wx/thread.h>
#endif

#include "EventSource.inl"
#include "MemsetFast.inl"

template class EventSource<IEventListener_PageFault>;

SrcType_PageFault *Source_PageFault = NULL;
Threading::Mutex PageFault_Mutex;

void pxInstallSignalHandler()
{
    if (!Source_PageFault) {
        Source_PageFault = new SrcType_PageFault();
    }

    _platform_InstallSignalHandler();

    // NOP on Win32 systems -- we use __try{} __except{} instead.
}

// --------------------------------------------------------------------------------------
//  EventListener_PageFault  (implementations)
// --------------------------------------------------------------------------------------
EventListener_PageFault::EventListener_PageFault()
{
    pxAssert(Source_PageFault);
    Source_PageFault->Add(*this);
}

EventListener_PageFault::~EventListener_PageFault()
{
    if (Source_PageFault)
        Source_PageFault->Remove(*this);
}

void SrcType_PageFault::Dispatch(const PageFaultInfo &params)
{
    m_handled = false;
    _parent::Dispatch(params);
}

void SrcType_PageFault::_DispatchRaw(ListenerIterator iter, const ListenerIterator &iend, const PageFaultInfo &evt)
{
    do {
        (*iter)->DispatchEvent(evt, m_handled);
    } while ((++iter != iend) && !m_handled);
}

static size_t pageAlign(size_t size)
{
    return (size + __pagesize - 1) / __pagesize * __pagesize;
}

// --------------------------------------------------------------------------------------
//  VirtualMemoryManager  (implementations)
// --------------------------------------------------------------------------------------

VirtualMemoryManager::VirtualMemoryManager(uptr base, size_t size, uptr upper_bounds, bool strict)
    : m_baseptr(0), m_pageuse(nullptr), m_pages_reserved(0)
{
    if (!size) return;

    uptr reserved_bytes = pageAlign(size);
    m_pages_reserved = reserved_bytes / __pagesize;

    m_baseptr = (uptr)HostSys::MmapReserve(base, reserved_bytes);

    if (!m_baseptr || (upper_bounds != 0 && (((uptr)m_baseptr + reserved_bytes) > upper_bounds)))
    {
	    SafeSysMunmap(m_baseptr, reserved_bytes);

	    // Let's try again at an OS-picked memory area, 
            // and then hope it meets needed
	    // boundschecking criteria below.
	    if (base)
		    m_baseptr = (uptr)HostSys::MmapReserve(0, reserved_bytes);
    }

    bool fulfillsRequirements = true;
    if (strict && m_baseptr != base)
        fulfillsRequirements = false;
    if ((upper_bounds != 0) && ((m_baseptr + reserved_bytes) > upper_bounds))
        fulfillsRequirements = false;
    if (!fulfillsRequirements)
        SafeSysMunmap(m_baseptr, reserved_bytes);

    if (!m_baseptr) return;

    m_pageuse = new std::atomic<bool>[m_pages_reserved]();
}

VirtualMemoryManager::~VirtualMemoryManager()
{
    if (m_pageuse) delete[] m_pageuse;
    if (m_baseptr) HostSys::Munmap(m_baseptr, m_pages_reserved * __pagesize);
}

static bool VMMMarkPagesAsInUse(std::atomic<bool> *begin, std::atomic<bool> *end) {
    for (auto current = begin; current < end; current++) {
        bool expected = false;
        if (!current->compare_exchange_strong(expected, true), std::memory_order_relaxed) {
            // This was already allocated!  Undo the things we've set until this point
            while (--current >= begin) {
                if (!current->compare_exchange_strong(expected, false, std::memory_order_relaxed)) {
                    // In the time we were doing this, someone set one of the things we just set to true back to false
                    // This should never happen, but if it does we'll just stop and hope nothing bad happens
                    pxAssert(0);
                    return false;
                }
            }
            return false;
        }
    }
    return true;
}

void *VirtualMemoryManager::Alloc(uptr offsetLocation, size_t size) const
{
    size = pageAlign(size);
    if (!pxAssertDev(offsetLocation % __pagesize == 0))
        return nullptr;
    if (!pxAssertDev(size + offsetLocation <= m_pages_reserved * __pagesize))
        return nullptr;
    if (m_baseptr == 0)
        return nullptr;
    auto puStart = &m_pageuse[offsetLocation / __pagesize];
    auto puEnd = &m_pageuse[(offsetLocation+size) / __pagesize];
    if (!pxAssertDev(VMMMarkPagesAsInUse(puStart, puEnd)))
        return nullptr;
    return (void *)(m_baseptr + offsetLocation);
}

void VirtualMemoryManager::Free(void *address, size_t size) const
{
    uptr offsetLocation = (uptr)address - m_baseptr;
    if (!pxAssertDev(offsetLocation % __pagesize == 0))
    {
        uptr newLoc = pageAlign(offsetLocation);
        size -= (offsetLocation - newLoc);
        offsetLocation = newLoc;
    }
    if (!pxAssertDev(size % __pagesize == 0))
        size -= size % __pagesize;
    if (!pxAssertDev(size + offsetLocation <= m_pages_reserved * __pagesize))
        return;
    auto puStart = &m_pageuse[offsetLocation / __pagesize];
    auto puEnd = &m_pageuse[(offsetLocation+size) / __pagesize];
    for (; puStart < puEnd; puStart++) {
        bool expected = true;
        if (!puStart->compare_exchange_strong(expected, false, std::memory_order_relaxed)) { }
    }
}

// --------------------------------------------------------------------------------------
//  VirtualMemoryBumpAllocator  (implementations)
// --------------------------------------------------------------------------------------
VirtualMemoryBumpAllocator::VirtualMemoryBumpAllocator(VirtualMemoryManagerPtr allocator, uptr offsetLocation, size_t size)
    : m_allocator(std::move(allocator)), m_baseptr((uptr)m_allocator->Alloc(offsetLocation, size)), m_endptr(m_baseptr + size)
{
    if (m_baseptr.load() == 0)
        pxAssertDev(0);
}

void *VirtualMemoryBumpAllocator::Alloc(size_t size)
{
    if (m_baseptr.load() == 0) // True if constructed from bad VirtualMemoryManager (assertion was on initialization)
        return nullptr;

    size_t reservedSize = pageAlign(size);

    uptr out = m_baseptr.fetch_add(reservedSize, std::memory_order_relaxed);

    if (!pxAssertDev(out - reservedSize + size <= m_endptr))
        return nullptr;

    return (void *)out;
}

// --------------------------------------------------------------------------------------
//  VirtualMemoryReserve  (implementations)
// --------------------------------------------------------------------------------------
VirtualMemoryReserve::VirtualMemoryReserve(size_t size)
{
    m_defsize = size;

    m_allocator = nullptr;
    m_pages_commited = 0;
    m_pages_reserved = 0;
    m_baseptr = nullptr;
    m_prot_mode = PageAccess_None();
    m_allow_writes = true;
}

VirtualMemoryReserve &VirtualMemoryReserve::SetPageAccessOnCommit(const PageProtectionMode &mode)
{
    m_prot_mode = mode;
    return *this;
}

size_t VirtualMemoryReserve::GetSize(size_t requestedSize)
{
    if (!requestedSize)
        return pageAlign(m_defsize);
    return pageAlign(requestedSize);
}

// Notes:
//  * This method should be called if the object is already in an released (unreserved) state.
//    Subsequent calls will be ignored, and the existing reserve will be returned.
//
// Parameters:
//   baseptr - the new base pointer that's about to be assigned
//   size - size of the region pointed to by baseptr
//
void *VirtualMemoryReserve::Assign(VirtualMemoryManagerPtr allocator, void * baseptr, size_t size)
{
    if (!pxAssertDev(m_baseptr == NULL))
        return m_baseptr;

    if (!size)
        return nullptr;

    m_allocator = std::move(allocator);

    m_baseptr = baseptr;

    uptr reserved_bytes = pageAlign(size);
    m_pages_reserved = reserved_bytes / __pagesize;

    if (!m_baseptr)
        return nullptr;
    return m_baseptr;
}

void VirtualMemoryReserve::ReprotectCommittedBlocks(const PageProtectionMode &newmode)
{
    if (!m_pages_commited)
        return;
    HostSys::MemProtect(m_baseptr, m_pages_commited * __pagesize, newmode);
}

// Clears all committed blocks, restoring the allocation to a reserve only.
void VirtualMemoryReserve::Reset()
{
    if (!m_pages_commited)
        return;

    ReprotectCommittedBlocks(PageAccess_None());
    HostSys::MmapResetPtr(m_baseptr, m_pages_commited * __pagesize);
    m_pages_commited = 0;
}

void VirtualMemoryReserve::Release()
{
    if (!m_baseptr) return;
    Reset();
    m_allocator->Free(m_baseptr, m_pages_reserved * __pagesize);
    m_baseptr = nullptr;
}

bool VirtualMemoryReserve::Commit()
{
    if (!m_pages_reserved)
        return false;
    if (!pxAssert(!m_pages_commited))
        return true;

    m_pages_commited = m_pages_reserved;
    return HostSys::MmapCommitPtr(m_baseptr, m_pages_reserved * __pagesize, m_prot_mode);
}

void VirtualMemoryReserve::ForbidModification()
{
    m_allow_writes = false;
    HostSys::MemProtect(m_baseptr, m_pages_commited * __pagesize, PageProtectionMode(m_prot_mode).Write(false));
}


// If growing the array, or if shrinking the array to some point that's still *greater* than the
// committed memory range, then attempt a passive "on-the-fly" resize that maps/unmaps some portion
// of the reserve.
//
// If the above conditions are not met, or if the map/unmap fails, this method returns false.
// The caller will be responsible for manually resetting the reserve.
//
// Parameters:
//  newsize - new size of the reserved buffer, in bytes.
bool VirtualMemoryReserve::TryResize(uint newsize)
{
    uint newPages = pageAlign(newsize) / __pagesize;

    if (newPages > m_pages_reserved)
    {
	    uint toReservePages = newPages - m_pages_reserved;
	    uint toReserveBytes = toReservePages * __pagesize;

	    if (!m_allocator->AllocAtAddress(GetPtrEnd(), toReserveBytes))
		    return false;
    }
    else if (newPages < m_pages_reserved)
    {
	    if (m_pages_commited > newsize)
		    return false;

	    uint toRemovePages = m_pages_reserved - newPages;
	    uint toRemoveBytes = toRemovePages * __pagesize;

	    m_allocator->Free(GetPtrEnd() - toRemoveBytes, toRemoveBytes);
    }

    m_pages_reserved = newPages;
    return true;
}

// --------------------------------------------------------------------------------------
//  Common HostSys implementation
// --------------------------------------------------------------------------------------
void HostSys::Munmap(void *base, size_t size)
{
    Munmap((uptr)base, size);
}
