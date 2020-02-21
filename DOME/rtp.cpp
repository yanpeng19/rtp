#include "rtp.h"

using namespace std;

// �ڴ˴�����Ϊ�Լ���ʵ��ip
//const auto MY_IP = inet_addr("127.0.0.1");

const unsigned long MY_IP = inet_addr("127.0.0.1");

#define DEBUG

const int ERROR_STATE = 0;
const int RTP_NEW = 20;         // Ĭ���½���״̬�������������պͷ��� ��rtp��Ϣ
const int RTP_HALF_NEW = 25;
const int RTP_SYN_SENT = 30;    // rtp_send ״̬������������������ ack ��Ϣ����������Ϣ
const int RTP_SYN_RECVED = 40;
const int RTP_RIGHT = 50;       // rtp_right �Ѿ��������ӣ�״̬�� ����������������Ϣ
const int RTP_FIN_SEND = 60;    // ��ر�״̬���Ѿ������˽��������ɽ��� ack ��Ϣ����������Ϣ
const int RTP_FIN_RECVED = 70;
const int RTP_CLOSE = 80;       // �ر�״̬�����ܺͷ�����Ϣ����ԴҲ��������

const vector<int> v_state{ RTP_NEW,RTP_SYN_SENT,RTP_SYN_RECVED,RTP_RIGHT,RTP_FIN_SEND,RTP_FIN_RECVED,RTP_CLOSE };

rtp_addr _a;
rtp_system rtp_control;
rtp RTP_ERROR(_a.get_sockaddr(), RTP_CLOSE, 0);
rtp_data DATA_ERROR;

const unsigned long long SEQ_ERROR = 0; // 0Ϊ�Ƿ�seq ��

const map<string,const rtp*> RTP_TABLE_ERROR;

const unsigned int min_rtp_data_size = sizeof(rtp_addr)*2+sizeof(bool) * 3 + sizeof(unsigned long long) + sizeof(int) + sizeof(char)*3;
const unsigned int max_rtp_data_size = 1500;

const int size_bool = sizeof(bool);
const int size_uint = sizeof(unsigned int);
const int size_uchar = sizeof(char);
const int size_ulong = sizeof(unsigned  long long);
const int size_int = sizeof(int);
const int size_char_p = sizeof(char*);
const int size_char = sizeof(char);

mutex m;
mutex m_recv_cache;
mutex m_seq;
mutex m_data;

// rtp ���
rtp::rtp() : l_or_r(1)
{
	{
		st = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		unsigned long ul = 1;
		ioctlsocket(st, FIONBIO, (unsigned long*)&ul);
		rtp_control.set_state(*this, RTP_HALF_NEW);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = MY_IP;
		addr.sin_port = htons(0);
		
		bind(st, (SOCKADDR*)&addr, sizeof(addr));

		int sin_size = sizeof(addr);
		getsockname(st, (struct sockaddr*)& addr, &sin_size);

		rtp_control.intilized_local(*this);
		rtp_control.set_state(*this, RTP_HALF_NEW);

		rtp_run_server(*this);
	}
}

rtp::rtp(const SOCKADDR_IN& a, const int& sta, bool _l_or_r) :addr(a),  l_or_r(_l_or_r)
{
	rtp_control.intilized_local(*this);
	rtp_control.set_state(*this, sta);

	if (_l_or_r)
	{
		st = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		unsigned long ul = 1;
		ioctlsocket(st, FIONBIO, (unsigned long*)&ul);
		bind(st, (SOCKADDR*)&addr, sizeof(addr));
		int sin_size = sizeof(addr);
		getsockname(st, (struct sockaddr*) & addr, &sin_size);
		rtp_run_server(*this);
	}

};

void rtp::set_state(const int& sta) const
{
	for(auto a :v_state)
		if (a == sta)
		{
			rtp_control.set_state(*this, sta);
			return;
		}
}

string rtp::get_identity() const
{
	stringstream ss;
	ss << (char*)inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port);
	return ss.str();
}

int rtp::state() const
{
	return rtp_control.get_state(*this);
}

rtp_addr rtp::get_rpt_addr() const
{

	unsigned int port = ntohs(addr.sin_port);
	unsigned char ip0 = addr.sin_addr.S_un.S_un_b.s_b1;
	unsigned char ip1 = addr.sin_addr.S_un.S_un_b.s_b2;
	unsigned char ip2 = addr.sin_addr.S_un.S_un_b.s_b3;
	unsigned char ip3 = addr.sin_addr.S_un.S_un_b.s_b4;
	return rtp_addr(ip0, ip1, ip2, ip3, port);
}

bool operator==(const rtp& r1, const rtp& r2)
{
	return r1.l_or_r == r2.l_or_r && r1.get_rpt_addr() == r2.get_rpt_addr() && r1.st == r2.st;
}

bool operator!=(const rtp& r1, const rtp& r2)
{
	return !(r1 == r2);
}

// rtp_addr ���

rtp_addr::rtp_addr(const string& ip, int _port)
{
	auto pos = ip.find('.');
	auto pos1 = ip.find('.', pos + 1);
	auto pos2 = ip.find('.', pos1 + 1);

	ip_0 = stoi(string(ip, 0, pos));
	ip_1 = stoi(string(ip.begin() + pos + 1, ip.begin() + pos1));
	ip_2 = stoi(string(ip.begin() + pos1 + 1, ip.begin() + pos2));
	ip_3 = stoi(string(ip.begin() + pos2 + 1, ip.end()));
	port = _port;
}

SOCKADDR_IN rtp_addr::get_sockaddr() const
{
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(get_ip().c_str());
	addr.sin_port = htons(get_port());
	return addr;
}

bool operator==(const rtp_addr& r1, const rtp_addr& r2)
{
	return r1.ip_0 == r2.ip_0 && r1.ip_1 == r2.ip_1 && r1.ip_2 == r2.ip_2 && r1.ip_3 == r2.ip_3 && r1.port == r2.port;
}

// rtp_data ���
rtp_data::rtp_data(const rtp_data& d) :syn(d.syn), ack(d.ack), fin(d.fin), seq(d.seq), len(d.len),sender(d.sender), recver(d.recver)
{
	if (len)
	{
		data = new char[len];
		for (int i = 0; i < len; i++)
			data[i] = d.data[i];	
	}
}

rtp_data::rtp_data(bool _syn, bool _ack, bool _fin, unsigned long long _seq,int _len, const char* _data, rtp_addr send, rtp_addr recv)
	: syn(_syn), ack(_ack), fin(_fin), seq(_seq), sender(send), recver(recv), data(NULL)
{
	len = _len;
	if (len)
	{
		data = new char[len];
		for (int i = 0; i < len; i++)
			data[i] = _data[i];
	}
}

rtp_data::~rtp_data()
{
	if (len)
	{
		memset(data, 1, len - 1);
		memset(data + len - 1, 0, 1);
		delete[] data;
	}
}


//rtp_data::~rtp_data()
//{
//	if (len)
//	{
//		memset(data, 1, len);
//		delete[] data;
//	}
//}

