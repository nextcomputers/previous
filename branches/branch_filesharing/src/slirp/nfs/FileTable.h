#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include<iostream>
#include <string>
#include <map>
#include <sys/stat.h>

#define FILE_TABLE_NAME ".nfsd_fattrs"

class FileAttrs {
public:
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    int      reserved; // for future extensions
    
    FileAttrs(const struct stat* stat = NULL);
    FileAttrs(FILE* fin, std::string& name);
    FileAttrs(const FileAttrs& attrs);
    void Write(FILE* fout, const std::string& name);
    ~FileAttrs(void);
};

class FileAttrDB {
    std::string                       path;
    std::map<std::string, FileAttrs*> attrs;
public:
    FileAttrDB(const std::string& directory);
    void Add(const std::string& name, const FileAttrs& attrs);
    void Remove(const std::string& name);
    ~FileAttrDB();
};

class FileHandle {
public:
    uint64_t magic;
    uint64_t handle0;
    uint64_t handle1;
    uint64_t pad3;
// NFS3 fields
    uint64_t pad4;
    uint64_t pad5;
    uint64_t pad6;
    uint64_t pad7;
// internal use only
    FileAttrs attrs;
    bool      dirty;
public:
    FileHandle(const struct stat* stat = NULL);
    
    void SetMode(uint32_t mode);
    void SetUID (uint32_t uid);
    void SetGID (uint32_t gid);
};

class FileTable {
    std::map<std::string, FileHandle*> path2handle;
    std::map<uint64_t,    std::string>  id2path;
public:
    FileTable();
    ~FileTable();
    
    uint32_t     cookie;
    
    int         Stat(const std::string& path, struct stat* stat);
    FileHandle* GetFileHandle(const std::string& path);
    bool        GetAbsolutePath(const FileHandle& fhandle, std::string& result);
    uint64_t    GetFileId(const std::string& path);
    void        Rename(const std::string& pathFrom, const std::string& pathTo);
    void        Remove(const std::string& path);
    void        SetMode(const std::string& path, uint32_t mode);
    void        SetUID (const std::string& path, uint32_t uid);
    void        SetGID (const std::string& path, uint32_t gid);
    void        Sync(void);
};

extern FileTable nfsd_ft;

#endif
