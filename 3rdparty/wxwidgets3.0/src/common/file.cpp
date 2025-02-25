/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/file.cpp
// Purpose:     wxFile - encapsulates low-level "file descriptor"
//              wxTempFile
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_FILE

// standard
#if defined(__WINDOWS__) && !defined(__GNUWIN32__)

#define   WIN32_LEAN_AND_MEAN
#define   NOSERVICE
#define   NOIME
#define   NOATOM
#define   NOGDI
#define   NOGDICAPMASKS
#define   NOMETAFILE
#define   NOMINMAX
#define   NOMSG
#define   NOOPENFILE
#define   NORASTEROPS
#define   NOSCROLL
#define   NOSOUND
#define   NOSYSMETRICS
#define   NOTEXTMETRIC
#define   NOWH
#define   NOCOMM
#define   NOKANJI
#define   NOCRYPT
#define   NOMCX

#elif (defined(__UNIX__) || defined(__GNUWIN32__))
    #include  <unistd.h>
    #include  <time.h>
    #include  <sys/stat.h>
    #ifdef __GNUWIN32__
        #include "wx/msw/wrapwin.h"
    #endif
#elif (defined(__WXMAC__))
#if __MSL__ < 0x6000
    int access( const char *path, int mode ) { return 0 ; }
#else
    int _access( const char *path, int mode ) { return 0 ; }
#endif
    char* mktemp( char * path ) { return path ;}
    #include <stat.h>
    #include <unistd.h>
#else
    #error  "Please specify the header with file functions declarations."
#endif  //Win/UNIX

#include  <stdio.h>       // SEEK_xxx constants
#include <errno.h>

// Windows compilers don't have these constants
#ifndef W_OK
    enum
    {
        F_OK = 0,   // test for existence
        X_OK = 1,   //          execute permission
        W_OK = 2,   //          write
        R_OK = 4    //          read
    };
#endif // W_OK

// wxWidgets
#ifndef WX_PRECOMP
    #include  "wx/string.h"
    #include "wx/crt.h"
#endif // !WX_PRECOMP

#include  "wx/filename.h"
#include  "wx/file.h"
#include  "wx/filefn.h"

// there is no distinction between text and binary files under Unix, so define
// O_BINARY as 0 if the system headers don't do it already
#if defined(__UNIX__) && !defined(O_BINARY)
    #define   O_BINARY    (0)
#endif  //__UNIX__

#ifndef MAX_PATH
    #define MAX_PATH 512
#endif

// ============================================================================
// implementation of wxFile
// ============================================================================

// ----------------------------------------------------------------------------
// static functions
// ----------------------------------------------------------------------------

bool wxFile::Exists(const wxString& name)
{
    return wxFileExists(name);
}

bool wxFile::Access(const wxString& name, OpenMode mode)
{
    int how;

    switch ( mode )
    {
        default:
            // bad wxFile::Access mode parameter
            // fall through

        case read:
            how = R_OK;
            break;

        case write:
            how = W_OK;
            break;

        case read_write:
            how = R_OK | W_OK;
            break;
    }

    return wxAccess(name, how) == 0;
}

// ----------------------------------------------------------------------------
// opening/closing
// ----------------------------------------------------------------------------

// ctors
wxFile::wxFile(const wxString& fileName, OpenMode mode)
{
    m_fd = fd_invalid;
    m_lasterror = 0;

    Open(fileName, mode);
}

bool wxFile::CheckForError(wxFileOffset rc) const
{
    if ( rc != -1 )
        return false;

    const_cast<wxFile *>(this)->m_lasterror =
                                                errno
                                                ;

    return true;
}

// create the file, fail if it already exists and bOverwrite
bool wxFile::Create(const wxString& fileName, bool bOverwrite, int accessMode)
{
    // if bOverwrite we create a new file or truncate the existing one,
    // otherwise we only create the new file and fail if it already exists
    int fildes = wxOpen( fileName,
                     O_BINARY | O_WRONLY | O_CREAT |
                     (bOverwrite ? O_TRUNC : O_EXCL),
                     accessMode );
    if ( CheckForError(fildes) )
        return false;

    Attach(fildes);
    return true;
}

