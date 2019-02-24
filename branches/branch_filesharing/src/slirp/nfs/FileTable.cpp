#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>

#include "FileTable.h"
#include "RPCProg.h"
#include "nfsd.h"

using namespace std;

FileAttrs::FileAttrs(XDRInput* xin) : reserved(0) {
    xin->Read(&mode);
    xin->Read(&uid);
    xin->Read(&gid);
    xin->Read(&size);
    xin->Read(&atime_sec);
    xin->Read(&atime_nsec);
    xin->Read(&mtime_sec);
    xin->Read(&mtime_nsec);
}

FileAttrs::FileAttrs(const struct stat* stat) :
mode      (stat->st_mode),
uid       (stat->st_uid),
gid       (stat->st_gid),
size      (stat->st_size),
atime_sec (stat->st_atimespec.tv_sec),
atime_nsec(stat->st_atimespec.tv_nsec),
mtime_sec (stat->st_mtimespec.tv_sec),
mtime_nsec(stat->st_mtimespec.tv_nsec),
reserved  (0) {}

FileAttrs::FileAttrs(const FileAttrs& attrs) {Update(attrs);}

void FileAttrs::Update(const FileAttrs& attrs) {
    mode       = attrs.mode;
    uid        = attrs.uid;
    gid        = attrs.gid;
    size       = attrs.size;
    atime_sec  = attrs.atime_sec;
    atime_nsec = attrs.atime_nsec;
    mtime_sec  = attrs.mtime_sec;
    mtime_nsec = attrs.mtime_nsec;
    reserved   = attrs.reserved;
}

FileAttrs::FileAttrs(FILE* fin, string& name) {
    char cname[MAXNAMELEN];
    if(fscanf(fin, "0%o:%d:%d:%d:%s\n", &mode, &uid, &gid, &reserved, cname))
        name = cname;
}

void FileAttrs::Write(FILE* fout, const string& name) {
    fprintf(fout, "0%o:%d:%d:%d:%s\n", mode, uid, gid, reserved, name.c_str());
}

bool FileAttrs::Valid(uint32_t statval) {return statval != 0xFFFFFFFF;}

string filename(const string& path) {
    return path.substr(path.find_last_of("/\\") + 1);
}

static int ThreadProc(void *lpParameter) {
    ((FileTable*)lpParameter)->Run();
    return 0;
}

//----- file tyble

FileTable::FileTable(const string& _basePath) : mutex(host_mutex_create()), cookie(rand()) {
    basePath = _basePath;
    if(basePath.compare(basePath.size() - 1, 1, "/") == 0) basePath.resize(basePath.size()-1);
    host_atomic_set(&doRun, 1);
    thread = host_thread_create(&ThreadProc, "FileTable", this);
}

FileTable::~FileTable() {
    {
        NFSDLock lock(mutex);
        
        host_atomic_set(&doRun, 0);
        host_thread_wait(thread);
        
        for(map<string,FileAttrDB*>::iterator it = path2db.begin(); it != path2db.end(); it++)
            delete it->second;
    }
    host_mutex_destroy(mutex);
}

void FileTable::Run(void) {
    while(host_atomic_get(&doRun)) {
        if(dirty.size()) Write();
        host_sleep_sec(1);
    }
}

static string canonicalize(const string& path) {
    char* rpath = realpath(path.c_str(), NULL);
    if(rpath) {
        string result = rpath;
        free(rpath);
        return result;
    } else {
        return path;
    }
}

int FileTable::Stat(const string& _path, struct stat* fstat) {
    NFSDLock lock(mutex);
    
    string     path   = canonicalize(_path);
    
    int        result = stat(path, fstat);
    FileAttrs* attrs  = GetFileAttrs(path);
    if(attrs) {
        fstat->st_mode = FileAttrs::Valid(attrs->mode) ? (attrs->mode | (fstat->st_mode & S_IFMT)) : fstat->st_mode;
        fstat->st_uid  = FileAttrs::Valid(attrs->uid)  ?  attrs->uid                               : fstat->st_uid;
        fstat->st_gid  = FileAttrs::Valid(attrs->gid)  ?  attrs->gid                               : fstat->st_gid;
    }
    return result;
}

