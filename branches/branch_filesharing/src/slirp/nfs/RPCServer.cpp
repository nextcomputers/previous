#include <stdio.h>

#include "RPCServer.h"
#include "TCPServerSocket.h"

#define MIN_PROG_NUM 100000
enum
{
	CALL = 0,
	REPLY = 1
};

enum
{
	MSG_ACCEPTED = 0,
	MSG_DENIED = 1
};

enum
{
	SUCCESS       = 0,
	PROG_UNAVAIL  = 1,
	PROG_MISMATCH = 2,
	PROC_UNAVAIL  = 3,
	GARBAGE_ARGS  = 4
};

typedef struct
{
	uint32_t flavor;
	uint32_t length;
} OPAQUE_AUTH;

typedef struct
{
	uint32_t header;
	uint32_t XID;
	uint32_t msg;
	uint32_t rpcvers;
	uint32_t prog;
	uint32_t vers;
	uint32_t proc;
	OPAQUE_AUTH cred;
	OPAQUE_AUTH verf;
} RPC_HEADER;

CRPCServer::CRPCServer()
{
	int i;

	for (i = 0; i < PROG_NUM; i++)
		m_pProgTable[i] = NULL;
    m_hMutex = host_mutex_create();
}

CRPCServer::~CRPCServer()
{
	host_mutex_destroy(m_hMutex);
}

void CRPCServer::Set(int nProg, CRPCProg *pRPCProg)
{
	m_pProgTable[nProg - MIN_PROG_NUM] = pRPCProg;  //set program handler
}

void CRPCServer::SetLogOn(bool bLogOn)
{
	int i;

	for (i = 0; i < PROG_NUM; i++)
		if (m_pProgTable[i] != NULL)
			m_pProgTable[i]->SetLogOn(bLogOn);
}

void CRPCServer::SocketReceived(CSocket *pSocket)
{
	XDRInput *pInStream;
	int nResult;

    host_mutex_lock(m_hMutex);
	pInStream = pSocket->GetInputStream();
	while (pInStream->GetSize() > 0)
	{
		nResult = Process(pSocket->GetType(), pInStream, pSocket->GetOutputStream(), pSocket->GetRemoteAddress());  //process input data
		pSocket->Send();  //send response
		if (nResult != PRC_OK || pSocket->GetType() == SOCK_DGRAM)
			break;
	}
	host_mutex_unlock(m_hMutex);
}

int CRPCServer::Process(int sockType, XDRInput* pInStream, XDROutput* pOutStream, const char* pRemoteAddr)
{
	RPC_HEADER header;
	int nPos, nSize;
	ProcessParam param;
	int nResult;

	nResult = PRC_OK;
	if (sockType == SOCK_STREAM)
		pInStream->Read(&header.header);
	pInStream->Read(&header.XID);
	pInStream->Read(&header.msg);
	pInStream->Read(&header.rpcvers);  //rpc version
	pInStream->Read(&header.prog);  //program
	pInStream->Read(&header.vers);  //program version
	pInStream->Read(&header.proc);  //procedure
	pInStream->Read(&header.cred.flavor);
	pInStream->Read(&header.cred.length);
	pInStream->Skip(header.cred.length);
	pInStream->Read(&header.verf.flavor);  //vefifier
	if (pInStream->Read(&header.verf.length) < sizeof(header.verf.length))
		nResult = PRC_FAIL;
	if (pInStream->Skip(header.verf.length) < header.verf.length)
		nResult = PRC_FAIL;

	if (sockType == SOCK_STREAM)
	{
		nPos = pOutStream->GetPosition();  //remember current position
		pOutStream->Write(header.header);  //this value will be updated later
	}
	pOutStream->Write(header.XID);
	pOutStream->Write(REPLY);
	pOutStream->Write(MSG_ACCEPTED);
	pOutStream->Write(header.verf.flavor);
	pOutStream->Write(header.verf.length);
	if (nResult == PRC_FAIL)  //input data is truncated
		pOutStream->Write(GARBAGE_ARGS);
	else if (header.prog < MIN_PROG_NUM || header.prog >= MIN_PROG_NUM + PROG_NUM)  //program is unavailable
		pOutStream->Write(PROG_UNAVAIL);
	else if (m_pProgTable[header.prog - MIN_PROG_NUM] == NULL)  //program is unavailable
		pOutStream->Write(PROG_UNAVAIL);
	else
	{
		pOutStream->Write(SUCCESS);  //this value may be modified later if process failed
		param.version    = header.vers;
		param.proc       = header.proc;
		param.remoteAddr = pRemoteAddr;
        param.sockType   = sockType;
        
        m_pProgTable[header.prog - MIN_PROG_NUM]->Setup(pInStream, pOutStream, &param);
		nResult = m_pProgTable[header.prog - MIN_PROG_NUM]->Process();

		if (nResult == PRC_NOTIMP)  //procedure is not implemented
		{
			pOutStream->Seek(-4, SEEK_CUR);
			pOutStream->Write(PROC_UNAVAIL);
		}
		else if (nResult == PRC_FAIL)  //input data is truncated
		{
			pOutStream->Seek(-4, SEEK_CUR);
			pOutStream->Write(GARBAGE_ARGS);
		}
	}

	if (sockType == SOCK_STREAM)
	{
		nSize = pOutStream->GetPosition();  //remember current position
		pOutStream->Seek(nPos, SEEK_SET);  //seek to the position of head
		header.header = 0x80000000 + nSize - nPos - 4;  //size of output data
		pOutStream->Write(header.header);  //update header
	}
	return nResult;
}