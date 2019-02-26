#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <ftw.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>

#include "NFS2Prog.h"
#include "FileTable.h"
#include "nfsd.h"

using namespace std;

enum {
	NFS_OK = 0,
	NFSERR_PERM = 1,
	NFSERR_NOENT = 2,
	NFSERR_IO = 5,
	NFSERR_NXIO = 6,
	NFSERR_ACCES = 13,
	NFSERR_EXIST = 17,
	NFSERR_NODEV = 19,
	NFSERR_NOTDIR = 20,
	NFSERR_ISDIR = 21,
	NFSERR_FBIG = 27,
	NFSERR_NOSPC = 28,
	NFSERR_ROFS = 30,
	NFSERR_NAMETOOLONG = 63,
	NFSERR_NOTEMPTY = 66,
	NFSERR_DQUOT = 69,
	NFSERR_STALE = 70,
	NFSERR_WFLUSH = 99,
};

enum {
	NFNON = 0,
	NFREG = 1,
	NFDIR = 2,
	NFBLK = 3,
	NFCHR = 4,
	NFLNK = 5,
};

CNFS2Prog::CNFS2Prog() : CRPCProg(PROG_NFS, 2, "nfsd"), m_defUID(0), m_defGID(0) {
    #define RPC_PROG_CLASS CNFS2Prog
    SetProc(1,  GETATTR);
    SetProc(2,  SETATTR);
    SetProc(4,  LOOKUP);
    SetProc(5,  READLINK);
    SetProc(6,  READ);
    SetProc(7,  WRITECACHE);
    SetProc(8,  WRITE);
    SetProc(9,  CREATE);
    SetProc(10, REMOVE);
    SetProc(11, RENAME);
    SetProc(12, LINK);
    SetProc(13, SYMLINK);
    SetProc(14, MKDIR);
    SetProc(15, RMDIR);
    SetProc(16, READDIR);
    SetProc(17, STATFS);
}

CNFS2Prog::~CNFS2Prog() { }

void CNFS2Prog::SetUserID(unsigned int nUID, unsigned int nGID) {
	m_defUID = nUID;
	m_defGID = nGID;
}

int CNFS2Prog::ProcedureGETATTR(void) {
    string path;

	GetPath(path);
    Log("GETATTR %s", path.c_str());
	if (!(CheckFile(path)))
		return PRC_OK;

	m_out->Write(NFS_OK);
	WriteFileAttributes(path);
    return PRC_OK;
}

static void set_attrs(const string& path, const FileAttrs& fstat) {
    if(FileAttrs::Valid(fstat.mode))
        nfsd_fts[0]->chmod(path, fstat.mode | S_IWUSR);
    
    timeval times[2];
    timeval now;
    gettimeofday(&now, NULL);
    times[0].tv_sec  = FileAttrs::Valid(fstat.atime_sec)  ? fstat.atime_sec  : now.tv_sec;
    times[0].tv_usec = FileAttrs::Valid(fstat.atime_usec) ? fstat.atime_usec : now.tv_usec;
    times[1].tv_sec  = FileAttrs::Valid(fstat.mtime_sec)  ? fstat.mtime_sec  : now.tv_sec;
    times[1].tv_usec = FileAttrs::Valid(fstat.mtime_usec) ? fstat.mtime_usec : now.tv_usec;
    if(FileAttrs::Valid(fstat.atime_sec) || FileAttrs::Valid(fstat.mtime_sec))
        nfsd_fts[0]->utimes(path, times);
    nfsd_fts[0]->SetFileAttrs(path, fstat);
}

int CNFS2Prog::ProcedureSETATTR(void) {
    string   path;

	Log("SETATTR");
	GetPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

    FileAttrs fstat(m_in);
    set_attrs(path, fstat);
    
    m_out->Write(NFS_OK);
	WriteFileAttributes(path);
    return PRC_OK;
}

static void write_handle(XDROutput* xout, uint64_t handle) {
    uint64_t data[4] = {handle,0,0,0};
    xout->Write(data, FHSIZE);
}

int CNFS2Prog::ProcedureLOOKUP(void) {
    string path;

	GetFullPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

	m_out->Write(NFS_OK);
    write_handle(m_out, nfsd_fts[0]->GetFileHandle(path));
	WriteFileAttributes(path);
    return PRC_OK;
}

int nfs_err(int error) {
    switch (error) {
        case ENOENT: return NFSERR_NOENT;
        case EACCES: return NFSERR_ACCES;
        default:     return NFSERR_IO;
    }
}

