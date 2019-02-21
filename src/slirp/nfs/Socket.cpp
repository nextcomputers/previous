#include <arpa/inet.h>

#include "Socket.h"
#include "nfsd.h"

static int ThreadProc(void *lpParameter)
{
	CSocket *pSocket;

	pSocket = (CSocket *)lpParameter;
	pSocket->Run();
	return 0;
}

CSocket::CSocket(int nType) : m_nType(nType), m_Socket(-1), m_pListener(NULL), m_bActive(false), m_hThread(NULL) 
{
	memset(&m_RemoteAddr, 0, sizeof(m_RemoteAddr));
}

CSocket::~CSocket()
{
	Close();
}

int CSocket::GetType(void)
{
	return m_nType;  //socket type
}

void CSocket::Open(int socket, ISocketListener *pListener, struct sockaddr_in *pRemoteAddr)
{
	Close();

	m_Socket = socket;  //socket
	m_pListener = pListener;  //listener
	if (pRemoteAddr != NULL)
		m_RemoteAddr = *pRemoteAddr;  //remote address
	if (m_Socket != INVALID_SOCKET)
	{
		m_bActive = true;
		m_hThread = host_thread_create(ThreadProc, "CSocket", this);  //begin thread
	}
}

void CSocket::Close(void)
{
	if (m_Socket != INVALID_SOCKET)
	{
		close(m_Socket);
		m_Socket = INVALID_SOCKET;
	}

	if (m_hThread != NULL)
	{
        host_thread_wait(m_hThread);
		m_hThread = NULL;
	}
}

void CSocket::Send(void) {
	if (m_Socket == INVALID_SOCKET)
		return;

	if (m_nType == SOCK_STREAM)
		send(m_Socket, (const char *)m_Output.GetBuffer(), m_Output.GetSize(), 0);
	else if (m_nType == SOCK_DGRAM)
		sendto(m_Socket, (const char *)m_Output.GetBuffer(), m_Output.GetSize(), 0, (struct sockaddr *)&m_RemoteAddr, sizeof(struct sockaddr));
	m_Output.Reset();  //clear output buffer
}

bool CSocket::Active(void)
{
	return m_bActive;  //thread is active or not
}

char *CSocket::GetRemoteAddress(void)
{
    return inet_ntoa(m_RemoteAddr.sin_addr);
}

int CSocket::GetRemotePort(void)
{
    return htons(m_RemoteAddr.sin_port);
}

XDRInput* CSocket::GetInputStream(void)
{
	return &m_Input;
}

XDROutput* CSocket::GetOutputStream(void)
{
	return &m_Output;
}

void CSocket::Run(void)
{
	socklen_t nSize, nBytes;

	nSize = sizeof(m_RemoteAddr);
	for (;;)
	{
		if (m_nType == SOCK_STREAM)
			nBytes = recv(m_Socket, m_Input.GetBuffer(), 1024 /*m_Input.GetCapacity()*/, 0);
		else if (m_nType == SOCK_DGRAM)
			nBytes = recvfrom(m_Socket, m_Input.GetBuffer(), m_Input.GetCapacity(), 0, (struct sockaddr *)&m_RemoteAddr, &nSize);
		if (nBytes > 0)
		{
			m_Input.SetSize(nBytes);  //bytes received
			if (m_pListener != NULL)
				m_pListener->SocketReceived(this);  //notify listener
		}
		else
			break;
	}
	m_bActive = false;
}

void CSocket::map_port(int type, int progNum, uint16_t port) {
    switch(type) {
        case SOCK_DGRAM:
            switch(progNum) {
                case PROG_PORTMAP: mapped_udp_portmap_port = port; break;
                case PROG_MOUNT:   udp_mount_port          = port; break;
                case PROG_NFS:     mapped_udp_nfs_port     = port; break;
            }
            break;
        case SOCK_STREAM:
            switch(progNum) {
                case PROG_PORTMAP: mapped_tcp_portmap_port = port; break;
                case PROG_MOUNT:   tcp_mount_port          = port; break;
                case PROG_NFS:     mapped_tcp_nfs_port     = port; break;
            }
            break;
    }
}

uint16_t CSocket::map_and_htons(int sockType, uint16_t port) {
    if     (sockType == SOCK_DGRAM  && port == PORT_PORTMAP) return htons(mapped_udp_portmap_port);
    else if(sockType == SOCK_STREAM && port == PORT_PORTMAP) return htons(mapped_tcp_portmap_port);
    if     (sockType == SOCK_DGRAM  && port == PORT_NFS)     return htons(mapped_udp_nfs_port);
    else if(sockType == SOCK_STREAM && port == PORT_NFS)     return htons(mapped_tcp_nfs_port);
    else                                                     return htons(port);
}