bool operator==(const rtp_data& d1, const rtp_data& d2)
{
	if (d1.ack == d2.ack && d1.syn == d2.syn && d1.fin == d2.fin && d1.len == d2.len 
		&& d1.sender == d2.sender && d1.recver == d2.recver && d1.len == d2.len)
	{
		for (int i = 0; i < d1.len; i++)
			if (d1.data[i] != d2.data[i]) return 0;
		return 1;
	}
	return 0;
}

// mes_list
void mes_list::push_back(const rtp_data& mes, const time_t& t)
{
	m_data.lock();
	mes_list.push_back(mes);
	time_list.push_back(t);
	s += mes.size();
	m_data.unlock();
}

pair<rtp_data, time_t> mes_list::front()
{
	if (empty()) return pair<rtp_data, time_t>();
	m_data.lock();
	rtp_data mes = mes_list.front();
	time_t t = time_list.front();
	mes_list.pop_front();
	time_list.pop_front();
	m_data.unlock();
	return make_pair(mes, t);
}

deque<rtp_data> mes_list::transer_get(const size_t&i)
{
	if (i > mes_list.size())
	{
		deque<rtp_data> r(mes_list);
		mes_list.clear();
		return r;
	}
	deque<rtp_data> r(mes_list.begin(), mes_list.begin() + i);
	unsigned long long j = i;
	while (j > 0)
	{
		mes_list.pop_front();
		time_list.pop_front();
		j--;
	}
	return r;
}

// send_mes_list
void send_mes_list::push_back(const rtp_data& mes, const time_t& t)
{
	mes_list::push_back(mes, t);
	time(&timer);
	if (!countdown_run)
	{
		thread t1(&send_mes_list::countdown,this);
		t1.detach();
		countdown_run = 1;
	}
}

void send_mes_list::countdown()
{
	// ��ʱ���� ������Ʒ�����Ϊ�����0.2��δ�������� ���� ���ͻ���������5000������з���
	// ֱ�����������пɷ������ݣ��Ž���
	time_t now;
	time(&now);

	// ���������� 1.������Ϊ��  2.���ͻ�����Ϊ��  3.����seq��Ϊ��
	while (!sender.empty() || !mes_list::empty() || !sender.seq_table_empty())
	{
		// ����һ�η��͵������������ͻ�����������5000 ���� 0.2��δ�������ݣ� ���� seq��ǿ�

		// ������ݴ���5000 ���� 0.2��δ�������� ��ô����һ�η���,��ȷ��ack �������ڴ�С

		//cout << "countdown is run" << endl;
		if (mes_list::size() > 5000 || (now - timer) > 0.2)
		{
			unsigned size = (sender.get_wz() - sender.size()) > 0 ? sender.get_wz() - sender.size() : 0; // ��Ҫ���͵�������  = ���ڴ�С-�������е�����
			deque<rtp_data> temp = mes_list::transer_get(size);
			sender.transfer_data(temp);
			sender.do_send();
			// 0.2����ȷ��
			Sleep(20);
			sender.confirm_ack();
		}
		time(&now);
		Sleep(20);
	}
	countdown_run = 0;
	cout << "sender countdown_run end" << endl;
	return;
}

// mes_sender
void mes_sender::transfer_data(deque<rtp_data> d)
{
	for (auto a : d)
		list.push_back(a);
}

/*
1.���ȼ��seq���ȼ�¼���Ƚ���¼���е�seq����,���沿��
2.ѭ��Ϊ���� rtp_data û��seq�� rtp_data ע��Ϸ�seq, ���ҷ��� 
3.3����� ack_list ��ȷ�Ϲ��İ��ӷ��Ͷ����޳���δȷ�ϵİ� ��ʧ�ܴ����б�+1 ���ʧ�ܴ�������20����ô�޳�,������seq ���뵽ĳ����¼��
4. 
*/
unsigned int mes_sender::do_send()
{
	// ���͵�ʵ�ʹ���
	unsigned long long seq = 0;

	if (list.empty())
	{
		//�����������ôΪseq_table ���ݷ��Ϳհ�
		while (!seq_table.empty())
		{
			auto seq = seq_table.front();
			seq_table.pop_front();

			rtp_data data = rtp_data(0, 0, 0, seq, 0, NULL,sender_rtp.get_rpt_addr(),recver_rtp.get_rpt_addr());
			list.push_back(data);
			return do_send();
		}
	}
	else
	{
		for (size_t i = 0; i < list.size(); i++)
		{
			if (list[i].seq == SEQ_ERROR)
			{
				if (!seq_table.empty())
				{
					seq = seq_table.front();
					seq_table.pop_front();
				}
				else seq = rtp_control.get_right_send_seq(list[i].get_sender_identity(), list[i].get_recver_identity());
				list[i].seq = seq;
			}

			char temp[1500] = { 0 };
			int len = pack_rtp_data(temp, list[i], 1500);
			return sendto(sender_rtp.get_socket(), temp, len, 0, (SOCKADDR*)&list[i].recver.get_sockaddr(), sizeof(SOCKADDR));
		}
	}
	return 0;
}

void mes_sender::confirm_ack()
{
	// ȷ��ack ���ҵ������ڴ�С
	sw_ad = 0;

	for (size_t i = 0; i < list.size(); i++)
	{
		// ��鷢����ÿ����Ա���Ƿ��յ�����Ӧ��ack
		if (rtp_control.inspection_ack(list[i].get_sender_identity(), list[i].get_recver_identity(), list[i].get_seq()))
		{
			// �յ����

			// ������ɾ��������,ʧ�ܼ�¼��
			fail_times.erase(list[i].get_seq());
			list.erase(list.begin() + i);
			
			// ���ӷ��ʹ���
			if (pack_loss) send_windows_size++;
			else send_windows_size = +1 / send_windows_size;
			i--;
		}
		else
		{
			// δ�յ�
			if (!sw_ad)
			{
				sw_ad = 1;
				send_windows_size /= 2;
			}

			fail_times[list[i].get_seq()]++;
			if (fail_times[list[i].get_seq()] == 20)
			{
				//20�η���ʧ�ܣ���ӷ������޳������ݰ������ҽ�seq���뵽���ȼ�����
				fail_times[list[i].get_seq()] = 0;
				seq_table.push_back(list[i].get_seq());
				list.erase(list.begin() + i);
				i--;
			}
		}
	}
}

//  recv_mes_list
void recv_mes_list::push_back(const rtp_data& data, const time_t& t)
{
	time(&timer);
	string loc = data.recver.get_identity();
	string rem = data.sender.get_identity();

	unsigned long long right_seq = rtp_control.get_right_recv_seq(loc,rem);
	if (data.get_seq() == right_seq)
	{
		// seq��ȷ ֱ�Ӵ洢
		mes_list::push_back(data, t);
		rtp_control.add_right_recv_seq(loc, rem);
		right_seq++;
		while (recv_cache_list.find(right_seq) != recv_cache_list.end())
		{
			auto p = recv_cache_list[right_seq];
			recv_cache_list.erase(right_seq);
			rtp_control.add_right_recv_seq(loc, rem);
			right_seq++;
			mes_list::push_back(p.first, p.second);
		}
	}
	else
	{
		// seq ����������ʱ�������Ҵ��뻺������
		m_recv_cache.lock();
		push_back_to_chache(data,t);
		m_recv_cache.unlock();

		if (!countdown_run)
		{
			countdown_run = 1;
			thread t(&recv_mes_list::countdown, this);
			t.detach();
		}
	}
}

