/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/string.cpp
// Purpose:     wxString class
// Author:      Vadim Zeitlin, Ryan Norton
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
//              (c) 2004 Ryan Norton <wxprojects@comcast.net>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// headers, declarations, constants
// ===========================================================================

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/wxcrtvararg.h"
#endif

#include <ctype.h>

#include <errno.h>

#include <string.h>
#include <stdlib.h>

#include "wx/hashmap.h"
#include "wx/vector.h"

#ifdef __WINDOWS__
    #include "wx/msw/wrapwin.h"
#endif // __WINDOWS__

#if wxUSE_STD_IOSTREAM
    #include <sstream>
#endif

// string handling functions used by wxString:
#if wxUSE_UNICODE_UTF8
    #define wxStringMemcpy   memcpy
    #define wxStringMemcmp   memcmp
    #define wxStringMemchr   memchr
    #define wxStringStrlen   strlen
#else
    #define wxStringMemcpy   wxTmemcpy
    #define wxStringMemcmp   wxTmemcmp
    #define wxStringMemchr   wxTmemchr
    #define wxStringStrlen   wxStrlen
#endif

// define a function declared in wx/buffer.h here as we don't have buffer.cpp
// and don't want to add it just because of this simple function
namespace wxPrivate
{

// wxXXXBuffer classes can be (implicitly) used during global statics
// initialization so wrap the status UntypedBufferData variable in a function
// to make it safe to access it even before all global statics are initialized
UntypedBufferData *GetUntypedNullData()
{
    static UntypedBufferData s_untypedNullData(NULL, 0);

    return &s_untypedNullData;
}

} // namespace wxPrivate

// ---------------------------------------------------------------------------
// static class variables definition
// ---------------------------------------------------------------------------

//According to STL _must_ be a -1 size_t
const size_t wxString::npos = (size_t) -1;

#if wxUSE_STRING_POS_CACHE

#ifdef wxHAS_COMPILER_TLS

wxTLS_TYPE(wxString::Cache) wxString::ms_cache;

#else // !wxHAS_COMPILER_TLS

struct wxStrCacheInitializer
{
    wxStrCacheInitializer()
    {
        // calling this function triggers s_cache initialization in it, and
        // from now on it becomes safe to call from multiple threads
        wxString::GetCache();
    }
};

static wxStrCacheInitializer gs_stringCacheInit;

#endif // wxHAS_COMPILER_TLS/!wxHAS_COMPILER_TLS

#endif // wxUSE_STRING_POS_CACHE

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

#if wxUSE_STD_IOSTREAM

#include <iostream>

wxSTD ostream& operator<<(wxSTD ostream& os, const wxCStrData& str)
{
#if wxUSE_UNICODE && !wxUSE_UNICODE_UTF8
    const wxScopedCharBuffer buf(str.AsCharBuf());
    if ( !buf )
        os.clear(wxSTD ios_base::failbit);
    else
        os << buf.data();

    return os;
#else
    return os << str.AsInternal();
#endif
}

wxSTD ostream& operator<<(wxSTD ostream& os, const wxString& str)
{
    return os << str.c_str();
}

wxSTD ostream& operator<<(wxSTD ostream& os, const wxScopedCharBuffer& str)
{
    return os << str.data();
}

wxSTD ostream& operator<<(wxSTD ostream& os, const wxScopedWCharBuffer& str)
{
    return os << str.data();
}

#if wxUSE_UNICODE && defined(HAVE_WOSTREAM)

wxSTD wostream& operator<<(wxSTD wostream& wos, const wxString& str)
{
    return wos << str.wc_str();
}

wxSTD wostream& operator<<(wxSTD wostream& wos, const wxCStrData& str)
{
    return wos << str.AsWChar();
}

wxSTD wostream& operator<<(wxSTD wostream& wos, const wxScopedWCharBuffer& str)
{
    return wos << str.data();
}

#endif  // wxUSE_UNICODE && defined(HAVE_WOSTREAM)

#endif // wxUSE_STD_IOSTREAM

// ===========================================================================
// wxString class core
// ===========================================================================

#if wxUSE_UNICODE_UTF8

void wxString::PosLenToImpl(size_t pos, size_t len,
                            size_t *implPos, size_t *implLen) const
{
    if ( pos == npos )
    {
        *implPos = npos;
    }
    else // have valid start position
    {
        const const_iterator b = GetIterForNthChar(pos);
        *implPos = wxStringImpl::const_iterator(b.impl()) - m_impl.begin();
        if ( len == npos )
        {
            *implLen = npos;
        }
        else // have valid length too
        {
            // we need to handle the case of length specifying a substring
            // going beyond the end of the string, just as std::string does
            const const_iterator e(end());
            const_iterator i(b);
            while ( len && i <= e )
            {
                ++i;
                --len;
            }

            *implLen = i.impl() - b.impl();
        }
    }
}

#endif // wxUSE_UNICODE_UTF8

// ----------------------------------------------------------------------------
// wxCStrData converted strings caching
// ----------------------------------------------------------------------------

// ===========================================================================
// wxString class core
// ===========================================================================

// ---------------------------------------------------------------------------
// construction and conversion
// ---------------------------------------------------------------------------

#if wxUSE_UNICODE_WCHAR
/* static */
wxString::SubstrBufFromMB wxString::ConvertStr(const char *psz, size_t nLength,
                                               const wxMBConv& conv)
{
    // anything to do?
    if ( !psz || nLength == 0 )
        return SubstrBufFromMB(wxWCharBuffer(L""), 0);

    if ( nLength == npos )
        nLength = wxNO_LEN;

    size_t wcLen;
    wxScopedWCharBuffer wcBuf(conv.cMB2WC(psz, nLength, &wcLen));
    if ( !wcLen )
        return SubstrBufFromMB(wxWCharBuffer(L""), 0);
    else
        return SubstrBufFromMB(wcBuf, wcLen);
}
#endif // wxUSE_UNICODE_WCHAR

