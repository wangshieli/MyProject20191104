#include "pch.h"
#include "IOCPBase.h"
#include <string>

#pragma comment(lib, "ws2_32.lib")

#ifdef UNICODE
#define __MYFINE__		__FILEW__
#define __MYFUNCTION__		__FUNCTIONW__
#ifdef _STRING_
typedef std::wstring	mystring;
#endif // _STRING_
#else
#define __MYFINE__		__FILE__
#define __MYFUNCTION__		__FUNCTION__
#ifdef _STRING_
typedef std::string	mystring;
#endif // _STRING_
#endif // UNICODE

#define SERVER_IP	_T("192.168.24.104")
#define SERVER_PORT	6666

HANDLE g_evtListen[WSA_MAXIMUM_WAIT_EVENTS];
DWORD g_nListens = 0;

HANDLE g_evtConnect[WSA_MAXIMUM_WAIT_EVENTS];
SOCKET g_sockConnect[WSA_MAXIMUM_WAIT_EVENTS];
DWORD g_nConnects = 0;
void(*g_fucEvent[WSA_MAXIMUM_WAIT_EVENTS])(DWORD _dwIndex);

HANDLE g_evtWaitSunitThreadOn = INVALID_HANDLE_VALUE;

HANDLE g_evtSend = INVALID_HANDLE_VALUE;
HANDLE g_evtReConnect = INVALID_HANDLE_VALUE;
HANDLE g_evtSUnit = INVALID_HANDLE_VALUE;

void LOG(LPCTSTR format, LPCTSTR _filename, LPCTSTR _funcname, LONG _linenum, ...)
{
	mystring _temp(_T("%s_%s_%d:"));
	_temp.append(format);
	_temp.append(_T("\n"));
	va_list va;
	va_start(va, format);
	_vtprintf_s(_temp.c_str(), va);
	va_end(va);
}

#define log_printf(format, ...) LOG(format, __MYFINE__	, __MYFUNCTION__, __LINE__, __VA_ARGS__)

#define SPServer_THE_ONE_INSTANCE	_T("Global\\Event_SPServer_The_One_Instance")

LPFN_ACCEPTEX	IOCPBase::m_pfnAcceptEx = NULL;
LPFN_GETACCEPTEXSOCKADDRS	IOCPBase::m_pfnAcceptExSockaddrs = NULL;
LPFN_DISCONNECTEX	IOCPBase::m_pfnConnectEx = NULL;
HANDLE IOCPBase::m_hIocp = INVALID_HANDLE_VALUE;
PListen_Handle IOCPBase::m_pListenHandle = NULL;
PSock_Handle IOCPBase::m_pSUnitHandle = NULL;
SOCKET IOCPBase::m_sockSend = INVALID_SOCKET;
SOCKET IOCPBase::m_sockReConnect = INVALID_SOCKET;
DWORD IOCPBase::m_dwCpunums = 0;
DWORD IOCPBase::m_dwPagesize = 0;
CBufferRing* IOCPBase::m_pCBufRing = new CBufferRing();

IOCPBase::IOCPBase(): m_hUniqueInstance(INVALID_HANDLE_VALUE)
	//, m_pfnAcceptEx(NULL)
	//, m_pfnAcceptExSockaddrs(NULL)
	//, m_pfnConnectEx(NULL)
	//, m_hIocp(INVALID_HANDLE_VALUE)
	//, m_pListenHandle(NULL)
{
	if (!UniqueServerInstance() || !InitWinsock2() || !GetExtensFunction() || !CreateIoCompletionPortHandle() || !InitListenSocket(LISTEN_PORT) || !GetCpuNumsAndPagesize())
		return;

	StartServer();
}

IOCPBase::~IOCPBase()
{
	if (NULL != m_pListenHandle)
	{
		closesocket(m_pListenHandle->sListenSock);
		m_pListenHandle->sListenSock = INVALID_SOCKET;
		if (WSA_INVALID_EVENT != m_pListenHandle->evtPostAcceptEx)
			WSACloseEvent(m_pListenHandle->evtPostAcceptEx);
		delete m_pListenHandle;
		m_pListenHandle = NULL;
	}

	WSACleanup();

	CloseHandle(m_hUniqueInstance);  // CloseHandle减少对Event的引用，当最后一个引用被关闭的时候Event释放。进程结束的时候系统也会自动释放
	m_hUniqueInstance = INVALID_HANDLE_VALUE;
}

