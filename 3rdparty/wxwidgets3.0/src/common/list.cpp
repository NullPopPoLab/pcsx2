////////////////////////////////////////////////////////////////////////////////
// Name:        src/common/list.cpp
// Purpose:     wxList implementation
// Author:      Julian Smart
// Modified by: VZ at 16/11/98: WX_DECLARE_LIST() and typesafe lists added
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
////////////////////////////////////////////////////////////////////////////////

// =============================================================================
// declarations
// =============================================================================

// -----------------------------------------------------------------------------
// headers
// -----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef WX_PRECOMP
    #include "wx/list.h"
    #include "wx/crt.h"
#endif

// =============================================================================
// implementation
// =============================================================================

// -----------------------------------------------------------------------------
// wxListKey
// -----------------------------------------------------------------------------
wxListKey wxDefaultListKey;

bool wxListKey::operator==(wxListKeyValue value) const
{
    switch ( m_keyType )
    {
        default:
            // let compiler optimize the line above away in release build
            // by not putting return here...

        case wxKEY_STRING:
            return *m_key.string == *value.string;

        case wxKEY_INTEGER:
            return m_key.integer == value.integer;
    }
}

// -----------------------------------------------------------------------------
// wxNodeBase
// -----------------------------------------------------------------------------

wxNodeBase::wxNodeBase(wxListBase *list,
                       wxNodeBase *previous, wxNodeBase *next,
                       void *data, const wxListKey& key)
{
    m_list = list;
    m_data = data;
    m_previous = previous;
    m_next = next;

    switch ( key.GetKeyType() )
    {
        case wxKEY_NONE:
            break;

        case wxKEY_INTEGER:
            m_key.integer = key.GetNumber();
            break;

        case wxKEY_STRING:
            // to be free()d later
            m_key.string = new wxString(key.GetString());
            break;

        default:
	    break;
    }

    if ( previous )
        previous->m_next = this;

    if ( next )
        next->m_previous = this;
}

wxNodeBase::~wxNodeBase()
{
    // handle the case when we're being deleted from the list by the user (i.e.
    // not by the list itself from DeleteNode) - we must do it for
    // compatibility with old code
    if ( m_list != NULL )
    {
        if ( m_list->m_keyType == wxKEY_STRING )
        {
            delete m_key.string;
        }

        m_list->DetachNode(this);
    }
}

int wxNodeBase::IndexOf() const
{
    // It would be more efficient to implement IndexOf() completely inside
    // wxListBase (only traverse the list once), but this is probably a more
    // reusable way of doing it. Can always be optimized at a later date (since
    // IndexOf() resides in wxListBase as well) if efficiency is a problem.
    int i;
    wxNodeBase *prev = m_previous;

    for( i = 0; prev; i++ )
    {
        prev = prev->m_previous;
    }

    return i;
}

// -----------------------------------------------------------------------------
// wxListBase
// -----------------------------------------------------------------------------

void wxListBase::Init(wxKeyType keyType)
{
  m_nodeFirst =
  m_nodeLast = NULL;
  m_count = 0;
  m_destroy = false;
  m_keyType = keyType;
}

wxListBase::wxListBase(size_t count, void *elements[])
{
  Init();

  for ( size_t n = 0; n < count; n++ )
  {
      Append(elements[n]);
  }
}

void wxListBase::DoCopy(const wxListBase& list)
{
    m_destroy = list.m_destroy;
    m_keyType = list.m_keyType;
    m_nodeFirst =
    m_nodeLast = NULL;

    switch (m_keyType)
    {
        case wxKEY_INTEGER:
            {
                for ( wxNodeBase *node = list.GetFirst(); node; node = node->GetNext() )
                {
                    Append(node->GetKeyInteger(), node->GetData());
                }
                break;
            }

        case wxKEY_STRING:
            {
                for ( wxNodeBase *node = list.GetFirst(); node; node = node->GetNext() )
                {
                    Append(node->GetKeyString(), node->GetData());
                }
                break;
            }

        default:
            {
                for ( wxNodeBase *node = list.GetFirst(); node; node = node->GetNext() )
                {
                    Append(node->GetData());
                }
                break;
            }
    }
}

wxListBase::~wxListBase()
{
  wxNodeBase *each = m_nodeFirst;
  while ( each != NULL )
  {
      wxNodeBase *next = each->GetNext();
      DoDeleteNode(each);
      each = next;
  }
}

wxNodeBase *wxListBase::AppendCommon(wxNodeBase *node)
{
    if ( !m_nodeFirst )
    {
        m_nodeFirst = node;
        m_nodeLast = m_nodeFirst;
    }
    else
    {
        m_nodeLast->m_next = node;
        m_nodeLast = node;
    }

    m_count++;

    return node;
}

wxNodeBase *wxListBase::Append(void *object)
{
    // we use wxDefaultListKey even though it is the default parameter value
    // because gcc under Mac OS X seems to miscompile this call otherwise
    wxNodeBase *node = CreateNode(m_nodeLast, NULL, object,
                                  wxDefaultListKey);

    return AppendCommon(node);
}

