#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>

#include "FileTable.h"
#include "RPCProg.h"

using namespace std;

FileTable nfsd_ft;

FileAttrs::FileAttrs(const struct stat* stat) :
mode(stat ? stat->st_mode : 0),
uid (stat ? stat->st_uid : 0),
gid (stat ? stat->st_gid : 0),
reserved(0) {}

FileAttrs::FileAttrs(const FileAttrs& attrs) :
mode(attrs.mode),
uid (attrs.uid),
gid (attrs.gid),
reserved(attrs.reserved) {}

FileAttrs::FileAttrs(FILE* fin, string& name) {
    char cname[MAXNAMELEN];
    if(fscanf(fin, "0%o:%d:%d:%d:%s\n", &mode, &uid, &gid, &reserved, cname))
        name = cname;
}

void FileAttrs::Write(FILE* fout, const string& name) {
    fprintf(fout, "0%o:%d:%d:%d:%s\n", mode, uid, gid, reserved, name.c_str());
}

FileAttrs::~FileAttrs(void) {}

string filename(const string& path) {
    return path.substr(path.find_last_of("/\\") + 1);
}

void FileAttrDB::Add(const std::string& path, const FileAttrs& fattrs) {
    string fname = filename(path);
    if("." == fname || ".." == fname) return;
    attrs[fname] = new FileAttrs(fattrs);
}

void FileAttrDB::Remove(const std::string& path) {
    string name = filename(path);
    map<string, FileAttrs*>::iterator iter = attrs.find(name);
    if(iter != attrs.end()) {
        delete iter->second;
        attrs.erase(iter);
    }
}

FileHandle::FileHandle(const struct stat* stat) :
magic(0x73756f6976657270LL),
handle0(stat ? stat->st_dev : -1),
handle1(stat ? stat->st_ino : -1),
pad3(0), pad4(0), pad5(0), pad6(0), pad7(0),
attrs(stat ? stat : NULL),
dirty(false)
{}

void FileHandle::SetMode(uint32_t mode) {
    this->attrs.mode = mode;
    this->dirty      = true;
}

void FileHandle::SetUID(uint32_t uid) {
    this->attrs.uid = uid;
    this->dirty     = true;
}

void FileHandle::SetGID(uint32_t gid) {
    this->attrs.gid = gid;
    this->dirty     = true;
}

FileTable::FileTable() : cookie(rand()) {}
FileTable::~FileTable() {}

static string canonicalize(const string& path) {
    char* rpath = realpath(path.c_str(), NULL);
    string result = rpath;
    free(rpath);
    return result;
}

int FileTable::Stat(const string& _path, struct stat* statbuf) {
    string path = canonicalize(_path);
    
    int result = stat(path.c_str(), statbuf);
    FileHandle* handle = GetFileHandle(path);
    if(handle) {
        statbuf->st_mode = handle->attrs.mode | (statbuf->st_mode & S_IFMT);
        statbuf->st_uid  = handle->attrs.uid;
        statbuf->st_gid  = handle->attrs.gid;
    }
    return result;
}

uint64_t file_id(const FileHandle* handle) {
    return handle ? handle->handle0 ^ handle->handle1 : 1;
}

FileHandle* FileTable::GetFileHandle(const string& _path) {
    string path = canonicalize(_path);
    
    FileHandle* result = NULL;
    map<string, FileHandle*>::iterator iter = path2handle.find(path);
    if(iter != path2handle.end())
        result = iter->second;
    else {
        struct stat statbuf;
        if(stat(path.c_str(), &statbuf) == 0) {
            result = new FileHandle(&statbuf);
            path2handle[path]        = result;
            id2path[file_id(result)] = path;
            
        }
    }
    return result;
}

bool FileTable::GetAbsolutePath(const FileHandle& handle, string& result) {
    map<uint64_t, string>::iterator iter = id2path.find(file_id(&handle));
    if(iter != id2path.end()) {
        result = iter->second;
        return true;
    }
    return false;
}