BOOL IOCPBase::UniqueServerInstance()
{
	// OpenEvent/CreateEvent失败返回的是NULL
	m_hUniqueInstance = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, SPServer_THE_ONE_INSTANCE);
	if (NULL != m_hUniqueInstance)
	{
		log_printf(_T("已经有对应的服务在运行中%s\n"), _T("test"));
		return FALSE;
	}

	m_hUniqueInstance = ::CreateEvent(NULL, FALSE, FALSE, SPServer_THE_ONE_INSTANCE);
	if (NULL == m_hUniqueInstance)
	{
		log_printf(_T("UniqueInstance事件初始化错误：%d\n"), GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL IOCPBase::InitWinsock2()
{
	WSADATA _wsadata;
	if (0 != WSAStartup(MAKEWORD(2, 2), &_wsadata))
	{
		log_printf(_T("初始化winsock2失败:%d"), GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL IOCPBase::GetExtensFunction()
{
	SOCKET _s = INVALID_SOCKET;
	_s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _s)
	{
		log_printf(_T("获取外部扩展函数失败:%d"), WSAGetLastError());
		return FALSE;
	}

	do
	{
		DWORD _dwBytes = 0;
		GUID GuidAcceptEx = WSAID_ACCEPTEX,
			GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS,
			GuidConnectEx = WSAID_CONNECTEX;
		if (SOCKET_ERROR == WSAIoctl(_s, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &GuidAcceptEx, sizeof(GuidAcceptEx)
			, &m_pfnAcceptEx, sizeof(m_pfnAcceptEx)
			, &_dwBytes, NULL, NULL))
		{
			log_printf(_T("获取外部扩展函数失败:%d"), WSAGetLastError());
			break;
		}

		if (SOCKET_ERROR == WSAIoctl(_s, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs)
			, &m_pfnAcceptExSockaddrs, sizeof(m_pfnAcceptExSockaddrs)
			, &_dwBytes, NULL, NULL))
		{
			log_printf(_T("获取外部扩展函数失败:%d"), WSAGetLastError());
			break;
		}

		if (SOCKET_ERROR == WSAIoctl(_s, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &GuidConnectEx, sizeof(GuidConnectEx)
			, &m_pfnConnectEx, sizeof(m_pfnConnectEx)
			, &_dwBytes, NULL, NULL))
		{
			log_printf(_T("获取外部扩展函数失败:%d"), WSAGetLastError());
			break;
		}
		closesocket(_s);
		_s = INVALID_SOCKET;
		return TRUE;
	} while (true);

	closesocket(_s);
	_s = INVALID_SOCKET;
	return FALSE;
}

BOOL IOCPBase::CreateIoCompletionPortHandle()
{
	m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);
	if (NULL == m_hIocp)
	{
		log_printf(_T("创建端口失败:%d"), WSAGetLastError());
		return FALSE;
	}
	return TRUE;
}

BOOL IOCPBase::InitListenSocket(USHORT _port)
{
	do
	{
		m_pListenHandle = new Listen_Handle();
		if (NULL == m_pListenHandle)
		{
			log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
			break;
		}

		m_pListenHandle->evtPostAcceptEx = WSACreateEvent();
		if (WSA_INVALID_EVENT == m_pListenHandle->evtPostAcceptEx)
		{
			log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
			break;
		}

		m_pListenHandle->sListenSock = INVALID_SOCKET;

		m_pListenHandle->sListenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == m_pListenHandle->sListenSock)
		{
			log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
			break;
		}

		if (NULL == CreateIoCompletionPort((HANDLE)m_pListenHandle->sListenSock, m_hIocp, (ULONG_PTR)m_pListenHandle, 0))
		{
			log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
			break;
		}

		if (SOCKET_ERROR == WSAEventSelect(m_pListenHandle->sListenSock, m_pListenHandle->evtPostAcceptEx, FD_ACCEPT))
		{
			log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
			break;
		}
		g_evtListen[g_nListens++] = m_pListenHandle->evtPostAcceptEx;

		struct sockaddr_in laddr;
		memset(&laddr, 0x00, sizeof(laddr));
		laddr.sin_family = AF_INET;
		laddr.sin_port = htons(_port);
		laddr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (SOCKET_ERROR == bind(m_pListenHandle->sListenSock, (SOCKADDR*)&laddr, sizeof(laddr)))
		{
			log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
			break;
		}

		if (SOCKET_ERROR == listen(m_pListenHandle->sListenSock, SOMAXCONN))
		{
			log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
			break;
		}

		return TRUE;
	} while (FALSE);
	
	return FALSE;
}

BOOL IOCPBase::InitSUnit(CONST TCHAR* _sip, USHORT _port)
{
	m_sockSend = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	g_evtSend = WSACreateEvent();
	if (SOCKET_ERROR == WSAEventSelect(m_sockSend, g_evtSend, FD_WRITE))
	{
		log_printf(_T("连接SUnit服务失败:%d"), WSAGetLastError());
		return FALSE;
	}
	g_sockConnect[g_nConnects] = m_sockSend;
	g_evtConnect[g_nConnects] = g_evtSend;
	g_fucEvent[g_nConnects++] = Fuc_Send;

	m_sockReConnect = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	g_evtReConnect = WSACreateEvent();
	if (SOCKET_ERROR == WSAEventSelect(m_sockReConnect, g_evtReConnect, FD_WRITE))
	{
		log_printf(_T("连接SUnit服务失败:%d"), WSAGetLastError());
		return FALSE;
	}
	g_sockConnect[g_nConnects] = m_sockReConnect;
	g_evtConnect[g_nConnects] = g_evtReConnect;
	g_fucEvent[g_nConnects++] = Fuc_ReConnect;

	m_pCBufRing->Init(m_dwPagesize * PAGE_NUMS * 2);
	g_evtSUnit = WSACreateEvent();
	m_pSUnitHandle = (PSock_Handle)malloc(m_dwPagesize * PAGE_NUMS);
	if (NULL == m_pSUnitHandle)
	{
		log_printf(_T("InitSUnit失败:%d"), WSAGetLastError());
		return FALSE;
	}
	InitializeCriticalSection(&m_pSUnitHandle->cs);
	m_pSUnitHandle->lstSend = new std::list<PSock_Buf>;
	if (NULL == m_pSUnitHandle->lstSend)
	{
		log_printf(_T("InitSUnit失败:%d"), WSAGetLastError());
		return FALSE;
	}
	m_pSUnitHandle->Init(m_dwPagesize * PAGE_NUMS - SOCK_HANDLE_T_SIZE);
	m_pSUnitHandle->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (NULL == CreateIoCompletionPort((HANDLE)m_pSUnitHandle->s, m_hIocp, (ULONG_PTR)m_pSUnitHandle, 0))
	{
		log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
		return FALSE;
	}

	if (SOCKET_ERROR == WSAEventSelect(m_pSUnitHandle->s, g_evtSUnit, FD_WRITE))
	{
		log_printf(_T("连接SUnit服务失败:%d"), WSAGetLastError());
		return FALSE;
	}
	g_sockConnect[g_nConnects] = m_pSUnitHandle->s;
	g_evtConnect[g_nConnects] = g_evtSUnit;
	g_fucEvent[g_nConnects++] = Fuc_SUnit;

	g_evtWaitSunitThreadOn = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, sunitthread, NULL, 0, NULL);
	if (WAIT_OBJECT_0 != WaitForSingleObject(g_evtWaitSunitThreadOn, 5000))
	{
		log_printf(_T("初始化SUnit失:%d"), GetLastError());
		return FALSE;
	}

	struct sockaddr_in addr;
	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	InetPton(AF_INET, _sip, &addr.sin_addr.s_addr);
	if (SOCKET_ERROR == connect(m_pSUnitHandle->s, (const sockaddr*)&addr, sizeof(addr))) // 如果服务器没打开会卡着
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			log_printf(_T("连接服务器失败:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	PSock_Buf pBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
	pBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
	if (!SU_postZeroRecv(m_pSUnitHandle, pBuf))
	{
		if (0 == InterlockedDecrement(&m_pSUnitHandle->nRef))
		{
			ClearSingleData(NULL, pBuf);
			ReConnect_PostEventMessage();
			return FALSE;//  启动重连
		}
		free(pBuf);
	}

	return TRUE;
}

BOOL IOCPBase::GetCpuNumsAndPagesize()
{
	SYSTEM_INFO sys;
	GetSystemInfo(&sys);
	m_dwCpunums = sys.dwNumberOfProcessors;
	m_dwPagesize = sys.dwPageSize;

	return TRUE;
}

BOOL IOCPBase::StartServer()
{
	//192.168.24.104 6666
	InitSUnit(SERVER_IP, SERVER_PORT);

	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, toolthread, NULL, 0, NULL);

	DWORD _dwThreadCounts = m_dwCpunums * 2 + 2;
	for (DWORD i = 0; i < _dwThreadCounts; i++)
	{
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, workthread, NULL, 0, NULL);
	}

	DWORD _dwClienCounts = _dwThreadCounts * 5;
	for (DWORD i = 0; i < _dwClienCounts; i++)
	{
		postAcceptEx();
	}
	return 0;
}

