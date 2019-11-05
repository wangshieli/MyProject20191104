// RWFile.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <fstream>
#include <vector>

// std::ios::app;	// 以追加的方式打开文件
// std::ios::ate;	// 文件打开之后定位到文件尾，ios:app就包含此属性 
// std::ios::binary;// 以二进制方式打开文件		
// std::ios::in;	// 文件已输入的方式打开（文件数据输入到内存）
// std::ios::out;	// 文件以输出方式打开（内存数据输出到文件）
// std::ios::nocreate;// 不建立文件，文件不存在时打开失败
// std::ios::noreplace;// 不覆盖文件，文件存在时打开失败
// std::ios::trunc;	// 如果文件存在，把文件长度设为0

// 0：普通文件，打开访问
// 1：只读文件
// 2：隐含文件
// 3：系统文件

// ifstream：已输入方式打开
// ofstream：以输出方式打开
// fstream：已输入输出方式打开

// seekg()：设置度位置
// seekp()：设置写位置
// std::ios::beg: 文件开头
// std::ios::cur: 文件当前位置
// std::ios::end: 文件结尾

#ifdef UNICODE
#define myifstream std::wifstream
#define myofstream std::wofstream
#else
#define myifstream std::ifstream
#define myofstream std::ofstream
#endif // UNICODE

int main()
{
	myifstream inf(_T("SPServer.exe"), std::ios::binary);
	if (!inf)
		std::cout << "open f" << std::endl;

	myofstream outf(_T("SPServer_1.exe"), std::ios::binary);
	if (!outf)
		std::cout << "open f1" << std::endl;
	
	// 读写1
	if (0)
	{
		std::vector<TCHAR> buf((const unsigned int)inf.seekg(0, std::ios::end).tellg());
		inf.seekg(0, std::ios::beg).read(&buf[0], buf.size());

		outf.write(&buf[0], buf.size());
	}

	// 读写2
	if (1)
	{
		TCHAR buf[1024];
		while (!inf.eof())
		{
			inf.read(buf, 1024);
			outf.write(buf, inf.gcount());
		}
	}

	inf.close();
	outf.close();
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
