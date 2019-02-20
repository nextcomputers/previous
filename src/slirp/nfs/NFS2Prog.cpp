#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "NFSProg.h"
#include "FileTable.h"
#include "nfsd.h"

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

CNFS2Prog::CNFS2Prog() : CRPCProg(PROG_NFS, 2, "nfsd")
{
	m_nUID = m_nGID = 0;
}

CNFS2Prog::~CNFS2Prog()
{
}

void CNFS2Prog::SetUserID(unsigned int nUID, unsigned int nGID)
{
	m_nUID = nUID;
	m_nGID = nGID;
}

int CNFS2Prog::Process(void)
{
	static PPROC pf[] = {
        &CRPCProg::Null,
        (PPROC)&CNFS2Prog::ProcedureGETATTR,
        (PPROC)&CNFS2Prog::ProcedureSETATTR,
        &CRPCProg::Notimp,
        (PPROC)&CNFS2Prog::ProcedureLOOKUP,
        &CRPCProg::Notimp,
        (PPROC)&CNFS2Prog::ProcedureREAD,
        &CRPCProg::Notimp,
        (PPROC)&CNFS2Prog::ProcedureWRITE,
        (PPROC)&CNFS2Prog::ProcedureCREATE,
        (PPROC)&CNFS2Prog::ProcedureREMOVE,
        (PPROC)&CNFS2Prog::ProcedureRENAME,
        &CRPCProg::Notimp,
        &CRPCProg::Notimp,
        (PPROC)&CNFS2Prog::ProcedureMKDIR,
        (PPROC)&CNFS2Prog::ProcedureRMDIR,
        (PPROC)&CNFS2Prog::ProcedureREADDIR,
        (PPROC)&CNFS2Prog::ProcedureSTATFS
        
    };

	if (m_param->proc >= sizeof(pf) / sizeof(PPROC)) return PRC_NOTIMP;

    return (this->*pf[m_param->proc])();
}

int CNFS2Prog::ProcedureGETATTR(void) {
    std::string path;

	GetPath(path);
    Log("GETATTR %s", path.c_str());
	if (!(CheckFile(path.c_str())))
		return PRC_OK;

	m_out->Write(NFS_OK);
	WriteFileAttributes(path.c_str());
    return PRC_OK;
}

int CNFS2Prog::ProcedureSETATTR(void) {
    std::string path;
	uint32_t nMode, nAttr;

	Log("SETATTR");
	GetPath(path);
	if (!(CheckFile(path.c_str())))
		return PRC_OK;

	m_in->Read(&nMode);
	nAttr = 0;
	if ((nMode & 0x100) != 0)
		nAttr |= S_IREAD;
	if ((nMode & 0x80) != 0)
		nAttr |= S_IWRITE;
	chmod(path.c_str(), nAttr);
	m_out->Write(NFS_OK);
	WriteFileAttributes(path.c_str());
    return PRC_OK;
}

int CNFS2Prog::ProcedureLOOKUP(void) {
    std::string path;

	Log("LOOKUP");
	GetFullPath(path);
	if (!(CheckFile(path.c_str())))
		return PRC_OK;

	m_out->Write(NFS_OK);
    m_out->Write(GetFileHandle(path.c_str()), FHSIZE);
	WriteFileAttributes(path.c_str());
    return PRC_OK;
}

int CNFS2Prog::ProcedureREAD(void) {
    std::string path;
    
	uint32_t nOffset, nCount, nTotalCount;
	FILE *file;
	char *pBuffer;
	unsigned char opaque[3] = {0, 0, 0};

	Log("READ");
	GetPath(path);
	if (!(CheckFile(path.c_str())))
		return PRC_OK;

	m_in->Read(&nOffset);
	m_in->Read(&nCount);
	m_in->Read(&nTotalCount);
	file = fopen(path.c_str(), "rb");
	fseek(file, nOffset, SEEK_SET);
	pBuffer = new char[nCount];
	nCount = fread(pBuffer, sizeof(char), nCount, file);
	fclose(file);

	m_out->Write(NFS_OK);
	WriteFileAttributes(path);
	m_out->Write(nCount);  //length
	m_out->Write(pBuffer, nCount);  //contents
	nCount &= 3;
	if (nCount != 0)
		m_out->Write(opaque, 4 - nCount);  //opaque bytes
	delete[] pBuffer;

    return PRC_OK;
}

int CNFS2Prog::ProcedureWRITE(void) {
    std::string path;
	uint32_t nBeginOffset, nOffset, nTotalCount, nCount;
	FILE *file;
	char *pBuffer;

	Log("WRITE");
	GetPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

	m_in->Read(&nBeginOffset);
	m_in->Read(&nOffset);
	m_in->Read(&nTotalCount);
	m_in->Read(&nCount);
	pBuffer = new char[nCount];
	m_in->Read(pBuffer, nCount);

	file = fopen(path.c_str(), "r+b");
	fseek(file, nOffset, SEEK_SET);
	nCount = fwrite(pBuffer, sizeof(char), nCount, file);
	fclose(file);
	delete[] pBuffer;

	m_out->Write(NFS_OK);
	WriteFileAttributes(path);

    return PRC_OK;
}

int CNFS2Prog::ProcedureCREATE(void)
{
    std::string path;
	FILE *file;

	Log("CREATE");
	if(!(GetFullPath(path)))
		return PRC_OK;

	file = fopen(path.c_str(), "wb");
	fclose(file);
	m_out->Write(NFS_OK);
    m_out->Write(GetFileHandle(path.c_str()), FHSIZE);
	WriteFileAttributes(path);

    return PRC_OK;
}