unsigned int _stdcall IOCPBase::sunitthread(PVOID pVoid)
{
	SetEvent(g_evtWaitSunitThreadOn);
	while (true)
	{
		DWORD dwIndex = WSAWaitForMultipleEvents(g_nConnects, g_evtConnect, FALSE, INFINITE, FALSE);
		if (WSA_WAIT_FAILED == dwIndex)
		{
			log_printf(_T("异常退出:%d"), WSAGetLastError());
			return 0;
		}
		DWORD dwRealIndex = dwIndex - WSA_WAIT_EVENT_0;
		// 返回的是触发事件的最小索引，为了确保此索引之后的事件能被及时处理，需要遍历一遍   目前只监听一个事件，先不处理
		for (DWORD i = dwRealIndex; i < g_nConnects; i++)
		{
			DWORD dwError = WSAWaitForMultipleEvents(1, &g_evtConnect[i], TRUE, 0, FALSE);
			if (WSA_WAIT_TIMEOUT == dwError || WSA_WAIT_FAILED == dwError)
				continue;

			WSAResetEvent(g_evtConnect[i]);

			g_fucEvent[i](i);
		}
	}
	return 0;
}

unsigned int _stdcall IOCPBase::toolthread(PVOID pVoid)
{
	while (true)
	{
		DWORD dwIndex = WSAWaitForMultipleEvents(g_nListens, g_evtListen, FALSE, INFINITE, FALSE);
		if (WSA_WAIT_FAILED == dwIndex)
		{
			log_printf(_T("异常退出:%d"), WSAGetLastError());
			return 0;
		}
		WSAResetEvent(g_evtListen[dwIndex - WSA_WAIT_EVENT_0]);

		for (DWORD i = 0; i < m_dwCpunums * 2; i++)
		{
			postAcceptEx();
		}
	}
	return 0;
}

