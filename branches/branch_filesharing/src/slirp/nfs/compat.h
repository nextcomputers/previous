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

#include "host.h"

class NFSDLock {
    mutex_t* mutex;
public:
    NFSDLock(mutex_t* mutex) : mutex(mutex) {host_mutex_lock(mutex);}
    ~NFSDLock()                             {host_mutex_unlock(mutex);}
};

char*   strcpy_s (char* dst,  size_t  maxLen, const char* src);
char*   strncpy_s(char* dst,  size_t  maxLen, const char* src, size_t len);
errno_t strcat_s (char* dest, rsize_t maxLen, const char* src);
char*   strcat_s (char* s1, const char* s2);

#endif /* compat_h */
