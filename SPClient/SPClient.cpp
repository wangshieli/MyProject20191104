// SPClient.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

BYTE csum(unsigned char *addr, int count)
{
	BYTE sum = 0;
	for (int i = 0; i < count; i++)
	{
		sum += (BYTE)addr[i];
	}
	return sum;
}

int main()
{
	WSADATA _wsadata;
	if (0 != WSAStartup(MAKEWORD(2, 2), &_wsadata))
	{
		return 0;
	}

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr;
	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(6086);
	//addr.sin_addr.s_addr = inet_addr(_T("192.168.24.168"));
	InetPton(AF_INET, _T("192.168.24.168"), &addr.sin_addr.s_addr);

	connect(sock, (const sockaddr*)&addr, sizeof(addr));
	std::cout << WSAGetLastError() << std::endl;

	msgpack::sbuffer sbuf5;
	msgpack::packer<msgpack::sbuffer> pk5(&sbuf5);
	sbuf5.write("\xfb\xfc", 6);
	pk5.pack("TestData");
	size_t nLen5 = sbuf5.size();
	unsigned char* pData = (unsigned char*)sbuf5.data();
	BYTE nSum5 = csum(pData + 6, nLen5 - 6);
	sbuf5.write("\x00", 1);
	memcpy(pData + nLen5, &nSum5, 1);
	sbuf5.write("\x0d", 1);
	nLen5 = sbuf5.size();
	nLen5 -= 8;
	memcpy(pData + 2, &nLen5, 4);
	nLen5 += 8;

	char rBuf[1024*4] = { 0 };
	//std::cout << "begin" << std::endl;
	//recv(sock, rBuf, 1024, 0);
	//std::cout << "test" << std::endl;

	send(sock, sbuf5.data(), sbuf5.size(), 0);
	//		std::cout << WSAGetLastError() << std::endl;

	std::ofstream outf(_T("Test.exe"), std::ios::binary);

	int recvlen = 0;
	int rl = 1024 * 4;
	recvlen = recv(sock, rBuf, rl, 0);
	std::cout << "test:" << recvlen << ":" << rBuf << std::endl;

	DWORD alllen = (DWORD)atoll(rBuf);
	
	recvlen = 0;
	DWORD hl = 0;
	DWORD dwStart = GetTickCount();
	do
	{
		recvlen = recv(sock, rBuf, rl, 0);
		//std::cout << recvlen << std::endl;
		outf.write(rBuf, recvlen);
		hl += recvlen;
	} while (alllen > hl);
	DWORD dwEnd = GetTickCount();
	outf.close();

	std::cout << dwEnd - dwStart << std::endl;

	_tsystem(_T("pause"));
    std::cout << "Hello World!\n"; 
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
