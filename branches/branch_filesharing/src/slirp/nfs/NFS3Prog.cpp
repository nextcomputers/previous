#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#include <dirent.h>

#include <string>
#include <algorithm>

#include "NFS3Prog.h"
#include "FileTable.h"
#include "nfsd.h"
#include "compat.h"
#include "RPCProg.h"

#define BUFFER_SIZE 1000

enum {
    NFS3_OK = 0,
    NFS3ERR_PERM = 1,
    NFS3ERR_NOENT = 2,
    NFS3ERR_IO = 5,
    NFS3ERR_NXIO = 6,
    NFS3ERR_ACCES = 13,
    NFS3ERR_EXIST = 17,
    NFS3ERR_XDEV = 18,
    NFS3ERR_NODEV = 19,
    NFS3ERR_NOTDIR = 20,
    NFS3ERR_ISDIR = 21,
    NFS3ERR_INVAL = 22,
    NFS3ERR_FBIG = 27,
    NFS3ERR_NOSPC = 28,
    NFS3ERR_ROFS = 30,
    NFS3ERR_MLINK = 31,
    NFS3ERR_NAMETOOLONG = 63,
    NFS3ERR_NOTEMPTY = 66,
    NFS3ERR_DQUOT = 69,
    NFS3ERR_STALE = 70,
    NFS3ERR_REMOTE = 71,
    NFS3ERR_BADHANDLE = 10001,
    NFS3ERR_NOT_SYNC = 10002,
    NFS3ERR_BAD_COOKIE = 10003,
    NFS3ERR_NOTSUPP = 10004,
    NFS3ERR_TOOSMALL = 10005,
    NFS3ERR_SERVERFAULT = 10006,
    NFS3ERR_BADTYPE = 10007,
    NFS3ERR_JUKEBOX = 10008
};

enum
{
    NF3REG = 1,
    NF3DIR = 2,
    NF3BLK = 3,
    NF3CHR = 4,
    NF3LNK = 5,
    NF3SOCK = 6,
    NF3FIFO = 7
};

enum
{
    ACCESS3_READ = 0x0001,
    ACCESS3_LOOKUP = 0x0002,
    ACCESS3_MODIFY = 0x0004,
    ACCESS3_EXTEND = 0x0008,
    ACCESS3_DELETE = 0x0010,
    ACCESS3_EXECUTE = 0x0020
};

enum
{
    FSF3_LINK = 0x0001,
    FSF3_SYMLINK = 0x0002,
    FSF3_HOMOGENEOUS = 0x0008,
    FSF3_CANSETTIME = 0x0010
};

enum
{
    UNSTABLE = 0,
    DATA_SYNC = 1,
    FILE_SYNC = 2
};

enum
{
    DONT_CHANGE = 0,
    SET_TO_SERVER_TIME = 1,
    SET_TO_CLIENT_TIME = 2
};

enum
{
    UNCHECKED = 0,
    GUARDED = 1,
    EXCLUSIVE = 2
};

CNFS3Prog::CNFS3Prog() : CRPCProg(PROG_NFS, 3, "nfds"), m_nUID(0), m_nGID(0) {
    #define RPC_PROG_CLASS CNFS3Prog
    SetProc(1,  GETATTR);
    SetProc(2,  SETATTR);
    SetProc(3,  LOOKUP);
    SetProc(4,  ACCESS);
    SetProc(5,  READLINK);
    SetProc(6,  READ);
    SetProc(7,  WRITE);
    SetProc(8,  CREATE);
    SetProc(9,  MKDIR);
    SetProc(10, SYMLINK);
    SetProc(11, MKNOD);
    SetProc(12, REMOVE);
    SetProc(13, RMDIR);
    SetProc(14, RENAME);
    SetProc(15, LINK);
    SetProc(16, READDIR);
    SetProc(17, READDIRPLUS);
    SetProc(18, FSSTAT);
    SetProc(19, FSINFO);
    SetProc(20, PATHCONF);
    SetProc(21, COMMIT);
}

CNFS3Prog::~CNFS3Prog() {}

void CNFS3Prog::SetUserID(unsigned int nUID, unsigned int nGID) {
    m_nUID = nUID;
    m_nGID = nGID;
}

const char* nfsstat3str(nfsstat3 stat) {
    switch (stat) {
        case NFS3_OK:             return "OK";
        case NFS3ERR_PERM:        return "PERM";
        case NFS3ERR_NOENT:       return "NOENT";
        case NFS3ERR_IO:          return "IO";
        case NFS3ERR_NXIO:        return "NXIO";
        case NFS3ERR_ACCES:       return "ACCESS";
        case NFS3ERR_EXIST:       return "EXIST";
        case NFS3ERR_XDEV:        return "XDEV";
        case NFS3ERR_NODEV:       return "NODEV";
        case NFS3ERR_NOTDIR:      return "NOTDIR";
        case NFS3ERR_ISDIR:       return "ISDIR";
        case NFS3ERR_INVAL:       return "INVAL";
        case NFS3ERR_FBIG:        return "FBIG";
        case NFS3ERR_NOSPC:       return "NOSPC";
        case NFS3ERR_ROFS:        return "ROFS";
        case NFS3ERR_MLINK:       return "MLINK";
        case NFS3ERR_NAMETOOLONG: return "NAMETOOLONG";
        case NFS3ERR_NOTEMPTY:    return "NOTEMPTY";
        case NFS3ERR_DQUOT:       return "DQUOT";
        case NFS3ERR_STALE:       return "STALE";
        case NFS3ERR_REMOTE:      return "REMOTE";
        case NFS3ERR_BADHANDLE:   return "BADHANDLE";
        case NFS3ERR_NOT_SYNC:    return "NOT_SYNC";
        case NFS3ERR_BAD_COOKIE:  return "BAD_COOKIE";
        case NFS3ERR_NOTSUPP:     return "NOTSUPP";
        case NFS3ERR_TOOSMALL:    return "TOOSMALL";
        case NFS3ERR_SERVERFAULT: return "SERVERFAULT";
        case NFS3ERR_BADTYPE:     return "BADTYPE";
        case NFS3ERR_JUKEBOX:     return "JUKEBOX";
        default:                  return "UNKNOWN_ERROR";
    }
}