unsigned int _stdcall IOCPBase::workthread(PVOID pVoid)
{
	ULONG_PTR key;
	PSock_Buf pBuf;
	LPOVERLAPPED lpol;
	DWORD dwTranstion;
	BOOL bSuccess = FALSE;

	while (true)
	{
		bSuccess = GetQueuedCompletionStatus(m_hIocp, &dwTranstion, &key, &lpol, INFINITE);
		if (NULL == lpol)
		{
			log_printf(_T("此处应该设置退出信号"));
			return 0;
		}

		pBuf = CONTAINING_RECORD(lpol, Sock_Buf, ol);

		if (!bSuccess)
			pBuf->pfnFailed((PVOID)key, pBuf);
		else
			pBuf->pfnSuccess(dwTranstion, (PVOID)key, pBuf);
	}

	return 0;
}

void IOCPBase::ClearSingleData(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	if (NULL != _pSock_Handle)
	{
		DeleteCriticalSection(&_pSock_Handle->cs);
		if (NULL != _pSock_Handle->lstSend)
			delete _pSock_Handle->lstSend;
		if (INVALID_SOCKET != _pSock_Handle->s)
		{
			closesocket(_pSock_Handle->s);
			_pSock_Handle->s = INVALID_SOCKET;
		}
		free(_pSock_Handle);
		_pSock_Handle = NULL;

	}

	if (NULL != _pBuf)
	{
		free(_pBuf);
		_pBuf = NULL;
	}
}

BOOL IOCPBase::postAcceptEx()
{
	DWORD dwBytes = 0;

	PSock_Handle _pSock_Handle = (PSock_Handle)malloc(m_dwPagesize * PAGE_NUMS);
	PSock_Buf _pBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);

	do
	{
		if (NULL == _pSock_Handle)
		{
			log_printf(_T("AcceptEx失败:%d"), WSAGetLastError());
			break;
		}
		InitializeCriticalSection(&_pSock_Handle->cs);
		_pSock_Handle->lstSend = new std::list<PSock_Buf>;
		if (NULL == _pSock_Handle->lstSend)
		{
			log_printf(_T("AcceptEx失败:%d"), WSAGetLastError());
			break;
		}
		_pSock_Handle->Init(m_dwPagesize * PAGE_NUMS - SOCK_HANDLE_T_SIZE);
		_pSock_Handle->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (SOCKET_ERROR == _pSock_Handle->s)
		{
			log_printf(_T("AcceptEx失败:%d"), WSAGetLastError());
			break;
		}

		_pBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
		_pBuf->pRelateSockHandle = _pSock_Handle;
		_pBuf->pfnSuccess = AcceptExSuccess;
		_pBuf->pfnFailed = AcceptExFaile;
		if (!m_pfnAcceptEx(m_pListenHandle->sListenSock, _pSock_Handle->s, _pSock_Handle->buf
			, _pSock_Handle->dwBufsize - ((sizeof(sockaddr_in) + 16) * 2)
			, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &dwBytes, &_pBuf->ol))
		{
			if (WSA_IO_PENDING != WSAGetLastError())
			{
				log_printf(_T("AcceptEx失败:%d"), WSAGetLastError());
				break;
			}
		}
		return TRUE;
	} while (FALSE);
	
	ClearSingleData(_pSock_Handle, _pBuf);

	return FALSE;
}

struct tcp_keepalive alive_in = { TRUE, 1000 * 10, 1000 };
struct tcp_keepalive alive_out = { 0 };
unsigned long ulBytesReturn = 0;