static uint64_t rotl(uint64_t x, uint64_t n) {return (x<<n) | (x>>(64LL-n));}

static uint64_t make_file_handle(const struct stat* fstat) {
    uint64_t result = fstat->st_dev;
    result = rotl(result, 32) ^ fstat->st_ino;
    if(result == 0) result = ~result;
    return result;
}

uint64_t FileTable::GetFileHandle(const string& _path) {
    NFSDLock lock(mutex);

    string path = canonicalize(_path);
    
    uint64_t result = 0;
    map<string, uint64_t>::iterator iter = path2handle.find(path);
    if(iter != path2handle.end())
        result = iter->second;
    else {
        struct stat fstat;
        if(stat(path, &fstat) == 0) {
            result              = make_file_handle(&fstat);
            path2handle[path]   = result;
            handle2path[result] = path;
        }
    }
    return result;
}

bool FileTable::GetAbsolutePath(uint64_t handle, string& result) {
    NFSDLock lock(mutex);

    map<uint64_t, string>::iterator iter = handle2path.find(handle);
    if(iter != handle2path.end()) {
        result = iter->second;
        return true;
    }
    return false;
}

void FileTable::Move(const string& _pathFrom, const string& _pathTo) {
    NFSDLock lock(mutex);

    string pathFrom = canonicalize(_pathFrom);
    string pathTo   = canonicalize(_pathTo);
    
    map<string, uint64_t>::iterator iter = path2handle.find(pathFrom);
    if(iter != path2handle.end()) {
        struct stat fstat;
        int statResult = Stat(pathFrom, &fstat);

        uint64_t handle = iter->second;
        path2handle.erase(iter);
        map<uint64_t, string>::iterator iter = handle2path.find(handle);
        if(iter != handle2path.end())
            handle2path.erase(iter);
        path2handle[pathTo] = handle;
        handle2path[handle] = pathTo;

        if(!(statResult)) {
            FileAttrs xstat(&fstat);
            GetDB(pathTo)->SetFileAttrs(pathTo, xstat);
        }
        
        GetDB(pathFrom)->Remove(pathFrom);
    }
}

void FileTable::Remove(const string& _path) {
    NFSDLock lock(mutex);

    string path = canonicalize(_path);

    map<string, uint64_t>::iterator iter = path2handle.find(path);
    if(iter != path2handle.end()) {
        uint64_t handle = iter->second;
        path2handle.erase(iter);
        map<uint64_t, string>::iterator iter = handle2path.find(handle);
        if(iter != handle2path.end())
            handle2path.erase(iter);
        
        GetDB(path)->Remove(path);
    }
}

FileAttrDB* FileTable::GetDB(const std::string& _path) {
    NFSDLock lock(mutex);
    
    string path = canonicalize(_path);
    char tmp[MAXPATHLEN];
    strncpy(tmp, path.c_str(), MAXPATHLEN-1);
    string dbdir = dirname(tmp);

    FileAttrDB* result = NULL;
    map<string, FileAttrDB*>::iterator iter = path2db.find(dbdir);
    if(iter != path2db.end())
        result = iter->second;
    else {
        result         = new FileAttrDB(*this, dbdir);
        path2db[dbdir] = result;
    }
    return result;
}

void FileTable::SetFileAttrs(const std::string& path, const FileAttrs& fstat) {
    NFSDLock lock(mutex);

    GetDB(path)->SetFileAttrs(path, fstat);
}

FileAttrs* FileTable::GetFileAttrs(const std::string& path) {
    NFSDLock lock(mutex);

    return GetDB(path)->GetFileAttrs(path);
}

void FileTable::Write(void) {
    NFSDLock lock(mutex);

    for(set<FileAttrDB*>::iterator it = dirty.begin(); it != dirty.end(); it++)
        (*it)->Write();
    
    dirty.clear();
}