int CNFS3Prog::Process(void) {
    int result = PRC_OK;
    
    static PPROC pf[] = { 
    };

    nfsstat3 stat = NFS3_OK;

    if (m_param->proc >= sizeof(pf) / sizeof(PPROC))
        return PRC_NOTIMP;

    try {
        stat = (this->*pf[m_param->proc])();
    } catch (const std::runtime_error& re) {
        result = PRC_FAIL;
        Log("Runtime error: %s", re.what());
    } catch (const std::exception& ex) {
        result = PRC_FAIL;
        Log("Exception: %s", ex.what());
    } catch (...) {
        result = PRC_FAIL;
        Log("Unknown failure: Possible memory corruption");
    }

    if (stat != NFS3_OK) Log("Error: %s", nfsstat3str(stat));

    return result;
}

nfsstat3 CNFS3Prog::ProcedureNULL(void)
{
    Log("NULL");
    return NFS3_OK;
}

nfsstat3 CNFS3Prog::ProcedureGETATTR(void)
{
    std::string path;
    fattr3 obj_attributes;
    nfsstat3 stat;

    Log("GETATTR");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    stat = CheckFile(cStr);
	//printf("\nscanned file %s\n", path);
    if (stat == NFS3ERR_NOENT) {
        stat = NFS3ERR_STALE;
    } else if (stat == NFS3_OK) {
        if (!GetFileAttributesForNFS(cStr, &obj_attributes)) {
            stat = NFS3ERR_IO;
        }
    }

    Write(&stat);

    if (stat == NFS3_OK) {
        Write(&obj_attributes);
    }

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureSETATTR(void)
{
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    std::string path;
    sattr3 new_attributes;
    sattrguard3 guard;
    wcc_data obj_wcc;
    nfsstat3 stat;
    int nMode;
    FILE *pFile;
    HANDLE hFile;
    FILETIME fileTime;
    SYSTEMTIME systemTime;

    Log("SETATTR");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    Read(&new_attributes);
    Read(&guard);
    stat = CheckFile(cStr);
    obj_wcc.before.attributes_follow = GetFileAttributesForNFS(cStr, &obj_wcc.before.attributes);

    if (stat == NFS3_OK) {
        if (new_attributes.mode.set_it) {
            nMode = 0;

            if ((new_attributes.mode.mode & 0x100) != 0) {
                nMode |= S_IREAD;
            }

            // Always set read and write permissions (deliberately implemented this way)
            // if ((new_attributes.mode.mode & 0x80) != 0) {
            nMode |= S_IWRITE;
            // }

            // S_IEXEC is not availabile on windows
            // if ((new_attributes.mode.mode & 0x40) != 0) {
            //     nMode |= S_IEXEC;
            // }

            if (_chmod(cStr, nMode) != 0) {
                stat = NFS3ERR_INVAL;
            } else {

            }
        }   

        // deliberately not implemented because we cannot reflect uid/gid on windows (easliy)
        if (new_attributes.uid.set_it){}
        if (new_attributes.gid.set_it){}

        // deliberately not implemented
        if (new_attributes.mtime.set_it == SET_TO_CLIENT_TIME){}
        if (new_attributes.atime.set_it == SET_TO_CLIENT_TIME){}

        if (new_attributes.mtime.set_it == SET_TO_SERVER_TIME || new_attributes.atime.set_it == SET_TO_SERVER_TIME){
            hFile = CreateFile(cStr, FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
            if (hFile != INVALID_HANDLE_VALUE) {
                GetSystemTime(&systemTime);
                SystemTimeToFileTime(&systemTime, &fileTime);
                if (new_attributes.mtime.set_it == SET_TO_SERVER_TIME){
                    SetFileTime(hFile, NULL, NULL, &fileTime);
                }
                if (new_attributes.atime.set_it == SET_TO_SERVER_TIME){
                    SetFileTime(hFile, NULL, &fileTime, NULL);
                }
            }
            CloseHandle(hFile);
        }

        if (new_attributes.size.set_it){
            pFile = _fsopen(cStr, "r+b", _SH_DENYWR);
            if (pFile != NULL) {
                int filedes = fileno(pFile);
                _chsize_s(filedes, new_attributes.size.size);
                fclose(pFile);
            }
        }
    }

    obj_wcc.after.attributes_follow = GetFileAttributesForNFS(cStr, &obj_wcc.after.attributes);

    Write(&stat);
    Write(&obj_wcc);

    return stat;
#endif
}

nfsstat3 CNFS3Prog::ProcedureLOOKUP(void)
{
    char *path;
    nfs_fh3 object;
    post_op_attr obj_attributes;
    post_op_attr dir_attributes;
    nfsstat3 stat;

    Log("LOOKUP");

    std::string dirName;
    std::string fileName;
    ReadDirectory(dirName, fileName);

    path = GetFullPath(dirName, fileName);
    stat = CheckFile((char*)dirName.c_str(), path);

    if (stat == NFS3_OK) {
        GetFileHandle(path, &object);
        obj_attributes.attributes_follow = GetFileAttributesForNFS(path, &obj_attributes.attributes);
    }

    dir_attributes.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_attributes.attributes);

    Write(&stat);

    if (stat == NFS3_OK) {
        Write(&object);
        Write(&obj_attributes);
    }

    Write(&dir_attributes);

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureACCESS(void)
{
    std::string path;
    uint32_t access;
    post_op_attr obj_attributes;
    nfsstat3 stat;

    Log("ACCESS");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    Read(&access);
    stat = CheckFile(cStr);

    if (stat == NFS3ERR_NOENT) {
        stat = NFS3ERR_STALE;
    }

    obj_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &obj_attributes.attributes);

    Write(&stat);
    Write(&obj_attributes);

    if (stat == NFS3_OK) {
        Write(&access);
    }

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureREADLINK(void) {
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    Log("READLINK");
    std::string path;
    char *pMBBuffer = 0;

    post_op_attr symlink_attributes;
    nfspath3 data = nfspath3();

    //opaque data;
    nfsstat3 stat;

    HANDLE hFile;
    REPARSE_DATA_BUFFER *lpOutBuffer;
    lpOutBuffer = (REPARSE_DATA_BUFFER*)malloc(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    uint32_t bytesReturned;

    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    stat = CheckFile(cStr);
    if (stat == NFS3_OK) {

        hFile = CreateFile(cStr, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_REPARSE_POINT | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            stat = NFS3ERR_IO;
        }
        else
        {
            lpOutBuffer = (REPARSE_DATA_BUFFER*)malloc(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
            if (!lpOutBuffer) {
                stat = NFS3ERR_IO;
            }
            else {
                DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, lpOutBuffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &bytesReturned, NULL);
                std::string finalSymlinkPath;
                if (lpOutBuffer->ReparseTag == IO_REPARSE_TAG_SYMLINK || lpOutBuffer->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
                {
                    if (lpOutBuffer->ReparseTag == IO_REPARSE_TAG_SYMLINK)
                    {
                        size_t plen = lpOutBuffer->SymbolicLinkReparseBuffer.PrintNameLength / sizeof(WCHAR);
                        WCHAR *szPrintName = new WCHAR[plen + 1];
                        wcsncpy_s(szPrintName, plen + 1, &lpOutBuffer->SymbolicLinkReparseBuffer.PathBuffer[lpOutBuffer->SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(WCHAR)], plen);
                        szPrintName[plen] = 0;
                        std::wstring wStringTemp(szPrintName);
                        delete[] szPrintName;
                        std::string cPrintName(wStringTemp.begin(), wStringTemp.end());
                        finalSymlinkPath.assign(cPrintName);
                        // TODO: Revisit with cleaner solution
                        if (!PathIsRelative(cPrintName.c_str()))
                        {
                            std::string strFromChar;
                            strFromChar.append("\\\\?\\");
                            strFromChar.append(cPrintName);
                            char *target = _strdup(strFromChar.c_str());
                            // remove last folder
                            size_t lastFolderIndex = path.find_last_of(PATH_SEP);
                            if (lastFolderIndex != std::string::npos) {
                                path = path.substr(0, lastFolderIndex);
                            }
                            char szOut[MAX_PATH] = "";
                            PathRelativePathTo(szOut, cStr, FILE_ATTRIBUTE_DIRECTORY, target, FILE_ATTRIBUTE_DIRECTORY);
                            std::string symlinkPath(szOut);
                            finalSymlinkPath.assign(symlinkPath);
                        }
                    }

                    // TODO: Revisit with cleaner solution
                    if (lpOutBuffer->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
                    {
                        size_t slen = lpOutBuffer->MountPointReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
                        WCHAR *szSubName = new WCHAR[slen + 1];
                        wcsncpy_s(szSubName, slen + 1, &lpOutBuffer->MountPointReparseBuffer.PathBuffer[lpOutBuffer->MountPointReparseBuffer.SubstituteNameOffset / sizeof(WCHAR)], slen);
                        szSubName[slen] = 0;
                        std::wstring wStringTemp(szSubName);
                        delete[] szSubName;
                        std::string target(wStringTemp.begin(), wStringTemp.end());
                        target.erase(0, 2);
                        target.insert(0, 2, PATH_SEP);
                        // remove last folder, see above
                        size_t lastFolderIndex = path.find_last_of(PATH_SEP);
                        if (lastFolderIndex != std::string::npos) {
                            path = path.substr(0, lastFolderIndex);
                        }
                        char szOut[MAX_PATH] = "";
                        PathRelativePathTo(szOut, cStr, FILE_ATTRIBUTE_DIRECTORY, target.c_str(), FILE_ATTRIBUTE_DIRECTORY);
                        std::string symlinkPath = szOut;
                        finalSymlinkPath.assign(symlinkPath);
                    }

                    // write path always with / separator, so windows created symlinks work too
                    std::replace(finalSymlinkPath.begin(), finalSymlinkPath.end(), PATH_SEP, '/');
                    char *result = _strdup(finalSymlinkPath.c_str());
                    data.Set(result);
                }
                free(lpOutBuffer);
            }
        }
        CloseHandle(hFile);
    }

    symlink_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &symlink_attributes.attributes);

    Write(&stat);
    Write(&symlink_attributes);
    if (stat == NFS3_OK) {
        Write(&data);
    }

    return stat;
#endif
}

nfsstat3 CNFS3Prog::ProcedureREAD(void)
{
    std::string path;
    offset3 offset;
    count3 count;
    post_op_attr file_attributes;
    bool eof;
    XDROpaque data;
    nfsstat3 stat;
    FILE *pFile;

    Log("READ");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    Read(&offset);
    Read(&count);
    stat = CheckFile(cStr);

    if (stat == NFS3_OK) {
        data.Alloc(count);
        pFile = fopen(cStr, "rb");

        if (pFile != NULL) {
            fseek(pFile, offset, SEEK_SET) ;
            count = fread(data.m_data, sizeof(char), count, pFile);
            eof = fgetc(pFile) == EOF;
            fclose(pFile);
        } else {
            errno_t errorNumber = errno;
            Log(strerror(errno));

            if (errorNumber == EACCES) {
                stat = NFS3ERR_ACCES;
            } else {
                stat = NFS3ERR_IO;
            }
        }
    }

    file_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &file_attributes.attributes);

    Write(&stat);
    Write(&file_attributes);

    if (stat == NFS3_OK) {
        Write(&count);
        Write(&eof);
        Write(&data);
    }

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureWRITE(void)
{
    std::string path;
    offset3 offset;
    count3 count;
    stable_how stable;
    XDROpaque data;
    wcc_data file_wcc;
    writeverf3 verf;
    nfsstat3 stat;
    FILE *pFile;

    Log("WRITE");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    Read(&offset);
    Read(&count);
    Read(&stable);
    Read(&data);
    stat = CheckFile(cStr);

    file_wcc.before.attributes_follow = GetFileAttributesForNFS(cStr, &file_wcc.before.attributes);

    if (stat == NFS3_OK) {

        if (stable == UNSTABLE) {
            nfs_fh3 handle;
            GetFileHandle(cStr, &handle);
            int handleId = *(unsigned int *)handle.m_data;

            if (unstableStorageFile.count(handleId) == 0){
                pFile = fopen(cStr, "r+b");
                if (pFile != NULL) {
                    unstableStorageFile.insert(std::make_pair(handleId, pFile));
                }
            } else {
                pFile = unstableStorageFile[handleId];
            }

            if (pFile != NULL) {
                fseek(pFile, offset, SEEK_SET);
                count = fwrite(data.m_data, sizeof(char), data.m_size, pFile);
            } else {
                errno_t errorNumber = errno;
                Log(strerror(errorNumber));

                if (errorNumber == EACCES) {
                    stat = NFS3ERR_ACCES;
                }
                else {
                    stat = NFS3ERR_IO;
                }
            }
            // this should not be zero but a timestamp (process start time) instead
            verf = 0;
            // we can reuse this, because no physical write has happend
            file_wcc.after.attributes_follow = file_wcc.before.attributes_follow;
        } else {

            pFile = fopen(cStr, "r+b");

            if (pFile != NULL) {
                fseek(pFile, offset, SEEK_SET) ;
                count = fwrite(data.m_data, sizeof(char), data.m_size, pFile);
                fclose(pFile);
            } else {
                errno_t errorNumber = errno;
                Log(strerror(errorNumber));

                if (errorNumber == EACCES) {
                    stat = NFS3ERR_ACCES;
                } else {
                    stat = NFS3ERR_IO;
                }
            }

            stable = FILE_SYNC;
            verf = 0;

            file_wcc.after.attributes_follow = GetFileAttributesForNFS(cStr, &file_wcc.after.attributes);
        }
    }

    Write(&stat);
    Write(&file_wcc);

    if (stat == NFS3_OK) {
        Write(&count);
        Write(&stable);
        Write(&verf);
    }

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureCREATE(void)
{
    char *path;
    createhow3 how;
    post_op_fh3 obj;
    post_op_attr obj_attributes;
    wcc_data dir_wcc;
    nfsstat3 stat;
    FILE *pFile;

    Log("CREATE");
    std::string dirName;
    std::string fileName;
    ReadDirectory(dirName, fileName);
    path = GetFullPath(dirName, fileName);
    Read(&how);

    dir_wcc.before.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.before.attributes);

    pFile = fopen(path, "wb");
       
    if (pFile != NULL) {
        fclose(pFile);
        stat = NFS3_OK;
    } else {
        errno_t errorNumber = errno;
        Log(strerror(errorNumber));

        if (errorNumber == ENOENT) {
            stat = NFS3ERR_STALE;
        } else if (errorNumber == EACCES) {
            stat = NFS3ERR_ACCES;
        } else {
            stat = NFS3ERR_IO;
        }
    }

    if (stat == NFS3_OK) {
        obj.handle_follows = GetFileHandle(path, &obj.handle);
        obj_attributes.attributes_follow = GetFileAttributesForNFS(path, &obj_attributes.attributes);
    }
    
    dir_wcc.after.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.after.attributes);

    Write(&stat);

    if (stat == NFS3_OK) {
        Write(&obj);
        Write(&obj_attributes);
    }

    Write(&dir_wcc);

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureMKDIR(void)
{
    char *path;
    sattr3 attributes;
    post_op_fh3 obj;
    post_op_attr obj_attributes;
    wcc_data dir_wcc;
    nfsstat3 stat;

    Log("MKDIR");

    std::string dirName;
    std::string fileName;
    ReadDirectory(dirName, fileName);
    path = GetFullPath(dirName, fileName);
    Read(&attributes);

    dir_wcc.before.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.before.attributes);

    int result = mkdir(path, ACCESSPERMS);

    if (result == 0) {
        stat = NFS3_OK;
        obj.handle_follows = GetFileHandle(path, &obj.handle);
        obj_attributes.attributes_follow = GetFileAttributesForNFS(path, &obj_attributes.attributes);
    } else if (errno == EEXIST) {
        Log("Directory already exists.");
        stat = NFS3ERR_EXIST;
    } else if (errno == ENOENT) {
        stat = NFS3ERR_NOENT;
    } else {
        stat = CheckFile(path);

        if (stat != NFS3_OK) {
            stat = NFS3ERR_IO;
        }
    }

    dir_wcc.after.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.after.attributes);

    Write(&stat);

    if (stat == NFS3_OK) {
        Write(&obj);
        Write(&obj_attributes);
    }

    Write(&dir_wcc);

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureSYMLINK(void)
{
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    Log("SYMLINK");

    char* path;
    post_op_fh3 obj;
    post_op_attr obj_attributes;
    wcc_data dir_wcc;
    nfsstat3 stat;

    diropargs3 where;
    symlinkdata3 symlink;

    uint32_t targetFileAttr;
    uint32_t dwFlags;

    std::string dirName;
    std::string fileName;
    ReadDirectory(dirName, fileName);
    path = GetFullPath(dirName, fileName);

    Read(&symlink);

    _In_ LPTSTR lpSymlinkFileName = path; // symlink (full path)

    // TODO: Maybe revisit this later for a cleaner solution
    // Convert target path to windows path format, maybe this could also be done
    // in a safer way by a combination of PathRelativePathTo and GetFullPathName.
    // Without this conversion nested folder symlinks do not work cross platform.
    std::string strFromChar;
    strFromChar.append(symlink.symlink_data.path); // target (should be relative path));
    std::replace(strFromChar.begin(), strFromChar.end(), '/', PATH_SEP);
    _In_ LPTSTR lpTargetFileName = const_cast<LPSTR>(strFromChar.c_str());

    std::string fullTargetPath = dirName + std::string(PATH_SEPS) + std::string(lpTargetFileName);

    // Relative path do not work with GetFileAttributes (directory are not recognized)
    // so we normalize the path before calling GetFileAttributes
    TCHAR fullTargetPathNormalized[MAX_PATH];
    _In_ LPTSTR fullTargetPathString = const_cast<LPSTR>(fullTargetPath.c_str());
    GetFullPathName(fullTargetPathString, MAX_PATH, fullTargetPathNormalized, NULL);
    targetFileAttr = GetFileAttributes(fullTargetPathNormalized);

    dwFlags = 0x0;
	if (targetFileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        dwFlags = SYMBOLIC_LINK_FLAG_DIRECTORY;
    }

    BOOLEAN failed = CreateSymbolicLink(lpSymlinkFileName, lpTargetFileName, dwFlags);

    if (failed != 0) {
        stat = NFS3_OK;
        obj.handle_follows = GetFileHandle(path, &obj.handle);
        obj_attributes.attributes_follow = GetFileAttributesForNFS(path, &obj_attributes.attributes);
    }
    else {
        stat = NFS3ERR_IO;
        Log("An error occurs or file already exists.");
        stat = CheckFile(path);
        if (stat != NFS3_OK) {
            stat = NFS3ERR_IO;
        }
    }

    dir_wcc.after.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.after.attributes);

    Write(&stat);

    if (stat == NFS3_OK) {
        Write(&obj);
        Write(&obj_attributes);
    }

    Write(&dir_wcc);

    return stat;
#endif
}

nfsstat3 CNFS3Prog::ProcedureMKNOD(void)
{
    //TODO
    Log("MKNOD");

    return NFS3ERR_NOTSUPP;
}

nfsstat3 CNFS3Prog::ProcedureREMOVE(void) {
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    char *path;
    wcc_data dir_wcc;
    nfsstat3 stat;
    uint32_t returnCode;

    Log("REMOVE");

    std::string dirName;
    std::string fileName;
    ReadDirectory(dirName, fileName);
    path = GetFullPath(dirName, fileName);
    stat = CheckFile((char*)dirName.c_str(), path);

    dir_wcc.before.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.before.attributes);

    if (stat == NFS3_OK) {
        uint32_t fileAttr = GetFileAttributes(path);
        if ((fileAttr & FILE_ATTRIBUTE_DIRECTORY) && (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT)) {
            returnCode = RemoveFolder(path);
            if (returnCode != 0) {
                if (returnCode == ERROR_DIR_NOT_EMPTY) {
                    stat = NFS3ERR_NOTEMPTY;
                } else {
                    stat = NFS3ERR_IO;
                }
            }
        } else {
            if (!RemoveFile(path)) {
                stat = NFS3ERR_IO;
            }
        }
    }

    dir_wcc.after.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.after.attributes);

    Write(&stat);
    Write(&dir_wcc);

    return stat;
#endif
}

