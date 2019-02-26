#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

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
    xin->Read(&atime_usec);
    xin->Read(&mtime_sec);
    xin->Read(&mtime_usec);
}

FileAttrs::FileAttrs(const struct stat* stat) :
mode      (stat->st_mode),
uid       (stat->st_uid),
gid       (stat->st_gid),
size      (stat->st_size),
atime_sec (stat->st_atimespec.tv_sec),
atime_usec(stat->st_atimespec.tv_nsec / 1000),
mtime_sec (stat->st_mtimespec.tv_sec),
mtime_usec(stat->st_mtimespec.tv_nsec / 1000),
reserved  (0) {}

FileAttrs::FileAttrs(const FileAttrs& attrs) :
mode      (~0),
uid       (~0),
gid       (~0),
size      (~0),
atime_sec (~0),
atime_usec(~0),
mtime_sec (~0),
mtime_usec(~0),
reserved  (0) {Update(attrs);}

void FileAttrs::Update(const FileAttrs& attrs) {
#define UPDATE_ATTR(a) a = Valid(attrs. a) ? attrs. a : a
    UPDATE_ATTR(mode);
    UPDATE_ATTR(uid);
    UPDATE_ATTR(gid);
    UPDATE_ATTR(size);
    UPDATE_ATTR(atime_sec);
    UPDATE_ATTR(atime_usec);
    UPDATE_ATTR(mtime_sec);
    UPDATE_ATTR(mtime_usec);
    UPDATE_ATTR(reserved);
}

