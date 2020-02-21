// rtp_client.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <string>
#include <Winsock2.h>
#include <sstream>
#include <vector>
#include <thread>
#include <map>
#include "rtp.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

void fun_server(const rtp& local, const rtp& remote)
{

	while (1)
	{
		auto data = rtp_recv(remote);
		if (data == DATA_ERROR)
		{
			Sleep(10);
			continue;
		}
		char temp[1500] = { 0 };
		memcpy(temp, data.data, data.len);
		cout << "recv data seq: " << data.seq << endl;
		if (data.data) cout << "data : " << temp << endl;
	}
}


int main()
{
	WSAData wsd;           //初始化信息

	//启动Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
	{
		cout << "WSAStartup Error = " << WSAGetLastError() << endl;
		return 0;
	}

	rtp client;
	cout << client.get_identity() << endl;
	SOCKET c = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	SOCKADDR_IN addr;

	addr.sin_family = 2;
	addr.sin_port = htons(5000);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	rtp_data data(0, 1, 0, 123123, 0, NULL, client.get_rpt_addr(), client.get_rpt_addr());
	char ch[1500] = { 0 };
	pack_rtp_data(ch, data, 1500);

	
	
	cout << client.get_identity() << endl;
	rtp server = rtp_connect(client, get_rtp_addr_from_addr_in(addr));
	
	thread t(&fun_server,client, server);
	t.detach();

	string s;
	while (cin >> s)
	{
		rtp_send(server, s.c_str(), s.size()+1);
		cout << "send message seq: " << rtp_control.get_right_send_seq(client,server)-1 << endl;
	}

 	rtp_shutdown(server);

	while (cin >> s)
	{
		rtp_send(server, s.c_str(), s.size() + 1);
		cout << "send message seq: " << rtp_control.get_right_send_seq(client, server) - 1 << endl;
	}
	
	return 1;
}
