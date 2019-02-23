#include "NFSProg.h"
#include "nfsd.h"

CNFSProg::CNFSProg() : CRPCProg(PROG_NFS, 0, "nfsd") {
    SetUserID(0, 0);
    m_NFS3Prog.SetLogOn(m_bLogOn);
}

CNFSProg::~CNFSProg() {}

void CNFSProg::SetUserID(unsigned int nUID, unsigned int nGID) {
    m_nUID = nUID;
    m_nGID = nGID;
    
    m_NFS2Prog.SetUserID(m_nUID, m_nGID);
    m_NFS3Prog.SetUserID(m_nUID, m_nGID);
}

int CNFSProg::Process(void) {
    if (m_param->version == 2) {
        m_NFS2Prog.Setup(m_in, m_out, m_param);
        return m_NFS2Prog.Process();
    } else if (m_param->version == 3) {
        m_NFS3Prog.Setup(m_in, m_out, m_param);
        return m_NFS3Prog.Process();
    } else {
        Log("Client requested NFS version %u which isn't supported.\n", m_param->version);
        return PRC_NOTIMP;
    }
}

void CNFSProg::SetLogOn(bool bLogOn) {
    CRPCProg::SetLogOn(bLogOn);

    m_NFS2Prog.SetLogOn(bLogOn);
    m_NFS3Prog.SetLogOn(bLogOn);
}
