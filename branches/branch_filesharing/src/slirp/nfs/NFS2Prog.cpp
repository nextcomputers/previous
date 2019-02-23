#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>

#include "NFS2Prog.h"
#include "FileTable.h"
#include "nfsd.h"

using namespace std;

enum
{
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

enum
{
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

static void set_attrs(const string& path, uint32_t mode, uint32_t uid, uint32_t gid) {
    if(mode != ~0) chmod(path.c_str(), mode | S_IWUSR);
    nfsd_ft.SetMode(path, mode);
    nfsd_ft.SetUID(path, uid);
    nfsd_ft.SetGID(path, gid);
    nfsd_ft.Sync();
}

int CNFS2Prog::ProcedureSETATTR(void) {
    string   path;

	Log("SETATTR");
	GetPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint64_t atime;
    uint64_t mtime;

	m_in->Read(&mode);
    m_in->Read(&uid);
    m_in->Read(&gid);
    m_in->Read(&size);
    m_in->Read(&atime);
    m_in->Read(&mtime);
    
    set_attrs(path, mode, uid, gid);
    
    m_out->Write(NFS_OK);
	WriteFileAttributes(path);
    return PRC_OK;
}

int CNFS2Prog::ProcedureLOOKUP(void) {
    string path;

	GetFullPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

	m_out->Write(NFS_OK);
    m_out->Write(nfsd_ft.GetFileHandle(path), FHSIZE);
	WriteFileAttributes(path.c_str());
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
    FILE* file = fopen(path.c_str(), "rb");
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

	FILE* file = fopen(path.c_str(), "r+b");
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
    
	Log("CREATE");
	if(!(GetFullPath(path)))
		return PRC_OK;
    
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint64_t atime;
    uint64_t mtime;
    
    m_in->Read(&mode);
    m_in->Read(&uid);
    m_in->Read(&gid);
    m_in->Read(&size);
    m_in->Read(&atime);
    m_in->Read(&mtime);

    fclose(fopen(path.c_str(), "wb")); // touch
    
    set_attrs(path, mode, uid, gid);
    
	m_out->Write(NFS_OK);
    m_out->Write(nfsd_ft.GetFileHandle(path), FHSIZE);
	WriteFileAttributes(path);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureREMOVE(void) {
    string path;

	Log("REMOVE");
	GetFullPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

    nfsd_ft.Remove(path);
	remove(path.c_str());
	m_out->Write(NFS_OK);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureRENAME(void) {
    string pathFrom;
    string pathTo;

	Log("RENAME");
	GetFullPath(pathFrom);
	if (!(CheckFile(pathFrom)))
		return PRC_OK;
	GetFullPath(pathTo);
    
    nfsd_ft.Rename(pathFrom, pathTo);
    rename(pathFrom.c_str(), pathTo.c_str());
	m_out->Write(NFS_OK);

    return PRC_OK;
}

int CNFS2Prog::ProcedureMKDIR(void) {
    string path;

	Log("MKDIR");
	if(!(GetFullPath(path)))
		return PRC_OK;

    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint64_t atime;
    uint64_t mtime;
    
    m_in->Read(&mode);
    m_in->Read(&uid);
    m_in->Read(&gid);
    m_in->Read(&size);
    m_in->Read(&atime);
    m_in->Read(&mtime);

	mkdir(path.c_str(), ACCESSPERMS);
    set_attrs(path, mode, uid, gid);
    
	m_out->Write(NFS_OK);
    m_out->Write(nfsd_ft.GetFileHandle(path), FHSIZE);
	WriteFileAttributes(path);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureRMDIR(void) {
    string path;

	Log("RMDIR");
	GetFullPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

    nfsd_ft.Remove(path);
	rmdir(path.c_str());
	m_out->Write(NFS_OK);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureREADDIR(void) {
    string path;
	DIR*   handle;

	GetPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

	m_out->Write(NFS_OK);
    handle = opendir(path.c_str());
	if (handle) {
        for(struct dirent* fileinfo = readdir(handle); fileinfo; fileinfo = readdir(handle)) {
            if(strcmp(FILE_TABLE_NAME, fileinfo->d_name)) {
                m_out->Write(1);  //value follows
                string fpath = path;
                fpath += "/";
                fpath += fileinfo->d_name;
                m_out->Write((uint32_t)nfsd_ft.GetFileId(fpath));  //file id
                XDRString name(fileinfo->d_name);
                m_out->Write(name);
                m_out->Write(nfsd_ft.cookie);  //cookie
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

    statvfs(path.c_str(), &data);
    
	m_out->Write(NFS_OK);
	m_out->Write(data.f_bsize);  //transfer size
	m_out->Write(data.f_bsize);  //block size
	m_out->Write(data.f_blocks);  //total blocks
	m_out->Write(data.f_bfree);  //free blocks
	m_out->Write(data.f_bavail);  //available blocks
    
    return PRC_OK;
}

bool CNFS2Prog::GetPath(string& result) {
	FileHandle fhandle;
	m_in->Read((void*)&fhandle, FHSIZE);
    return nfsd_ft.GetAbsolutePath(fhandle, result);
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
    if(access(path.c_str(), R_OK)) {
		m_out->Write(NFSERR_NOENT);
		return false;
	}
	return true;
}

bool CNFS2Prog::WriteFileAttributes(const string& path) {
	struct stat data;

	if (nfsd_ft.Stat(path, &data) != 0)
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