#if wxUSE_UNICODE_UTF8
/* static */
wxString::SubstrBufFromMB wxString::ConvertStr(const char *psz, size_t nLength,
                                               const wxMBConv& conv)
{
    // anything to do?
    if ( !psz || nLength == 0 )
        return SubstrBufFromMB(wxCharBuffer(""), 0);

    // if psz is already in UTF-8, we don't have to do the roundtrip to
    // wchar_t* and back:
    if ( conv.IsUTF8() )
    {
        // we need to validate the input because UTF8 iterators assume valid
        // UTF-8 sequence and psz may be invalid:
        if ( wxStringOperations::IsValidUtf8String(psz, nLength) )
        {
            // we must pass the real string length to SubstrBufFromMB ctor
            if ( nLength == npos )
                nLength = psz ? strlen(psz) : 0;
            return SubstrBufFromMB(wxScopedCharBuffer::CreateNonOwned(psz, nLength),
                                   nLength);
        }
        // else: do the roundtrip through wchar_t*
    }

    if ( nLength == npos )
        nLength = wxNO_LEN;

    // first convert to wide string:
    size_t wcLen;
    wxScopedWCharBuffer wcBuf(conv.cMB2WC(psz, nLength, &wcLen));
    if ( !wcLen )
        return SubstrBufFromMB(wxCharBuffer(""), 0);

    // and then to UTF-8:
    SubstrBufFromMB buf(ConvertStr(wcBuf, wcLen, wxMBConvStrictUTF8()));
    return buf;
}
#endif // wxUSE_UNICODE_UTF8

#if wxUSE_UNICODE_UTF8 || !wxUSE_UNICODE
/* static */
wxString::SubstrBufFromWC wxString::ConvertStr(const wchar_t *pwz, size_t nLength,
                                               const wxMBConv& conv)
{
    // anything to do?
    if ( !pwz || nLength == 0 )
        return SubstrBufFromWC(wxCharBuffer(""), 0);

    if ( nLength == npos )
        nLength = wxNO_LEN;

    size_t mbLen;
    wxScopedCharBuffer mbBuf(conv.cWC2MB(pwz, nLength, &mbLen));
    if ( !mbLen )
        return SubstrBufFromWC(wxCharBuffer(""), 0);
    else
        return SubstrBufFromWC(mbBuf, mbLen);
}
#endif // wxUSE_UNICODE_UTF8 || !wxUSE_UNICODE

// This std::string::c_str()-like method returns a wide char pointer to string
// contents. In wxUSE_UNICODE_WCHAR case it is trivial as it can simply return
// a pointer to the internal representation. Otherwise a conversion is required
// and it returns a temporary buffer.
//
// However for compatibility with c_str() and to avoid breaking existing code
// doing
//
//      for ( const wchar_t *p = s.wc_str(); *p; p++ )
//          ... use *p...
//
// we actually need to ensure that the returned buffer is _not_ temporary and
// so we use wxString::m_convertedToWChar to store the returned data
#if !wxUSE_UNICODE_WCHAR

const wchar_t *wxString::AsWChar(const wxMBConv& conv) const
{
    const char * const strMB = m_impl.c_str();
    const size_t lenMB = m_impl.length();

    // find out the size of the buffer needed
    const size_t lenWC = conv.ToWChar(NULL, 0, strMB, lenMB);
    if ( lenWC == wxCONV_FAILED )
        return NULL;

    // keep the same buffer if the string size didn't change: this is not only
    // an optimization but also ensure that code which modifies string
    // character by character (without changing its length) can continue to use
    // the pointer returned by a previous wc_str() call even after changing the
    // string

    // TODO-UTF8: we could check for ">" instead of "!=" here as this would
    //            allow to save on buffer reallocations but at the cost of
    //            consuming (even) more memory, we should benchmark this to
    //            determine if it's worth doing
    if ( !m_convertedToWChar.m_str || lenWC != m_convertedToWChar.m_len )
    {
        if ( !const_cast<wxString *>(this)->m_convertedToWChar.Extend(lenWC) )
            return NULL;
    }

    // finally do convert
    m_convertedToWChar.m_str[lenWC] = L'\0';
    if ( conv.ToWChar(m_convertedToWChar.m_str, lenWC,
                      strMB, lenMB) == wxCONV_FAILED )
        return NULL;

    return m_convertedToWChar.m_str;
}

#endif // !wxUSE_UNICODE_WCHAR


// Same thing for mb_str() which returns a normal char pointer to string
// contents: this always requires converting it to the specified encoding in
// non-ANSI build except if we need to convert to UTF-8 and this is what we
// already use internally.
#if wxUSE_UNICODE

const char *wxString::AsChar(const wxMBConv& conv) const
{
#if wxUSE_UNICODE_UTF8
    if ( conv.IsUTF8() )
        return m_impl.c_str();

    const wchar_t * const strWC = AsWChar(wxMBConvStrictUTF8());
    const size_t lenWC = m_convertedToWChar.m_len;
#else // wxUSE_UNICODE_WCHAR
    const wchar_t * const strWC = m_impl.c_str();
    const size_t lenWC = m_impl.length();
#endif // wxUSE_UNICODE_UTF8/wxUSE_UNICODE_WCHAR

    const size_t lenMB = conv.FromWChar(NULL, 0, strWC, lenWC);
    if ( lenMB == wxCONV_FAILED )
        return NULL;

    if ( !m_convertedToChar.m_str || lenMB != m_convertedToChar.m_len )
    {
        if ( !const_cast<wxString *>(this)->m_convertedToChar.Extend(lenMB) )
            return NULL;
    }

    m_convertedToChar.m_str[lenMB] = '\0';
    if ( conv.FromWChar(m_convertedToChar.m_str, lenMB,
                        strWC, lenWC) == wxCONV_FAILED )
        return NULL;

    return m_convertedToChar.m_str;
}

#endif // wxUSE_UNICODE

// shrink to minimal size (releasing extra memory)
bool wxString::Shrink()
{
  wxString tmp(begin(), end());
  swap(tmp);
  return tmp.length() == length();
}

// ---------------------------------------------------------------------------
// data access
// ---------------------------------------------------------------------------

// all functions are inline in string.h

// ---------------------------------------------------------------------------
// concatenation operators
// ---------------------------------------------------------------------------

/*
 * concatenation functions come in 5 flavours:
 *  string + string
 *  char   + string      and      string + char
 *  C str  + string      and      string + C str
 */

wxString operator+(const wxString& str1, const wxString& str2)
{
    wxString s = str1;
    s += str2;
    return s;
}

wxString operator+(const wxString& str, wxUniChar ch)
{
    wxString s = str;
    s += ch;

    return s;
}

