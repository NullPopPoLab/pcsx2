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

#include "Threading.h"

using Threading::ScopedLock;

template <typename ListenerType>
typename EventSource<ListenerType>::ListenerIterator EventSource<ListenerType>::Add(ListenerType &listener)
{
    ScopedLock locker(m_listeners_lock);
    return _AddFast_without_lock(listener);
}

template <typename ListenerType>
void EventSource<ListenerType>::Remove(ListenerType &listener)
{
    ScopedLock locker(m_listeners_lock);
    m_cache_valid = false;
    m_listeners.remove(&listener);
}

template <typename ListenerType>
void EventSource<ListenerType>::Remove(const ListenerIterator &listenerHandle)
{
    ScopedLock locker(m_listeners_lock);
    m_cache_valid = false;
    m_listeners.erase(listenerHandle);
}

template <typename ListenerType>
typename EventSource<ListenerType>::ListenerIterator EventSource<ListenerType>::_AddFast_without_lock(ListenerType &listener)
{
    m_cache_valid = false;
    m_listeners.push_front(&listener);
    return m_listeners.begin();
}


template <typename ListenerType>
__fi void EventSource<ListenerType>::_DispatchRaw(ListenerIterator iter, const ListenerIterator &iend, const EvtParams &evtparams)
{
    while (iter != iend) {
        try {
            (*iter)->DispatchEvent(evtparams);
        } catch (Exception::RuntimeError &ex) {
		log_cb(RETRO_LOG_ERROR, "Ignoring runtime error thrown from event listener: %s\n", ex.FormatDiagnosticMessage().c_str());
        } catch (BaseException &ex) {
            log_cb(RETRO_LOG_ERROR, "Ignoring non-runtime BaseException thrown from event listener: %s\n", ex.FormatDiagnosticMessage().c_str());
        }
        ++iter;
    }
}

template <typename ListenerType>
void EventSource<ListenerType>::Dispatch(const EvtParams &evtparams)
{
    if (!m_cache_valid) {
        m_cache_copy = m_listeners;
        m_cache_valid = true;
    }

    if (m_cache_copy.empty())
        return;
    _DispatchRaw(m_cache_copy.begin(), m_cache_copy.end(), evtparams);
}