void recv_mes_list::push_back_to_chache(const rtp_data& data, const time_t& t)
{
	pair<rtp_data, time_t> p = make_pair(data, t);
	unsigned long long seq = data.get_seq();
	recv_cache_list[seq] = p;
}

void  recv_mes_list::countdown()
{
	// ��ʱ���������60��δ�յ����ݣ�����ս��ܻ�����,�������գ��������񼴿ɽ���
	time_t now;
	
	while (!recv_cache_list.empty())
	{
		time(&now);
		if ((now - timer) > 60)
		{
			m_recv_cache.lock();
			recv_cache_list.clear();
			m_recv_cache.unlock();
			countdown_run = 0;
		}
	}
}

// a_list
pair<unsigned long long, time_t> a_list::front()
{
	m_data.lock();
	if (empty()) return pair<unsigned long long , time_t>();

#ifdef DEBUG
	if (time_list.empty())
	{
		cout << "time_list is empty,bug seq_list is not" << endl;
		cout << time_list.size() << endl;
	}
#endif


	unsigned long long i = seq_list.front();
	time_t t = time_list.front();
	seq_list.pop_front();
	time_list.pop_front();
	m_data.unlock();
	return make_pair(i, t);
}

// rtp_system ���
void rtp_system::intilized_local(const rtp &server)
{
	//������Դ��������socket
	string ser = server.get_identity();
	recv_list[ser];
	send_list[ser];
	ack_list[ser];
	connect_list[ser]=new mes_list;
	seq_send_table[ser];
	seq_recv_table[ser];
	syn_fin_list[ser];
	local_rtp_table[ser];
	state_table[ser];
}

int rtp_system::intilized_remote(const rtp &server,const rtp &client,const unsigned long long &send_seq,const  unsigned long long&recv_seq)
{
	// ����Դ����Ӧ��
	if (recv_list.find(server.get_identity()) == recv_list.end()) return -1;

	// �Ѿ�������� ��Ҫ�ͷ���Դ
	if (recv_list[server.get_identity()].find(client.get_identity()) != recv_list[server.get_identity()].end())
		free_remote(server,client);
	
	string ser = server.get_identity();
	string cli = client.get_identity();

	// �ڷ������з���Ը� �ͻ��˵���Դ
	recv_list[ser][cli] = new recv_mes_list;
	send_list[ser][cli] = new send_mes_list(server,client);
	ack_list[ser][cli] = new a_list;
	syn_fin_list[ser][cli] = new mes_list;
	seq_send_table[ser][cli] = send_seq;
	seq_recv_table[ser][cli] = recv_seq;
	sender_table[cli] = &server;
	local_rtp_table[ser][cli] = new rtp(client);
	state_table[cli];

	return 1;
}

void rtp_system::free_remote(const rtp &server,const rtp& client)
{
	// �ͷ�һ���ͻ��˵���Դ
	m.lock();
	string ser = server.get_identity();
	string cli = client.get_identity();

	delete recv_list[ser][cli];
	delete send_list[ser][cli];
	delete ack_list[ser][cli];
	delete syn_fin_list[ser][cli];
	delete local_rtp_table[ser][cli];

	recv_list[ser][cli] = NULL;
	send_list[ser][cli] = NULL;
	ack_list[ser][cli] = NULL;

	recv_list[ser].erase(cli);
	send_list[ser].erase(cli);
	ack_list[ser].erase(cli);
	syn_fin_list[ser].erase(cli);
	seq_send_table[ser].erase(cli);
	seq_recv_table[ser].erase(cli);
	sender_table.erase(cli);
	local_rtp_table[ser].erase(cli);
	state_table.erase(client.get_identity());
	m.unlock();
}

void rtp_system::free_local(const rtp &server)
{
	// �ͷŶ˿ں���Դ
	closesocket(server.st);

	string ser = server.get_identity();
	while (!recv_list[ser].empty())
	{
		auto temp = recv_list[ser].begin();
		delete temp->second;
		//recv_list[ser].erase(temp);
	}
	recv_list.erase(ser);
	while (!send_list[ser].empty())
	{
		auto temp = send_list[ser].begin();
		delete temp->second;
		//send_list[ser].erase(temp);
	}
	send_list.erase(ser);
	while (!ack_list[ser].empty())
	{
		auto temp = ack_list[ser].begin();
		delete temp->second;
		//ack_list[ser].erase(temp);
	}
	ack_list.erase(ser);
	while (!syn_fin_list[ser].empty())
	{
		auto temp = syn_fin_list[ser].begin();
		delete temp->second;
		//syn_fin_list[ser].erase(temp);
	}
	syn_fin_list.erase(ser);

	delete connect_list[ser];
	connect_list.erase(ser);

	seq_send_table.erase(ser);
	seq_recv_table.erase(ser);

	local_rtp_table.erase(ser);
	state_table.erase(ser);
}

bool rtp_system::is_intilized_local(const string& local) const
{
	auto it = connect_list.find(local);
	if (it == connect_list.end()) return 0;
	return 1;
}

bool rtp_system::is_intilized_local(const rtp& server) const
{
	return is_intilized_local(server.get_identity());
}

bool rtp_system::is_intilized_remote(const string& loc, const string& rem)
{
	if (!is_intilized_local(loc)) return 0;
	if (ack_list[loc].find(rem) == ack_list[loc].end()) return 0;
	else return 1;
}

bool rtp_system::is_intilized_remote(const rtp& local, const rtp& remote)
{
	return is_intilized_remote(local.get_identity(), remote.get_identity());
}

bool rtp_system::is_intilized_remote(const rtp& local, const rtp_data& data)
{
	if (!is_intilized_local(local)) return 0;
	string ser = local.get_identity();
	string cli = data.sender.get_identity();

	if (ack_list[ser].find(cli) == ack_list[ser].end()) return 0;
	else return 1;
}

bool rtp_system::connect_list_empty(const rtp& server)
{
	if (!is_intilized_local(server)) return 0;
	return connect_list[server.get_identity()]->empty();
}

pair<rtp, unsigned long long> rtp_system::get_rtp_from_connce_list(const rtp &server)
{
	// �ӷ�������������Ϣ�������ݣ�����һ���Ϸ�����ʱ rtp ������������
	if (connect_list_empty(server)) return make_pair(RTP_ERROR,0);

	string ser = server.get_identity();
	mes_list* ms = connect_list[ser];
	if (ms->empty()) return make_pair(RTP_ERROR, 0);

	bool f = 0;
	rtp ender = RTP_ERROR;
	unsigned long long send_seq = SEQ_ERROR;

	m.lock();
	time_t now;
	time(&now);
	pair<rtp_data, time_t> p = ms->front();
	rtp_data data = p.first;
	time_t t = p.second;
	
	f = (t - now) < 15;
	if (f)
	{
		SOCKADDR_IN sender = data.sender.get_sockaddr();
		ender = rtp(sender,RTP_NEW,0);
		// ��һ��������seq �����´η�����Ϣʹ�õ� seq
		send_seq = data.get_seq();
	}
	while (!f && !ms->empty())
	{
		p = ms->front();
		data = p.first;
		time_t t = p.second;
		f = (t - now) < 15;
		if (f)
		{
			SOCKADDR_IN sender = data.sender.get_sockaddr();
			ender = rtp(sender, RTP_NEW, 0);
			send_seq = data.get_seq();
		}
	}
	m.unlock();
	if (f) return make_pair(ender,send_seq);
	return make_pair(RTP_ERROR, 0);
}

