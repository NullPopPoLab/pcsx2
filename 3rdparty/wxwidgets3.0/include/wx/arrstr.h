///////////////////////////////////////////////////////////////////////////////
// Name:        wx/arrstr.h
// Purpose:     wxArrayString class
// Author:      Mattia Barbon and Vadim Zeitlin
// Modified by:
// Created:     07/07/03
// Copyright:   (c) 2003 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ARRSTR_H
#define _WX_ARRSTR_H

#include "wx/defs.h"
#include "wx/string.h"

// this shouldn't be defined for compilers not supporting template methods or
// without std::distance()
//
// FIXME-VC6: currently it's only not defined for VC6 in DLL build as it
//            doesn't export template methods from DLL correctly so even though
//            it compiles them fine, we get link errors when using wxArrayString
#define wxHAS_VECTOR_TEMPLATE_ASSIGN

#ifdef wxHAS_VECTOR_TEMPLATE_ASSIGN
    #include "wx/beforestd.h"
    #include <iterator>
    #include "wx/afterstd.h"
#endif // wxHAS_VECTOR_TEMPLATE_ASSIGN

class WXDLLIMPEXP_BASE wxArrayString
{
public:
  // type of function used by wxArrayString::Sort()
  typedef int (wxCMPFUNC_CONV *CompareFunction)(const wxString& first,
                                 const wxString& second);
  // type of function used by wxArrayString::Sort(), for compatibility with
  // wxArray
  typedef int (wxCMPFUNC_CONV *CompareFunction2)(wxString* first,
                                  wxString* second);

  // constructors and destructor
    // default ctor
  wxArrayString() { Init(false); }
    // if autoSort is true, the array is always sorted (in alphabetical order)
    //
    // NB: the reason for using int and not bool is that like this we can avoid
    //     using this ctor for implicit conversions from "const char *" (which
    //     we'd like to be implicitly converted to wxString instead!). This
    //     wouldn't be needed if the 'explicit' keyword was supported by all
    //     compilers, or if this was protected ctor for wxSortedArrayString,
    //     but we're stuck with it now.
  wxEXPLICIT wxArrayString(int autoSort) { Init(autoSort != 0); }
    // C string array ctor
  wxArrayString(size_t sz, const char** a);
  wxArrayString(size_t sz, const wchar_t** a);
    // wxString string array ctor
  wxArrayString(size_t sz, const wxString* a);
    // copy ctor
  wxArrayString(const wxArrayString& array);
    // assignment operator
  wxArrayString& operator=(const wxArrayString& src);
    // not virtual, this class should not be derived from
 ~wxArrayString();

  // memory management
    // empties the list, but doesn't release memory
  void Empty();
    // empties the list and releases memory
  void Clear();
    // preallocates memory for given number of items
  void Alloc(size_t nCount);
    // minimzes the memory usage (by freeing all extra memory)
  void Shrink();

  // simple accessors
    // number of elements in the array
  size_t GetCount() const { return m_nCount; }
    // is it empty?
  bool IsEmpty() const { return m_nCount == 0; }
    // number of elements in the array (GetCount is preferred API)
  size_t Count() const { return m_nCount; }

  // items access (range checking is done in debug version)
    // get item at position uiIndex
  wxString& Item(size_t nIndex)
    {
        return m_pItems[nIndex];
    }
  const wxString& Item(size_t nIndex) const { return const_cast<wxArrayString*>(this)->Item(nIndex); }

    // same as Item()
  wxString& operator[](size_t nIndex) { return Item(nIndex); }
  const wxString& operator[](size_t nIndex) const { return Item(nIndex); }
    // get last item
  wxString& Last()
  {
      return Item(GetCount() - 1);
  }
  const wxString& Last() const { return const_cast<wxArrayString*>(this)->Last(); }


  // item management
    // Search the element in the array, starting from the beginning if
    // bFromEnd is false or from end otherwise. If bCase, comparison is case
    // sensitive (default). Returns index of the first item matched or
    // wxNOT_FOUND
  int  Index (const wxString& str, bool bCase = true, bool bFromEnd = false) const;
    // add new element at the end (if the array is not sorted), return its
    // index
  size_t Add(const wxString& str, size_t nInsert = 1);
    // add new element at given position
  void Insert(const wxString& str, size_t uiIndex, size_t nInsert = 1);
    // expand the array to have count elements
  void SetCount(size_t count);
    // remove first item matching this value
  void Remove(const wxString& sz);
    // remove item by index
  void RemoveAt(size_t nIndex, size_t nRemove = 1);

  // sorting
    // sort array elements in alphabetical order (or reversed alphabetical
    // order if reverseOrder parameter is true)
  void Sort(bool reverseOrder = false);
    // sort array elements using specified comparison function
  void Sort(CompareFunction compareFunction);
  void Sort(CompareFunction2 compareFunction);

  // comparison
    // compare two arrays case sensitively
  bool operator==(const wxArrayString& a) const;
    // compare two arrays case sensitively
  bool operator!=(const wxArrayString& a) const { return !(*this == a); }

  // STL-like interface
  typedef wxString value_type;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef value_type* iterator;
  typedef const value_type* const_iterator;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef int difference_type;
  typedef size_t size_type;