// open the file
bool wxFile::Open(const wxString& fileName, OpenMode mode, int accessMode)
{
    int flags = O_BINARY;

    switch ( mode )
    {
        case read:
            flags |= O_RDONLY;
            break;

        case write_append:
            if ( wxFile::Exists(fileName) )
            {
                flags |= O_WRONLY | O_APPEND;
                break;
            }
            //else: fall through as write_append is the same as write if the
            //      file doesn't exist

        case write:
            flags |= O_WRONLY | O_CREAT | O_TRUNC;
            break;

        case write_excl:
            flags |= O_WRONLY | O_CREAT | O_EXCL;
            break;

        case read_write:
            flags |= O_RDWR;
            break;
    }

#ifdef __WINDOWS__
    // only read/write bits for "all" are supported by this function under
    // Windows, and VC++ 8 returns EINVAL if any other bits are used in
    // accessMode, so clear them as they have at best no effect anyhow
    accessMode &= wxS_IRUSR | wxS_IWUSR;
#endif // __WINDOWS__

    int fildes = wxOpen( fileName, flags, accessMode);

    if ( CheckForError(fildes) )
        return false;

    Attach(fildes);
    return true;
}

// close
bool wxFile::Close()
{
    if ( IsOpened() ) {
        if ( CheckForError(wxClose(m_fd)) )
        {
            m_fd = fd_invalid;
            return false;
        }
        else
            m_fd = fd_invalid;
    }

    return true;
}

// ----------------------------------------------------------------------------
// read/write
// ----------------------------------------------------------------------------

// read
ssize_t wxFile::Read(void *pBuf, size_t nCount)
{
    ssize_t iRc = wxRead(m_fd, pBuf, nCount);

    if ( CheckForError(iRc) )
        return wxInvalidOffset;

    return iRc;
}

// write
size_t wxFile::Write(const void *pBuf, size_t nCount)
{
    ssize_t iRc = wxWrite(m_fd, pBuf, nCount);

    if ( CheckForError(iRc) )
        iRc = 0;

    return iRc;
}

bool wxFile::Write(const wxString& s, const wxMBConv& conv)
{
    // Writing nothing always succeeds -- and simplifies the check for
    // conversion failure below.
    if ( s.empty() )
        return true;

    const wxWX2MBbuf buf = s.mb_str(conv);

#if wxUSE_UNICODE
    const size_t size = buf.length();

    if ( !size )
    {
        // This means that the conversion failed as the original string wasn't
        // empty (we explicitly checked for this above) and in this case we
        // must fail too to indicate that we can't save the data.
        return false;
    }
#else
    const size_t size = s.length();
#endif

    return Write(buf, size) == size;
}

// flush
bool wxFile::Flush()
{
#ifdef HAVE_FSYNC
    // fsync() only works on disk files and returns errors for pipes, don't
    // call it then
    if ( IsOpened() && GetKind() == wxFILE_KIND_DISK )
    {
        if ( CheckForError(wxFsync(m_fd)) )
            return false;
    }
#endif // HAVE_FSYNC

    return true;
}

// ----------------------------------------------------------------------------
// seek
// ----------------------------------------------------------------------------

// seek
wxFileOffset wxFile::Seek(wxFileOffset ofs, wxSeekMode mode)
{
    int origin;
    switch ( mode ) {
        default:
            // unknown seek origin

        case wxFromStart:
            origin = SEEK_SET;
            break;

        case wxFromCurrent:
            origin = SEEK_CUR;
            break;

        case wxFromEnd:
            origin = SEEK_END;
            break;
    }

    wxFileOffset iRc = wxSeek(m_fd, ofs, origin);
    return iRc;
}

// get current file offset
wxFileOffset wxFile::Tell() const
{
    wxFileOffset iRc = wxTell(m_fd);
    return iRc;
}