nfsstat3 CNFS3Prog::ProcedureRMDIR(void) {
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    char *path;
    wcc_data dir_wcc;
    nfsstat3 stat;
    uint32_t returnCode;

    Log("RMDIR");

    std::string dirName;
    std::string fileName;
    ReadDirectory(dirName, fileName);
    path = GetFullPath(dirName, fileName);
    stat = CheckFile((char*)dirName.c_str(), path);

    dir_wcc.before.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.before.attributes);

    if (stat == NFS3_OK) {
        returnCode = RemoveFolder(path);
        if (returnCode != 0) {
            if (returnCode == ERROR_DIR_NOT_EMPTY) {
                stat = NFS3ERR_NOTEMPTY;
            } else {
                stat = NFS3ERR_IO;
            }
        }
    }
    
    dir_wcc.after.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.after.attributes);

    Write(&stat);
    Write(&dir_wcc);

    return stat;
#endif
}

nfsstat3 CNFS3Prog::ProcedureRENAME(void) {
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    char pathFrom[MAXPATHLEN], *pathTo;
    wcc_data fromdir_wcc, todir_wcc;
    nfsstat3 stat;
    uint32_t returnCode;

    Log("RENAME");

    std::string dirFromName;
    std::string fileFromName;
    ReadDirectory(dirFromName, fileFromName);
    strcpy_s(pathFrom, GetFullPath(dirFromName, fileFromName));

    std::string dirToName;
    std::string fileToName;
    ReadDirectory(dirToName, fileToName);
    pathTo = GetFullPath(dirToName, fileToName);

    stat = CheckFile((char*)dirFromName.c_str(), pathFrom);

    fromdir_wcc.before.attributes_follow = GetFileAttributesForNFS((char*)dirFromName.c_str(), &fromdir_wcc.before.attributes);
    todir_wcc.before.attributes_follow = GetFileAttributesForNFS((char*)dirToName.c_str(), &todir_wcc.before.attributes);
    
    if (FileExists(pathTo)) {
		uint32_t fileAttr = GetFileAttributes(pathTo);
		if ((fileAttr & FILE_ATTRIBUTE_DIRECTORY) && (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT)) {
			returnCode = RemoveFolder(pathTo);
			if (returnCode != 0) {
				if (returnCode == ERROR_DIR_NOT_EMPTY) {
					stat = NFS3ERR_NOTEMPTY;
				} else {
					stat = NFS3ERR_IO;
				}
			}
		}
		else {
			if (!RemoveFile(pathTo)) {
				stat = NFS3ERR_IO;
			}
		}
    } 
    
    if (stat == NFS3_OK) {
        errno_t errorNumber = RenameDirectory(pathFrom, pathTo);

        if (errorNumber != 0) {
            Log(strerror(errorNumber));

            if (errorNumber == EACCES) {
                stat = NFS3ERR_ACCES;
            } else {
                stat = NFS3ERR_IO;
            }
        }
    }

    fromdir_wcc.after.attributes_follow = GetFileAttributesForNFS((char*)dirFromName.c_str(), &fromdir_wcc.after.attributes);
    todir_wcc.after.attributes_follow = GetFileAttributesForNFS((char*)dirToName.c_str(), &todir_wcc.after.attributes);

    Write(&stat);
    Write(&fromdir_wcc);
    Write(&todir_wcc);

    return stat;
#endif
}

