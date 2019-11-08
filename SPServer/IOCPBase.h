#pragma once
#include "singledata.h"
#include "CBufferRing.h"

#define LISTEN_PORT		6086
#define PAGE_NUMS		8

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

	static unsigned int _stdcall logthread(PVOID pVoid);
	static unsigned int _stdcall sunitthread(PVOID pVoid);	// ������Ϊ�ͻ���ʱ�ķ���
	static unsigned int _stdcall toolthread(PVOID pVoid);	// ������accept
	static unsigned int _stdcall workthread(PVOID pVoid);	// ������ҵ��

private:
	HANDLE m_hUniqueInstance;	// HANDLE���ָ���Ǻ��Ķ�����ĳһ�������е�Ψһ������������ָ�롣ʹ�ã�INVALID_HANDLE_VALUE����ʼ��
	static LPFN_ACCEPTEX	m_pfnAcceptEx;
	static LPFN_GETACCEPTEXSOCKADDRS	m_pfnAcceptExSockaddrs;
	static LPFN_DISCONNECTEX	m_pfnConnectEx;
	static HANDLE m_hIocp;
	static PListen_Handle m_pListenHandle;	// ����������
	static PSock_Handle m_pSUnitHandle;		// �ͻ�������
	static SOCKET m_sockSend;				// �̷߳����¼�ʹ��
	static SOCKET m_sockReConnect;
	static DWORD m_dwCpunums;
	static DWORD m_dwThreadCounts;
	static DWORD m_dwPagesize;
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*-------------------------------------------------------------------------------------------------*/
private:
	static void ClearSingleData(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);

	static BOOL postAcceptEx();	// ׼�����ܿͻ�������
	static void AcceptExSuccess(DWORD _dwTranstion, PVOID _pListen_Handle, PVOID _pBuf);
	static void AcceptExFaile(PVOID _pListen_Handle, PVOID _pBuf);

	static BOOL postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// ��黺�����Ƿ��н�������
	static void ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// ��������
	static void RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void RecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL postSend(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// ��������
	static void SendSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void SendFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL SU_postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// ��黺�����Ƿ��н�������
	static void SU_ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void SU_ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL SU_postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// ��������
	static void SU_RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void SU_RecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

private:
	static void DoWorkProcessSuccess(DWORD _dwTranstion, PVOID _pBuf, PVOID pBuf_);	// ҵ����
	static void DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_);

	static void SU_DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_);
/*-------------------------------------------------------------------------------------------------*/

/***************************************************************************************************/
private:
	static void FD_Connect();
	static void FD_Read();
	static void FD_Write();	// �������������
	static void FD_Close();
	static void Fuc_SUnit(DWORD _dwIndex);	// �����¼��������

	static void Send_PostEventMessage(TCHAR* _buf, DWORD _bufsize);	// �����¼�
	static void ReConnect_PostEventMessage();	// �����¼�
	static void Fuc_Send(DWORD _dwIndex);	// ������������
	static void Fuc_ReConnect(DWORD _dwIndex);

private:
	static CBufferRing* m_pCBufRing;
/***************************************************************************************************/

/*=================================================================================================*/
	static void SendFile(PVOID _pSock_Handle, PVOID _pBuf);
/*=================================================================================================*/
};


/*
	
	�Ż���
		1.accept�߳�����Ҫ����ʱ������
		2.�ͻ������ݷ������֮����Ҫ���������ͷŵ���Դ
		3.����ҵ���������accept�ȴ�������
		4.���ҵ��ʱ
		5.��ҵ�����
		6.SUnit���ܷ��ͻ���ȡ�Ż�


*/

