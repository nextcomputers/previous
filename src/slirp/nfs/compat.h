//
//  compat.h
//  Previous
//
//  Created by Simon Schubiger on 20.02.19.
//

#ifndef compat_h
#define compat_h

#include <string>

#include <stdint.h>
#include <time.h>
#include <stdio.h>

#ifdef WIN32
    #define PATH_SEP '\\'
    #define LPATH_SEP L'\\'
    #define PATH_SEPS "\\"
#else
    #define PATH_SEP '/'
    #define LPATH_SEP L'/'
    #define PATH_SEPS "/"
#endif


class FILE_HANDLE {
protected:
    uint64_t handle;
    uint64_t magic;
    uint64_t pad2;
    uint64_t pad3;
    uint64_t pad4;
    uint64_t pad5;
    uint64_t pad6;
    uint64_t pad7;
public:
    FILE_HANDLE(uint64_t handle) : handle(handle), magic(0x73756f6976657270LL), pad2(0), pad3(0), pad4(0), pad5(0), pad6(0), pad7(0) {}
    uint64_t Get() {return handle;}
};

/*
 * The following structure is filled in by _findfirst or _findnext when
 * they succeed in finding a match.
 */
struct _finddata_t
{
    uint32_t    attrib;        /* Attributes, see constants above. */
    time_t      time_create;
    time_t      time_access;    /* always midnight local time */
    time_t      time_write;
    size_t      size;
    char        name[FILENAME_MAX];    /* may include spaces. */
};

char*   strcpy_s (char* dst,  size_t  maxLen, const char* src);
char*   strncpy_s(char* dst,  size_t  maxLen, const char* src, size_t len);
errno_t strcat_s (char* dest, rsize_t maxLen, const char* src);
char*   strcat_s (char* s1, const char* s2);

/*
 * Functions for searching for files. _findfirst returns -1 if no match
 * is found. Otherwise it returns a handle to be used in _findnext and
 * _findclose calls. _findnext also returns -1 if no match could be found,
 * and 0 if a match was found. Call _findclose when you are finished.
 */
int    _findfirst (const char*, struct _finddata_t*);
int    _findnext (int, struct _finddata_t*);
int    _findclose (int);

int    _chsize(int fd, size_t size);

bool         FileExists(const char* path);
uint32_t     GetFileID(const char* path);
FILE_HANDLE* GetFileHandle(const char* path);
bool         GetFilePath(FILE_HANDLE* handle, std::string &filePath);
int          RenameFile(const char* pathFrom, const char* pathTo);
int          RenameDirectory(const char* pathFrom, const char* pathTo);
int          RemoveFolder(const char* path);
int          RemoveDirectory(const char* path);
int          RemoveFile(const char* path);
int          GetLastError(void);
uint32_t     GetFileAttributes(const char* filename);
wchar_t*    multibyteToWide(const char* s);
char*       wideToMultiByte(const wchar_t* s);

#endif /* compat_h */