wxNodeBase *wxListBase::Append(long key, void *object)
{
    wxNodeBase *node = CreateNode(m_nodeLast, NULL, object, key);
    return AppendCommon(node);
}

wxNodeBase *wxListBase::Append (const wxString& key, void *object)
{
    wxNodeBase *node = CreateNode(m_nodeLast, NULL, object, key);
    return AppendCommon(node);
}

wxNodeBase *wxListBase::Insert(wxNodeBase *position, void *object)
{
    // previous and next node for the node being inserted
    wxNodeBase *prev, *next;
    if ( position )
    {
        prev = position->GetPrevious();
        next = position;
    }
    else
    {
        // inserting in the beginning of the list
        prev = NULL;
        next = m_nodeFirst;
    }

    // wxDefaultListKey: see comment in Append() above
    wxNodeBase *node = CreateNode(prev, next, object, wxDefaultListKey);
    if ( !m_nodeFirst )
    {
        m_nodeLast = node;
    }

    if ( prev == NULL )
    {
        m_nodeFirst = node;
    }

    m_count++;

    return node;
}

wxNodeBase *wxListBase::Item(size_t n) const
{
    for ( wxNodeBase *current = GetFirst(); current; current = current->GetNext() )
    {
        if ( n-- == 0 )
            return current;
    }

    return NULL;
}

wxNodeBase *wxListBase::Find(const wxListKey& key) const
{
    for ( wxNodeBase *current = GetFirst(); current; current = current->GetNext() )
    {
        if ( key == current->m_key )
        {
            return current;
        }
    }

    // not found
    return NULL;
}

wxNodeBase *wxListBase::Find(const void *object) const
{
    for ( wxNodeBase *current = GetFirst(); current; current = current->GetNext() )
    {
        if ( current->GetData() == object )
            return current;
    }

    // not found
    return NULL;
}

int wxListBase::IndexOf(void *object) const
{
    wxNodeBase *node = Find( object );

    return node ? node->IndexOf() : wxNOT_FOUND;
}

void wxListBase::DoDeleteNode(wxNodeBase *node)
{
    // free node's data
    if ( m_keyType == wxKEY_STRING )
    {
        free(node->m_key.string);
    }

    if ( m_destroy )
    {
        node->DeleteData();
    }

    // so that the node knows that it's being deleted by the list
    node->m_list = NULL;
    delete node;
}

wxNodeBase *wxListBase::DetachNode(wxNodeBase *node)
{
    // update the list
    wxNodeBase **prevNext = node->GetPrevious() ? &node->GetPrevious()->m_next
                                                : &m_nodeFirst;
    wxNodeBase **nextPrev = node->GetNext() ? &node->GetNext()->m_previous
                                            : &m_nodeLast;

    *prevNext = node->GetNext();
    *nextPrev = node->GetPrevious();

    m_count--;

    // mark the node as not belonging to this list any more
    node->m_list = NULL;

    return node;
}

bool wxListBase::DeleteNode(wxNodeBase *node)
{
    if ( !DetachNode(node) )
        return false;

    DoDeleteNode(node);

    return true;
}

bool wxListBase::DeleteObject(void *object)
{
    for ( wxNodeBase *current = GetFirst(); current; current = current->GetNext() )
    {
        if ( current->GetData() == object )
        {
            DeleteNode(current);
            return true;
        }
    }

    // not found
    return false;
}

void wxListBase::Clear()
{
    wxNodeBase *current = m_nodeFirst;
    while ( current )
    {
        wxNodeBase *next = current->GetNext();
        DoDeleteNode(current);
        current = next;
    }

    m_nodeFirst =
    m_nodeLast = NULL;

    m_count = 0;
}

// (stefan.hammes@urz.uni-heidelberg.de)
//
// function for sorting lists. the concept is borrowed from 'qsort'.
// by giving a sort function, arbitrary lists can be sorted.
// method:
// - put wxObject pointers into an array
// - sort the array with qsort
// - put back the sorted wxObject pointers into the list
//
// CAVE: the sort function receives pointers to wxObject pointers (wxObject **),
//       so dereference right!
// EXAMPLE:
//   int listcompare(const void *arg1, const void *arg2)
//   {
//      return(compare(**(wxString **)arg1,
//                     **(wxString **)arg2));
//   }
//
//   void main()
//   {
//     wxListBase list;
//
//     list.Append(new wxString("DEF"));
//     list.Append(new wxString("GHI"));
//     list.Append(new wxString("ABC"));
//     list.Sort(listcompare);
//   }

void wxListBase::Sort(const wxSortCompareFunction compfunc)
{
    // allocate an array for the wxObject pointers of the list
    const size_t num = GetCount();
    void **objArray = new void *[num];
    void **objPtr = objArray;

    // go through the list and put the pointers into the array
    wxNodeBase *node;
    for ( node = GetFirst(); node; node = node->GetNext() )
    {
        *objPtr++ = node->GetData();
    }

    // sort the array
    qsort((void *)objArray,num,sizeof(wxObject *),
        compfunc);

    // put the sorted pointers back into the list
    objPtr = objArray;
    for ( node = GetFirst(); node; node = node->GetNext() )
    {
        node->SetData(*objPtr++);
    }

    // free the array
    delete[] objArray;
}

