#include <string.h>
#include <assert.h>
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

#include "devices.h"

using namespace std;

static const uint32_t INVALID = ~0;

FileAttrs::FileAttrs(XDRInput* xin) : rdev(INVALID) {
    xin->Read(&mode);
    xin->Read(&uid);
    xin->Read(&gid);
    xin->Read(&size);
    xin->Read(&atime_sec);
    xin->Read(&atime_usec);
    xin->Read(&mtime_sec);
    xin->Read(&mtime_usec);
    
}

FileAttrs::FileAttrs(const struct stat& stat) :
mode      (stat.st_mode),
uid       (stat.st_uid),
gid       (stat.st_gid),
size      (stat.st_size),
atime_sec (stat.st_atimespec.tv_sec),
atime_usec(stat.st_atimespec.tv_nsec / 1000),
mtime_sec (stat.st_mtimespec.tv_sec),
mtime_usec(stat.st_mtimespec.tv_nsec / 1000),
rdev      (stat.st_rdev)
{}

FileAttrs::FileAttrs(const FileAttrs& attrs) :
mode      (INVALID),
uid       (INVALID),
gid       (INVALID),
size      (INVALID),
atime_sec (INVALID),
atime_usec(INVALID),
mtime_sec (INVALID),
mtime_usec(INVALID),
rdev      (INVALID)
{Update(attrs);}

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
    UPDATE_ATTR(rdev);
}

FileAttrs::FileAttrs(FILE* fin, string& name) {
    char cname[MAXNAMELEN];
    if(fscanf(fin, "0%o:%d:%d:%d:%s\n", &mode, &uid, &gid, &rdev, cname) == 5)
        name = cname;
    else
        name.resize(0);
}

void FileAttrs::Write(FILE* fout, const string& name) {
    fprintf(fout, "0%o:%d:%d:%d:%s\n", mode, uid, gid, rdev, name.c_str());
}

bool FileAttrs::Valid(uint32_t statval) {return statval != 0xFFFFFFFF;}

static string filename(const string& path) {
    return path.substr(path.find_last_of("/\\") + 1);
}

static string dirname(const string& path) {
    string result;
    char tmp[MAXPATHLEN];
    strncpy(tmp, path.c_str(), MAXPATHLEN-1);
    result = ::dirname(tmp);
    return result;
}

int FileTable::ThreadProc(void *lpParameter) {
    ((FileTable*)lpParameter)->Run();
    return 0;
}

//----- file table

FileTable::FileTable(const string& _basePath, const string& _basePathAlais) : mutex(host_mutex_create()) {
    basePathAlias = _basePathAlais;
    basePath      = _basePath;
    if(basePath[basePath.length()-1] == '/') basePath.resize(basePath.size()-1);

    for(int d = 0; DEVICES[d][0]; d++) {
        const char* perm  = DEVICES[d][0];
        uint32_t    major = atoi(DEVICES[d][1]);
        uint32_t    minor = atoi(DEVICES[d][2]);
        const char* name  = DEVICES[d][3];
        uint32_t    rdev  = major << 8 | minor;
        if(     perm[0] == 'b') blockDevices[name]     = rdev;
        else if(perm[0] == 'c') characterDevices[name] = rdev;
    }
    
    host_atomic_set(&doRun, 1);
    thread = host_thread_create(&ThreadProc, "FileTable", this);
}

FileTable::~FileTable() {
    host_atomic_set(&doRun, 0);
    host_thread_wait(thread);
    {
        NFSDLock lock(mutex);
        
        for(map<uint64_t,FileAttrDB*>::iterator it = handle2db.begin(); it != handle2db.end(); it++)
            delete it->second;
    }
    host_mutex_destroy(mutex);
}