nfsstat3 CNFS3Prog::ProcedureLINK(void) {
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    Log("LINK");
    std::string path;
    diropargs3 link;
    std::string dirName;
    std::string fileName;
    nfsstat3 stat;
    post_op_attr obj_attributes;
    wcc_data dir_wcc;

    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    ReadDirectory(dirName, fileName);

    char *linkFullPath = GetFullPath(dirName, fileName);

    //TODO: Improve checks here, cStr may be NULL because handle is invalid
    if (CreateHardLink(linkFullPath, cStr, NULL) == 0) {
        stat = NFS3ERR_IO;
    }
    stat = CheckFile(linkFullPath);
    if (stat == NFS3_OK) {
        obj_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &obj_attributes.attributes);

        if (!obj_attributes.attributes_follow) {
            stat = NFS3ERR_IO;
        }
    }

    dir_wcc.after.attributes_follow = GetFileAttributesForNFS((char*)dirName.c_str(), &dir_wcc.after.attributes);

    Write(&stat);
    Write(&obj_attributes);
    Write(&dir_wcc);

    return stat;
#endif
}

nfsstat3 CNFS3Prog::ProcedureREADDIR(void) {
    std::string path;
    cookie3 cookie;
    cookieverf3 cookieverf;
    count3 count;
    post_op_attr dir_attributes;
    fileid3 fileid;
    filename3 name;
    bool eof;
    bool bFollows;
    nfsstat3 stat;
    char filePath[MAXPATHLEN];
    DIR* handle;
    struct dirent* fileinfo;
    
    Log("READDIR");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    Read(&cookie);
    Read(&cookieverf);
    Read(&count);
    stat = CheckFile(cStr);

    if (stat == NFS3_OK) {
        dir_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &dir_attributes.attributes);

        if (!dir_attributes.attributes_follow) {
            stat = NFS3ERR_IO;
        }    
    }

    Write(&stat);
    Write(&dir_attributes);

    if (stat == NFS3_OK) {
        Write(&cookieverf);
        sprintf(filePath, "%s"PATH_SEPS"*", cStr);
        eof = true;
        handle = opendir(cStr);
        bFollows = true;

        if (handle) {
            for (cookie3 i = cookie; i > 0; i--)
                fileinfo = readdir(handle);

            // TODO: Implement this workaround correctly with the
            // count variable and not a fixed threshold of 10
            if (fileinfo) {
                int j = 10;

                for(fileinfo = readdir(handle); fileinfo; fileinfo = readdir(handle)) {
                    Write(&bFollows); //value follows
                    sprintf(filePath, "%s"PATH_SEPS"%s", cStr, fileinfo->d_name);
                    fileid = GetFileID(filePath);
                    Write(&fileid); //file id
                    name.Set(fileinfo->d_name);
                    Write(&name); //name
                    ++cookie;
                    Write(&cookie); //cookie
                    if (--j == 0) {
                        eof = false;
                        break;
                    }
                };
            }

            closedir(handle);
        }

        bFollows = false;
        Write(&bFollows);
        Write(&eof); //eof
    }

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureREADDIRPLUS(void)
{
    std::string path;
    cookie3 cookie;
    cookieverf3 cookieverf;
    count3 dircount, maxcount;
    post_op_attr dir_attributes;
    fileid3 fileid;
    filename3 name;
    post_op_attr name_attributes;
    post_op_fh3 name_handle;
    bool eof;
    nfsstat3 stat;
    char filePath[MAXPATHLEN];
    DIR* handle;
    struct dirent* fileinfo;
    bool bFollows;

    Log("READDIRPLUS");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    Read(&cookie);
    Read(&cookieverf);
    Read(&dircount);
    Read(&maxcount);
    stat = CheckFile(cStr);

    if (stat == NFS3_OK) {
        dir_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &dir_attributes.attributes);
        
        if (!dir_attributes.attributes_follow) {
            stat = NFS3ERR_IO;
        }
    }

    Write(&stat);
    Write(&dir_attributes);

    if (stat == NFS3_OK) {
        Write(&cookieverf);
        handle = opendir(cStr);
        eof = true;

        if (handle) {
            for (cookie3 i = cookie; i > 0; i--)
                fileinfo = readdir(handle);

            if (fileinfo) {
                bFollows = true;
                int j = 10;

                for(fileinfo =  readdir(handle); fileinfo; fileinfo = readdir(handle)) {
                    Write(&bFollows); //value follows
                    sprintf(filePath, "%s"PATH_SEPS"%s", cStr, fileinfo->d_name);
                    fileid = GetFileID(filePath);
                    Write(&fileid); //file id
                    name.Set(fileinfo->d_name);
                    Write(&name); //name
                    ++cookie;
                    Write(&cookie); //cookie
                    name_attributes.attributes_follow = GetFileAttributesForNFS(filePath, &name_attributes.attributes);
                    Write(&name_attributes);
                    name_handle.handle_follows = GetFileHandle(filePath, &name_handle.handle);
                    Write(&name_handle);

                    if (--j == 0) {
                        eof = false;
                        break;
                    }
                };
            }

            closedir(handle);
        }

        bFollows = false;
        Write(&bFollows); //value follows
        Write(&eof); //eof
    }

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureFSSTAT(void) {
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    std::string path;
    post_op_attr obj_attributes;
    size3 tbytes, fbytes, abytes, tfiles, ffiles, afiles;
    uint32_t invarsec;

    nfsstat3 stat;

    Log("FSSTAT");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    stat = CheckFile(cStr);

    if (stat == NFS3_OK) {
        obj_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &obj_attributes.attributes);

        if (obj_attributes.attributes_follow
            && GetDiskFreeSpaceEx(cStr, (PULARGE_INTEGER)&fbytes, (PULARGE_INTEGER)&tbytes, (PULARGE_INTEGER)&abytes)
            ) {
            //tfiles = 99999999999;
            //ffiles = 99999999999;
            //afiles = 99999999999;
            invarsec = 0;
        } else {
            stat = NFS3ERR_IO;
        }
    }

    Write(&stat);
    Write(&obj_attributes);

    if (stat == NFS3_OK) {
        Write(&tbytes);
        Write(&fbytes);
        Write(&abytes);
        Write(&tfiles);
        Write(&ffiles);
        Write(&afiles);
        Write(&invarsec);
    }

    return stat;