bool rtp_system::inspection_ack(const rtp& server, const rtp& client,const unsigned long long & right_seq)
{
	if (!rtp_control.is_intilized_local(server)) return 0;
	string ser = server.get_identity();
	string cli = client.get_identity();

	if (ack_list[ser].find(cli) == ack_list[ser].end()) return 0;

	time_t begin, now;
	time(&begin);
	time(&now);

	//unsigned long long right_seq = rtp_control.get_right_recv_seq(server, client);

	// ��ʱʱ�� 3��
	while ((now - begin) < 3)
	{
		time(&now);
		auto cl = ack_list[ser][cli];
		if (cl->empty())
		{
			Sleep(10);
			continue;
		}
		pair<unsigned long long, time_t> p = cl->front();
		time_t t = p.second;
		unsigned long long seq = p.first;

		time(&now);
		if (now - t < 15 && seq == right_seq)
		{
			/*if (seq_send_table[ser][cli] == ULLONG_MAX) seq_send_table[ser][cli] = 0;
			else seq_send_table[ser][cli]++;*/
			/*if (seq_recv_table[ser][cli] == ULLONG_MAX) seq_recv_table[ser][cli] = 0;
			else seq_recv_table[ser][cli]++;*/

			return 1;
		}
	}
	return 0;
}

bool rtp_system::inspection_ack(const string& ser, const string& cli, const unsigned long long& right_seq)
{
	if (ack_list[ser].find(cli) == ack_list[ser].end()) return 0;

	time_t begin, now;
	time(&begin);
	time(&now);

	// ��ʱʱ�� 3��
	while ((now - begin) < 3)
	{
		time(&now);
		auto cl = ack_list[ser][cli];
		if (cl->empty()) continue;
		m.lock();
		pair<unsigned long long, time_t> p = cl->front();
		m.unlock();
		time_t t = p.second;
		unsigned long long seq = p.first;

		time(&now);
		if (now - t < 15 && seq == right_seq)
		{
			/*if (seq_send_table[ser][cli] == ULLONG_MAX) seq_send_table[ser][cli] = 0;
			else seq_send_table[ser][cli]++;*/
			/*if (seq_recv_table[ser][cli] == ULLONG_MAX) seq_recv_table[ser][cli] = 0;
			else seq_recv_table[ser][cli]++;*/

			return 1;
		}
		
	}
	return 0;
}

unsigned long long rtp_system::client_inspection_syn(const rtp& local, const rtp& remote)
 {
	if (remote.state() != RTP_SYN_SENT || recv_list.find(local.get_identity()) == recv_list.end() ||
		recv_list[local.get_identity()].find(remote.get_identity()) == recv_list[local.get_identity()].end())
	
		return SEQ_ERROR;

	string loc = local.get_identity();
	string rem = remote.get_identity();

	// �Ѿ������˵�һ��syn ��Ҫȷ�϶Է��Ƿ��� syn 
	if (syn_fin_list[loc][rem]->empty()) return SEQ_ERROR;

	m.lock();
	auto p = syn_fin_list[loc][rem]->front();
	m.unlock();
	auto data = p.first;
	char temp[1500] = { 0 };
	memcpy(temp, data.data, data.len);
	stringstream ss;
	ss << temp;
	// ��ȡ�� �Է�������seq
	return stoull(ss.str());
}

unsigned long long rtp_system::client_inspection_fin(const rtp& local, const rtp& remote)
{
	if (remote.state() != RTP_FIN_SEND || recv_list.find(local.get_identity()) == recv_list.end() ||
		recv_list[local.get_identity()].find(remote.get_identity()) == recv_list[local.get_identity()].end())
		return SEQ_ERROR;
	string loc = local.get_identity();
	string rem = remote.get_identity();

	// �Ѿ������˵�һ��syn ��Ҫȷ�϶Է��Ƿ��� syn 
	if (syn_fin_list[loc][rem]->empty()) return SEQ_ERROR;
	auto p = syn_fin_list[loc][rem]->front();
	auto data = p.first;

	if (data.len == 0)
		return 1;

	stringstream ss;
	ss << data.data;
	// ��ȡ�� �Է�������seq
	return stoull(ss.str());
}

void rtp_system::client_push_syn_fin(const rtp& local, const rtp& remote, const rtp_data& data)
{
	if (!rtp_control.is_intilized_local(local) || !rtp_control.is_intilized_remote(local, remote)) return;
	if ((local.state() == RTP_SYN_SENT && data.syn && data.ack)||
		(local.state() == RTP_FIN_SEND && data.fin && data.ack))
	{
		time_t now;
		time(&now);
		syn_fin_list[local.get_identity()][remote.get_identity()]->push_back(data,now);
	}

	return;
}

void rtp_system::client_push_syn_fin(const string& local, const string& remote, const rtp_data& data)
{
	if (!rtp_control.is_intilized_local(local) || !rtp_control.is_intilized_remote(local, remote)) return;
	auto m = get_local_rtp_table(local);
	auto rtp_it = m.find(remote);
	if (rtp_it == m.end()) return;
	
	auto rtp_remote = rtp_it->second;
	auto rtp_local = get_send_rtp(remote);

	if ((rtp_remote->state() == RTP_SYN_SENT && data.syn && data.ack) ||
		(rtp_remote->state() == RTP_FIN_SEND && data.fin))
	{
		time_t now;
		time(&now);
		syn_fin_list[local][remote]->push_back(data, now);
	}

	return;
}

void rtp_system::clear_syn_fin(const rtp& local, const rtp& remote)
{
	if (is_intilized_local(local) && is_intilized_remote(local, remote))
		syn_fin_list[local.get_identity()][remote.get_identity()]->clear();
}

unsigned long long rtp_system::get_right_recv_seq(const string& loc, const string& rem)
{
	if (!rtp_control.is_intilized_local(loc)) return SEQ_ERROR;

	if (seq_recv_table[loc].find(rem) == seq_recv_table[loc].end()) return SEQ_ERROR;
	return seq_recv_table[loc][rem];
}

unsigned long long rtp_system::get_right_recv_seq(const rtp& server, const rtp& client)
{
	return get_right_recv_seq(server.get_identity(), client.get_identity());
}

void rtp_system::add_right_recv_seq(const string& loc, const string& rem)
{
	if (seq_recv_table.find(loc) == seq_recv_table.end() ||
		seq_recv_table[loc].find(rem) == seq_recv_table[loc].end()) return;
	seq_recv_table[loc][rem]++;

	if (seq_recv_table[loc][rem] == 0) seq_recv_table[loc][rem]++;
}