uint64_t FileTable::GetFileId(const string& _path) {
    string path = canonicalize(_path);
    
    return file_id(GetFileHandle(path));
}

static string parent_dir(const string& path) {
    char tmp[MAXPATHLEN];
    strncpy(tmp, path.c_str(), MAXPATHLEN-1);
    return dirname(tmp);
}


void FileTable::Rename(const string& _pathFrom, const string& _pathTo) {
    string pathFrom = canonicalize(_pathFrom);
    string pathTo   = canonicalize(_pathTo);
    
    map<string, FileHandle*>::iterator iter = path2handle.find(pathFrom);
    if(iter != path2handle.end()) {
        FileHandle* handle = iter->second;
        path2handle.erase(iter);
        map<uint64_t, string>::iterator iter = id2path.find(file_id(handle));
        if(iter != id2path.end())
            id2path.erase(iter);
        path2handle[pathTo]      = handle;
        id2path[file_id(handle)] = pathTo;
        
        FileAttrDB fattr(parent_dir(pathFrom));
        fattr.Remove(pathFrom);

        handle->dirty = true;
    }
    
    // TODO: rename in file attr db
}

void FileTable::Remove(const string& _path) {
    string path = canonicalize(_path);

    map<string, FileHandle*>::iterator iter = path2handle.find(path);
    if(iter != path2handle.end()) {
        FileHandle* handle = iter->second;
        path2handle.erase(iter);
        map<uint64_t, string>::iterator iter = id2path.find(file_id(handle));
        if(iter != id2path.end())
            id2path.erase(iter);
        
        FileAttrDB fattr(parent_dir(path));
        fattr.Remove(path);

        delete handle;
    }
    
}

void FileTable::SetMode(const string& _path, uint32_t mode) {
    if(mode == ~0) return;
    
    string path = canonicalize(_path);
    
    FileHandle* handle = GetFileHandle(path);
    if(handle) handle->SetMode(mode);
}

void FileTable::SetUID(const string& _path, uint32_t uid) {
    if(uid == ~0) return;
    
    string path = canonicalize(_path);

    FileHandle* handle = GetFileHandle(path);
    if(handle) handle->SetUID(uid);
}

void FileTable::SetGID(const string& _path, uint32_t gid) {
    if(gid == ~0) return;
    
    string path = canonicalize(_path);

    FileHandle* handle = GetFileHandle(path);
    if(handle) handle->SetGID(gid);
}

void FileTable::Sync(void) {
    map<string, FileAttrDB*> dbs;
    for(map<string, FileHandle*>::iterator it = path2handle.begin(); it != path2handle.end(); it++) {
        FileHandle* handle = it->second;
        if(handle->dirty) {
            string db_path = parent_dir(it->first);
            map<string, FileAttrDB*>::iterator iter = dbs.find(db_path);
            if(iter == dbs.end()) dbs[db_path] = new FileAttrDB(db_path);
            dbs[db_path]->Add(it->first, handle->attrs);
            handle->dirty = false;
        }
    }
    
    for(map<string, FileAttrDB*>::iterator it = dbs.begin(); it != dbs.end(); it++) delete it->second;
}

FileAttrDB::FileAttrDB(const string& directory) {
    path = directory;
    path += "/" FILE_TABLE_NAME;
    FILE* file = fopen(path.c_str(), "r");
    if(file) {
        for(;;) {
            string name;
            FileAttrs* fa = new FileAttrs(file, name);
            if(name.length() == 0) break;
            attrs[name] = fa;
        }
        fclose(file);
    }
}

FileAttrDB::~FileAttrDB() {
    if(attrs.size() > 0) {
        FILE* file = fopen(path.c_str(), "w");
        if(file) {
            for(map<string, FileAttrs*>::iterator it = attrs.begin(); it != attrs.end(); it++)
                it->second->Write(file, it->first);
            fclose(file);
        }
    }
}