void IOCPBase::AcceptExSuccess(DWORD _dwTranstion, PVOID _pListen_Handle, PVOID _pBuf)
{
	if (_dwTranstion <= 0)
		return AcceptExFaile(_pListen_Handle, _pBuf);

	PListen_Handle pListen_Hnalde = (PListen_Handle)_pListen_Handle;
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;

	if (SOCKET_ERROR == WSAIoctl(pSock_Handle->s, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in),
		&alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL))
	{
		log_printf(_T("设置客户端超时检测失败:%d"), WSAGetLastError());
	}

	if (NULL == CreateIoCompletionPort((HANDLE)pSock_Handle->s, m_hIocp, (ULONG_PTR)pSock_Handle, 0))
	{
		log_printf(_T("客户端绑定端口失败:%d"), WSAGetLastError());
		AcceptExFaile(_pListen_Handle, _pBuf);
		return;
	}

	SOCKADDR* localAddr = NULL;
	SOCKADDR* remoteAddr = NULL;
	int localAddrlen = 0;
	int remoteAddrlen = 0;

	m_pfnAcceptExSockaddrs(pSock_Handle->buf, pSock_Handle->dwBufsize - ((sizeof(sockaddr_in) + 16) * 2)
		, sizeof(sockaddr_in) + 16
		, sizeof(sockaddr_in) + 16
		, &localAddr, &localAddrlen
		, &remoteAddr, &remoteAddrlen);

	pSock_Handle->InitWRpos(_dwTranstion);
	DWORD len = pSock_Handle->GetCmdDataLength();
	while (len)
	{
		PSock_Buf workBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
		workBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
		pSock_Handle->Read(workBuf->data, len);
		workBuf->dwRecvedCount = len;
		workBuf->pRelateSockHandle = pSock_Handle;
		pSock_Handle->AddRef();
		workBuf->pfnFailed = DoWorkProcessFaile;	// 处理业务的过程
		workBuf->pfnSuccess = DoWorkProcessSuccess;
		if (!PostQueuedCompletionStatus(m_hIocp, len, (ULONG_PTR)workBuf, &workBuf->ol))
		{
			log_printf(_T("业务投递错误:%d"), WSAGetLastError());
			pSock_Handle->DecRef();
			free(workBuf);
		}
		len = pSock_Handle->GetCmdDataLength();
	}

	if (!postZeroRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(pSock_Handle, pBuf);
			return;
		}
		free(pBuf);
	}
}

void IOCPBase::AcceptExFaile(PVOID _pListen_Handle, PVOID _pBuf)
{
	PListen_Handle pListen_Hnalde = (PListen_Handle)_pListen_Handle;
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;

	if (NULL != pSock_Handle)
	{
		DeleteCriticalSection(&pSock_Handle->cs);
		if (NULL != pSock_Handle->lstSend)
			delete pSock_Handle->lstSend;
		if (INVALID_SOCKET != pSock_Handle->s)
		{
			closesocket(pSock_Handle->s);
			pSock_Handle->s = INVALID_SOCKET;
		}
		free(pSock_Handle);
		pSock_Handle = NULL;

	}

	if (NULL != pBuf)
	{
		free(pBuf);
		pBuf = NULL;
	}
}

BOOL IOCPBase::postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	log_printf(_T("postZeroRecv"));
	DWORD dwBytes = 0;
	DWORD dwFlags = 0;

	_pSock_Handle->wsabuf[0].buf = NULL;
	_pSock_Handle->wsabuf[0].len = 0;
	_pSock_Handle->wsabuf[1].buf = NULL;
	_pSock_Handle->wsabuf[1].len = 0;

	_pBuf->pfnFailed = ZeroRecvFaile;
	_pBuf->pfnSuccess = ZeroRecvSuccess;

	if (SOCKET_ERROR == WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			log_printf(_T("接收客户端送数据失败（0字节）:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	log_printf(_T("ZeroRecvSuccess"));
	//char buf[1024] = { 0 };
	//recv(pSock_Handle->s, buf, 1024, 0);
	//log_printf(_T("ZeroRecvSuccess:%s"), buf);

	if (!postRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(pSock_Handle, pBuf);
			return;
		}
		free(pBuf);
	}
}

void IOCPBase::ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	log_printf(_T("ZeroRecvFaile"));
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		ClearSingleData(pSock_Handle, pBuf);
		return;
	}
	free(pBuf);
}

BOOL IOCPBase::postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	DWORD dwBytes = 0;
	DWORD dwFlags = 0;

	_pSock_Handle->InitWSABUFS();

	_pBuf->pfnFailed = RecvFaile;
	_pBuf->pfnSuccess = RecvSuccess;

	int r = WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL);
	if (SOCKET_ERROR == r)
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			log_printf(_T("接收户发送的数据失败:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	if (_dwTranstion <= 0)
		return RecvFaile(_pSock_Handle, _pBuf);
	
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	log_printf(_T("RecvSuccess:%d"), _dwTranstion);

	pSock_Handle->InitWRpos(_dwTranstion);
	DWORD len = pSock_Handle->GetCmdDataLength();
	while (len)
	{
		PSock_Buf workBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
		workBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
		pSock_Handle->Read(workBuf->data, len);
		workBuf->dwRecvedCount = len;
		workBuf->pRelateSockHandle = pSock_Handle;
		pSock_Handle->AddRef();
		workBuf->pfnFailed = DoWorkProcessFaile;	// 处理业务的过程
		workBuf->pfnSuccess = DoWorkProcessSuccess;
		if (!PostQueuedCompletionStatus(m_hIocp, len, (ULONG_PTR)workBuf, &workBuf->ol))
		{
			log_printf(_T("业务投递错误:%d"), WSAGetLastError());
			pSock_Handle->DecRef();
			free(workBuf);			
		}
		len = pSock_Handle->GetCmdDataLength();
	}

	if (!postZeroRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(pSock_Handle, pBuf);
			return;
		}
		free(pBuf);
	}
}

