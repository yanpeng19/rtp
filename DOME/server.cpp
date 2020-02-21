// main.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <string>
#include "rtp.h"

using namespace std;

void fun_server(const rtp& local,const rtp& remote)
{
	// 接收到的所有消息都回射
	while (rtp_control.get_state(local)==RTP_RIGHT&& rtp_control.get_state(remote) == RTP_RIGHT)
	{
		auto data = rtp_recv(remote);
		if (data == DATA_ERROR)
		{
			Sleep(10);
			continue;
		}
		auto seq = rtp_control.get_right_send_seq(local.get_identity(), remote.get_identity());
		rtp_data new_d(0, 0, 0, seq ,data.len, data.data, local.get_rpt_addr(), remote.get_rpt_addr());

		rtp_send(remote, new_d);
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

	rtp_addr addr("127.0.0.1", 5000);
	rtp server(addr.get_sockaddr(), RTP_RIGHT, 1);
	cout << server.get_identity() << endl;
	//rtp_run_server(server);

	rtp_data ack(0, 1, 0, 123, 0, NULL, server.get_rpt_addr(), server.get_rpt_addr());
	char buff[1500] = { 0 };
	pack_rtp_data(buff, ack, 1500);
	auto rtp_back = unpack_rtp_data(buff, 1500, server.get_rpt_addr());


	rtp client = rtp_accept(server);
	thread t(&fun_server, server,client);
	t.detach();

	string s;
	while (cin >> s)
	{
		
	}
	
	rtp_shutdown(server);


	system("pause");
}