wxString operator+(wxUniChar ch, const wxString& str)
{
    wxString s = ch;
    s += str;

    return s;
}

wxString operator+(const wxString& str, const char *psz)
{
    wxString s;
    s.Alloc(strlen(psz) + str.length());
    s += str;
    s += psz;

    return s;
}

wxString operator+(const wxString& str, const wchar_t *pwz)
{
    wxString s;
    s.Alloc(wxWcslen(pwz) + str.length());
    s += str;
    s += pwz;

    return s;
}

wxString operator+(const char *psz, const wxString& str)
{
    wxString s;
    s.Alloc(strlen(psz) + str.length());
    s = psz;
    s += str;

    return s;
}

wxString operator+(const wchar_t *pwz, const wxString& str)
{
    wxString s;
    s.Alloc(wxWcslen(pwz) + str.length());
    s = pwz;
    s += str;

    return s;
}

// ---------------------------------------------------------------------------
// string comparison
// ---------------------------------------------------------------------------

bool wxString::IsSameAs(wxUniChar c, bool compareWithCase) const
{
    return (length() == 1) && (compareWithCase ? GetChar(0u) == c
                               : wxToupper(GetChar(0u)) == wxToupper(c));
}

#ifdef HAVE_STD_STRING_COMPARE

// NB: Comparison code (both if HAVE_STD_STRING_COMPARE and if not) works with
//     UTF-8 encoded strings too, thanks to UTF-8's design which allows us to
//     sort strings in characters code point order by sorting the byte sequence
//     in byte values order (i.e. what strcmp() and memcmp() do).

int wxString::compare(const wxString& str) const
{
    return m_impl.compare(str.m_impl);
}

int wxString::compare(size_t nStart, size_t nLen,
                      const wxString& str) const
{
    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);
    return m_impl.compare(pos, len, str.m_impl);
}

int wxString::compare(size_t nStart, size_t nLen,
                      const wxString& str,
                      size_t nStart2, size_t nLen2) const
{
    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);

    size_t pos2, len2;
    str.PosLenToImpl(nStart2, nLen2, &pos2, &len2);

    return m_impl.compare(pos, len, str.m_impl, pos2, len2);
}

int wxString::compare(const char* sz) const
{
    return m_impl.compare(ImplStr(sz));
}

int wxString::compare(const wchar_t* sz) const
{
    return m_impl.compare(ImplStr(sz));
}

int wxString::compare(size_t nStart, size_t nLen,
                      const char* sz, size_t nCount) const
{
    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);

    SubstrBufFromMB str(ImplStr(sz, nCount));

    return m_impl.compare(pos, len, str.data, str.len);
}

int wxString::compare(size_t nStart, size_t nLen,
                      const wchar_t* sz, size_t nCount) const
{
    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);

    SubstrBufFromWC str(ImplStr(sz, nCount));

    return m_impl.compare(pos, len, str.data, str.len);
}

#else // !HAVE_STD_STRING_COMPARE

static inline int wxDoCmp(const wxStringCharType* s1, size_t l1,
                          const wxStringCharType* s2, size_t l2)
{
    if( l1 == l2 )
        return wxStringMemcmp(s1, s2, l1);
    else if( l1 < l2 )
    {
        int ret = wxStringMemcmp(s1, s2, l1);
        return ret == 0 ? -1 : ret;
    }
    else
    {
        int ret = wxStringMemcmp(s1, s2, l2);
        return ret == 0 ? +1 : ret;
    }
}

int wxString::compare(const wxString& str) const
{
    return ::wxDoCmp(m_impl.data(), m_impl.length(),
                     str.m_impl.data(), str.m_impl.length());
}

int wxString::compare(size_t nStart, size_t nLen,
                      const wxString& str) const
{
    size_type strLen = length() - nStart;
    nLen = strLen < nLen ? strLen : nLen;

    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);

    return ::wxDoCmp(m_impl.data() + pos,  len,
                     str.m_impl.data(), str.m_impl.length());
}

int wxString::compare(size_t nStart, size_t nLen,
                      const wxString& str,
                      size_t nStart2, size_t nLen2) const
{
    size_type strLen  =     length() - nStart,
              strLen2 = str.length() - nStart2;
    nLen  = strLen  < nLen  ? strLen  : nLen;
    nLen2 = strLen2 < nLen2 ? strLen2 : nLen2;

    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);
    size_t pos2, len2;
    str.PosLenToImpl(nStart2, nLen2, &pos2, &len2);

    return ::wxDoCmp(m_impl.data() + pos, len,
                     str.m_impl.data() + pos2, len2);
}

int wxString::compare(const char* sz) const
{
    SubstrBufFromMB str(ImplStr(sz, npos));
    if ( str.len == npos )
        str.len = wxStringStrlen(str.data);
    return ::wxDoCmp(m_impl.data(), m_impl.length(), str.data, str.len);
}

int wxString::compare(const wchar_t* sz) const
{
    SubstrBufFromWC str(ImplStr(sz, npos));
    if ( str.len == npos )
        str.len = wxStringStrlen(str.data);
    return ::wxDoCmp(m_impl.data(), m_impl.length(), str.data, str.len);
}

int wxString::compare(size_t nStart, size_t nLen,
                      const char* sz, size_t nCount) const
{
    size_type strLen = length() - nStart;
    nLen = strLen < nLen ? strLen : nLen;

    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);

    SubstrBufFromMB str(ImplStr(sz, nCount));
    if ( str.len == npos )
        str.len = wxStringStrlen(str.data);

    return ::wxDoCmp(m_impl.data() + pos, len, str.data, str.len);
}

int wxString::compare(size_t nStart, size_t nLen,
                      const wchar_t* sz, size_t nCount) const
{
    size_type strLen = length() - nStart;
    nLen = strLen < nLen ? strLen : nLen;

    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);

    SubstrBufFromWC str(ImplStr(sz, nCount));
    if ( str.len == npos )
        str.len = wxStringStrlen(str.data);

    return ::wxDoCmp(m_impl.data() + pos, len, str.data, str.len);
}

#endif // HAVE_STD_STRING_COMPARE/!HAVE_STD_STRING_COMPARE


// ---------------------------------------------------------------------------
// find_{first,last}_[not]_of functions
// ---------------------------------------------------------------------------