void rtp_system::add_right_send_seq(const string& loc, const string& rem)
{
	if (seq_send_table.find(loc) == seq_send_table.end() ||
		seq_send_table[loc].find(rem) == seq_send_table[loc].end()) return;
	seq_send_table[loc][rem]++;

	if (seq_send_table[loc][rem] == 0) seq_send_table[loc][rem]++;
}

unsigned long long rtp_system::get_right_send_seq(const string &loc, const string &rem )
{
	if (seq_send_table.find(loc) == seq_send_table.end() || seq_send_table[loc].find(rem) == seq_send_table[loc].end())
		return SEQ_ERROR;
	else return seq_send_table[loc][rem];
}

void rtp_system::set_send_seq(const string& loc, const string& rem, const unsigned long long seq)
{
	if (is_intilized_local(loc) && is_intilized_remote(loc, rem)&&seq!=SEQ_ERROR)
		seq_send_table[loc][rem] = seq;
	
}

void rtp_system::set_send_seq(const rtp& local, const rtp& remote, const unsigned long long seq)
{
	set_send_seq(local.get_identity(), remote.get_identity(), seq);
}

void rtp_system::set_recv_seq(const string& loc, const string& rem, const unsigned long long seq)
{
	if (is_intilized_local(loc) && is_intilized_remote(loc, rem) && seq != SEQ_ERROR)
		seq_recv_table[loc][rem] = seq;
}

void rtp_system::set_recv_seq(const rtp& local, const rtp& remot, const unsigned long long seq)
{
	set_recv_seq(local.get_identity(),remot.get_identity(),seq);
}

void rtp_system::set_state(const rtp& ender, const int& sta)
{
	if ( find(v_state.begin(), v_state.end(), sta) == v_state.end()) return;
	auto p = state_table.find(ender.get_identity());
	if (p == state_table.end()) return;
	state_table[ender.get_identity()] = sta;
}

int rtp_system::get_state(const rtp& ender)
{
	if (state_table.find(ender.get_identity()) == state_table.end()) return ERROR_STATE;
	else return state_table[ender.get_identity()];
}

int rtp_system::get_state(const string& id)
{
	if (state_table.find(id) == state_table.end()) return ERROR_STATE;
	return state_table[id];
}

unsigned long long rtp_system::get_right_send_seq(const rtp& local, const rtp& remote)
{
	return get_right_send_seq(local.get_identity(), remote.get_identity());
}

void rtp_system::connect_list_push(const rtp& server, const rtp_data& data)
{
	m.lock();
	if (!is_intilized_local(server))
	{
		m.unlock();
		return;
	}
	time_t now;
	time(&now);
	connect_list[server.get_identity()]->push_back(data, now);
	m.unlock();
	return;
}

void rtp_system::ack_list_push(const rtp& server, const rtp_data& data)
{
	m.lock();
	if (!is_intilized_local(server) || !is_intilized_remote(server, data))
	{
		m.unlock();
		return;
	}

	time_t now;
	time(&now);
	unsigned long long seq = data.get_seq();

	ack_list[server.get_identity()][data.get_sender_identity()]->push_back(seq, now);
	m.unlock();
	return;
}

void rtp_system::recv_list_push(const rtp& server, const rtp_data& data)
{
	m.lock();
	if (!is_intilized_local(server) || !is_intilized_remote(server, data))
	{
		m.unlock();
		return;
	}
	time_t now;
	time(&now);
	string ser = server.get_identity();
	string cli = data.get_sender_identity();
	unsigned long long right_seq = get_right_recv_seq(ser,cli);

	// seq ��ȷ����·���ack���ģ�������������κ�����
	if (data.get_seq() == right_seq)
	{
		// �˲������벢��������ȷ����seq
		recv_list[ser][cli]->push_back(data, now);

		rtp_data ack(0, 1, 0, data.get_seq(), 0, NULL, server.get_rpt_addr(), data.sender);
		char buff[1500] = { 0 };
		pack_rtp_data(buff, ack, 1500);

		if (sendto(server.get_socket(), buff, data.size(), 0, (SOCKADDR*)&data.sender.get_sockaddr(), sizeof(SOCKADDR)) == -1)
			cout << "send to error " << errno << endl;
		
	}
	m.unlock();
	return;
}

const rtp* rtp_system::get_send_rtp(const string& s) const
{
	if (sender_table.find(s) == sender_table.end()) return NULL;
	auto p = sender_table.find(s);
	if (p == sender_table.end()) return NULL;
	return p->second;
}

int rtp_system::send_mes(const rtp& remote, const rtp_data& data)
{
	auto local = get_send_rtp(remote.get_identity());
	if (local)
	{
		time_t now;
		time(&now);
		send_list[local->get_identity()][remote.get_identity()]->push_back(data, now);
		m_seq.lock();
		rtp_control.add_right_send_seq(local->get_identity(), remote.get_identity());
		m_seq.unlock();
		return data.size();
	}
	return -1;
}

rtp_data rtp_system::recv_mes(const string& loc,const string& rem)
{
	if (!is_intilized_local(loc) || !is_intilized_remote(loc, rem))
		return DATA_ERROR;
	time_t begin, now;
	time(&begin);
	time(&now);

	while ((now - begin) < 15)
	{
		if (recv_list.empty())
		{
			time(&now);
			continue;
		}
		auto p = recv_list[loc][rem]->front();
		return p.first;
	}
	return DATA_ERROR;
}

rtp_data rtp_system::recv_mes(const rtp& local, const rtp& remote)
{
	return recv_mes(local.get_identity(), remote.get_identity());
}

map<string,const rtp*> rtp_system::get_local_rtp_table(const rtp& local) const
{
	if (!is_intilized_local(local)) return RTP_TABLE_ERROR;
	else return local_rtp_table.at(local.get_identity());
}

map<string, const rtp*> rtp_system::get_local_rtp_table(const string& local) const
{
	if (!is_intilized_local(local)) return RTP_TABLE_ERROR;
	else return local_rtp_table.at(local);
}

bool rtp_system::local_rtp_table_empty(const rtp& local) const
{
	if (!is_intilized_local(local)) return 1;
	else if (local_rtp_table.empty()) return 1;
	else return 0;
}

const rtp* rtp_system::get_rtp_by_data(const rtp& local, const rtp_data& data) const
{
	if (!is_intilized_local(local)) return NULL;

	auto p = local_rtp_table.find(local.get_identity());
	auto r = p->second.find(data.get_sender_identity());
	if (r == p->second.end()) return NULL;
	return r->second;
}

rtp rtp_accept(const rtp& server)
{
	if (!rtp_control.is_intilized_local(server)) return RTP_ERROR;

	// 15�볬ʱ
	// ��������Ϣ���л�ȡ��Ϣ��������������֤
	string ser = server.get_identity();
	time_t begin, end;
	time(&begin);
	time(&end);

	char buffer[1500] = { 0 };
	while ((end - begin) < 15)
	{
		if (!rtp_control.connect_list_empty(server))
		{
			return local_three_handshake(server);

		}
		Sleep(10);
		time(&end);
	}
return RTP_ERROR;
}

// ȫ�ֺ���
void rtp_run_server(const rtp& server)
{
	thread t(&rtp_run,server);
	t.detach();
}