  // TODO: this code duplicates the one in dynarray.h
  class reverse_iterator
  {
    typedef wxString value_type;
    typedef value_type* pointer;
    typedef value_type& reference;
    typedef reverse_iterator itor;
    friend itor operator+(int o, const itor& it);
    friend itor operator+(const itor& it, int o);
    friend itor operator-(const itor& it, int o);
    friend difference_type operator -(const itor& i1, const itor& i2);
  public:
    pointer m_ptr;
    reverse_iterator() : m_ptr(NULL) { }
    wxEXPLICIT reverse_iterator(pointer ptr) : m_ptr(ptr) { }
    reverse_iterator(const itor& it) : m_ptr(it.m_ptr) { }
    reference operator*() const { return *m_ptr; }
    pointer operator->() const { return m_ptr; }
    itor& operator++() { --m_ptr; return *this; }
    const itor operator++(int)
      { reverse_iterator tmp = *this; --m_ptr; return tmp; }
    itor& operator--() { ++m_ptr; return *this; }
    const itor operator--(int) { itor tmp = *this; ++m_ptr; return tmp; }
    bool operator ==(const itor& it) const { return m_ptr == it.m_ptr; }
    bool operator !=(const itor& it) const { return m_ptr != it.m_ptr; }
  };

  class const_reverse_iterator
  {
    typedef wxString value_type;
    typedef const value_type* pointer;
    typedef const value_type& reference;
    typedef const_reverse_iterator itor;
    friend itor operator+(int o, const itor& it);
    friend itor operator+(const itor& it, int o);
    friend itor operator-(const itor& it, int o);
    friend difference_type operator -(const itor& i1, const itor& i2);
  public:
    pointer m_ptr;
    const_reverse_iterator() : m_ptr(NULL) { }
    wxEXPLICIT const_reverse_iterator(pointer ptr) : m_ptr(ptr) { }
    const_reverse_iterator(const itor& it) : m_ptr(it.m_ptr) { }
    const_reverse_iterator(const reverse_iterator& it) : m_ptr(it.m_ptr) { }
    reference operator*() const { return *m_ptr; }
    pointer operator->() const { return m_ptr; }
    itor& operator++() { --m_ptr; return *this; }
    const itor operator++(int)
      { itor tmp = *this; --m_ptr; return tmp; }
    itor& operator--() { ++m_ptr; return *this; }
    const itor operator--(int) { itor tmp = *this; ++m_ptr; return tmp; }
    bool operator ==(const itor& it) const { return m_ptr == it.m_ptr; }
    bool operator !=(const itor& it) const { return m_ptr != it.m_ptr; }
  };

  wxArrayString(const_iterator first, const_iterator last)
    { Init(false); assign(first, last); }
  wxArrayString(size_type n, const_reference v) { Init(false); assign(n, v); }

#ifdef wxHAS_VECTOR_TEMPLATE_ASSIGN
  template <class Iterator>
  void assign(Iterator first, Iterator last)
  {
      clear();
      reserve(std::distance(first, last));
      for(; first != last; ++first)
          push_back(*first);
  }
#else // !wxHAS_VECTOR_TEMPLATE_ASSIGN
  void assign(const_iterator first, const_iterator last)
  {
      clear();
      reserve(last - first);
      for(; first != last; ++first)
          push_back(*first);
  }
#endif // wxHAS_VECTOR_TEMPLATE_ASSIGN/!wxHAS_VECTOR_TEMPLATE_ASSIGN

  void assign(size_type n, const_reference v)
    { clear(); Add(v, n); }
  reference back() { return *(end() - 1); }
  const_reference back() const { return *(end() - 1); }
  iterator begin() { return m_pItems; }
  const_iterator begin() const { return m_pItems; }
  size_type capacity() const { return m_nSize; }
  void clear() { Clear(); }
  bool empty() const { return IsEmpty(); }
  iterator end() { return begin() + GetCount(); }
  const_iterator end() const { return begin() + GetCount(); }
  iterator erase(iterator first, iterator last)
  {
      size_t idx = first - begin();
      RemoveAt(idx, last - first);
      return begin() + idx;
  }
  iterator erase(iterator it) { return erase(it, it + 1); }
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  void insert(iterator it, size_type n, const_reference v)
    { Insert(v, it - begin(), n); }
  iterator insert(iterator it, const_reference v = value_type())
    { size_t idx = it - begin(); Insert(v, idx); return begin() + idx; }
  void insert(iterator it, const_iterator first, const_iterator last);
  size_type max_size() const { return INT_MAX; }
  void pop_back() { RemoveAt(GetCount() - 1); }
  void push_back(const_reference v) { Add(v); }
  reverse_iterator rbegin() { return reverse_iterator(end() - 1); }
  const_reverse_iterator rbegin() const
    { return const_reverse_iterator(end() - 1); }
  reverse_iterator rend() { return reverse_iterator(begin() - 1); }
  const_reverse_iterator rend() const
    { return const_reverse_iterator(begin() - 1); }
  void reserve(size_type n) /* base::reserve*/;
  void resize(size_type n, value_type v = value_type());
  size_type size() const { return GetCount(); }
  void swap(wxArrayString& other)
  {
      wxSwap(m_nSize, other.m_nSize);
      wxSwap(m_nCount, other.m_nCount);
      wxSwap(m_pItems, other.m_pItems);
      wxSwap(m_autoSort, other.m_autoSort);
  }

protected:
  void Init(bool autoSort);             // common part of all ctors
  void Copy(const wxArrayString& src);  // copies the contents of another array

private:
  // Allocate the new buffer big enough to hold m_nCount + nIncrement items and
  // return the pointer to the old buffer, which must be deleted by the caller
  // (if the old buffer is big enough, just return NULL).
  wxString *Grow(size_t nIncrement);

  size_t  m_nSize,    // current size of the array
          m_nCount;   // current number of elements

  wxString *m_pItems; // pointer to data

  bool    m_autoSort; // if true, keep the array always sorted
};

#endif // _WX_ARRSTR_H