int CNFS2Prog::ProcedureREADLINK(void) {
    string path;

    GetPath(path);
    Log("READLINK %s", path.c_str());
    if (!(CheckFile(path)))
        return PRC_OK;
    
    string result;
    int err = nfsd_fts[0]->readlink(path, result);
    if(err) {
        m_out->Write(nfs_err(err));
    } else {
        m_out->Write(NFS_OK);
        XDRString data(result);
        m_out->Write(data);
    }
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureREAD(void) {
    string path;
    
	uint32_t nOffset, nCount, nTotalCount;

	GetPath(path);
    Log("READ %s", path.c_str());
	if (!(CheckFile(path)))
		return PRC_OK;

	m_in->Read(&nOffset);
	m_in->Read(&nCount);
	m_in->Read(&nTotalCount);
    
    XDROpaque buffer(nCount);
    FILE* file = nfsd_fts[0]->fopen(path, "rb");
    if(file) {
        fseek(file, nOffset, SEEK_SET);
        nCount = fread(buffer.m_data, sizeof(uint8_t), buffer.m_size, file);
        fclose(file);
        buffer.SetSize(nCount);
        m_out->Write(NFS_OK);
    } else {
        buffer.SetSize(0);
        m_out->Write(nfs_err(errno));
    }
	WriteFileAttributes(path);
    m_out->Write(buffer);

    return PRC_OK;
}

int CNFS2Prog::ProcedureWRITECACHE(void) {
    m_out->Write(NFS_OK);
    return PRC_OK;
}

int CNFS2Prog::ProcedureWRITE(void) {
    string path;
	uint32_t nBeginOffset, nOffset, nTotalCount;

	GetPath(path);
    Log("WRITE %s", path.c_str());
	if (!(CheckFile(path)))
		return PRC_OK;

	m_in->Read(&nBeginOffset);
	m_in->Read(&nOffset);
	m_in->Read(&nTotalCount);

    XDROpaque buffer;
	m_in->Read(buffer);

	FILE* file = nfsd_fts[0]->fopen(path, "r+b");
    if(file) {
        fseek(file, nOffset, SEEK_SET);
        fwrite(buffer.m_data, sizeof(uint8_t), buffer.m_size, file);
        fclose(file);

        m_out->Write(NFS_OK);
    } else {
        m_out->Write(nfs_err(errno));
    }
	WriteFileAttributes(path);

    return PRC_OK;
}

int CNFS2Prog::ProcedureCREATE(void) {
    string path;
    
	if(!(GetFullPath(path)))
		return PRC_OK;
    Log("CREATE %s", path.c_str());

    FileAttrs fstat(m_in);
    if(!(FileAttrs::Valid(fstat.uid))) fstat.uid = m_defUID;
    if(!(FileAttrs::Valid(fstat.gid))) fstat.gid = m_defGID;
    fclose(nfsd_fts[0]->fopen(path, "wb")); // touch
    set_attrs(path, fstat);
    
	m_out->Write(NFS_OK);
    write_handle(m_out, nfsd_fts[0]->GetFileHandle(path));
	WriteFileAttributes(path);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureREMOVE(void) {
    string path;

	GetFullPath(path);
    Log("REMOVE %s", path.c_str());
	if (!(CheckFile(path)))
		return PRC_OK;

    nfsd_fts[0]->Remove(path);
	nfsd_fts[0]->remove(path);
	m_out->Write(NFS_OK);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureRENAME(void) {
    string pathFrom;
    string pathTo;

	GetFullPath(pathFrom);
	if (!(CheckFile(pathFrom)))
		return PRC_OK;
	GetFullPath(pathTo);
    Log("RENAME %s->%s", pathFrom.c_str(), pathTo.c_str());

    nfsd_fts[0]->Move(pathFrom, pathTo);
    nfsd_fts[0]->rename(pathFrom, pathTo);
	m_out->Write(NFS_OK);

    return PRC_OK;
}

int CNFS2Prog::ProcedureLINK(void) {
    string to;
    string from;

    GetPath(to);
    GetFullPath(from);
    Log("LINK %s->%s", from.c_str(), to.c_str());
    
    if(nfsd_fts[0]->link(from, to, false))
        m_out->Write(nfs_err(errno));
    else
        m_out->Write(NFS_OK);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureSYMLINK(void) {
    string to;
    
    GetFullPath(to);
    XDRString from;
    m_in->Read(from);
    Log("SYMLINK %s->%s", from.Get(), to.c_str());
    
    FileAttrs fstat(m_in);
    nfsd_fts[0]->link(from.Get(), to, true);
    set_attrs(to, fstat);

    m_out->Write(NFS_OK);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureMKDIR(void) {
    string path;

	Log("MKDIR");
	if(!(GetFullPath(path)))
		return PRC_OK;

    FileAttrs fstat(m_in);
	nfsd_fts[0]->mkdir(path, ACCESSPERMS);
    set_attrs(path, fstat);
    
	m_out->Write(NFS_OK);
    write_handle(m_out, nfsd_fts[0]->GetFileHandle(path));
	WriteFileAttributes(path);
    
    return PRC_OK;
}

static int remove(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    return remove(fpath);
}

int CNFS2Prog::ProcedureRMDIR(void) {
    string path;

	Log("RMDIR");
	GetFullPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;
    
    nfsd_fts[0]->Remove(path);
    nfsd_fts[0]->nftw(path, remove, 64, FTW_DEPTH | FTW_PHYS);
	m_out->Write(NFS_OK);
    
    return PRC_OK;
}

uint32_t make_file_id(uint64_t handle) {
    return (uint32_t)(handle ^ (handle >> 32LL));
}

int CNFS2Prog::ProcedureREADDIR(void) {
    string   path;
	DIR*     handle;
    uint32_t cookie;
    uint32_t count;

	GetPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;
    m_in->Read(&cookie);
    m_in->Read(&count);
    
	m_out->Write(NFS_OK);
    handle = nfsd_fts[0]->opendir(path);
    uint32_t eof = true;
	if (handle) {
        int skip = cookie;
        for(struct dirent* fileinfo = readdir(handle); fileinfo; fileinfo = readdir(handle)) {
            if(strcmp(FILE_TABLE_NAME, fileinfo->d_name)) {
                if(--skip <= 0) {
                    m_out->Write(1);  //value follows
                    m_out->Write(make_file_id(nfsd_fts[0]->GetFileHandle(FileTable::MakePath(path, fileinfo->d_name))));  //file id
                    string dname = fileinfo->d_name;
                    XDRString name(dname);
                    m_out->Write(name);
                    m_out->Write(cookie);
                    cookie++;
                }
            }
            if(m_out->GetSize() >= count) {
                eof = false;
                break;
            }
		};
		closedir(handle);
	}
	m_out->Write(0);  //no value follows
	m_out->Write(eof);

    return PRC_OK;
}

int CNFS2Prog::ProcedureSTATFS(void) {
    string path;
	struct statvfs data;

	Log("STATFS");
	GetPath(path);
	if(!(CheckFile(path)))
		return PRC_OK;

    nfsd_fts[0]->statvfs(path, &data);
    
	m_out->Write(NFS_OK);
	m_out->Write(data.f_bsize);  //transfer size
	m_out->Write(data.f_bsize);  //block size
	m_out->Write(data.f_blocks);  //total blocks
	m_out->Write(data.f_bfree);  //free blocks
	m_out->Write(data.f_bavail);  //available blocks
    
    return PRC_OK;
}

static uint64_t read_handle(XDRInput* xin) {
    uint64_t data[4];
    xin->Read((void*)data, FHSIZE);
    return data[0];
}

bool CNFS2Prog::GetPath(string& result) {
    return nfsd_fts[0]->GetAbsolutePath(read_handle(m_in), result);
}

bool CNFS2Prog::GetFullPath(string& result) {
    if(!(GetPath(result)))
        return false;
    
    XDRString path;
    m_in->Read(path);
    if(result[result.length()-1] != '/') result += "/";
    result += path.Get();
    return true;
}

bool CNFS2Prog::CheckFile(const string& path) {
	if (path.length() == 0) {
		m_out->Write(NFSERR_STALE);
		return false;
	}
    
    // links always pass (will be resolved on the client side via readlink)
    struct stat fstat;
    nfsd_fts[0]->Stat(path, &fstat);
    if((fstat.st_mode & S_IFMT) == S_IFLNK)
        return true;
    
    if(nfsd_fts[0]->access(path, F_OK)) {
		m_out->Write(NFSERR_NOENT);
		return false;
	}

    return true;
}

bool CNFS2Prog::WriteFileAttributes(const string& path) {
	struct stat data;

	if (nfsd_fts[0]->Stat(path, &data) != 0)
		return false;

    uint32_t type = NFNON;
    if     (S_ISREG(data.st_mode)) type = NFREG;
    else if(S_ISDIR(data.st_mode)) type = NFDIR;
    else if(S_ISBLK(data.st_mode)) type = NFBLK;
    else if(S_ISCHR(data.st_mode)) type = NFCHR;
    else if(S_ISLNK(data.st_mode)) type = NFLNK;
            
	m_out->Write(type);  //type
	m_out->Write(data.st_mode);  //mode
	m_out->Write(data.st_nlink);  //nlink	
	m_out->Write(data.st_uid);  //uid
	m_out->Write(data.st_gid);  //gid
	m_out->Write(data.st_size);  //size
	m_out->Write(data.st_blksize);  //blocksize
	m_out->Write(data.st_rdev);  //rdev
	m_out->Write(data.st_blocks);  //blocks
	m_out->Write(data.st_dev);  //fsid
    m_out->Write(data.st_ino);  //fileid
	m_out->Write(data.st_atimespec.tv_sec);  //atime
	m_out->Write(data.st_atimespec.tv_nsec / 1000);  //atime
	m_out->Write(data.st_mtimespec.tv_sec);  //mtime
	m_out->Write(data.st_mtimespec.tv_nsec / 1000);  //mtime
	m_out->Write(data.st_ctimespec.tv_sec);  //ctime
	m_out->Write(data.st_ctimespec.tv_nsec / 1000);  //ctime
	return true;
}
