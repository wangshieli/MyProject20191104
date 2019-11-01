#pragma once
#include "singledata.h"
#include "CBufferRing.h"

#define LISTEN_PORT		6086

class IOCPBase
{
public:
	IOCPBase();
	virtual ~IOCPBase();

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
private:
	BOOL UniqueServerInstance();	// ȷ��ֻ��һ��������������
	BOOL InitWinsock2();			// ��ʼ��winsock2����
	static BOOL GetExtensFunction();		// ��ȡ�ⲿ��չ����
	static BOOL CreateIoCompletionPortHandle();	// �����˿�
	static BOOL InitListenSocket(USHORT _port);			// ��ʼ�������˿�
	static BOOL InitSUnit(CONST TCHAR* _sip, USHORT _port);
	static BOOL GetCpuNumsAndPagesize();
	static BOOL StartServer();					// ���������߳�

	static unsigned int _stdcall sunitthread(PVOID pVoid);
	static unsigned int _stdcall toolthread(PVOID pVoid);
	static unsigned int _stdcall workthread(PVOID pVoid);

private:
	HANDLE m_hUniqueInstance;	// HANDLE���ָ���Ǻ��Ķ�����ĳһ�������е�Ψһ������������ָ�롣ʹ�ã�INVALID_HANDLE_VALUE����ʼ��
	static LPFN_ACCEPTEX	m_pfnAcceptEx;
	static LPFN_GETACCEPTEXSOCKADDRS	m_pfnAcceptExSockaddrs;
	static LPFN_DISCONNECTEX	m_pfnConnectEx;
	static HANDLE m_hIocp;
	static PListen_Handle m_pListenHandle;
	static PSock_Handle m_pSUnitHandle;
	static SOCKET m_sockSend;
	static DWORD m_dwCpunums;
	static DWORD m_dwPagesize;
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*-------------------------------------------------------------------------------------------------*/
private:
	static void ClearSingleData(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);

	static BOOL postAcceptEx();
	static void AcceptExSuccess(DWORD _dwTranstion, PVOID _pListen_Handle, PVOID _pBuf);
	static void AcceptExFaile(PVOID _pListen_Handle, PVOID _pBuf);

	static BOOL postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);
	static void ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);
	static void RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void RecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL postSend(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);
	static void SendSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void SendFaile(PVOID _pSock_Handle, PVOID _pBuf);

private:
	static void DoWorkProcessSuccess(DWORD _dwTranstion, PVOID _pBuf, PVOID pBuf_);
	static void DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_);
/*-------------------------------------------------------------------------------------------------*/

/***************************************************************************************************/
private:
	static void FD_Connect();
	static void FD_Read();
	static void FD_Write();
	static void FD_Close();
	static void Fuc_SUnit(DWORD _dwIndex);

	static void Send_PostEventMessage(TCHAR* _buf, DWORD _bufsize);
	static void Fuc_Send(DWORD _dwIndex);

private:
	static CBufferRing* m_pCBufRing;
/***************************************************************************************************/
};