#endif
}

nfsstat3 CNFS3Prog::ProcedureFSINFO(void)
{
    std::string path;
    post_op_attr obj_attributes;
    uint32_t rtmax, rtpref, rtmult, wtmax, wtpref, wtmult, dtpref;
    size3 maxfilesize;
    nfstime3 time_delta;
    uint32_t properties;
    nfsstat3 stat;

    Log("FSINFO");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    stat = CheckFile(cStr);

    if (stat == NFS3_OK) {
        obj_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &obj_attributes.attributes);

        if (obj_attributes.attributes_follow) {
            rtmax = 65536;
            rtpref = 32768;
            rtmult = 4096;
            wtmax = 65536;
            wtpref = 32768;
            wtmult = 4096;
            dtpref = 8192;
            maxfilesize = 0x7FFFFFFFFFFFFFFF;
            time_delta.seconds = 1;
            time_delta.nseconds = 0;
            properties = FSF3_LINK | FSF3_SYMLINK | FSF3_CANSETTIME;
        } else {
            stat = NFS3ERR_SERVERFAULT;
        }         
    }

    Write(&stat);
    Write(&obj_attributes);

    if (stat == NFS3_OK) {
        Write(&rtmax);
        Write(&rtpref);
        Write(&rtmult);
        Write(&wtmax);
        Write(&wtpref);
        Write(&wtmult);
        Write(&dtpref);
        Write(&maxfilesize);
        Write(&time_delta);
        Write(&properties);
    }

    return stat;
}