void FileTable::Dirty(FileAttrDB* db) {
    NFSDLock lock(mutex);

    dirty.insert(db);
}

//----- file io

FILE* FileTable::fopen(const string& _path, const char* mode) {
    string path = basePath;
    path += _path;
    return ::fopen(path.c_str(), mode);
}

int FileTable::chmod(const string& _path, mode_t mode) {
    string path = basePath;
    path += _path;
    return ::chmod(path.c_str(), mode);
}

int FileTable::access(const string& _path, int mode) {
    string path = basePath;
    path += _path;
    return ::access(path.c_str(), mode);
}

DIR* FileTable::opendir(const string& _path) {
    string path = basePath;
    path += _path;
    return ::opendir(path.c_str());
}

int FileTable::remove(const string& _path) {
    string path = basePath;
    path += _path;
    return ::remove(path.c_str());
}

int FileTable::rename(const string& _from, const string& _to) {
    string from = basePath;
    from += _from;
    string to = basePath;
    to += _to;
    return ::rename(from.c_str(), to.c_str());
}

int FileTable::symlink(const string& _path1, const string& _path2) {
    string path1 = basePath;
    path1 += _path1;
    string path2 = basePath;
    path2 += _path2;
    return ::symlink(path1.c_str(), path2.c_str());
}

int FileTable::mkdir(const string& _path, mode_t mode) {
    string path = basePath;
    path += _path;
    return ::mkdir(path.c_str(), mode);
}

int FileTable::nftw(const std::string& _path, int (*fn)(const char *, const struct stat *ptr, int flag, struct FTW *), int depth, int flags) {
    string path = basePath;
    path += _path;
    return ::nftw(path.c_str(), fn, depth, flags);
}

int FileTable::statvfs(const std::string& _path, struct statvfs* buf) {
    string path = basePath;
    path += _path;
    return ::statvfs(path.c_str(), buf);
}

int FileTable::stat(const std::string& _path, struct stat* buf) {
    string path = basePath;
    path += _path;
    return ::stat(path.c_str(), buf);
}

//----- file attribute database

FileAttrDB::FileAttrDB(FileTable& ft, const string& directory) : ft(ft) {
    path = directory;
    path += "/" FILE_TABLE_NAME;
    FILE* file = ft.fopen(path, "r");
    if(file) {
        for(;;) {
            string fname;
            FileAttrs* fattrs = new FileAttrs(file, fname);
            if(fname.length() == 0) break;
            attrs[fname] = fattrs;
        }
        fclose(file);
    }
}

FileAttrDB::~FileAttrDB() {
    for(map<string, FileAttrs*>::iterator it = attrs.begin(); it != attrs.end(); it++)
        delete it->second;
}

FileAttrs* FileAttrDB::GetFileAttrs(const std::string& path) {
    string fname = filename(path);
    
    map<string, FileAttrs*>::iterator iter = attrs.find(fname);
    return iter == attrs.end() ? NULL : iter->second;
}

void FileAttrDB::SetFileAttrs(const std::string& path, const FileAttrs& fattrs) {
    string fname = filename(path);
    if("." == fname || ".." == fname) return;
    
    map<string, FileAttrs*>::iterator iter = attrs.find(fname);
    if(iter == attrs.end()) attrs[fname] = new FileAttrs(fattrs);
    else                    attrs[fname]->Update(fattrs);
    
    ft.Dirty(this);
}

void FileAttrDB::Remove(const std::string& path) {
    string fname = filename(path);
    
    map<string, FileAttrs*>::iterator iter = attrs.find(fname);
    if(iter != attrs.end()) {
        delete iter->second;
        attrs.erase(iter);
        ft.Dirty(this);
    }
}

void FileAttrDB::Write(void) {
    if(attrs.size() > 0) {
        FILE* file = ft.fopen(path, "w");
        
        if(file) {
            for(map<string, FileAttrs*>::iterator it = attrs.begin(); it != attrs.end(); it++)
                it->second->Write(file, it->first);
            fclose(file);
        }
    }
}