void rtp_run(const rtp &server)
{
	// ִ�м��������ܷ�����server����Ϣ�����ҽ���ʶ�𣬹��ൽ��Ӧ����
	if (server == RTP_ERROR || server.state() == RTP_NEW || server.state() == RTP_CLOSE)
	{
		// ������
		if (server.l_or_r && !rtp_control.is_intilized_local(server))
		{
			cout << "local rtp :" << server.get_identity() << "run error" << endl;
			return;
		}
		else if (!server.l_or_r)
		{
			// �ǿͻ���
			auto remote = rtp_control.get_send_rtp(server.get_identity());
			if (*remote == RTP_ERROR)
			{
				cout << "remote rtp :" << server.get_identity() << "run error" << endl;
				return;
			}
			if (!rtp_control.is_intilized_remote(*remote, server))
			{
				cout << "remote rtp :" << server.get_identity() << "run error" << endl;
				return;
			}
		}
	}
	cout << server.get_identity()<< "server run" << endl;

	char buff[1500] = { 0 };
	int n = sizeof(struct sockaddr_in);
	
	//cout << server.state();
	while(server.state()!=RTP_NEW&&server.state()!=RTP_CLOSE)
	{
		char buff[1500] = { 0 };
		SOCKADDR_IN client;
  		int len = recvfrom(server.st, buff, 1500, 0, (SOCKADDR*)&client, &n);
		
		if (len == -1)
		{
			Sleep(10);
			continue;
		}
		rtp_data mes = unpack_rtp_data(buff, len,get_rtp_addr_from_addr_in(client));  // �˲������rpt_data ���Ϸ��Լ��
		if (mes == DATA_ERROR)
		{
			Sleep(10);
			continue;
		}

		m.lock();
		cout << "get mes from: " << (char*)inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port) << " is a ";
		if (mes.syn && mes.ack) cout << "syn&&ack message" << endl;
		else if (mes.syn && mes.fin) cout << "syn&&fin message" << endl;
		else if (mes.syn) cout << "syn message" << endl;
		else if (mes.ack) cout << "ack message" << endl;
		else if (mes.fin) cout << "fin message" << endl;
		else cout << "normol message" << endl;
		m.unlock();
		mes.sender = get_rtp_addr_from_addr_in(client);

		if (mes.ack)
		{
			if (mes.syn||mes.fin)    //�ڶ�������ack ��Ϣ  ack&&fin  ||  ack&&syn
			{
				auto remote_id = mes.get_sender_identity();
				rtp_control.client_push_syn_fin(server.get_identity(),remote_id,mes);
			}
			else rtp_control.ack_list_push(server, mes);   // ��ͨ ack ��Ϣ
		}
		else if (mes.fin)
		{
			auto remote_id = mes.sender.get_identity();
			if (!rtp_control.is_intilized_remote(server.get_identity(), remote_id))
				continue;
			
			if(rtp_control.get_state(remote_id)==RTP_FIN_SEND) // 4�������еĵڶ�����Ϣ
				rtp_control.client_push_syn_fin(server.get_identity(), remote_id, mes);
			else local_four_handshake(server, mes);  // 4�������еĵ�һ��
		}
		else if (mes.syn) rtp_control.connect_list_push(server, mes); // syn ��Ϣ
		else rtp_control.recv_list_push(server, mes);  // ��ͨ��Ϣ
		
	}
	return;
}

rtp local_three_handshake(const rtp &server)
{

	// ��������3������ʵ�ʲ���

	// ������ֶ����Ƿ��� �Ϸ������ֶ���
	// �������֮������ֲ���-> ������Ϣ -> ȷ��ack 

	// rtp ������ ˦��һ�� rtp
	auto r = rtp_control.get_rtp_from_connce_list(server);
	rtp* r_temp = new rtp(r.first);

	if (*r_temp == RTP_ERROR) return RTP_ERROR;

	// ��ʱrtp �Ϸ�,Ϊ�������Դ�����ҽ������ֶ���
	time_t now;
	time(&now);
	srand(now);

	time_t syn_time;
	time(&syn_time);

	unsigned long long send_seq = r.second;
	unsigned long long recv_seq = now%rand();
	//������Դ
	rtp_control.intilized_remote(server,*r_temp,send_seq,recv_seq);
	r_temp->set_state(RTP_SYN_SENT);

	stringstream ss;
	ss << recv_seq;
	string recv_num = ss.str();
	char seq_buff[1500] = { 0 };
	for (size_t i = 0; i < recv_num.size(); i++)
		seq_buff[i] = recv_num[i]; 

	rtp_data mes(1,1, 0, send_seq, recv_num.size(), seq_buff, rtp_addr(), r_temp->get_rpt_addr());      // ����������һ�� ���н������ �ĵڶ������� ����
	char buff[1500] = { 0 };

	pack_rtp_data(buff, mes, 1500);
	SOCKADDR_IN addr = r_temp->get_rpt_addr().get_sockaddr();
	// ����10�볬ʱ
	while ((now - syn_time) < 10)
	{
		int i = sendto(server.get_socket(), buff, mes.size(), 0, (SOCKADDR*)&addr, sizeof(SOCKADDR));
		if (!rtp_control.inspection_ack(server, *r_temp, send_seq))
		{
			Sleep(1000);// Ȼ����ack ���У��Ƿ�Է��Եڶ��α��Ľ�����ȷ��,1��һ��
			time(&now);
			continue;
		}
		// ״̬���������ҷ���
		r_temp->set_state(RTP_RIGHT);
		rtp_control.clear_syn_fin(server,*r_temp);
		auto r = *r_temp;
		delete r_temp;
		return r;
	}

	// ʧ�����ͷ���Դ
	rtp_control.free_remote(server, *r_temp);
	delete r_temp;
	return RTP_ERROR;
}

rtp remote_three_handshake(const rtp& local,const rtp_addr& addr)
{
	// �ͻ��������������3���������󣬳�ʱʱ��10��

	// ��ʱ-> ���� syn -> ���syn -> ����ack -> ����rtp
	time_t begin, now;
	time(&begin);
	time(&now);
	srand(begin);

	// 
	unsigned long long send_seq = SEQ_ERROR;  // �Ժ���Ҫ�ӵڶ��α����л�ȡ���Ҹ���
	unsigned long long recv_seq = rand();

	rtp server(addr.get_sockaddr(), RTP_NEW, 0);
	rtp_control.intilized_remote(local, server, send_seq, recv_seq);

	// ����һ�� syn ���ݱ�������seq ��ϣ���յ����´�ϣ���յ���seq
	rtp_data syn_data(1, 0, 0, recv_seq, 0,NULL, local.get_rpt_addr(), addr); 

	char buff[1500] = { 0 };
	int len = pack_rtp_data(buff, syn_data, syn_data.size());

	/*struct sockaddr_in a;
	a = addr.get_sockaddr();
	cout << (char*)inet_ntoa(a.sin_addr) << ":" << htons(a.sin_port);*/
	int end_time = 1;

	while ((now - begin) < 10&&(now-begin)<end_time)
	{
		int i = sendto(local.get_socket(),buff,len,0,(SOCKADDR*)&(addr.get_sockaddr()), sizeof(SOCKADDR));
		//cout << WSAGetLastError();
		server.set_state(RTP_SYN_SENT);

		end_time += 2;

		// ���syn/fin list ����кϷ���syn �����ȡseq�����ҷ���ack;������sever
		while ((now - begin) < end_time)
		{
			time(&now);
			// ���ڶ���syn
			send_seq = rtp_control.client_inspection_syn(local, server);
			if (send_seq!=SEQ_ERROR)
			{
				rtp_data ack_data(0, 1, 0, recv_seq, 0, NULL, local.get_rpt_addr(), addr); // ����һ�� ack ���ݱ�
				memset(buff, 0, 1500);
				len = pack_rtp_data(buff, ack_data, ack_data.size());
				rtp_control.set_send_seq(local, server, send_seq);

				cout << "connect " << server.get_identity() << " succes" << endl;

				//����ack�����ҽ���
				sendto(local.get_socket(), buff, len, 0, (SOCKADDR*)&addr.get_sockaddr(), sizeof(SOCKADDR));
				return server;
			}
			Sleep(10);
		}
	}
	rtp_control.free_remote(local, server);
	return RTP_ERROR;
}