nfsstat3 CNFS3Prog::ProcedurePATHCONF(void)
{
    std::string path;
    post_op_attr obj_attributes;
    nfsstat3 stat;
    uint32_t linkmax, name_max;
    bool no_trunc, chown_restricted, case_insensitive, case_preserving;

    Log("PATHCONF");
    bool validHandle = GetPath(path);
    const char* cStr = validHandle ? path.c_str() : NULL;
    stat = CheckFile(cStr);

    if (stat == NFS3_OK) {
        obj_attributes.attributes_follow = GetFileAttributesForNFS(cStr, &obj_attributes.attributes);

        if (obj_attributes.attributes_follow) {
            linkmax = 1023;
            name_max = 255;
            no_trunc = true;
            chown_restricted = true;
            case_insensitive = true;
            case_preserving = true;
        } else {
            stat = NFS3ERR_SERVERFAULT;
        }
    }

    Write(&stat);
    Write(&obj_attributes);

    if (stat == NFS3_OK) {
        Write(&linkmax);
        Write(&name_max);
        Write(&no_trunc);
        Write(&chown_restricted);
        Write(&case_insensitive);
        Write(&case_preserving);
    }

    return stat;
}

nfsstat3 CNFS3Prog::ProcedureCOMMIT(void)
{
    std::string path;
    int handleId;
    offset3 offset;
    count3 count;
    wcc_data file_wcc;
    nfsstat3 stat;
    nfs_fh3 file;
    writeverf3 verf;

    Log("COMMIT");
    Read(&file);
    bool validHandle = GetFilePath((FILE_HANDLE*)file.m_data, path);
    const char* cStr = validHandle ? path.c_str() : NULL;

    if (validHandle) {
        Log(" %s ", path.c_str());
    }

    // offset and count are unused
    // offset never was anything else than 0 in my tests
    // count does not matter in the way COMMIT is implemented here
    // to fulfill the spec this should be improved
    Read(&offset);
    Read(&count);

    file_wcc.before.attributes_follow = GetFileAttributesForNFS(cStr, &file_wcc.before.attributes);

    handleId = *(unsigned int*)file.m_data;

    if (unstableStorageFile.count(handleId) != 0) {
        if (unstableStorageFile[handleId] != NULL) {
            fclose(unstableStorageFile[handleId]);
            unstableStorageFile.erase(handleId);
            stat = NFS3_OK;
        } else {
            stat = NFS3ERR_IO;
        }
    } else {
        stat = NFS3_OK;
    }

    file_wcc.after.attributes_follow = GetFileAttributesForNFS(cStr, &file_wcc.after.attributes);

    Write(&stat);
    Write(&file_wcc);
    // verf should be the timestamp the server startet to notice reboots
    verf = 0;
    Write(&verf);

    return stat;
}

