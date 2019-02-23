#ifndef _NFS3PROG_H_
#define _NFS3PROG_H_

#include "RPCProg.h"
#include <string>
#include <unordered_map>

typedef uint64_t fileid3;
typedef uint64_t cookie3;
typedef uint32_t uid3;
typedef uint32_t gid3;
typedef uint64_t size3;
typedef uint64_t offset3;
typedef uint32_t mode3;
typedef uint32_t count3;
typedef int      nfsstat3;
typedef uint32_t ftype3;
typedef uint32_t stable_how;
typedef uint32_t time_how;
typedef uint32_t createmode3;
typedef uint64_t cookieverf3;
typedef uint64_t createverf3;
typedef uint64_t writeverf3;

class nfs_fh3   : public XDROpaque {};
class filename3 : public XDRString {};
class nfspath3  : public XDRString {};

typedef struct
{
    uint32_t specdata1;
    uint32_t specdata2;
} specdata3;

typedef struct
{
    uint32_t seconds;
    uint32_t nseconds;
} nfstime3;

typedef struct
{
    bool check;
    nfstime3 obj_ctime;
} sattrguard3;

typedef struct
{
    ftype3 type;
    mode3 mode;
    uint32_t nlink;
    uid3 uid;
    gid3 gid;
    size3 size;
    size3 used;
    specdata3 rdev;
    uint64_t fsid;
    fileid3 fileid;
    nfstime3 atime;
    nfstime3 mtime;
    nfstime3 ctime;
} fattr3;

typedef struct
{
    bool attributes_follow;
    fattr3 attributes;
} post_op_attr;

typedef struct
{
    size3 size;
    nfstime3 mtime;
    nfstime3 ctime;
} wcc_attr;

typedef struct
{
    bool attributes_follow;
    wcc_attr attributes;
} pre_op_attr;

typedef struct
{
    pre_op_attr before;
    post_op_attr after;
} wcc_data;

typedef struct
{
    bool handle_follows;
    nfs_fh3 handle;
} post_op_fh3;

typedef struct
{
    bool set_it;
    mode3 mode;
} set_mode3;

typedef struct
{
    bool set_it;
    uid3 uid;
} set_uid3;

typedef struct
{
    bool set_it;
    gid3 gid;
} set_gid3;

typedef struct
{
    bool set_it;
    size3 size;
} set_size3;

typedef struct
{
    time_how set_it;
    nfstime3 atime;
} set_atime;

typedef struct
{
    time_how set_it;
    nfstime3 mtime;
} set_mtime;

typedef struct
{
    set_mode3 mode;
    set_uid3 uid;
    set_gid3 gid;
    set_size3 size;
    set_atime atime;
    set_mtime mtime;
} sattr3;

typedef struct
{
    nfs_fh3 dir;
    filename3 name;
} diropargs3;

typedef struct
{
    createmode3 mode;
    sattr3 obj_attributes;
    createverf3 verf;
} createhow3;

typedef struct 
{
	sattr3 symlink_attributes;
	nfspath3 symlink_data;
} symlinkdata3;

typedef struct _REPARSE_DATA_BUFFER {
	uint32_t  ReparseTag;
	uint16_t ReparseDataLength;
	uint16_t Reserved;
	union {
		struct {
			uint16_t  SubstituteNameOffset;
			uint16_t  SubstituteNameLength;
			uint16_t  PrintNameOffset;
			uint16_t  PrintNameLength;
			uint32_t  Flags;
			uint16_t  PathBuffer[1];
		} SymbolicLinkReparseBuffer;
		struct {
			uint16_t SubstituteNameOffset;
			uint16_t SubstituteNameLength;
			uint16_t PrintNameOffset;
			uint16_t PrintNameLength;
			uint16_t  PathBuffer[1];
		} MountPointReparseBuffer;
		struct {
			uint8_t DataBuffer[1];
		} GenericReparseBuffer;
	};
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

class CNFS3Prog : public CRPCProg
{
public:
    CNFS3Prog();
    ~CNFS3Prog();
    void SetUserID(unsigned int nUID, unsigned int nGID);
    virtual int  Process();

protected:
    uint32_t m_defUID, m_defGID;

    nfsstat3 ProcedureNULL(void);
    nfsstat3 ProcedureGETATTR(void);
    nfsstat3 ProcedureSETATTR(void);
    nfsstat3 ProcedureLOOKUP(void);
    nfsstat3 ProcedureACCESS(void);
    nfsstat3 ProcedureREADLINK(void);
    nfsstat3 ProcedureREAD(void);
    nfsstat3 ProcedureWRITE(void);
    nfsstat3 ProcedureCREATE(void);
    nfsstat3 ProcedureMKDIR(void);
    nfsstat3 ProcedureSYMLINK(void);
    nfsstat3 ProcedureMKNOD(void);
    nfsstat3 ProcedureREMOVE(void);
    nfsstat3 ProcedureRMDIR(void);
    nfsstat3 ProcedureRENAME(void);
    nfsstat3 ProcedureLINK(void);
    nfsstat3 ProcedureREADDIR(void);
    nfsstat3 ProcedureREADDIRPLUS(void);
    nfsstat3 ProcedureFSSTAT(void);
    nfsstat3 ProcedureFSINFO(void);
    nfsstat3 ProcedurePATHCONF(void);
    nfsstat3 ProcedureCOMMIT(void);
    nfsstat3 ProcedureNOIMP(void);

private:
    bool GetPath(std::string &path);
    bool ReadDirectory(std::string &dirName, std::string &fileName);
    char *GetFullPath(std::string &dirName, std::string &fileName);
    nfsstat3 CheckFile(const char *fullPath);
    nfsstat3 CheckFile(const char *directory, const char *fullPath);
    bool GetFileHandle(const char *path, nfs_fh3 *pObject);
    bool GetFileAttributesForNFS(const char *path, wcc_attr *pAttr);
    bool GetFileAttributesForNFS(const char *path, fattr3 *pAttr);
    uint32_t FileTimeToPOSIX(struct timespec);
    std::unordered_map<int, FILE*> unstableStorageFile;
    
    void Read(bool* pBool);
    void Read(uint32_t* pUint32);
    void Read(uint64_t* pUint64);
    void Read(sattr3* pAttr);
    void Read(sattrguard3* pGuard);
    void Read(diropargs3* pDir);
    void Read(XDROpaque* pOpaque);
    void Read(nfstime3* pTime);
    void Read(createhow3* pHow);
    void Read(symlinkdata3* pSymlink);
    
    void Write(bool* pBool);
    void Write(int* pInt);
    void Write(uint32_t* pUint32);
    void Write(uint64_t* pUint64);
    void Write(fattr3* pAttr);
    void Write(XDROpaque* pOpaque);
    void Write(wcc_data* pWcc);
    void Write(post_op_attr* pAttr);
    void Write(pre_op_attr* pAttr);
    void Write(post_op_fh3* pObj);
    void Write(nfstime3* pTime);
    void Write(specdata3* pSpec);
    void Write(wcc_attr* pAttr);
};

#endif