#if !wxUSE_STL_BASED_WXSTRING || wxUSE_UNICODE_UTF8

// NB: All these functions are implemented  with the argument being wxChar*,
//     i.e. widechar string in any Unicode build, even though native string
//     representation is char* in the UTF-8 build. This is because we couldn't
//     use memchr() to determine if a character is in a set encoded as UTF-8.

size_t wxString::find_first_of(const wxChar* sz, size_t nStart) const
{
    return find_first_of(sz, nStart, wxStrlen(sz));
}

size_t wxString::find_first_not_of(const wxChar* sz, size_t nStart) const
{
    return find_first_not_of(sz, nStart, wxStrlen(sz));
}

size_t wxString::find_first_of(const wxChar* sz, size_t nStart, size_t n) const
{
    size_t idx = nStart;
    for ( const_iterator i = begin() + nStart; i != end(); ++idx, ++i )
    {
        if ( wxTmemchr(sz, *i, n) )
            return idx;
    }

    return npos;
}

size_t wxString::find_first_not_of(const wxChar* sz, size_t nStart, size_t n) const
{
    size_t idx = nStart;
    for ( const_iterator i = begin() + nStart; i != end(); ++idx, ++i )
    {
        if ( !wxTmemchr(sz, *i, n) )
            return idx;
    }

    return npos;
}


size_t wxString::find_last_of(const wxChar* sz, size_t nStart) const
{
    return find_last_of(sz, nStart, wxStrlen(sz));
}

size_t wxString::find_last_not_of(const wxChar* sz, size_t nStart) const
{
    return find_last_not_of(sz, nStart, wxStrlen(sz));
}

size_t wxString::find_last_of(const wxChar* sz, size_t nStart, size_t n) const
{
    size_t len = length();

    if ( nStart == npos )
        nStart = len - 1;

    size_t idx = nStart;
    for ( const_reverse_iterator i = rbegin() + (len - nStart - 1);
          i != rend(); --idx, ++i )
    {
        if ( wxTmemchr(sz, *i, n) )
            return idx;
    }

    return npos;
}

size_t wxString::find_last_not_of(const wxChar* sz, size_t nStart, size_t n) const
{
    size_t len = length();

    if ( nStart == npos )
        nStart = len - 1;

    size_t idx = nStart;
    for ( const_reverse_iterator i = rbegin() + (len - nStart - 1);
          i != rend(); --idx, ++i )
    {
        if ( !wxTmemchr(sz, *i, n) )
            return idx;
    }

    return npos;
}

size_t wxString::find_first_not_of(wxUniChar ch, size_t nStart) const
{
    size_t idx = nStart;
    for ( const_iterator i = begin() + nStart; i != end(); ++idx, ++i )
    {
        if ( *i != ch )
            return idx;
    }

    return npos;
}

size_t wxString::find_last_not_of(wxUniChar ch, size_t nStart) const
{
    size_t len = length();

    if ( nStart == npos )
        nStart = len - 1;

    size_t idx = nStart;
    for ( const_reverse_iterator i = rbegin() + (len - nStart - 1);
          i != rend(); --idx, ++i )
    {
        if ( *i != ch )
            return idx;
    }

    return npos;
}

// the functions above were implemented for wchar_t* arguments in Unicode
// build and char* in ANSI build; below are implementations for the other
// version:
#if wxUSE_UNICODE
    #define wxOtherCharType char
    #define STRCONV         (const wxChar*)wxConvLibc.cMB2WC
#else
    #define wxOtherCharType wchar_t
    #define STRCONV         (const wxChar*)wxConvLibc.cWC2MB
#endif

size_t wxString::find_first_of(const wxOtherCharType* sz, size_t nStart) const
    { return find_first_of(STRCONV(sz), nStart); }

size_t wxString::find_first_of(const wxOtherCharType* sz, size_t nStart,
                               size_t n) const
    { return find_first_of(STRCONV(sz, n, NULL), nStart, n); }
size_t wxString::find_last_of(const wxOtherCharType* sz, size_t nStart) const
    { return find_last_of(STRCONV(sz), nStart); }
size_t wxString::find_last_of(const wxOtherCharType* sz, size_t nStart,
                              size_t n) const
    { return find_last_of(STRCONV(sz, n, NULL), nStart, n); }
size_t wxString::find_first_not_of(const wxOtherCharType* sz, size_t nStart) const
    { return find_first_not_of(STRCONV(sz), nStart); }
size_t wxString::find_first_not_of(const wxOtherCharType* sz, size_t nStart,
                                   size_t n) const
    { return find_first_not_of(STRCONV(sz, n, NULL), nStart, n); }
size_t wxString::find_last_not_of(const wxOtherCharType* sz, size_t nStart) const
    { return find_last_not_of(STRCONV(sz), nStart); }
size_t wxString::find_last_not_of(const wxOtherCharType* sz, size_t nStart,
                                  size_t n) const
    { return find_last_not_of(STRCONV(sz, n, NULL), nStart, n); }

#undef wxOtherCharType
#undef STRCONV

#endif // !wxUSE_STL_BASED_WXSTRING || wxUSE_UNICODE_UTF8

// ===========================================================================
// other common string functions
// ===========================================================================