void IOCPBase::RecvFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	log_printf(_T("RecvFaile"));
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		ClearSingleData(pSock_Handle, pBuf);
		return;
	}
	free(pBuf);
}

BOOL IOCPBase::postSend(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	DWORD dwBytes = 0;

	_pBuf->wsaBuf.buf = _pBuf->data + _pBuf->dwSendedCount;
	_pBuf->wsaBuf.len = _pBuf->dwRecvedCount - _pBuf->dwSendedCount;

	_pBuf->pfnFailed = SendFaile;
	_pBuf->pfnSuccess = SendSuccess;
	log_printf(_T("postSend"));
	if (SOCKET_ERROR == WSASend(_pSock_Handle->s, &_pBuf->wsaBuf, 1, &dwBytes, 0, &_pBuf->ol, NULL))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			log_printf(_T("向客户发送数据失败:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::SendSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	log_printf(_T("SendSuccess:%d"), _dwTranstion);
	if (_dwTranstion <= 0)
		return SendFaile(_pSock_Handle, _pBuf);

	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;

	pBuf->dwSendedCount += _dwTranstion;
	if (pBuf->dwSendedCount < pBuf->dwRecvedCount)
	{
		if (!postSend(pSock_Handle, pBuf))
		{
			return SendFaile(_pSock_Handle, _pBuf);
		}
		return;
	}

	free(pBuf);
	pBuf = pSock_Handle->GetNextData();
	if (pBuf)
	{
		InterlockedDecrement(&pSock_Handle->nRef);
		pBuf->pfnFailed = SendFaile;
		pBuf->pfnSuccess = SendSuccess;
		if (!postSend(pSock_Handle, pBuf))
		{
			SendFaile(pSock_Handle, pBuf);
		}
	}
	else
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			if (NULL != _pSock_Handle)
			{
				DeleteCriticalSection(&pSock_Handle->cs);
				if (NULL != pSock_Handle->lstSend)
					delete pSock_Handle->lstSend;
				if (INVALID_SOCKET != pSock_Handle->s)
				{
					closesocket(pSock_Handle->s);
					pSock_Handle->s = INVALID_SOCKET;
				}
				free(pSock_Handle);
				pSock_Handle = NULL;
			}
		}
	}
}

void IOCPBase::SendFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	log_printf(_T("SendFaile"));
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	free(pBuf);

	pBuf = pSock_Handle->GetNextData();
	if (pBuf)
	{
		InterlockedDecrement(&pSock_Handle->nRef);
		pBuf->pfnFailed = SendFaile;
		pBuf->pfnSuccess = SendSuccess;
		if (!postSend(pSock_Handle, pBuf))
		{
			SendFaile(pSock_Handle, pBuf);
		}
	}
	else
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			if (NULL != pSock_Handle)
			{
				DeleteCriticalSection(&pSock_Handle->cs);
				if (NULL != pSock_Handle->lstSend)
					delete pSock_Handle->lstSend;
				if (INVALID_SOCKET != pSock_Handle->s)
				{
					closesocket(pSock_Handle->s);
					pSock_Handle->s = INVALID_SOCKET;
				}
				free(pSock_Handle);
				pSock_Handle = NULL;
			}
		}
	}
}

BOOL IOCPBase::SU_postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	log_printf(_T("SU_postZeroRecv"));
	DWORD dwBytes = 0;
	DWORD dwFlags = 0;

	_pSock_Handle->wsabuf[0].buf = NULL;
	_pSock_Handle->wsabuf[0].len = 0;
	_pSock_Handle->wsabuf[1].buf = NULL;
	_pSock_Handle->wsabuf[1].len = 0;

	_pBuf->pfnFailed = SU_ZeroRecvFaile;
	_pBuf->pfnSuccess = SU_ZeroRecvSuccess;

	if (SOCKET_ERROR == WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			log_printf(_T("接收客户端SU送数据失败（0字节）:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::SU_ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	log_printf(_T("SU_ZeroRecvSuccess"));

	if (!SU_postRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(NULL, pBuf);
			ReConnect_PostEventMessage();
			return;// 重新连接
		}
		free(pBuf);
	}
}

void IOCPBase::SU_ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	log_printf(_T("SU_ZeroRecvFaile"));
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		ClearSingleData(NULL, pBuf);
		ReConnect_PostEventMessage();
		return;// 重启连接
	}
	free(pBuf);
}