// get current file length
wxFileOffset wxFile::Length() const
{
    // we use a special method for Linux systems where files in sysfs (i.e.
    // those under /sys typically) return length of 4096 bytes even when
    // they're much smaller -- this is a problem as it results in errors later
    // when we try reading 4KB from them
#ifdef __LINUX__
    struct stat st;
    if ( fstat(m_fd, &st) == 0 )
    {
        // returning 0 for the special files indicates to the caller that they
        // are not seekable
        return st.st_blocks ? st.st_size : 0;
    }
    //else: failed to stat, try the normal method
#endif // __LINUX__

    wxFileOffset iRc = Tell();
    if ( iRc != wxInvalidOffset ) {
        wxFileOffset iLen = const_cast<wxFile *>(this)->SeekEnd();
        if ( iLen != wxInvalidOffset ) {
            // restore old position
            if ( ((wxFile *)this)->Seek(iRc) == wxInvalidOffset ) {
                // error
                iLen = wxInvalidOffset;
            }
        }

        iRc = iLen;
    }

    return iRc;
}

// is end of file reached?
bool wxFile::Eof() const
{
    wxFileOffset iRc;

#if defined(__UNIX__) || defined(__GNUWIN32__)
    // @@ this doesn't work, of course, on unseekable file descriptors
    wxFileOffset ofsCur = Tell(),
    ofsMax = Length();
    if ( ofsCur == wxInvalidOffset || ofsMax == wxInvalidOffset )
        iRc = wxInvalidOffset;
    else
        iRc = ofsCur == ofsMax;
#else  // Windows and "native" compiler
    iRc = wxEof(m_fd);
#endif // Windows/Unix

    if ( iRc == 0 )
        return false;

    return true;
}

// ============================================================================
// implementation of wxTempFile
// ============================================================================

// ----------------------------------------------------------------------------
// construction
// ----------------------------------------------------------------------------

wxTempFile::wxTempFile(const wxString& strName)
{
    Open(strName);
}

bool wxTempFile::Open(const wxString& strName)
{
    // we must have an absolute filename because otherwise CreateTempFileName()
    // would create the temp file in $TMP (i.e. the system standard location
    // for the temp files) which might be on another volume/drive/mount and
    // wxRename()ing it later to m_strName from Commit() would then fail
    //
    // with the absolute filename, the temp file is created in the same
    // directory as this one which ensures that wxRename() may work later
    wxFileName fn(strName);
    if ( !fn.IsAbsolute() )
    {
        fn.Normalize(wxPATH_NORM_ABSOLUTE);
    }

    m_strName = fn.GetFullPath();

    m_strTemp = wxFileName::CreateTempFileName(m_strName, &m_file);

    if ( m_strTemp.empty() )
    {
        // CreateTempFileName() failed
        return false;
    }

#ifdef __UNIX__
    // the temp file should have the same permissions as the original one
    mode_t mode;

    wxStructStat st;
    if ( stat( (const char*) m_strName.fn_str(), &st) == 0 )
    {
        mode = st.st_mode;
    }
    else
    {
        // file probably didn't exist, just give it the default mode _using_
        // user's umask (new files creation should respect umask)
        mode_t mask = umask(0777);
        mode = 0666 & ~mask;
        umask(mask);
    }

    if ( chmod( (const char*) m_strTemp.fn_str(), mode) == -1 )
    {
    }
#endif // Unix

    return true;
}

// ----------------------------------------------------------------------------
// destruction
// ----------------------------------------------------------------------------

wxTempFile::~wxTempFile()
{
    if ( IsOpened() )
        Discard();
}

bool wxTempFile::Commit()
{
    m_file.Close();

    if ( wxFile::Exists(m_strName) && wxRemove(m_strName) != 0 )
        return false;

    if ( !wxRenameFile(m_strTemp, m_strName)  )
        return false;

    return true;
}

void wxTempFile::Discard()
{
    m_file.Close();
    if ( wxRemove(m_strTemp) != 0 ) { }
}

#endif // wxUSE_FILE