int wxString::CmpNoCase(const wxString& s) const
{
#if !wxUSE_UNICODE_UTF8
    // We compare NUL-delimited chunks of the strings inside the loop. We will
    // do as many iterations as there are embedded NULs in the string, i.e.
    // usually we will run it just once.

    typedef const wxStringImpl::value_type *pchar_type;
    const pchar_type thisBegin = m_impl.c_str();
    const pchar_type thatBegin = s.m_impl.c_str();

    const pchar_type thisEnd = thisBegin + m_impl.length();
    const pchar_type thatEnd = thatBegin + s.m_impl.length();

    pchar_type thisCur = thisBegin;
    pchar_type thatCur = thatBegin;

    int rc;
    for ( ;; )
    {
        // Compare until the next NUL, if the strings differ this is the final
        // result.
        rc = wxStricmp(thisCur, thatCur);
        if ( rc )
            break;

        const size_t lenChunk = wxStrlen(thisCur);
        thisCur += lenChunk;
        thatCur += lenChunk;

        // Skip all the NULs as wxStricmp() doesn't handle them.
        for ( ; !*thisCur; thisCur++, thatCur++ )
        {
            // Check if we exhausted either of the strings.
            if ( thisCur == thisEnd )
            {
                // This one is exhausted, is the other one too?
                return thatCur == thatEnd ? 0 : -1;
            }

            if ( thatCur == thatEnd )
            {
                // Because of the test above we know that this one is not
                // exhausted yet so it's greater than the other one that is.
                return 1;
            }

            if ( *thatCur )
            {
                // Anything non-NUL is greater than NUL.
                return -1;
            }
        }
    }

    return rc;
#else // wxUSE_UNICODE_UTF8
    // CRT functions can't be used for case-insensitive comparison of UTF-8
    // strings so do it in the naive, simple and inefficient way.

    // FIXME-UTF8: use wxUniChar::ToLower/ToUpper once added
    const_iterator i1 = begin();
    const_iterator end1 = end();
    const_iterator i2 = s.begin();
    const_iterator end2 = s.end();

    for ( ; i1 != end1 && i2 != end2; ++i1, ++i2 )
    {
        wxUniChar lower1 = (wxChar)wxTolower(*i1);
        wxUniChar lower2 = (wxChar)wxTolower(*i2);
        if ( lower1 != lower2 )
            return lower1 < lower2 ? -1 : 1;
    }

    size_t len1 = length();
    size_t len2 = s.length();

    if ( len1 < len2 )
        return -1;
    else if ( len1 > len2 )
        return 1;
    return 0;
#endif // !wxUSE_UNICODE_UTF8/wxUSE_UNICODE_UTF8
}


#if wxUSE_UNICODE

wxString wxString::FromAscii(const char *ascii, size_t len)
{
    if (!ascii || len == 0)
       return wxEmptyString;

    wxString res;

    {
        wxStringInternalBuffer buf(res, len);
        wxStringCharType *dest = buf;

        for ( ; len > 0; --len )
        {
            unsigned char c = (unsigned char)*ascii++;
            *dest++ = (wchar_t)c;
        }
    }

    return res;
}

wxString wxString::FromAscii(const char *ascii)
{
    return FromAscii(ascii, wxStrlen(ascii));
}

wxString wxString::FromAscii(char ascii)
{
    // What do we do with '\0' ?

    unsigned char c = (unsigned char)ascii;

    // NB: the cast to wchar_t causes interpretation of 'ascii' as Latin1 value
    return wxString(wxUniChar((wchar_t)c));
}

const wxScopedCharBuffer wxString::ToAscii() const
{
    // this will allocate enough space for the terminating NUL too
    wxCharBuffer buffer(length());
    char *dest = buffer.data();

    for ( const_iterator i = begin(); i != end(); ++i )
    {
        wxUniChar c(*i);
        // FIXME-UTF8: unify substituted char ('_') with wxUniChar ('?')
        *dest++ = c.IsAscii() ? (char)c : '_';

        // the output string can't have embedded NULs anyhow, so we can safely
        // stop at first of them even if we do have any
        if ( !c )
            break;
    }

    return buffer;
}

#endif // wxUSE_UNICODE

// extract string of length nCount starting at nFirst
wxString wxString::Mid(size_t nFirst, size_t nCount) const
{
    size_t nLen = length();

    // default value of nCount is npos and means "till the end"
    if ( nCount == npos )
        nCount = nLen - nFirst;

    // out-of-bounds requests return sensible things
    // AllocCopy() will return empty string
    if ( nFirst > nLen )
        return wxEmptyString;

    if ( nCount > nLen - nFirst )
        nCount = nLen - nFirst;

    wxString dest(*this, nFirst, nCount);

    return dest;
}

// check that the string starts with prefix and return the rest of the string
// in the provided pointer if it is not NULL, otherwise return false
bool wxString::StartsWith(const wxString& prefix, wxString *rest) const
{
    if ( compare(0, prefix.length(), prefix) != 0 )
        return false;

    if ( rest )
    {
        // put the rest of the string into provided pointer
        rest->assign(*this, prefix.length(), npos);
    }

    return true;
}


// check that the string ends with suffix and return the rest of it in the
// provided pointer if it is not NULL, otherwise return false
bool wxString::EndsWith(const wxString& suffix, wxString *rest) const
{
    int start = length() - suffix.length();

    if ( start < 0 || compare(start, npos, suffix) != 0 )
        return false;

    if ( rest )
    {
        // put the rest of the string into provided pointer
        rest->assign(*this, 0, start);
    }

    return true;
}

// get all characters after the last occurrence of ch
// (returns the whole string if ch not found)
wxString wxString::AfterLast(wxUniChar ch) const
{
  wxString str;
  int iPos = Find(ch, true);
  if ( iPos == wxNOT_FOUND )
    str = *this;
  else
    str.assign(*this, iPos + 1, npos);

  return str;
}

// extract nCount first (leftmost) characters
wxString wxString::Left(size_t nCount) const
{
  if ( nCount > length() )
    nCount = length();
  wxString dest(*this, 0, nCount);
  return dest;
}

// get all characters before the first occurrence of ch
// (returns the whole string if ch not found)
wxString wxString::BeforeFirst(wxUniChar ch, wxString *rest) const
{
  int iPos = Find(ch);
  if ( iPos == wxNOT_FOUND )
  {
    iPos = length();
    if ( rest )
      rest->clear();
  }
  else
  {
    if ( rest )
      rest->assign(*this, iPos + 1, npos);
  }

  return wxString(*this, 0, iPos);
}

/// get all characters before the last occurrence of ch
/// (returns empty string if ch not found)
wxString wxString::BeforeLast(wxUniChar ch, wxString *rest) const
{
  wxString str;
  int iPos = Find(ch, true);
  if ( iPos != wxNOT_FOUND )
  {
    if ( iPos != 0 )
      str.assign(*this, 0, iPos);

    if ( rest )
      rest->assign(*this, iPos + 1, npos);
  }
  else
  {
    if ( rest )
      *rest = *this;
  }

  return str;
}

/// get all characters after the first occurrence of ch
/// (returns empty string if ch not found)
wxString wxString::AfterFirst(wxUniChar ch) const
{
  wxString str;
  int iPos = Find(ch);
  if ( iPos != wxNOT_FOUND )
      str.assign(*this, iPos + 1, npos);

  return str;
}

