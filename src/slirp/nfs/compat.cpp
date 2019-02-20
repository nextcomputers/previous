//
//  compat.c
//  Previous
//
//  Created by Simon Schubiger on 20.02.19.
//

#include <algorithm>

#include <sys/errno.h>

#include "compat.h"
#include "nfsd.h"

char* strcpy_s(char * dst, size_t maxLen, const char * src) {
    return strncpy(dst, src, maxLen);
}

char* strncpy_s(char * dst, size_t maxLen, const char * src, size_t len) {
    return strncpy(dst, src, std::min(maxLen, len));
}

char* strcat_s(char* s1, const char* s2) {
    return strcat(s1, s2);
}

errno_t strcat_s(char* s1, size_t maxLen, const char* s2) {
    strncat(s1, s2, maxLen);
    return 0;
}

int _findclose (int) {
    NFSD_NOTIMPL
    return -1;
}

int _findfirst (const char*, struct _finddata_t*) {
    NFSD_NOTIMPL
    return -1;
}

int _findnext (int, struct _finddata_t*) {
    NFSD_NOTIMPL
    return -1;
}

int GetLastError(void) {
    return errno;
}

int RemoveDirectory(const char* path) {
    NFSD_NOTIMPL
    return -1;
}

wchar_t* multibyteToWide(const char* s) {
    size_t size = (strlen(s) + 1)*2;
    auto dst = new wchar_t[size];
    size = mbstowcs(dst, s, size);
    if (size <= 0) {
        delete[] dst;
        return NULL;
    }
    dst[size] = 0;
    return dst;
}

char* wideToMultiByte(const wchar_t* s) {
    size_t size = (wcslen(s) + 1)*2;
    auto dst = new char[size];
    size = wcstombs(dst, s, size);
    if (size <= 0) {
        delete[] dst;
        return NULL;
    }
    dst[size] = 0;
    return dst;
}