void CNFS3Prog::Read(bool *pBool)
{
    uint32_t b;

    if (m_in->Read(&b) < sizeof(uint32_t)) {
        throw __LINE__;
    }
        
    *pBool = b == 1;
}

void CNFS3Prog::Read(uint32_t *pUint32)
{
    if (m_in->Read(pUint32) < sizeof(uint32_t)) {
        throw __LINE__;
    }       
}

void CNFS3Prog::Read(uint64_t *pUint64)
{
    if (m_in->Read(pUint64) < sizeof(uint64_t)) {
        throw __LINE__;
    }        
}

void CNFS3Prog::Read(sattr3 *pAttr)
{
    Read(&pAttr->mode.set_it);

    if (pAttr->mode.set_it) {
        Read(&pAttr->mode.mode);
    }
        
    Read(&pAttr->uid.set_it);

    if (pAttr->uid.set_it) {
        Read(&pAttr->uid.uid);
    }  

    Read(&pAttr->gid.set_it);

    if (pAttr->gid.set_it) {
        Read(&pAttr->gid.gid);
    }       

    Read(&pAttr->size.set_it);

    if (pAttr->size.set_it) {
        Read(&pAttr->size.size);
    }       

    Read(&pAttr->atime.set_it);

    if (pAttr->atime.set_it == SET_TO_CLIENT_TIME) {
        Read(&pAttr->atime.atime);
    }
        
    Read(&pAttr->mtime.set_it);

    if (pAttr->mtime.set_it == SET_TO_CLIENT_TIME) {
        Read(&pAttr->mtime.mtime);
    }        
}

void CNFS3Prog::Read(sattrguard3 *pGuard)
{
    Read(&pGuard->check);

    if (pGuard->check) {
        Read(&pGuard->obj_ctime);
    }        
}

void CNFS3Prog::Read(diropargs3 *pDir)
{
    Read(&pDir->dir);
    Read(&pDir->name);
}

void CNFS3Prog::Read(XDROpaque* pOpaque) {
    m_in->Read(*pOpaque);
}

void CNFS3Prog::Read(nfstime3 *pTime)
{
    Read(&pTime->seconds);
    Read(&pTime->nseconds);
}

void CNFS3Prog::Read(createhow3 *pHow)
{
    Read(&pHow->mode);

    if (pHow->mode == UNCHECKED || pHow->mode == GUARDED) {
        Read(&pHow->obj_attributes);
    } else {
        Read(&pHow->verf);
    }       
}

void CNFS3Prog::Read(symlinkdata3 *pSymlink)
{
	Read(&pSymlink->symlink_attributes);
	Read(&pSymlink->symlink_data);
}

void CNFS3Prog::Write(bool *pBool)
{
    m_out->Write(*pBool ? 1 : 0);
}

void CNFS3Prog::Write(uint32_t *pUint32) {
    m_out->Write(*pUint32);
}

void CNFS3Prog::Write(int *pInt) {
    m_out->Write((uint32_t)*pInt);
}

void CNFS3Prog::Write(uint64_t *pUint64)
{
    m_out->Write8(*pUint64);
}

void CNFS3Prog::Write(fattr3 *pAttr)
{
    Write(&pAttr->type);
    Write(&pAttr->mode);
    Write(&pAttr->nlink);
    Write(&pAttr->uid);
    Write(&pAttr->gid);
    Write(&pAttr->size);
    Write(&pAttr->used);
    Write(&pAttr->rdev);
    Write(&pAttr->fsid);
    Write(&pAttr->fileid);
    Write(&pAttr->atime);
    Write(&pAttr->mtime);
    Write(&pAttr->ctime);
}

void CNFS3Prog::Write(XDROpaque* pOpaque) {
    m_out->Write(*pOpaque);
}

void CNFS3Prog::Write(wcc_data *pWcc)
{
    Write(&pWcc->before);
    Write(&pWcc->after);
}

void CNFS3Prog::Write(post_op_attr *pAttr)
{
    Write(&pAttr->attributes_follow);

    if (pAttr->attributes_follow) {
        Write(&pAttr->attributes);
    }      
}

void CNFS3Prog::Write(pre_op_attr *pAttr)
{
    Write(&pAttr->attributes_follow);

    if (pAttr->attributes_follow) {
        Write(&pAttr->attributes);
    }    
}

void CNFS3Prog::Write(post_op_fh3 *pObj)
{
    Write(&pObj->handle_follows);

    if (pObj->handle_follows) {
        Write(&pObj->handle);
    }     
}

void CNFS3Prog::Write(nfstime3 *pTime)
{
    Write(&pTime->seconds);
    Write(&pTime->nseconds);
}

void CNFS3Prog::Write(specdata3 *pSpec)
{
    Write(&pSpec->specdata1);
    Write(&pSpec->specdata2);
}

void CNFS3Prog::Write(wcc_attr *pAttr)
{
    Write(&pAttr->size);
    Write(&pAttr->mtime);
    Write(&pAttr->ctime);
}