// replace first (or all) occurrences of some substring with another one
size_t wxString::Replace(const wxString& strOld,
                         const wxString& strNew, bool bReplaceAll)
{
    wxSTRING_INVALIDATE_CACHE();

    size_t uiCount = 0;   // count of replacements made

    // optimize the special common case: replacement of one character by
    // another one (in UTF-8 case we can only do this for ASCII characters)
    //
    // benchmarks show that this special version is around 3 times faster
    // (depending on the proportion of matching characters and UTF-8/wchar_t
    // build)
    if ( strOld.m_impl.length() == 1 && strNew.m_impl.length() == 1 )
    {
        const wxStringCharType chOld = strOld.m_impl[0],
                               chNew = strNew.m_impl[0];

        // this loop is the simplified version of the one below
        for ( size_t pos = 0; ; )
        {
            pos = m_impl.find(chOld, pos);
            if ( pos == npos )
                break;

            m_impl[pos++] = chNew;

            uiCount++;

            if ( !bReplaceAll )
                break;
        }
    }
    else if ( !bReplaceAll)
    {
        size_t pos = m_impl.find(strOld.m_impl, 0);
        if ( pos != npos )
        {
            m_impl.replace(pos, strOld.m_impl.length(), strNew.m_impl);
            uiCount = 1;
        }
    }
    else // replace all occurrences
    {
        const size_t uiOldLen = strOld.m_impl.length();
        const size_t uiNewLen = strNew.m_impl.length();

        // first scan the string to find all positions at which the replacement
        // should be made
        wxVector<size_t> replacePositions;

        size_t pos;
        for ( pos = m_impl.find(strOld.m_impl, 0);
              pos != npos;
              pos = m_impl.find(strOld.m_impl, pos + uiOldLen))
        {
            replacePositions.push_back(pos);
            ++uiCount;
        }

        if ( !uiCount )
            return 0;

        // allocate enough memory for the whole new string
        wxString tmp;
        tmp.m_impl.reserve(m_impl.length() + uiCount*(uiNewLen - uiOldLen));

        // copy this string to tmp doing replacements on the fly
        size_t replNum = 0;
        for ( pos = 0; replNum < uiCount; replNum++ )
        {
            const size_t nextReplPos = replacePositions[replNum];

            if ( pos != nextReplPos )
            {
                tmp.m_impl.append(m_impl, pos, nextReplPos - pos);
            }

            tmp.m_impl.append(strNew.m_impl);
            pos = nextReplPos + uiOldLen;
        }

        if ( pos != m_impl.length() )
        {
            // append the rest of the string unchanged
            tmp.m_impl.append(m_impl, pos, m_impl.length() - pos);
        }

        swap(tmp);
    }

    return uiCount;
}

