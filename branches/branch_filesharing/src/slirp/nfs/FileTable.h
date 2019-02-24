#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include<iostream>
#include <string>
#include <map>
#include <set>
#include <sys/stat.h>

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
    uint32_t atime_nsec;
    uint32_t mtime_sec;
    uint32_t mtime_nsec;
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
    
    void     Remove      (const std::string& name);
    FileAttrs *GetFileAttrs(const std::string& name);
    void     SetFileAttrs(const std::string& path, const FileAttrs& fstat);
    void     Write(void);
};

class FileTable {
    atomic_int                         doRun;
    thread_t*                          thread;
    mutex_t*                           mutex;
    std::map<std::string, uint64_t>    path2handle;
    std::map<uint64_t,    std::string> handle2path;
    std::map<std::string, FileAttrDB*> path2db;
    std::set<FileAttrDB*>              dirty;
    
    void        Write(void);
    FileAttrDB* GetDB(const std::string& path);
public:
    uint32_t     cookie;
    
    FileTable();
    ~FileTable();
    
    int         Stat           (const std::string& path, struct stat* stat);
    bool        GetAbsolutePath(uint64_t fhandle, std::string& result);
    void        Move           (const std::string& pathFrom, const std::string& pathTo);
    void        Remove         (const std::string& path);
    uint64_t    GetFileHandle  (const std::string& path);
    FileAttrs*  GetFileAttrs   (const std::string& path);
    void        SetFileAttrs   (const std::string& path, const FileAttrs& fstat);
    void        Dirty          (FileAttrDB* db);
    void        Run            (void);
};

extern FileTable nfsd_ft;

#endif