BOOL IOCPBase::SU_postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	DWORD dwBytes = 0;
	DWORD dwFlags = 0;

	_pSock_Handle->InitWSABUFS();

	_pBuf->pfnFailed = SU_RecvFaile;
	_pBuf->pfnSuccess = SU_RecvSuccess;

	int r = WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL);
	if (SOCKET_ERROR == r)
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			log_printf(_T("接收户SU发送的数据失败:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::SU_RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	if (_dwTranstion <= 0)
		return SU_RecvFaile(_pSock_Handle, _pBuf);

	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	log_printf(_T("SU_RecvSuccess:%d"), _dwTranstion);

	pSock_Handle->InitWRpos(_dwTranstion);
	DWORD len = pSock_Handle->GetCmdDataLength();
	while (len)
	{
		PSock_Buf workBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
		workBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
		pSock_Handle->Read(workBuf->data, len);
		workBuf->dwRecvedCount = len;
		workBuf->pRelateSockHandle = pSock_Handle;
		pSock_Handle->AddRef();
		workBuf->pfnFailed = SU_DoWorkProcessFaile;	// 处理业务的过程
		workBuf->pfnSuccess = DoWorkProcessSuccess;
		if (!PostQueuedCompletionStatus(m_hIocp, len, (ULONG_PTR)workBuf, &workBuf->ol))
		{
			log_printf(_T("业务投递错误:%d"), WSAGetLastError());
			pSock_Handle->DecRef();
			free(workBuf);
		}
		len = pSock_Handle->GetCmdDataLength();
	}

	if (!SU_postZeroRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(NULL, pBuf);
			ReConnect_PostEventMessage();
			return;
		}
		free(pBuf);
	}
}

void IOCPBase::SU_RecvFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	log_printf(_T("SU_RecvFaile"));
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		ClearSingleData(NULL, pBuf);
		ReConnect_PostEventMessage();
		return;// 重新连接
	}
	free(pBuf);
}

void IOCPBase::DoWorkProcessSuccess(DWORD _dwTranstion, PVOID _pBuf, PVOID pBuf_)
{
	PSock_Buf pBuf = (PSock_Buf)pBuf_;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;
	log_printf(_T("DoWorkProcessSuccess"));

	msgpack::unpacker upk;
	upk.reserve_buffer(pBuf->dwRecvedCount - 2);
	memcpy_s(upk.buffer(), pBuf->dwRecvedCount - 2, pBuf->data, pBuf->dwRecvedCount - 2);
	upk.buffer_consumed(pBuf->dwRecvedCount - 2);
	msgpack::object_handle oh;
	mystring str;
	while (upk.next(oh))
	{
		str = oh.get().as<mystring>();
		log_printf(_T("接收到的数据：%s"), str.c_str());
	}
	
	_stprintf_s(pBuf->data, pBuf->datalen - 1, _T("RetrunData:%s"), str.c_str());
	pBuf->dwRecvedCount = _tcslen(pBuf->data);
	log_printf(_T("接收完成"));

	Send_PostEventMessage(pBuf->data, pBuf->dwRecvedCount);

	if (pSock_Handle->CheckSend(pBuf))
	{
		if (!postSend(pSock_Handle, pBuf))
		{
			return SendFaile(pSock_Handle, pBuf);
		}
	}
	else
	{
		log_printf(_T("CheckSend:FALSE"));
	}
}

void IOCPBase::DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_)
{
	PSock_Buf pBuf = (PSock_Buf)pBuf_;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		if (NULL != pSock_Handle)
		{
			DeleteCriticalSection(&pSock_Handle->cs);
			if (NULL != pSock_Handle->lstSend)
				delete pSock_Handle->lstSend;
			if (INVALID_SOCKET != pSock_Handle->s)
			{
				closesocket(pSock_Handle->s);
				pSock_Handle->s = INVALID_SOCKET;
			}
			free(pSock_Handle);
			pSock_Handle = NULL;
		}
	}
	free(pBuf);
}

void IOCPBase::SU_DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_)
{
	log_printf(_T("SU_DoWorkProcessFaile:这里的错误应该和业务对应的socket没有关系，不需要重新连接服务器"));
	PSock_Buf pBuf = (PSock_Buf)pBuf_;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		// 重新连接
		//ReConnect_PostEventMessage();
	}
	free(pBuf);
}

void IOCPBase::FD_Connect()
{
	log_printf(_T("FD_Connect"));
}

void IOCPBase::FD_Read()
{
	log_printf(_T("FD_Read"));
	TCHAR buf[1024] = { 0 };
	recv(m_pSUnitHandle->s, buf, 1024, 0);
	log_printf(_T("%s"), buf);
	//PostQueuedCompletionStatus(m_hIocp, 0, NULL, NULL);
}