bool wxString::IsAscii() const
{
    for ( const_iterator i = begin(); i != end(); ++i )
    {
        if ( !(*i).IsAscii() )
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// case conversion
// ---------------------------------------------------------------------------

wxString& wxString::MakeUpper()
{
  for ( iterator it = begin(), en = end(); it != en; ++it )
    *it = (wxChar)wxToupper(*it);

  return *this;
}

wxString& wxString::MakeLower()
{
  for ( iterator it = begin(), en = end(); it != en; ++it )
    *it = (wxChar)wxTolower(*it);

  return *this;
}

// ---------------------------------------------------------------------------
// trimming and padding
// ---------------------------------------------------------------------------

// some compilers (VC++ 6.0 not to name them) return true for a call to
// isspace('\xEA') in the C locale which seems to be broken to me, but we have
// to live with this by checking that the character is a 7 bit one - even if
// this may fail to detect some spaces (I don't know if Unicode doesn't have
// space-like symbols somewhere except in the first 128 chars), it is arguably
// still better than trimming away accented letters
inline int wxSafeIsspace(wxChar ch) { return (ch < 127) && wxIsspace(ch); }

// trims spaces (in the sense of isspace) from left or right side
wxString& wxString::Trim(bool bFromRight)
{
    // first check if we're going to modify the string at all
    if ( !empty() &&
         (
          (bFromRight && wxSafeIsspace(GetChar(length() - 1))) ||
          (!bFromRight && wxSafeIsspace(GetChar(0u)))
         )
       )
    {
        if ( bFromRight )
        {
            // find last non-space character
            reverse_iterator psz = rbegin();
            while ( (psz != rend()) && wxSafeIsspace(*psz) )
                ++psz;

            // truncate at trailing space start
            erase(psz.base(), end());
        }
        else
        {
            // find first non-space character
            iterator psz = begin();
            while ( (psz != end()) && wxSafeIsspace(*psz) )
                ++psz;

            // fix up data and length
            erase(begin(), psz);
        }
    }

    return *this;
}

// truncate the string
wxString& wxString::Truncate(size_t uiLen)
{
    if ( uiLen < length() )
    {
        erase(begin() + uiLen, end());
    }
    //else: nothing to do, string is already short enough

    return *this;
}

// ---------------------------------------------------------------------------
// finding (return wxNOT_FOUND if not found and index otherwise)
// ---------------------------------------------------------------------------

// find a character
int wxString::Find(wxUniChar ch, bool bFromEnd) const
{
    size_type idx = bFromEnd ? find_last_of(ch) : find_first_of(ch);

    return (idx == npos) ? wxNOT_FOUND : (int)idx;
}

// ----------------------------------------------------------------------------
// conversion to numbers
// ----------------------------------------------------------------------------

// The implementation of all the functions below is exactly the same so factor
// it out. Note that number extraction works correctly on UTF-8 strings, so
// we can use wxStringCharType and wx_str() for maximum efficiency.

#define DO_IF_NOT_WINCE(x) x

#define WX_STRING_TO_X_TYPE_START                                           \
    DO_IF_NOT_WINCE( errno = 0; )                                           \
    const wxStringCharType *start = wx_str();                               \
    wxStringCharType *end;

// notice that we return false without modifying the output parameter at all if
// nothing could be parsed but we do modify it and return false then if we did
// parse something successfully but not the entire string
#define WX_STRING_TO_X_TYPE_END                                             \
    if ( end == start DO_IF_NOT_WINCE(|| errno == ERANGE) )                 \
        return false;                                                       \
    *pVal = val;                                                            \
    return !*end;

bool wxString::ToLong(long *pVal, int base) const
{
    WX_STRING_TO_X_TYPE_START
    long val = wxStrtol(start, &end, base);
    WX_STRING_TO_X_TYPE_END
}

bool wxString::ToULong(unsigned long *pVal, int base) const
{
    WX_STRING_TO_X_TYPE_START
    unsigned long val = wxStrtoul(start, &end, base);
    WX_STRING_TO_X_TYPE_END
}

bool wxString::ToULongLong(wxULongLong_t *pVal, int base) const
{
    WX_STRING_TO_X_TYPE_START
    wxULongLong_t val = wxStrtoull(start, &end, base);
    WX_STRING_TO_X_TYPE_END
}

bool wxString::ToDouble(double *pVal) const
{
    WX_STRING_TO_X_TYPE_START
    double val = wxStrtod(start, &end);
    WX_STRING_TO_X_TYPE_END
}

// ----------------------------------------------------------------------------
// number to string conversion
// ----------------------------------------------------------------------------

/* static */
wxString wxString::FromDouble(double val, int precision)
{
    wxString format;
    if ( precision == -1 )
        format = "%g";
    else // Use fixed precision.
        format.Printf("%%.%df", precision);

    return wxString::Format(format, val);
}

// ---------------------------------------------------------------------------
// formatted output
// ---------------------------------------------------------------------------

#if !wxUSE_UTF8_LOCALE_ONLY
/* static */
#ifdef wxNEEDS_WXSTRING_PRINTF_MIXIN
wxString wxStringPrintfMixinBase::DoFormatWchar(const wxChar *format, ...)
#else
wxString wxString::DoFormatWchar(const wxChar *format, ...)
#endif
{
    va_list argptr;
    va_start(argptr, format);

    wxString s;
    s.PrintfV(format, argptr);

    va_end(argptr);

    return s;
}
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
/* static */
wxString wxString::DoFormatUtf8(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    wxString s;
    s.PrintfV(format, argptr);

    va_end(argptr);

    return s;
}
#endif // wxUSE_UNICODE_UTF8

/* static */
wxString wxString::FormatV(const wxString& format, va_list argptr)
{
    wxString s;
    s.PrintfV(format, argptr);
    return s;
}

#if !wxUSE_UTF8_LOCALE_ONLY
#ifdef wxNEEDS_WXSTRING_PRINTF_MIXIN
int wxStringPrintfMixinBase::DoPrintfWchar(const wxChar *format, ...)
#else
int wxString::DoPrintfWchar(const wxChar *format, ...)
#endif
{
    va_list argptr;
    va_start(argptr, format);

#ifdef wxNEEDS_WXSTRING_PRINTF_MIXIN
    // get a pointer to the wxString instance; we have to use dynamic_cast<>
    // because it's the only cast that works safely for downcasting when
    // multiple inheritance is used:
    wxString *str = static_cast<wxString*>(this);
#else
    wxString *str = this;
#endif

    int iLen = str->PrintfV(format, argptr);

    va_end(argptr);

    return iLen;
}
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
int wxString::DoPrintfUtf8(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int iLen = PrintfV(format, argptr);

    va_end(argptr);

    return iLen;
}
#endif // wxUSE_UNICODE_UTF8

/*
    Uses wxVsnprintf and places the result into the this string.

    In ANSI build, wxVsnprintf is effectively vsnprintf but in Unicode build
    it is vswprintf.  Due to a discrepancy between vsnprintf and vswprintf in
    the ISO C99 (and thus SUSv3) standard the return value for the case of
    an undersized buffer is inconsistent.  For conforming vsnprintf
    implementations the function must return the number of characters that
    would have been printed had the buffer been large enough.  For conforming
    vswprintf implementations the function must return a negative number
    and set errno.

    What vswprintf sets errno to is undefined but Darwin seems to set it to
    EOVERFLOW.  The only expected errno are EILSEQ and EINVAL.  Both of
    those are defined in the standard and backed up by several conformance
    statements.  Note that ENOMEM mentioned in the manual page does not
    apply to swprintf, only wprintf and fwprintf.

    Official manual page:
    http://www.opengroup.org/onlinepubs/009695399/functions/swprintf.html

    Some conformance statements (AIX, Solaris):
    http://www.opengroup.org/csq/view.mhtml?RID=ibm%2FSD1%2F3
    http://www.theopengroup.org/csq/view.mhtml?norationale=1&noreferences=1&RID=Fujitsu%2FSE2%2F10

    Since EILSEQ and EINVAL are rather common but EOVERFLOW is not and since
    EILSEQ and EINVAL are specifically defined to mean the error is other than
    an undersized buffer and no other errno are defined we treat those two
    as meaning hard errors and everything else gets the old behaviour which
    is to keep looping and increasing buffer size until the function succeeds.

    In practice it's impossible to determine before compilation which behaviour
    may be used.  The vswprintf function may have vsnprintf-like behaviour or
    vice-versa.  Behaviour detected on one release can theoretically change
    with an updated release.  Not to mention that configure testing for it
    would require the test to be run on the host system, not the build system
    which makes cross compilation difficult. Therefore, we make no assumptions
    about behaviour and try our best to handle every known case, including the
    case where wxVsnprintf returns a negative number and fails to set errno.

    There is yet one more non-standard implementation and that is our own.
    Fortunately, that can be detected at compile-time.

    On top of all that, ISO C99 explicitly defines snprintf to write a null
    character to the last position of the specified buffer.  That would be at
    at the given buffer size minus 1.  It is supposed to do this even if it
    turns out that the buffer is sized too small.

    Darwin (tested on 10.5) follows the C99 behaviour exactly.

    Glibc 2.6 almost follows the C99 behaviour except vswprintf never sets
    errno even when it fails.  However, it only seems to ever fail due
    to an undersized buffer.
*/
#if wxUSE_UNICODE_UTF8
template<typename BufferType>
#else
// we only need one version in non-UTF8 builds and at least two Windows
// compilers have problems with this function template, so use just one
// normal function here
#endif
static int DoStringPrintfV(wxString& str,
                           const wxString& format, va_list argptr)
{
    int size = 1024;

    for ( ;; )
    {
#if wxUSE_UNICODE_UTF8
        BufferType tmp(str, size + 1);
        typename BufferType::CharType *buf = tmp;
#else
        wxStringBuffer tmp(str, size + 1);
        wxChar *buf = tmp;
#endif

        if ( !buf )
        {
            // out of memory
            return -1;
        }

        // wxVsnprintf() may modify the original arg pointer, so pass it
        // only a copy
        va_list argptrcopy;
        wxVaCopy(argptrcopy, argptr);

	// Set errno to 0 to make it determinate if wxVsnprintf fails to set it.
	errno = 0;
        int len = wxVsnprintf(buf, size, format, argptrcopy);
        va_end(argptrcopy);

        // some implementations of vsnprintf() don't NUL terminate
        // the string if there is not enough space for it so
        // always do it manually
        // FIXME: This really seems to be the wrong and would be an off-by-one
        // bug except the code above allocates an extra character.
        buf[size] = wxT('\0');

        // vsnprintf() may return either -1 (traditional Unix behaviour) or the
        // total number of characters which would have been written if the
        // buffer were large enough (newer standards such as Unix98)
        if ( len < 0 )
        {
            // NB: wxVsnprintf() may call either wxCRT_VsnprintfW or
            //     wxCRT_VsnprintfA in UTF-8 build; wxUSE_WXVSNPRINTF
            //     is true if *both* of them use our own implementation,
            //     otherwise we can't be sure
#if wxUSE_WXVSNPRINTF
            // we know that our own implementation of wxVsnprintf() returns -1
            // only for a format error - thus there's something wrong with
            // the user's format string
            buf[0] = '\0';
            return -1;
#else // possibly using system version
            // assume it only returns error if there is not enough space, but
            // as we don't know how much we need, double the current size of
            // the buffer
            // still not enough, as we don't know how much we need, double the
            // current size of the buffer
                size *= 2;
#endif // wxUSE_WXVSNPRINTF/!wxUSE_WXVSNPRINTF
        }
        else if ( len >= size )
        {
#if wxUSE_WXVSNPRINTF
            // we know that our own implementation of wxVsnprintf() returns
            // size+1 when there's not enough space but that's not the size
            // of the required buffer!
            size *= 2;      // so we just double the current size of the buffer
#else
            // some vsnprintf() implementations NUL-terminate the buffer and
            // some don't in len == size case, to be safe always add 1
            // FIXME: I don't quite understand this comment.  The vsnprintf
            // function is specifically defined to return the number of
            // characters printed not including the null terminator.
            // So OF COURSE you need to add 1 to get the right buffer size.
            // The following line is definitely correct, no question.
            size = len + 1;
#endif
        }
        else // ok, there was enough space
        {
            break;
        }
    }

    // we could have overshot
    str.Shrink();

    return str.length();
}

int wxString::PrintfV(const wxString& format, va_list argptr)
{
#if wxUSE_UNICODE_UTF8
    #if wxUSE_STL_BASED_WXSTRING
        typedef wxStringTypeBuffer<char> Utf8Buffer;
    #else
        typedef wxStringInternalBuffer Utf8Buffer;
    #endif
#endif

#if wxUSE_UTF8_LOCALE_ONLY
    return DoStringPrintfV<Utf8Buffer>(*this, format, argptr);
#else
    #if wxUSE_UNICODE_UTF8
    if ( wxLocaleIsUtf8 )
        return DoStringPrintfV<Utf8Buffer>(*this, format, argptr);
    else
        // wxChar* version
        return DoStringPrintfV<wxStringBuffer>(*this, format, argptr);
    #else
        return DoStringPrintfV(*this, format, argptr);
    #endif // UTF8/WCHAR
#endif
}

// ----------------------------------------------------------------------------
// misc other operations
// ----------------------------------------------------------------------------

// returns true if the string matches the pattern which may contain '*' and
// '?' metacharacters (as usual, '?' matches any character and '*' any number
// of them)
bool wxString::Matches(const wxString& mask) const
{
  // FIXME-UTF8: implement using iterators, remove #if
#if wxUSE_UNICODE_UTF8
  const wxScopedWCharBuffer maskBuf = mask.wc_str();
  const wxScopedWCharBuffer txtBuf = wc_str();
  const wxChar *pszMask = maskBuf.data();
  const wxChar *pszTxt = txtBuf.data();
#else
  const wxChar *pszMask = mask.wx_str();
  // the char currently being checked
  const wxChar *pszTxt = wx_str();
#endif

  // the last location where '*' matched
  const wxChar *pszLastStarInText = NULL;
  const wxChar *pszLastStarInMask = NULL;

match:
  for ( ; *pszMask != wxT('\0'); pszMask++, pszTxt++ ) {
    switch ( *pszMask ) {
      case wxT('?'):
        if ( *pszTxt == wxT('\0') )
          return false;

        // pszTxt and pszMask will be incremented in the loop statement

        break;

      case wxT('*'):
        {
          // remember where we started to be able to backtrack later
          pszLastStarInText = pszTxt;
          pszLastStarInMask = pszMask;

          // ignore special chars immediately following this one
          // (should this be an error?)
          while ( *pszMask == wxT('*') || *pszMask == wxT('?') )
            pszMask++;

          // if there is nothing more, match
          if ( *pszMask == wxT('\0') )
            return true;

          // are there any other metacharacters in the mask?
          size_t uiLenMask;
          const wxChar *pEndMask = wxStrpbrk(pszMask, wxT("*?"));

          if ( pEndMask != NULL ) {
            // we have to match the string between two metachars
            uiLenMask = pEndMask - pszMask;
          }
          else {
            // we have to match the remainder of the string
            uiLenMask = wxStrlen(pszMask);
          }

          wxString strToMatch(pszMask, uiLenMask);
          const wxChar* pMatch = wxStrstr(pszTxt, strToMatch);
          if ( pMatch == NULL )
            return false;

          // -1 to compensate "++" in the loop
          pszTxt = pMatch + uiLenMask - 1;
          pszMask += uiLenMask - 1;
        }
        break;

      default:
        if ( *pszMask != *pszTxt )
          return false;
        break;
    }
  }

  // match only if nothing left
  if ( *pszTxt == wxT('\0') )
    return true;

  // if we failed to match, backtrack if we can
  if ( pszLastStarInText ) {
    pszTxt = pszLastStarInText + 1;
    pszMask = pszLastStarInMask;

    pszLastStarInText = NULL;

    // don't bother resetting pszLastStarInMask, it's unnecessary

    goto match;
  }

  return false;
}
