#ifndef _NFSPROG_H_
#define _NFSPROG_H_

#include "RPCProg.h"
#include "NFS2Prog.h"
#include "NFS3Prog.h"

class CNFSProg : public CRPCProg
{
    public:
    CNFSProg();
    ~CNFSProg();
    void SetUserID(unsigned int nUID, unsigned int nGID);
    virtual int  Process(void);
    void SetLogOn(bool bLogOn);

    private:
    unsigned int m_nUID, m_nGID;
    CNFS3Prog    m_NFS3Prog;
    CNFS2Prog    m_NFS2Prog;
};

#endif