rtp rtp_connect(const rtp& local,const rtp_addr &addr)
{
	// �ͻ������������������
	// 10�볬ʱ
	// 1��3, 5, 7, 9 ��ֱ�����
	// ���ֳɹ��� ����������
	time_t begin, end;

	time(&begin);
	time(&end);
	
	while (end - begin < 10)
	{
		time(&end);
		rtp server = remote_three_handshake(local,addr);
		if (server != RTP_ERROR) return server;
		Sleep(2000);
	}
	return RTP_ERROR;
}

int rtp_send(const rtp& remote, const rtp_data& data)
{
	// ������Ϣ data ���� remote
	return rtp_control.send_mes(remote, data);
}

int rtp_send(const rtp& remote, const char* c, const int& len)
{
	// ����rtp_data ����
	// ���� remote �� ��Ӧ��local 
	auto local = rtp_control.get_send_rtp(remote.get_identity());
	if (local == NULL) return -1;

	rtp_data data(0, 0, 0, rtp_control.get_right_send_seq(*local,remote), len, c, local->get_rpt_addr(), remote.get_rpt_addr());
	return rtp_send(remote, data);
}

rtp_data rtp_recv(const rtp& remote)
{
	auto local = rtp_control.get_send_rtp(remote.get_identity());
	if (local)
	{
		return rtp_control.recv_mes(local->get_identity(), remote.get_identity());
	}
	else return DATA_ERROR;
}

int rtp_shutdown(const rtp& ender)
{
	if (ender.is_local())
	{
		// �ر�һ������ rtp
		/*
		1.������ȡ���� �ͻ���rtp
		2.������rpt����fin����
		3.������rtp�ȴ�fin ���ҷ���ack��δ�յ�ack��rtp ���·��� fin ����
		4.��ʱʱ��30�룬���û����ɣ�Ҳ�ͷŵ�������Դ
		*/

		if(!rtp_control.local_rtp_table_empty(ender))
		{
			auto local_map = rtp_control.get_local_rtp_table(ender);

			for (auto p : local_map)
			{
				/*unsigned long long right_seq = rtp_control.get_right_send_seq(ender.get_identity(), p.second->get_identity());
				rtp_data fin(0, 0, 1, right_seq, 0, NULL, ender.get_rpt_addr(), p.second->get_rpt_addr());
				char buff[1500] = { 0 };
				pack_rtp_data(buff, fin, 1500);
				send(ender.get_socket(), buff, fin.size(), 0);*/
				thread t(&rtp_shutdown, *p.second);
				t.detach();
			}
			Sleep(30000);
		}
	}
	else
	{
		auto local = rtp_control.get_send_rtp(ender.get_identity());
		if (*local == RTP_ERROR) return 0;
		remote_four_handshake(*local, ender);
	}
	return 0;
}

// 4�����ֵĹ��� ���ض���Զ�̵�����
void local_four_handshake(const rtp& local,rtp& remote)
{
	//������ �����Ѿ�ʶ�� Զ�̶˷����� fin ���ģ���ʽ�����������

	/*
	1.�ж��Ƿ��Ѿ��������ӡ�
	2.���� ���ڸ� fin ��ack
	3.���� fin ���ģ����� romote socket ״̬����Ϊ fin_sent
	4.�ȴ� �� fin �� ack  ��δ�յ� ��1��3��5��7��9,11,13,�ط���15���ʱ
	5.�յ�ack ���߳�ʱ���ͷ� remote ��������Դ
	*/

	// �����Ƿ���
	if (!rtp_control.is_intilized_local(local) || !rtp_control.is_intilized_remote(local, remote)) return;

	// ���ɶ� fin_ack �ı���, �Լ�fin ����
	unsigned long long last_recv_seq = rtp_control.get_right_recv_seq(local.get_identity(), remote.get_identity());
	unsigned long long send_seq = rtp_control.get_right_send_seq(local.get_identity(), remote.get_identity());

	// ����ack
	rtp_data fin_ack(0, 1, 0, last_recv_seq, 0, NULL, local.get_rpt_addr(), remote.get_rpt_addr());
	// �ڶ��α���
	rtp_data fin_data(0, 0, 1,send_seq,0, NULL, local.get_rpt_addr(), remote.get_rpt_addr());

	time_t begin, now;
	time(&begin);
	time(&now);
	int end_time = 10;
	remote.set_state(RTP_FIN_SEND);

	while ((now - begin) < 15)
	{
		time(&now);
		char buff[1500] = { 0 };
		pack_rtp_data(buff, fin_ack,1500);
		sendto(local.get_socket(), buff, fin_ack.size(), 0,(SOCKADDR*)&remote.get_rpt_addr().get_sockaddr(),sizeof(SOCKADDR));

		memset(buff, 0, 1500);
		pack_rtp_data(buff, fin_data,1500);
		sendto(local.get_socket(), buff, fin_ack.size(), 0, (SOCKADDR*)&remote.get_rpt_addr().get_sockaddr(), sizeof(SOCKADDR));
		
		time(&now);
		//����Ƿ��յ� ���fin��ack
		if (rtp_control.inspection_ack(local, remote, send_seq))
		{
			//�յ����ͷ���Դ
			rtp_control.free_remote(local, remote);
			return;
		}
	}
	m.lock();
	rtp_control.free_remote(local, remote);
	m.unlock();
	return;
}

void local_four_handshake(const rtp& local, const rtp_data& data)
{
	auto remote = rtp_control.get_rtp_by_data(local, data);
	auto r = *remote;
	if (remote)
		local_four_handshake(local, r);
	else return;
}