void wxListBase::Reverse()
{
    wxNodeBase* node = m_nodeFirst;
    wxNodeBase* tmp;

    while (node)
    {
        // swap prev and next pointers
        tmp = node->m_next;
        node->m_next = node->m_previous;
        node->m_previous = tmp;

        // this is the node that was next before swapping
        node = tmp;
    }

    // swap first and last node
    tmp = m_nodeFirst; m_nodeFirst = m_nodeLast; m_nodeLast = tmp;
}

void wxListBase::DeleteNodes(wxNodeBase* first, wxNodeBase* last)
{
    wxNodeBase* node = first;

    while (node != last)
    {
        wxNodeBase* next = node->GetNext();
        DeleteNode(node);
        node = next;
    }
}

// ============================================================================
// compatibility section from now on
// ============================================================================

#ifdef wxLIST_COMPATIBILITY

// -----------------------------------------------------------------------------
// wxList (a.k.a. wxObjectList)
// -----------------------------------------------------------------------------

wxList::wxList( int key_type )
    : wxObjectList( (wxKeyType)key_type )
{
}

void wxObjectListNode::DeleteData()
{
    delete (wxObject *)GetData();
}

// ----------------------------------------------------------------------------
// wxStringList
// ----------------------------------------------------------------------------

static inline wxChar* MYcopystring(const wxChar* s)
{
    wxChar* copy = new wxChar[wxStrlen(s) + 1];
    return wxStrcpy(copy, s);
}

// instead of WX_DEFINE_LIST(wxStringListBase) we define this function
// ourselves
void wxStringListNode::DeleteData()
{
    delete [] (char *)GetData();
}

bool wxStringList::Delete(const wxChar *s)
{
    wxStringListNode *current;

    for ( current = GetFirst(); current; current = current->GetNext() )
    {
        if ( wxStrcmp(current->GetData(), s) == 0 )
        {
            DeleteNode(current);
            return true;
        }
    }

    // not found
    return false;
}

void wxStringList::DoCopy(const wxStringList& other)
{
    size_t count = other.GetCount();
    for ( size_t n = 0; n < count; n++ )
        Add(other.Item(n)->GetData());
}

wxStringList::wxStringList()
{
    DeleteContents(true);
}

// Variable argument list, terminated by a zero
// Makes new storage for the strings
wxStringList::wxStringList (const wxChar *first, ...)
{
  DeleteContents(true);
  if ( !first )
    return;

  va_list ap;
  va_start(ap, first);

  const wxChar *s = first;
  for (;;)
  {
      Add(s);
      s = va_arg(ap, const wxChar *);
      if ( !s )
          break;
  }

  va_end(ap);
}

// Only makes new strings if arg is true
wxChar **wxStringList::ListToArray(bool new_copies) const
{
    wxChar **string_array = new wxChar *[GetCount()];
    wxStringListNode *node = GetFirst();
    for (size_t i = 0; i < GetCount(); i++)
    {
        wxChar *s = node->GetData();
        if ( new_copies )
            string_array[i] = MYcopystring(s);
        else
            string_array[i] = s;
        node = node->GetNext();
    }

    return string_array;
}

// Checks whether s is a member of the list
bool wxStringList::Member(const wxChar *s) const
{
    for ( wxStringListNode *node = GetFirst(); node; node = node->GetNext() )
    {
        const wxChar *s1 = node->GetData();
        if (s == s1 || wxStrcmp (s, s1) == 0)
            return true;
    }

    return false;
}

extern "C"
{
static int LINKAGEMODE

wx_comparestrings(const void *arg1, const void *arg2)
{
  wxChar **s1 = (wxChar **) arg1;
  wxChar **s2 = (wxChar **) arg2;

  return wxStrcmp (*s1, *s2);
}

}   // end of extern "C" (required because of GCC Bug c++/33078

// Sort a list of strings - deallocates old nodes, allocates new
void wxStringList::Sort()
{
    size_t N = GetCount();
    wxChar **array = new wxChar *[N];
    wxStringListNode *node;

    size_t i = 0;
    for ( node = GetFirst(); node; node = node->GetNext() )
    {
        array[i++] = node->GetData();
    }

    qsort (array, N, sizeof (wxChar *), wx_comparestrings);

    i = 0;
    for ( node = GetFirst(); node; node = node->GetNext() )
        node->SetData( array[i++] );

    delete [] array;
}

wxNode *wxStringList::Add(const wxChar *s)
{
    return (wxNode *)(wxStringListBase::Node *)
            wxStringListBase::Append(MYcopystring(s));
}

wxNode *wxStringList::Prepend(const wxChar *s)
{
    return (wxNode *)(wxStringListBase::Node *)
            wxStringListBase::Insert(MYcopystring(s));
}

#endif // wxLIST_COMPATIBILITY
