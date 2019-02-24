#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <ftw.h>
#include <libgen.h>

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
    SetProc(6,  READ);
    SetProc(8,  WRITE);
    SetProc(9,  CREATE);
    SetProc(10, REMOVE);
    SetProc(11, RENAME);
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
    if(FileAttrs::Valid(fstat.mode)) nfsd_fts[0]->chmod(path, fstat.mode | S_IWUSR);
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
        case EACCES: return NFSERR_ACCES;
        default:     return NFSERR_IO;
    }
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

int CNFS2Prog::ProcedureSYMLINK(void) {
    string pathFrom;
    
    GetFullPath(pathFrom);
    XDRString pathTo;
    m_in->Read(pathTo);
    Log("SYMLINK %s->%s", pathFrom.c_str(), pathTo.Get());
    
    FileAttrs fstat(m_in);
    nfsd_fts[0]->symlink(pathTo.Get(), pathFrom);
    set_attrs(pathFrom, fstat);

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
    string path;
	DIR*   handle;

	GetPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

	m_out->Write(NFS_OK);
    handle = nfsd_fts[0]->opendir(path);
	if (handle) {
        for(struct dirent* fileinfo = readdir(handle); fileinfo; fileinfo = readdir(handle)) {
            if(strcmp(FILE_TABLE_NAME, fileinfo->d_name)) {
                m_out->Write(1);  //value follows
                string fpath = path;
                fpath += "/";
                fpath += fileinfo->d_name;
                m_out->Write(make_file_id(nfsd_fts[0]->GetFileHandle(fpath)));  //file id
                XDRString name(fileinfo->d_name);
                m_out->Write(name);
                m_out->Write(nfsd_fts[0]->cookie);  //cookie
            }
		};
		closedir(handle);
	}
	m_out->Write(0);  //no value follows
	m_out->Write(1);  //EOF

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
    result += "/";
    result += path.Get();
    return true;
}

bool CNFS2Prog::CheckFile(const string& path) {
	if (path.length() == 0) {
		m_out->Write(NFSERR_STALE);
		return false;
	}
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

    uint32_t type;
	switch (data.st_mode & S_IFMT) {
		case S_IFREG: type = NFREG; break;
		case S_IFDIR: type = NFDIR; break;
		case S_IFCHR: type = NFCHR; break;
		default:      type = NFNON; break;
	}
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
	m_out->Write(data.st_atime);  //atime
	m_out->Write(0);  //atime
	m_out->Write(data.st_mtime);  //mtime
	m_out->Write(0);  //mtime
	m_out->Write(data.st_ctime);  //ctime
	m_out->Write(0);  //ctime
	return true;
}