// 4�����ֹ��� �ͻ��˶��ڷ����� ����
void remote_four_handshake(const rtp& local,const rtp& remote)
{
	// �˴��� local �Ǳ���socket Ϊ�ͻ��˵����

	/*
	1.��ʱ 15�룬���ҷ��� ��һ��fin ����
	2.��ѯ�Ƿ��յ�fin ��ack �����δ�յ�����ô��1��3��5��7��9��11��13 ����ִ�в���1
	3.����յ��ˣ������ RTP_FIN_SEND ״̬���ȴ����������� fin ������ack
	4-1.����յ�fin ����ack���ģ��ͷű�����Դ��
	4-2.���û���յ�����ô15���ֱ����Դ
	*/
	time_t begin, now;
	time(&begin);
	time(&now);

	// fin ����
	unsigned long long right_send_seq = rtp_control.get_right_send_seq(local.get_identity(), remote.get_identity());
	if (right_send_seq == SEQ_ERROR) return;

	rtp_data fin(0, 0, 1, right_send_seq, 0, NULL, local.get_rpt_addr(), remote.get_rpt_addr());

	// 15�볬ʱ
	while ((now - begin) < 15)
	{
		char buff[1500] = { 0 };
		pack_rtp_data(buff, fin, fin.size());

		// ����fin
		sendto(local.get_socket(), buff, fin.size(), 0, (SOCKADDR*)&remote.get_rpt_addr().get_sockaddr(), sizeof(SOCKADDR));
		remote.set_state(RTP_FIN_SEND);
		// ѭ���ȴ�ack
		if (rtp_control.inspection_ack(local, remote, right_send_seq))
		{
			// �ȴ��Է���fin����
			while ((now - begin) < 15)
			{
				time(&now);
				unsigned long long fin_ack_seq = rtp_control.client_inspection_fin(local, remote);
				time(&now);
				if (fin_ack_seq == SEQ_ERROR)
				{
					Sleep(100);
					continue;
				}

				// �յ�fin ����ack ����
				rtp_data fin_ack(0,1, 0, fin_ack_seq, 0, NULL, local.get_rpt_addr(), remote.get_rpt_addr());
				memset(buff, 0, 1500);
				pack_rtp_data(buff, fin_ack, fin_ack.size());
				sendto(local.get_socket(), buff, fin_ack.size(), 0, (SOCKADDR*)&remote.get_rpt_addr().get_sockaddr(), sizeof(SOCKADDR));

				rtp_control.free_remote(local, remote);
				return;
			}

		}
		Sleep(1000);
		time(&now);
	}

	rtp_control.free_remote(local, remote);
	return;
}

int pack_rtp_data(char* buff, const rtp_data& data,const int &buff_len)
{
	// ��һ��rtp_data �� ʵ������ ��ŵ� buff �У���� buff �ռ䲻�㣬��������������

	if (buff_len>max_rtp_data_size||buff_len<min_rtp_data_size) return -1;
	if (data.size() > (buff_len - min_rtp_data_size - 3)) return -1;
	
	// 0 
	string s("rtp");
	int offset = 0;
	
	for (auto a : s)
	{
		memcpy(buff + offset, &a, size_char);
		offset += size_char;
	}
	
	// 3 + 8
	memcpy(buff + offset, &data.sender.ip_0, size_uchar);
	offset += size_uchar;
	memcpy(buff + offset, &data.sender.ip_1, size_uchar);
	offset += size_uchar;
	memcpy(buff + offset, &data.sender.ip_2, size_uchar);
	offset += size_uchar;
	memcpy(buff + offset, &data.sender.ip_3, size_uchar);
	offset += size_uchar;
	memcpy(buff + offset, &data.sender.port, size_uint);
	offset += size_uint;

	// 3+8+8
	memcpy(buff + offset, &data.recver.ip_0, size_uchar);
	offset += size_uchar;
	memcpy(buff + offset, &data.recver.ip_1, size_uchar);
	offset += size_uchar;
	memcpy(buff + offset, &data.recver.ip_2, size_uchar);
	offset += size_uchar;
	memcpy(buff + offset, &data.recver.ip_3, size_uchar);
	offset += size_uchar;
	memcpy(buff + offset, &data.recver.port, size_uint);
	offset += size_uint;

	// 3+8+8+3
	memcpy(buff+offset, &data.syn, size_bool);
	offset += size_bool;
	memcpy(buff + offset, &data.ack, size_bool);
	offset += size_bool;
	memcpy(buff + offset, &data.fin, size_bool);
	offset += size_bool;

	// 3+8+8+3+8
	memcpy(buff + offset, &data.seq, size_ulong);
	offset += size_ulong;

	// 3+8+8+3+8+4
	memcpy(buff + offset , &data.len, size_int);
	offset += size_int;

	if (data.len != 0)
	{
		for (int i = 0; i < data.len; i++)
		{
			memcpy(buff + offset, data.data+i, size_char);
			offset += size_char;
		}
	}

	return offset;
}

rtp_data unpack_rtp_data(char* buff,const int &buff_len,const rtp_addr& sender)
{
	// �����н����һ��rtp_data���ݣ���Ҫ���·����� ��ַ��ip
	
	// rtp ���� ͷ��3���ַ� rtp
	char check[10] = { 0 };
	memcpy(check, buff, size_bool * 3);

	stringstream ss;
	ss << check;
	if (ss.str() != "rtp") return DATA_ERROR;
	int offset = size_bool * 3;
	offset += sizeof(rtp_addr);  // ����sender����Ϊ��������ʱ��ʹ�õ�ip ���ܶ���0,��Ҫ��������
	
	
	unsigned char ip_0, ip_1, ip_2, ip_3;
	unsigned int port;

	memcpy(&ip_0, buff + offset, size_uchar);
	offset += size_uchar;
	memcpy(&ip_1, buff + offset, size_uchar);
	offset += size_uchar;
	memcpy(&ip_2, buff + offset, size_uchar);
	offset += size_uchar;
	memcpy(&ip_3, buff + offset, size_uchar);
	offset += size_uchar;
	memcpy(&port, buff + offset, size_uint);
	offset += size_uint;

	rtp_addr recver(ip_0, ip_1, ip_2, ip_3, port);

	bool syn, ack, fin;
	memcpy(&syn, buff + offset, size_bool);
	offset += size_bool;
	memcpy(&ack, buff + offset, size_bool);
	offset += size_bool;
	memcpy(&fin, buff + offset, size_bool);
	offset += size_bool;

	unsigned long long seq = 0;
	memcpy(&seq, buff + offset, size_ulong);
	offset += size_ulong;

	int len = 0;
	memcpy(&len, buff + offset, size_int);
	offset += size_int;

	if (len)
	{
		char data[1500] = { 0 };
		memcpy(data, buff + offset, len);
		return rtp_data(syn, ack, fin, seq, len, data, sender, recver);
	}
	else return rtp_data(syn, ack, fin, seq, len,NULL, sender, recver);
} 

rtp_addr get_rtp_addr_from_addr_in(const SOCKADDR_IN& addr)
{
	unsigned char ip_0 = addr.sin_addr.S_un.S_un_b.s_b1;
	unsigned char ip_1 = addr.sin_addr.S_un.S_un_b.s_b2;
	unsigned char ip_2 = addr.sin_addr.S_un.S_un_b.s_b3;
	unsigned char ip_3 = addr.sin_addr.S_un.S_un_b.s_b4;
	unsigned int port = ntohs(addr.sin_port);

	return rtp_addr(ip_0, ip_1, ip_2, ip_3, port);
}