void IOCPBase::FD_Write()
{
	m_pCBufRing->readData(m_pSUnitHandle->s);
	/*log_printf(_T("FD_Write"));
	const TCHAR* psend = _T("TEST");
	if (SOCKET_ERROR == send(m_pSUnitHandle->s, psend, _tcslen(psend), 0))
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			log_printf(_T("发送数据失败:%d"), WSAGetLastError());
			return;
		}
	}*/
}

void IOCPBase::FD_Close()
{
	log_printf(_T("FD_Close"));
}

void IOCPBase::Fuc_SUnit(DWORD _dwIndex)
{
	WSANETWORKEVENTS networkEvents;
	if (SOCKET_ERROR == WSAEnumNetworkEvents(g_sockConnect[_dwIndex], g_evtConnect[_dwIndex], &networkEvents))
	{
		log_printf(_T("检测触发事件失败:%d"), WSAGetLastError());
		return;
	}
	if (networkEvents.lNetworkEvents & FD_READ)
	{
		if (networkEvents.iErrorCode[FD_READ_BIT])
		{
			log_printf(_T("接收服务器数据失败:%d"), WSAGetLastError());
			return;
		}
		else
		{
			FD_Read();
		}
	}
	else if (networkEvents.lNetworkEvents & FD_WRITE)
	{
		if (networkEvents.iErrorCode[FD_WRITE_BIT])
		{
			log_printf(_T("向服务器发送数据失败:%d"), WSAGetLastError());
			return;
		}
		else
		{
			FD_Write();
		}
	}
	else if (networkEvents.lNetworkEvents & FD_CONNECT)
	{
		if (networkEvents.iErrorCode[FD_CONNECT_BIT])
		{
			log_printf(_T("连接服务器失败:%d"), WSAGetLastError());
			return;
		}
		else // 处理连接
		{
			FD_Connect();
		}
	}
	else/* if (networkEvents.lNetworkEvents & FD_CLOSE)*/
	{
		if (networkEvents.iErrorCode[FD_CLOSE_BIT])
		{
			log_printf(_T("关闭服务器连接失败:%d"), WSAGetLastError());
			return;
		}
		else
		{
			FD_Close();
		}
	}
}

void IOCPBase::Send_PostEventMessage(TCHAR* _buf, DWORD _bufsize)
{
	log_printf(_T("Send_PostEventMessage"));
	m_pCBufRing->writeData(_buf, _bufsize);
	WSASetEvent(g_evtSend);
}

void IOCPBase::ReConnect_PostEventMessage()
{
	log_printf(_T("ReConnect_PostEventMessage"));
	WSASetEvent(g_evtReConnect);
}

void IOCPBase::Fuc_Send(DWORD _dwIndex)
{
	log_printf(_T("Fuc_Send"));
	m_pCBufRing->readData(m_pSUnitHandle->s);
	/*log_printf(_T("Fuc_Send"));
	const TCHAR* psend = _T("TEST");
	if (SOCKET_ERROR == send(m_pSUnitHandle->s, psend, _tcslen(psend), 0))
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			log_printf(_T("发送数据失败:%d"), WSAGetLastError());
			return;
		}
	}*/
}

void IOCPBase::Fuc_ReConnect(DWORD _dwIndex)
{
	closesocket(m_pSUnitHandle->s);
	m_pSUnitHandle->s = INVALID_SOCKET;
	m_pCBufRing->Init(m_dwPagesize * PAGE_NUMS * 2);
	m_pSUnitHandle->Init(m_dwPagesize * PAGE_NUMS - SOCK_HANDLE_T_SIZE);
	m_pSUnitHandle->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (NULL == CreateIoCompletionPort((HANDLE)m_pSUnitHandle->s, m_hIocp, (ULONG_PTR)m_pSUnitHandle, 0))
	{
		log_printf(_T("初始化监听端口失败:%d"), WSAGetLastError());
		return;
	}

	if (SOCKET_ERROR == WSAEventSelect(m_pSUnitHandle->s, g_evtSUnit, FD_CLOSE | FD_WRITE))
	{
		log_printf(_T("连接SUnit服务失败:%d"), WSAGetLastError());
		return;
	}
	g_sockConnect[g_nConnects - 1] = m_pSUnitHandle->s;

	struct sockaddr_in addr;
	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	InetPton(AF_INET, SERVER_IP, &addr.sin_addr.s_addr);
	if (SOCKET_ERROR == connect(m_pSUnitHandle->s, (const sockaddr*)&addr, sizeof(addr))) // 如果服务器没打开会卡着
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			log_printf(_T("连接服务器失败:%d"), WSAGetLastError());
			return ;
		}
	}

	PSock_Buf pBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
	pBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
	if (!SU_postZeroRecv(m_pSUnitHandle, pBuf))
	{
		if (0 == InterlockedDecrement(&m_pSUnitHandle->nRef))
		{
			ClearSingleData(NULL, pBuf);
			return ;//  启动重连
		}
		free(pBuf);
	}
}