FileAttrs::FileAttrs(FILE* fin, string& name) {
    char cname[MAXNAMELEN];
    if(fscanf(fin, "0%o:%d:%d:%d:%s\n", &mode, &uid, &gid, &reserved, cname) == 5)
        name = cname;
    else
        name.resize(0);
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

//----- file table

FileTable::FileTable(const string& _basePath) : mutex(host_mutex_create()) {
    basePath = _basePath;
    if(basePath[basePath.length()-1] == '/') basePath.resize(basePath.size()-1);
    host_atomic_set(&doRun, 1);
    thread = host_thread_create(&ThreadProc, "FileTable", this);
}

FileTable::~FileTable() {
    {
        NFSDLock lock(mutex);
        
        host_atomic_set(&doRun, 0);
        host_thread_wait(thread);
        
        for(map<uint64_t,FileAttrDB*>::iterator it = handle2db.begin(); it != handle2db.end(); it++)
            delete it->second;
    }
    host_mutex_destroy(mutex);
}

void FileTable::Run(void) {
    const int POLL = 10;
    for(int count = POLL; host_atomic_get(&doRun); count--) {
        host_sleep_sec(1);
        {
            NFSDLock lock(mutex);
            if(count <= 0 && dirty.size()) {
                Write();
                count = POLL;
            }
        }
    }
}

string FileTable::Canonicalize(const string& _path) {
    string path = basePath;
    path += _path;
    char* rpath = realpath(path.c_str(), NULL);
    if(rpath) {
        string result = rpath;
        free(rpath);
        if(result.length() < basePath.length())
            return "/";
        else
            return result == basePath ? "/" : result.substr(basePath.length());
    }
    return _path;
}

void FileTable::Insert(uint64_t handle, const std::string& path) {
    assert(handle);
    path2handle[path]   = handle;
    handle2path[handle] = path;
}

int FileTable::Stat(const string& _path, struct stat* fstat) {
    NFSDLock lock(mutex);
    
    string     path   = Canonicalize(_path);
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

string FileTable::MakePath(const string& directory, const string& file) {
    string result = directory;
    if(directory[directory.length()-1] != '/')
        result += "/";
    result += file;
    return result;
}

static uint64_t make_file_handle(const struct stat* fstat) {
    uint64_t result = fstat->st_dev;
    result = rotl(result, 32) ^ fstat->st_ino;
    if(result == 0) result = ~result;
    return result;
}

uint64_t FileTable::GetFileHandle(const string& _path) {
    NFSDLock lock(mutex);

    string path = Canonicalize(_path);
    
    uint64_t result = 0;
    map<string, uint64_t>::iterator iter = path2handle.find(path);
    if(iter != path2handle.end())
        result = iter->second;
    else {
        struct stat fstat;
        if(stat(path, &fstat) == 0) {
            result = make_file_handle(&fstat);
            Insert(result, path);
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

    string pathFrom = Canonicalize(_pathFrom);
    string pathTo   = Canonicalize(_pathTo);

    if(pathFrom == pathTo) return;
    
    map<string, uint64_t>::iterator from_p2h = path2handle.find(pathFrom);
    if(from_p2h != path2handle.end()) {
        Insert(from_p2h->second, pathTo);

        map<uint64_t, string>::iterator from_h2p = handle2path.find(from_p2h->second);
        if(from_h2p != handle2path.end())
            handle2path.erase(from_h2p);
        path2handle.erase(from_p2h);
    }
}

void FileTable::Remove(const string& _path) {
    NFSDLock lock(mutex);

    string path = Canonicalize(_path);

    map<string, uint64_t>::iterator iter = path2handle.find(path);
    if(iter != path2handle.end()) {
        uint64_t handle = iter->second;
        path2handle.erase(iter);
        map<uint64_t, string>::iterator iter = handle2path.find(handle);
        if(iter != handle2path.end())
            handle2path.erase(iter);
        
        GetDB(handle)->Remove(path);
    }
}

FileAttrDB* FileTable::GetDB(uint64_t handle) {
    NFSDLock lock(mutex);
    
    string path = handle2path[handle];
    char tmp[MAXPATHLEN];
    strncpy(tmp, path.c_str(), MAXPATHLEN-1);
    string   dbdir     = dirname(tmp);
    uint64_t dirhandle = GetFileHandle(dbdir);
    
    FileAttrDB* result = NULL;
    map<uint64_t, FileAttrDB*>::iterator iter = handle2db.find(dirhandle);
    if(iter != handle2db.end())
        result = iter->second;
    else {
        result               = new FileAttrDB(*this, dbdir);
        handle2db[dirhandle] = result;
    }
    return result;
}

void FileTable::SetFileAttrs(const std::string& path, const FileAttrs& fstat) {
    NFSDLock lock(mutex);

    GetDB(GetFileHandle(path))->SetFileAttrs(path, fstat);
}

FileAttrs* FileTable::GetFileAttrs(const std::string& path) {
    NFSDLock lock(mutex);

    return GetDB(GetFileHandle(path))->GetFileAttrs(path);
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

int FileTable::readlink(const string& _path, string& result) {
    string path = basePath;
    path += _path;
    
    struct stat sb;
    ssize_t nbytes, bufsiz;
    
    if (lstat(path.c_str(), &sb) == -1)
        return errno;
    
    /* Add one to the link size, so that we can determine whether
     the buffer returned by readlink() was truncated. */
    
    bufsiz = sb.st_size + 1;
    
    /* Some magic symlinks under (for example) /proc and /sys
     report 'st_size' as zero. In that case, take PATH_MAX as
     a "good enough" estimate. */
    
    if (sb.st_size == 0)
        bufsiz = MAXPATHLEN;
    
    char buf[bufsiz];
    
    nbytes = ::readlink(path.c_str(), buf, bufsiz);
    if (nbytes == -1)
        return errno;
    
    buf[nbytes] = '\0';
    result = buf;
    
    return 0;
}

int FileTable::link(const string& _from, const string& _to, bool soft) {
    string from = _from
    ;
    if(!(soft)) {
        from  = basePath;
        from += _from;
    }
    
    string to = basePath;
    to += _to;
    
    if(soft) return ::symlink(from.c_str(), to.c_str());
    else     return ::link   (to.c_str(), from.c_str());
}

int FileTable::mkdir(const string& _path, mode_t mode) {
    string path = basePath;
    path += _path;
    return ::mkdir(path.c_str(), mode);
}

int FileTable::nftw(const string& _path, int (*fn)(const char *, const struct stat *ptr, int flag, struct FTW *), int depth, int flags) {
    string path = basePath;
    path += _path;
    return ::nftw(path.c_str(), fn, depth, flags);
}

int FileTable::statvfs(const string& _path, struct statvfs* buf) {
    string path = basePath;
    path += _path;
    return ::statvfs(path.c_str(), buf);
}

int FileTable::stat(const string& _path, struct stat* buf) {
    string path = basePath;
    path += _path;
    return lstat(path.c_str(), buf);
}

int FileTable::utimes(const string& _path, const struct timeval times[2]) {
    string path = basePath;
    path += _path;
    return ::utimes(path.c_str(), times);
}

//----- file attribute database

FileAttrDB::FileAttrDB(FileTable& ft, const string& directory) : ft(ft) {
    path = FileTable::MakePath(directory, FILE_TABLE_NAME);
    FILE* file = ft.fopen(path, "r");
    if(file) {
        for(;;) {
            string fname;
            FileAttrs* fattrs = new FileAttrs(file, fname);
            if(fname.length() == 0) {
                delete fattrs;
                break;
            }
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
