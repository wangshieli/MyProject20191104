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
	BOOL UniqueServerInstance();	// 确保只有一个服务正在运行
	BOOL InitWinsock2();			// 初始化winsock2环境
	static BOOL GetExtensFunction();		// 获取外部扩展函数
	static BOOL CreateIoCompletionPortHandle();	// 创建端口
	static BOOL InitListenSocket(USHORT _port);			// 初始化监听端口
	static BOOL InitSUnit(CONST TCHAR* _sip, USHORT _port);
	static BOOL GetCpuNumsAndPagesize();
	static BOOL StartServer();					// 启动服务线程

	static unsigned int _stdcall logthread(PVOID pVoid);
	static unsigned int _stdcall sunitthread(PVOID pVoid);	// 处理作为客户端时的发送
	static unsigned int _stdcall toolthread(PVOID pVoid);	// 服务器accept
	static unsigned int _stdcall workthread(PVOID pVoid);	// 服务器业务

private:
	HANDLE m_hUniqueInstance;	// HANDLE句柄指的是核心对象在某一个进程中的唯一索引，而不是指针。使用（INVALID_HANDLE_VALUE）初始化
	static LPFN_ACCEPTEX	m_pfnAcceptEx;
	static LPFN_GETACCEPTEXSOCKADDRS	m_pfnAcceptExSockaddrs;
	static LPFN_DISCONNECTEX	m_pfnConnectEx;
	static HANDLE m_hIocp;
	static PListen_Handle m_pListenHandle;	// 服务器监听
	static PSock_Handle m_pSUnitHandle;		// 客户端连接
	static SOCKET m_sockSend;				// 线程发送事件使用
	static SOCKET m_sockReConnect;
	static DWORD m_dwCpunums;
	static DWORD m_dwThreadCounts;
	static DWORD m_dwPagesize;
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*-------------------------------------------------------------------------------------------------*/
private:
	static void ClearSingleData(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);

	static BOOL postAcceptEx();	// 准备接受客户端连接
	static void AcceptExSuccess(DWORD _dwTranstion, PVOID _pListen_Handle, PVOID _pBuf);
	static void AcceptExFaile(PVOID _pListen_Handle, PVOID _pBuf);

	static BOOL postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// 检查缓存区是否有接收数据
	static void ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// 接收数据
	static void RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void RecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL postSend(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// 发送数据
	static void SendSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void SendFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL SU_postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// 检查缓存区是否有接收数据
	static void SU_ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void SU_ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

	static BOOL SU_postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf);	// 接收数据
	static void SU_RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf);
	static void SU_RecvFaile(PVOID _pSock_Handle, PVOID _pBuf);

private:
	static void DoWorkProcessSuccess(DWORD _dwTranstion, PVOID _pBuf, PVOID pBuf_);	// 业务处理
	static void DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_);

	static void SU_DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_);
/*-------------------------------------------------------------------------------------------------*/

/***************************************************************************************************/
private:
	static void FD_Connect();
	static void FD_Read();
	static void FD_Write();	// 缓冲区满后可用
	static void FD_Close();
	static void Fuc_SUnit(DWORD _dwIndex);	// 发送事件处理过程

	static void Send_PostEventMessage(TCHAR* _buf, DWORD _bufsize);	// 发送事件
	static void ReConnect_PostEventMessage();	// 发送事件
	static void Fuc_Send(DWORD _dwIndex);	// 主动发送数据
	static void Fuc_ReConnect(DWORD _dwIndex);

private:
	static CBufferRing* m_pCBufRing;
/***************************************************************************************************/

/*=================================================================================================*/
	static void SendFile(PVOID _pSock_Handle, PVOID _pBuf);
/*=================================================================================================*/
};


/*
	
	优化：
		1.accept线程中需要处理超时的连接
		2.客户端数据发送完成之后需要重新利用释放的资源
		3.根据业务情况控制accept等待的数量
		4.检查业务超时
		5.多业务管理
		6.SUnit接受发送缓存取优化


*/