string FileTable::GetBasePathAlias(void) {
    return basePathAlias;
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

static const char* memrchr(const char* s, char c, size_t n) {
    if (n > 0) {
        const char*  p = s;
        const char*  q = p + n;
        while (1) {
            q--; if (q < p || q[0] == c) break;
            q--; if (q < p || q[0] == c) break;
            q--; if (q < p || q[0] == c) break;
            q--; if (q < p || q[0] == c) break;
        }
        if (q >= p)
            return q;
    }
    return NULL;
}

string FileTable::ResolveAlias(const string& path) {
    string result = path;
    if(basePathAlias != "/" && path.find(basePathAlias) == 0)
        result.erase(0, basePathAlias.length());
    if(result.length()==0)
        result = "/";
    return result;
}

string FileTable::Canonicalize(const string& _path) {
#if 0
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
#else
    char pwd[] = "/";
    char res[MAXPATHLEN];
    const char* src = _path.c_str();
    size_t res_len;
    size_t src_len = strlen(src);
    
    const char * ptr = src;
    const char * end = &src[src_len];
    const char * next;
    
    if (src_len == 0 || src[0] != '/') {
        // relative path
        size_t pwd_len;
        
        pwd_len = strlen(pwd);
        memcpy(res, pwd, pwd_len);
        res_len = pwd_len;
    } else {
        res_len = 0;
    }
    
    for (ptr = src; ptr < end; ptr=next+1) {
        size_t len;
        next = (char*)memchr(ptr, '/', end-ptr);
        if (next == NULL)
            next = end;
        len = next-ptr;
        switch(len) {
            case 2:
                if (ptr[0] == '.' && ptr[1] == '.') {
                    const char * slash = (char*)memrchr(res, '/', res_len);
                    if (slash != NULL)
                        res_len = slash - res;
                    continue;
                }
                break;
            case 1:
                if (ptr[0] == '.') continue;
                else               break;
            case 0:                continue;
        }
        
        if (res_len != 1)
            res[res_len++] = '/';
        
        memcpy(&res[res_len], ptr, len);
        res_len += len;
    }
    
    if (res_len == 0) res[res_len++] = '/';
    res[res_len] = '\0';
    
    return res;
#endif
}

bool FileTable::IsBlockDevice(const string& fname) {
    return blockDevices.find(fname) != blockDevices.end();
}

bool FileTable::IsCharDevice(const string& fname) {
    return characterDevices.find(fname) != characterDevices.end();
}

bool FileTable::IsDevice(const string& path, string& fname) {
    string   directory = dirname(path);
    fname              = filename(path);
    
    const size_t len = directory.size();
    return
        len >= 4 &&
        directory[len-4] == '/' &&
        directory[len-3] == 'd' &&
        directory[len-2] == 'e' &&
        directory[len-1] == 'v' &&
    (IsBlockDevice(fname) || IsCharDevice(fname));
}

int FileTable::Stat(const string& _path, struct stat& fstat) {
    NFSDLock lock(mutex);
    
    string     path   = ResolveAlias(_path);
    int        result = stat(path, fstat);
    FileAttrs* attrs  = GetFileAttrs(path);
    if(attrs) {
        uint32_t mode = attrs->mode & S_IFMT          ? attrs->mode : fstat.st_mode;
        fstat.st_mode = FileAttrs::Valid(attrs->mode) ? mode        : fstat.st_mode;
        fstat.st_uid  = FileAttrs::Valid(attrs->uid)  ? attrs->uid  : fstat.st_uid;
        fstat.st_gid  = FileAttrs::Valid(attrs->gid)  ? attrs->gid  : fstat.st_gid;
        fstat.st_rdev = FileAttrs::Valid(attrs->rdev) ? attrs->rdev : fstat.st_rdev;
    }
    
    string fname;
    if(IsDevice(path, fname)) {
        map<string,uint32_t>::iterator iter = blockDevices.find(fname);
        if(iter != blockDevices.end()) {
            fstat.st_mode &= ~S_IFMT;
            fstat.st_mode |= S_IFBLK;
            fstat.st_rdev = iter->second;
        }
        iter = characterDevices.find(fname);
        if(iter != characterDevices.end()) {
            fstat.st_mode &= ~S_IFMT;
            fstat.st_mode |= S_IFCHR;
            fstat.st_rdev = iter->second;
        }
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

static uint64_t make_file_handle(const struct stat& fstat) {
    uint64_t result = fstat.st_dev;
    result = rotl(result, 32) ^ fstat.st_ino;
    if(result == 0) result = ~result;
    return result;
}

uint64_t FileTable::GetFileHandle(const string& _path) {
    NFSDLock lock(mutex);

    string path = ResolveAlias(_path);
    
    uint64_t result = 0;
    struct stat fstat;
    if(stat(path, fstat) == 0) {
        result = make_file_handle(fstat);
        handle2path[result] = path;
    } else {
        printf("No file handle for %s", _path.c_str());
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

void FileTable::Move(const string& pathFrom, const string& pathTo) {
    NFSDLock lock(mutex);

    uint64_t handle = GetFileHandle(pathFrom);
    if(handle) {
        map<uint64_t, string>::iterator iter = handle2path.find(handle);
        if(iter != handle2path.end())
            handle2path.erase(iter);
    }
}

void FileTable::Remove(const string& path) {
    NFSDLock lock(mutex);

    uint64_t handle = GetFileHandle(path);
    if(handle) {
        map<uint64_t, string>::iterator iter = handle2path.find(handle);
        if(iter != handle2path.end()) {
            handle2path.erase(iter);
            GetDB(handle)->Remove(path);
        }
    }
}

FileAttrDB* FileTable::GetDB(uint64_t handle) {
    NFSDLock lock(mutex);
    
    string   path      = handle2path[handle];
    string   dbdir     = dirname(path);
    uint64_t dirhandle = GetFileHandle(dbdir);
    
    if(dirhandle) {
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
    return NULL;
}

void FileTable::SetFileAttrs(const std::string& path, const FileAttrs& fstat) {
    NFSDLock lock(mutex);

    uint64_t handle = GetFileHandle(path);
    if(handle) GetDB(handle)->SetFileAttrs(path, fstat);
}

FileAttrs* FileTable::GetFileAttrs(const std::string& path) {
    NFSDLock lock(mutex);

    uint64_t handle = GetFileHandle(path);
    return handle ? GetDB(handle)->GetFileAttrs(path) : NULL;
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

uint32_t FileTable::FileId(uint64_t ino) {
    /*if(ino == rootParentIno)
        ino = rootIno;*/
    uint32_t result = ino;
    return (result ^ (ino >> 32LL)) & 0x7FFFFFFF;
}

//----- file io

string FileTable::ToHostPath(const string& path) {
    if(path[0] != '/')
        throw __LINE__;
    
    string result = basePath;
    result += path;
    return result;
}

FILE* FileTable::fopen(const string& path, const char* mode) {
    return ::fopen(ToHostPath(path).c_str(), mode);
}

static int get_error(int result) {
    return result < 0 ? errno : result;
}

int FileTable::chmod(const string& path, mode_t mode) {
    return get_error(::chmod(ToHostPath(path).c_str(), mode));
}

int FileTable::access(const string& path, int mode) {
    return get_error(::access(ToHostPath(path).c_str(), mode));
}

DIR* FileTable::opendir(const string& path) {
    return ::opendir(ToHostPath(path).c_str());
}

int FileTable::remove(const string& path) {
    return get_error(::remove(ToHostPath(path).c_str()));
}

int FileTable::rename(const string& from, const string& to) {
    return get_error(::rename(ToHostPath(from).c_str(), ToHostPath(to).c_str()));
}

int FileTable::readlink(const string& _path, string& result) {
    string path = ToHostPath(_path);
    
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
    string from = soft ? _from : ToHostPath(_from);
    string to   = ToHostPath(_to);
    
    if(soft) return get_error(::symlink(from.c_str(), to.c_str()));
    else     return get_error(::link   (to.c_str(), from.c_str()));
}

int FileTable::mkdir(const string& path, mode_t mode) {
    return get_error(::mkdir(ToHostPath(path).c_str(), mode));
}

int FileTable::nftw(const string& path, int (*fn)(const char *, const struct stat *ptr, int flag, struct FTW *), int depth, int flags) {
    return get_error(::nftw(ToHostPath(path).c_str(), fn, depth, flags));
}

int FileTable::statvfs(const string& path, struct statvfs& fsstat) {
    return get_error(::statvfs(ToHostPath(path).c_str(), &fsstat));
}

int FileTable::stat(const string& path, struct stat& fstat) {
    return get_error(::lstat(ToHostPath(path).c_str(), &fstat));
}

int FileTable::utimes(const string& path, const struct timeval times[2]) {
    return get_error(::utimes(ToHostPath(path).c_str(), times));
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