bool CNFS3Prog::GetPath(std::string &path)
{
    nfs_fh3 object;

    Read(&object);
    bool valid = GetFilePath((FILE_HANDLE*)object.m_data, path);
    if (valid) {
        Log(" %s ", path.c_str());
    } else {
        Log(" File handle is invalid ");
    }

    return valid;
}

bool CNFS3Prog::ReadDirectory(std::string &dirName, std::string &fileName)
{
    diropargs3 fileRequest;
    Read(&fileRequest);

    if (GetFilePath((FILE_HANDLE*)fileRequest.dir.m_data, dirName)) {
        fileName = std::string(fileRequest.name.Get());
        return true;
    } else {
        return false;
    }

    //Log(" %s | %s ", dirName.c_str(), fileName.c_str());
}

char *CNFS3Prog::GetFullPath(std::string &dirName, std::string &fileName)
{
    //TODO: Return std::string
    static char fullPath[MAXPATHLEN + 1];

    if (dirName.size() + 1 + fileName.size() > MAXPATHLEN) {
        return NULL;
    }

    sprintf(fullPath, "%s"PATH_SEPS"%s", dirName.c_str(), fileName.c_str()); //concate path and filename
    Log(" %s ", fullPath);

    return fullPath;
}

nfsstat3 CNFS3Prog::CheckFile(const char *fullPath) {
    if (fullPath == NULL)
        return NFS3ERR_STALE;
	else if (access(fullPath, 0) != 0)
        return NFS3ERR_NOENT;
    else
        return NFS3_OK;
}

nfsstat3 CNFS3Prog::CheckFile(const char *directory, const char *fullPath) {
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    
    // FileExists will not work for the root of a drive, e.g. \\?\D:\, therefore check if it is a drive root with GetDriveType
    if (directory == NULL || (!FileExists(directory) && GetDriveType(directory) < 2) || fullPath == NULL) {
        return NFS3ERR_STALE;
    }
        
    if (!FileExists(fullPath)) {
        return NFS3ERR_NOENT;
    }
        
    return NFS3_OK;
#endif
}

bool CNFS3Prog::GetFileHandle(const char *path, nfs_fh3* pObject) {
    FILE_HANDLE* handle = ::GetFileHandle(path);
	if (!(handle)) {
		Log("no filehandle(path %s)", path);
		return false;
	}
    pObject->Set(handle, FHSIZE_NFS3);
    return true;
}

bool CNFS3Prog::GetFileAttributesForNFS(const char *path, wcc_attr *pAttr)
{
    struct stat data;

    if (path == NULL) {
        return false;
    }

    if (stat(path, &data) != 0) {
        return false;
    }

    pAttr->size = data.st_size;
    pAttr->mtime.seconds = (unsigned int)data.st_mtime;
    pAttr->mtime.nseconds = 0;
	// TODO: This needs to be tested (not called on my setup)
	// This seems to be the changed time, not creation time.
    //pAttr->ctime.seconds = data.st_ctime;
    pAttr->ctime.seconds = (unsigned int)data.st_mtime;
    pAttr->ctime.nseconds = 0;

    return true;
}

bool CNFS3Prog::GetFileAttributesForNFS(const char *path, fattr3 *pAttr) {
    NFSD_NOTIMPL
    return NFS3ERR_NOTSUPP;
#if 0
    uint32_t fileAttr;
    BY_HANDLE_FILE_INFORMATION lpFileInformation;
    HANDLE hFile;
    DWORD dwFlagsAndAttributes;

    fileAttr = GetFileAttributes(path);

    if (path == NULL || fileAttr == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    dwFlagsAndAttributes = 0;
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        pAttr->type = NF3DIR;
        dwFlagsAndAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_FLAG_BACKUP_SEMANTICS;
    }
    else if (fileAttr & FILE_ATTRIBUTE_ARCHIVE) {
        pAttr->type = NF3REG;
        dwFlagsAndAttributes = FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_OVERLAPPED;
    }
    else if (fileAttr & FILE_ATTRIBUTE_NORMAL) {
        pAttr->type = NF3REG;
        dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;
    }
    else {
        pAttr->type = 0;
    }

    if (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT) {
        pAttr->type = NF3LNK;
		dwFlagsAndAttributes = FILE_ATTRIBUTE_REPARSE_POINT | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
    }

	hFile = CreateFile(path, FILE_READ_EA, FILE_SHARE_READ, NULL, OPEN_EXISTING, dwFlagsAndAttributes, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    GetFileInformationByHandle(hFile, &lpFileInformation);
	CloseHandle(hFile);
    pAttr->mode = 0;

	// Set execution right for all
    pAttr->mode |= 0x49;

    // Set read right for all
    pAttr->mode |= 0x124;

    //if ((lpFileInformation.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0) {
        pAttr->mode |= 0x92;
    //}

    ULONGLONG fileSize = lpFileInformation.nFileSizeHigh;
    fileSize <<= sizeof(lpFileInformation.nFileSizeHigh) * 8;
    fileSize |= lpFileInformation.nFileSizeLow;

    pAttr->nlink = lpFileInformation.nNumberOfLinks;
    pAttr->uid = m_nUID;
    pAttr->gid = m_nGID;
    pAttr->size = fileSize;
    pAttr->used = pAttr->size;
    pAttr->rdev.specdata1 = 0;
    pAttr->rdev.specdata2 = 0;
    pAttr->fsid = 7; //NTFS //4; 
    pAttr->fileid = GetFileID(path);
    pAttr->atime.seconds = FileTimeToPOSIX(lpFileInformation.ftLastAccessTime);
    pAttr->atime.nseconds = 0;
    pAttr->mtime.seconds = FileTimeToPOSIX(lpFileInformation.ftLastWriteTime);
    pAttr->mtime.nseconds = 0;
	// This seems to be the changed time, not creation time
	pAttr->ctime.seconds = FileTimeToPOSIX(lpFileInformation.ftLastWriteTime);
    pAttr->ctime.nseconds = 0;

    return true;
#endif
}

uint32_t CNFS3Prog::FileTimeToPOSIX(struct timespec ft) {
    NFSD_NOTIMPL
    return 0;
#if 0
    // takes the last modified date
    LARGE_INTEGER date, adjust;
    date.HighPart = ft.dwHighDateTime;
    date.LowPart = ft.dwLowDateTime;

    // 100-nanoseconds = milliseconds * 10000
    adjust.QuadPart = 11644473600000 * 10000;

    // removes the diff between 1970 and 1601
    date.QuadPart -= adjust.QuadPart;

    // converts back from 100-nanoseconds to seconds
    return (unsigned int)(date.QuadPart / 10000000);
#endif
}