int CNFS2Prog::ProcedureREMOVE(void) {
    std::string path;

	Log("REMOVE");
	GetFullPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

	remove(path.c_str());
	m_out->Write(NFS_OK);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureRENAME(void) {
    std::string path;
    std::string pathFrom;
    std::string pathTo;

	Log("RENAME");
	GetFullPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;
    pathFrom = path;
	GetFullPath(pathTo);

    RenameFile(pathFrom.c_str(), pathTo.c_str());
	m_out->Write(NFS_OK);

    return PRC_OK;
}

int CNFS2Prog::ProcedureMKDIR(void) {
    std::string path;

	Log("MKDIR");
	if(!(GetFullPath(path)))
		return PRC_OK;

	mkdir(path.c_str(), ACCESSPERMS);
	m_out->Write(NFS_OK);
    m_out->Write(GetFileHandle(path.c_str()), FHSIZE);
	WriteFileAttributes(path);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureRMDIR(void) {
    std::string path;

	Log("RMDIR");
	GetFullPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

	rmdir(path.c_str());
	m_out->Write(NFS_OK);
    
    return PRC_OK;
}

int CNFS2Prog::ProcedureREADDIR(void) {
    NFSD_NOTIMPL
    return PRC_FAIL;
#if 0
	unsigned char opaque[3] = {0, 0, 0};
    std::string path;
    char filePath[MAXPATHLEN + 1];
	int handle;
	struct _finddata_t fileinfo;
	uint32_t count;
	uint32_t nLen;

	Log("READDIR");
	GetPath(path);
	if (!(CheckFile(path)))
		return PRC_OK;

	m_out->Write(NFS_OK);
	sprintf(filePath, "%s"PATH_SEPS"*", path.c_str());
	count = 0;
	handle = _findfirst(filePath, &fileinfo);
	if (handle)
	{
		do
		{
			m_out->Write(1);  //value follows
			sprintf(filePath, "%s"PATH_SEPS"%s", path, fileinfo.name);
            m_out->Write(GetFileID(filePath));  //file id
			m_out->Write(nLen = strlen(fileinfo.name));
			m_out->Write((void*)fileinfo.name, nLen);
			nLen &= 3;
			if (nLen != 0)
				m_out->Write(opaque, 4 - nLen);  //opaque bytes
			m_out->Write(++count);  //cookie
		} while (_findnext(handle, &fileinfo) == 0);
		_findclose(handle);
	}
	m_out->Write(0);  //no value follows
	m_out->Write(1);  //EOF

    return PRC_OK;
#endif
}

int CNFS2Prog::ProcedureSTATFS(void) {
    std::string path;
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

bool CNFS2Prog::GetPath(std::string& path) {
	FILE_HANDLE fhandle(0);

	m_in->Read((void*)&fhandle, FHSIZE);
    return GetFilePath(&fhandle, path);
}

bool CNFS2Prog::GetFullPath(std::string& path) {
	static char filePath[MAXPATHLEN + 1];
	unsigned int nLen1, nBytes;
	uint32_t nLen2;

	if(!(GetPath(path)))
       return false;
/*
	nLen1 = path.length();
	m_in->Read(&nLen2);
	sprintf(filePath, "%s\\", path);
	m_in->Read(filePath + nLen1 + 1, nLen2);
	filePath[nLen1 + 1 + nLen2] = '\0';
	Log("%s", filePath + nLen1);

	if ((nLen2 & 3) != 0)
		m_in->Read(&nBytes, 4 - (nLen2 & 3));
  */
    return true;
}

bool CNFS2Prog::CheckFile(std::string path)
{
	if (path.length() == 0) {
		m_out->Write(NFSERR_STALE);
		return false;
	}
    if (!(FileExists(path.c_str()))) {
		m_out->Write(NFSERR_NOENT);
		return false;
	}
	return true;
}

bool CNFS2Prog::WriteFileAttributes(std::string path) {
	struct stat data;
	uint32_t nValue;

	if (stat(path.c_str(), &data) != 0)
		return false;

	switch (data.st_mode & S_IFMT)
	{
		case S_IFREG:
			nValue = NFREG;
			break;
		case S_IFDIR:
			nValue = NFDIR;
			break;
		case S_IFCHR:
			nValue = NFCHR;
			break;
		default:
			nValue = NFNON;
			break;
	}
	m_out->Write(nValue);  //type
	if (nValue == NFREG)
		nValue = 0x8000;
	else if (nValue == NFDIR)
		nValue = 0x4000;
	else
		nValue = 0;
	if ((data.st_mode & S_IREAD) != 0)
		nValue |= 0x124;
	if ((data.st_mode & S_IWRITE) != 0)
		nValue |= 0x92;
	if ((data.st_mode & S_IEXEC) != 0)
		nValue |= 0x49;
	m_out->Write(nValue);  //mode
	m_out->Write(data.st_nlink);  //nlink	
	m_out->Write(m_nUID);  //uid
	m_out->Write(m_nGID);  //gid
	m_out->Write(data.st_size);  //size
	m_out->Write(8192);  //blocksize
	m_out->Write(0);  //rdev
	m_out->Write((data.st_size + 8191) / 8192);  //blocks
	m_out->Write(4);  //fsid
    m_out->Write(GetFileID(path.c_str()));  //fileid
	m_out->Write(data.st_atime);  //atime
	m_out->Write(0);  //atime
	m_out->Write(data.st_mtime);  //mtime
	m_out->Write(0);  //mtime
	m_out->Write(data.st_ctime);  //ctime
	m_out->Write(0);  //ctime
	return true;
}
