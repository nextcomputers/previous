#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include<iostream>
#include <string>
#include <map>
#include <set>
#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "XDRStream.h"
#include "compat.h"

#define FILE_TABLE_NAME ".nfsd_fattrs"

class FileAttrs {
public:
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t atime_sec;
    uint32_t atime_usec;
    uint32_t mtime_sec;
    uint32_t mtime_usec;
    int      reserved; // for future extensions

    FileAttrs(XDRInput* xin);
    FileAttrs(const struct stat* stat);
    FileAttrs(FILE* fin, std::string& name);
    FileAttrs(const FileAttrs& attrs);
    void Update(const FileAttrs& attrs);
    void Write(FILE* fout, const std::string& name);
    
    static bool Valid(uint32_t statval);
};

class FileTable;

class FileAttrDB {
    FileTable&                        ft;
    std::string                       path;
    std::map<std::string, FileAttrs*> attrs;
public:
    FileAttrDB(FileTable& ft, const std::string& directory);
    ~FileAttrDB();
    
    void       Remove      (const std::string& name);
    FileAttrs *GetFileAttrs(const std::string& name);
    void       SetFileAttrs(const std::string& path, const FileAttrs& fstat);
    void       Write       (void);
};

class FileTable {
    atomic_int                      doRun;
    thread_t*                       thread;
    mutex_t*                        mutex;
    std::map<std::string, uint64_t> path2handle;
    std::map<uint64_t, std::string> handle2path;
    std::map<uint64_t, FileAttrDB*> handle2db;
    std::set<FileAttrDB*>           dirty;
    std::string                     basePath;
    uint64_t                        rootIno;
    uint64_t                        rootParentIno;

    static int  ThreadProc(void *lpParameter);
    
    void        Write(void);
    FileAttrDB* GetDB       (uint64_t handle);
    std::string Canonicalize(const std::string& path);
    void        Insert      (uint64_t handle, const std::string& path);
    FileAttrs*  GetFileAttrs(const std::string& path);
    void        Dirty          (FileAttrDB* db);
    void        Run            (void);
public:
    FileTable(const std::string& basePath);
    ~FileTable();
    
    int         Stat           (const std::string& path, struct stat* stat);
    bool        GetAbsolutePath(uint64_t fhandle, std::string& result);
    void        Move           (const std::string& pathFrom, const std::string& pathTo);
    void        Remove         (const std::string& path);
    uint64_t    GetFileHandle  (const std::string& path);
    void        SetFileAttrs   (const std::string& path, const FileAttrs& fstat);
    uint32_t    FileId         (uint64_t ino);

    FILE*       fopen   (const std::string& path, const char* mode);
    int         chmod   (const std::string& path, mode_t mode);
    int         access  (const std::string& path, int mode);
    DIR*        opendir (const std::string& path);
    int         remove  (const std::string& path);
    int         rename  (const std::string& from, const std::string& to);
    int         readlink(const std::string& path1, std::string& result);
    int         link    (const std::string& path1, const std::string& path2, bool soft);
    int         mkdir   (const std::string& path, mode_t mode);
    int         nftw    (const std::string& path, int (*fn)(const char *, const struct stat *ptr, int flag, struct FTW *), int depth, int flags);
    int         statvfs (const std::string& path, struct statvfs* buf);
    int         stat    (const std::string& path, struct stat* buf);
    int         utimes  (const std::string& path, const struct timeval times[2]);
    
    static std::string MakePath(const std::string& directory, const std::string& file);
    
    friend class FileAttrDB;
};

#endif
