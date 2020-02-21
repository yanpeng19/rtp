#pragma once

#include <Winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <string>
#include <sstream>
#include <iostream>
#include <time.h>
#include <fcntl.h>
#include <unordered_map>
#include <map>
#include <deque>
#include <mutex>
#include <utility>
#include <algorithm>
#include <memory>

using namespace std;

class rtp;
class rtp_data;
class rtp_system;

extern const unsigned long MY_IP;

extern rtp RTP_ERROR;
extern rtp_data DATA_ERROR;
extern const int ERROR_STATE;

extern const int RTP_NEW;     
extern const int RTP_HALF_NEW;
extern const int RTP_SYN_SENT;   
extern const int RTP_SYN_RECVED;
extern const int RTP_RIGHT;     
extern const int RTP_FIN_SEND; 
extern const int RTP_FIN_RECVED;
extern const int RTP_CLOSE;      
extern const vector<int> v_state;

extern const unsigned long long SEQ_ERROR;
extern const map<string,const rtp*> RTP_TABLE_ERROR;

extern const unsigned int min_rtp_data_size;
extern const unsigned int max_rtp_data_size;

extern const int size_bool;
extern const int size_uint;
extern const int size_uchar;
extern const int size_ulong;
extern const int size_int;
extern const int size_char_p;
extern const int size_char;

extern mutex m;
extern mutex m_data;
extern mutex m_recv_cache;
extern rtp_system rtp_control;

class rtp_addr;

//һ����udp��װ��������ɿ��Ե� ���ݽṹ�ͷ���/Э�� , rtp
// rtp : ��socket�͵�ַ�÷�װ�ۺ�,��������һ�� socket
class rtp
{
	friend bool operator==(const rtp& r1, const rtp& r2);
	friend bool operator!=(const rtp& r1, const rtp& r2);
	friend class rtp_system;
	friend void rtp_run(const rtp&);
	friend rtp local_three_handshake(rtp&);

public:
	rtp();                                                                         // Ĭ�ϳ�ʼ�������ڽ�������socket/rtp,��ʽbind��ö˿�,Ĭ����Ϊ������
	rtp(const SOCKADDR_IN& a, const int& sta, bool _sorc);                         // ��ַ��ʼ����Զ�� socket/rtp��������ʽʹ��
	rtp(const rtp& r) :l_or_r(r.l_or_r), st(r.st), addr(r.addr) {};                // ��������

	void set_state(const int&) const;
	int state() const;                                                             // ��ȡ״̬
	string get_identity() const;                                                   // ��ȡ�����Ϣ�� ip+�˿� �ַ���
	rtp_addr get_rpt_addr() const;                                                 // ����SOCKEADDR_IN ����һ�� rtp_addr ��������
	SOCKET get_socket() const { return st; };
	bool is_local()const { return l_or_r; }
	bool is_remote()const { return !l_or_r; }

private:
	bool l_or_r;                                                                   // ��rtp �ڱ����ǿͻ��˻��Ƿ���� 1Ϊ����ˣ�Ӱ����������ʽ
	SOCKET st;
	SOCKADDR_IN addr;
};

class rtp_addr
{
	friend bool operator==(const rtp_addr& r1, const rtp_addr& r2);
	friend bool operator!=(const rtp_addr& r1, const rtp_addr& r2);
	friend int pack_rtp_data(char* buff, const rtp_data& data, const int& buff_len);
	rtp_data unpack_rtp_data(char* buff, const int& data_len, const rtp_addr&);
public:
	rtp_addr() = default;

	rtp_addr(const rtp_addr& r) :ip_0(r.ip_0), ip_1(r.ip_1), ip_2(r.ip_2), ip_3(r.ip_3), port(r.port) {};
	rtp_addr(int _ip0, int _ip1, int _ip2, int _ip3, int _port) : ip_0(char(_ip0)), ip_1(_ip1), ip_2(_ip2), ip_3(_ip3), port(_port) {};
	rtp_addr(const string& ip, int _port);

	string get_identity() const
	{
		stringstream ss;
		ss << int(ip_0) << '.' << int(ip_1) << '.' << int(ip_2) << '.' << int(ip_3) << ':' << port;
		return ss.str();
	}
	string get_ip() const
	{
		stringstream ss;
		ss << int(ip_0) << '.' << int(ip_1) << '.' << int(ip_2) << '.' << int(ip_3);
		return ss.str();
	}
	unsigned get_port() const { return port; }
	SOCKADDR_IN get_sockaddr() const;

private:
	unsigned char ip_0;
	unsigned char ip_1;
	unsigned char ip_2;
	unsigned char ip_3;
	unsigned int port;
};

//rtp����
 
class rtp_data
{
	friend bool operator==(const rtp_data&, const rtp_data&);
	friend int pack_rtp_data(char* buff, const rtp_data& data, const int& buff_len);

public:
	rtp_data() = default;
	rtp_data(const rtp_data& d);
	rtp_data(bool _syn, bool _ack, bool _fin, unsigned long long _seq, int _len, const char* _data, rtp_addr send, rtp_addr recv);
	~rtp_data();

	size_t size() const { return min_rtp_data_size+len; };
	unsigned long long get_seq() const { return seq; };
	string get_sender_identity() const { return sender.get_identity(); };
	string get_recver_identity() const { return recver.get_identity(); };

	rtp_addr sender;
	rtp_addr recver;
	bool syn;
	bool ack;
	bool fin;
	unsigned long long seq;     // ���

	int len;    // data ����
	char* data; // data ����
};

// ��Ϣ������
class mes_sender
{
public:
	mes_sender() = delete;
	mes_sender(const rtp& sender, const rtp& recver) :sender_rtp(sender), recver_rtp(recver) {};

	void confirm_ack();                    // ȷ��ack���ҵ������ڴ�С
	void transfer_data(deque<rtp_data>);   // �ӻ�����ת����Դ��������
	unsigned int do_send();                // ʵ�ʷ�����Ϊ   
	int size() { return list.size(); }     // ���������ĸ���
	bool empty() { return list.empty(); }
	bool seq_table_empty() { return seq_table.empty(); }
	unsigned get_wz() { return (int)send_windows_size/1; }

	bool pack_loss = 0;                    // �Ƿ����˶���
	bool sw_ad = 0;                        // �Ƿ��ѵ��������ڴ�С
	float send_windows_size = 1;           // ���ʹ��ڴ�С

private:
	rtp sender_rtp;                                       // ����socket
	rtp recver_rtp;
	deque<rtp_data> list;                                 // ������  -> ��������С���� ���ʹ��ڴ�С
	map<unsigned long long, int> fail_times;              // ʧ�ܴ�����¼��
	deque<unsigned long long> seq_table;                  // ����seq��

};

// ��Ϣ������
class mes_list
{
public:
	size_t size() { return s; }
	bool empty() const { return mes_list.empty(); }
	void clear() { mes_list.clear(); time_list.clear(); s = 0; };
	void push_back(const rtp_data& mes, const time_t& t);
	pair<rtp_data, time_t> front();
	deque<rtp_data> transer_get(const size_t&);

private:
	deque<rtp_data> mes_list;
	deque<time_t> time_list;
	size_t s = 0;
};

// ������Ϣ��
class send_mes_list : public mes_list
{
public:
	send_mes_list() = delete;
	send_mes_list(const rtp& sender, const rtp& recver) :sender(sender, recver), countdown_run(0), recorder(0), recorder_high(0), timer(0) { };
	void push_back(const rtp_data& mes, const time_t& t);      // �洢�������ݺ�����һ����ʱ����,���Ʒ���
	void countdown();                                          // ��ʱ�������Ʒ�����Ϊ

private:
	mes_sender sender;                // ��Ϣ������
	bool countdown_run = 0;           // �Ƿ��м�ʱ�����������У��еĻ�������һ����ʱ��������
	time_t timer;                     // ��¼���push��ʱ��
	unsigned long long recorder;           // ��¼����8λ����¼��ʵ�ʷ������ݵĳ���     MAX = 42�� * 4G����
	unsigned int recorder_high;       // ��¼����4λ����¼��ʵ�ʷ������ݵĳ���
};

// ������Ϣ��
class recv_mes_list : public mes_list
{
public:
	void push_back(const rtp_data& data, const time_t&); // ������Ϣ,���ҷ���ack.  ������Ϣseq ��Ҫô������в��Ҽ�黺������Ƿ��н���������Ϣ��Ҫô���뻺�����
	void push_back_to_chache(const rtp_data& data, const time_t& t);
	void countdown();                                    // ��ʱ������60δ�յ�����
	pair<rtp_data, time_t> front() { return mes_list::front(); };

private:
	map<unsigned long long, pair<rtp_data, time_t>> recv_cache_list;   // ���ܻ�����
	time_t timer;                                                      // ��ʱ�� ����յ���Ϣ��ʱ��
	bool countdown_run = 0;
};

/*
������Ϣ�ļ������⣺
1.��ε��ڴ��ڴ�С                            ����ʹ��һ������
2.����ж�����Ƿ����˶���                  ͬ��ʹ��һ������

3.��������ζԴ������β�İ�����Ҫ������
����Ƚ�΢���ˣ���ΪЭ����ipv6��ʽ���޶�����1500byte.�������Ͷ���β����Ӱ����Ƕ�������1������
���ԣ�1.ʹ��һ�����ͻ�����С����ͻ�������ڵ���Ϣ�ﵽ5000������ô������������ʵ�ʷ��ͺ�������ǰ5000����Ϣת��
	  2.�������û��������ô���0.2���ڣ���Ϣ�������δ�����Ϣ����ô������е��÷��ͺ�������������Ϣת��

3.1 ʵ�ʷ��Ͳ��裺
	a.���Ͷ��г�ʼ�����ڳ���1��������Ϊ0
	b.ÿ�η��͡����ڳ��ȡ����������ڰ��м���seq;            ���ڳ��������ٶȣ�δ���� ��1��ack+1,��������һ��ack+��1/���ڳ��ȡ�
	c.�Զ�����ʽ���ͣ����3����δ�յ�ack����ô�� 1).���ȼ���Ƿ��ͻ����Ƿ�����Ϣ���������ص����裬����ȴ�0.2���ֱ���ط������ҽ���������Ϊ1��
	d.�ط�������Ϊ20�Σ����ĳ����20����δ���ͳɹ�����ô���䶪����������ʧ�˵�seq ������һ�������������û�а�������һ���հ�����ֹ����
	e.���п��ҷ����������ݺ��򽫷��ʹ�������1������ ����������Ϊ0

3.2 ʵ�ʽ��ܲ��裺
	a.�յ�����seq �Ϸ����������У�����ack,Ȼ��recv_seq++;
	b.�յ������Ϸ��������ֵ���洢�ڱ��ػ�������У�����ack�����ҵȴ��Ϸ���
	c.���1����δ�յ��Ϸ������򽫻�����������ȫ������
	d.����յ��˺Ϸ��������¼�ʱ�������Ҳ鿴�����������Ƿ��к�����������洢����λ

4.���͵�����������¼

���⣺ ��������ʹ��ȫ�ֹ���Ļ��� ÿ��server:client �����ģ�
��ʹ��ȫ�ֹ���ķ������������Ը��õ�����Ч�ʣ�һ���Եķ��͸���İ�����Դ������󻯡����������Ļ���ÿ���̻߳�������� ������Ȩ�ޡ�������Ƶ��������ȥ�����Լ���Ҫ�ƶ���Ӧ�����Ͳ���
	����� ÿ��server:client ʹ�õ����ķ����������Ը��õĽ������ͣ���⡣
	��ʱ�Ȳ��ö����Ľ��г���

*/

// ack ������
class a_list
{
public:
	a_list() = default;
	void push_back(const unsigned long long& seq, const time_t& t) { m_data.lock(); seq_list.push_back(seq); time_list.push_back(t); m_data.unlock(); };
	bool empty() { return  seq_list.empty()&&time_list.empty(); };
	pair<unsigned long long, time_t> front();

private:
	deque<unsigned long long> seq_list;              // ��Ϣ����
	deque<time_t> time_list;                         // ���͵�ʱ���list
};

// ȫ��ϵͳ�����������Ƹ��ֻ������Լ����ͱ�
class rtp_system
{
public:
	// *�κα���socket ����local ����Դ
	// *�κηǱ��� socket ���� remote ����Դ �� ���� ���ؿͻ��ˣ���Զ�̷�����socket ���������ӣ��ͻ���Ϊ���أ�������ΪԶ��
	// *һ���ͻ����õ�socket �������ڱ���Ҳ����Ϊ����ˣ� ��P2P�ṹ


	// ��ʼ�����估������Դ����
	void intilized_local(const rtp& server);
	int intilized_remote(const rtp& server, const rtp& client, const unsigned long long & send_seq, const unsigned long long & recv_seq);
	void free_local(const rtp& server);
	void free_remote(const rtp& server, const rtp& client);

	bool local_rtp_table_empty(const rtp& local) const;                         // ĳ������ rtp �Ѿ������� Զ�� rtp ���Ƿ�Ϊ��
	map<string,const rtp*> get_local_rtp_table(const rtp& local) const;         // ����ĳ������ rtp ��Ӧ�� Զ�� Rtp ����
	map<string, const rtp*> get_local_rtp_table(const string& local) const;

	const rtp* get_rtp_by_data(const rtp& local, const rtp_data&) const;

	bool is_intilized_local(const string& local) const;                         // ĳ�����ض��Ƿ��Ѿ���ʼ��
	bool is_intilized_local(const rtp& server) const;
	bool is_intilized_remote(const rtp& local, const rtp& remote);              // ĳ���ͻ����ڱ����Ƿ��Ѿ���ʼ��
	bool is_intilized_remote(const string&, const string&);
	bool is_intilized_remote(const rtp& local, const rtp_data& data);

	bool connect_list_empty(const rtp& server);                                 // ĳ�����ض˵����ֶ����Ƿ�Ϊ��
	void connect_list_push(const rtp& server, const rtp_data& data);
	void recv_list_push(const rtp& server, const rtp_data& data);
	pair<rtp, unsigned long long> get_rtp_from_connce_list(const rtp& server);       // ����������Ϣ���������ݣ�ȡ��һ�����ݣ���������һ����ʱrtp ����ȷ�������

	// ack ����
	bool inspection_ack(const rtp& local, const rtp& remote, const unsigned long long & sent_seq);  // �������ĳ��rtp_data����Ϣ �Է��Ƿ��յ���3����δ�յ�����ʱ
	bool inspection_ack(const string&, const string&, const unsigned long long & seq);
	unsigned long long client_inspection_syn(const rtp& local, const rtp& remote);                           // �ͻ���ȷ���Ƿ��յ�����˵�syn������յ��ˣ��򷵻ضԷ������� seq
	unsigned long long client_inspection_fin(const rtp& local, const rtp& remote);                           // �ͻ���ȷ���Ƿ��յ�����˵�fin, �򷵻ضԷ������� seq
	void client_push_syn_fin(const rtp& local, const rtp& remote, const rtp_data& data);               // ��һ�� ���ڶ������֡��ı��Ĵ�ŵ������� 
	void client_push_syn_fin(const string& local, const string& remote, const rtp_data& data);
	void clear_syn_fin(const rtp& local, const rtp& remote);

	void ack_list_push(const rtp& local, const rtp_data& data);
	unsigned long long get_right_recv_seq(const rtp& local, const rtp& remote);     // ��ȡĳ���ͻ�����һ����ȷ��seq
	unsigned long long get_right_recv_seq(const string&, const string&);
	void add_right_recv_seq(const string&, const string&);                     // send_seq++;
	void add_right_send_seq(const string&, const string&);
	unsigned long long get_right_send_seq(const rtp& local, const rtp& remote);
	unsigned long long get_right_send_seq(const string&, const string&);
	void set_send_seq(const string& loc, const string& rem, const unsigned long long seq);
	void set_send_seq(const rtp& local, const rtp& remot, const unsigned long long seq);
	void set_recv_seq(const string& loc, const string& rem, const unsigned long long seq);
	void set_recv_seq(const rtp& local, const rtp& remot, const unsigned long long seq);


	void set_state(const rtp&,const int&);
	int get_state(const rtp&);
	int get_state(const string&);

	// ����
	int send_mes(const rtp& ender, const rtp_data& data);                      // ������Ϣ��ʵ�ʲ��裬���ڷ��Ͷ��е����ݽ��з���
	const rtp* get_send_rtp(const string& s) const;                            // ��ȡ������rtp

	rtp_data recv_mes(const string&, const string&);
	rtp_data recv_mes(const rtp& local, const rtp& remote);                    // rtp ϵͳ��� ��Ϣ�ռ����У��ж��Ƿ����һ�� ��Ϣ              

private:

	// ������
	map<string, map<string, recv_mes_list*>> recv_list;              // ������Ϣ������  <������ip+�˿ڣ�<����ip+�˿�+socket������>>
	map<string, map<string, send_mes_list*>> send_list;              // ���ͻ�����
	map<string, map<string, a_list*>>  ack_list;                     // ack ר�û�����
	map<string, map<string, mes_list*>> syn_fin_list;                // �洢 syn/fin ������
	map<string, mes_list*> connect_list;                             // �������󻺴���  

	// �ѽ������Ӷ��� ��ű�
	map <string, map<string, unsigned long long>> seq_send_table;         // ������Ϣ����ȷ���
	map <string, map<string, unsigned long long>> seq_recv_table;         // ������Ϣ����ȷ���

	map<string,int> state_table;                                          // SOCKET ״̬��

	// ��Ϣ���ͱ�
	map<string, const rtp*> sender_table;                                   // ������Ϣʱ�����ݷ��ͱ�ѡ�� ʹ���ĸ�rtp������
	map<string, map<string,const rtp*>> local_rtp_table;                    // ��¼���е� ���� rpt �� �Ѿ��������� Զ�� rtp
};

// ȫ�ֹ��� ����
void rtp_run_server(const rtp& server);
void rtp_run(const rtp& server);                                            // ��Ϣ������������listen��ʼ�������͵����˿ڵ����ݣ����洢����Ӧ�������
rtp rtp_accept(const rtp& server);                                          // ����˽������ֲ�����10�볬ʱ��������˷����������󣬷��� RTP_ERROR
rtp rtp_connect(const rtp& client, const rtp_addr& addr);                   // �ͻ����������˷������ӣ�������֤�����֣��ɹ���rtp ��ʧ���� RTP_ERROR
rtp local_three_handshake(const rtp& local);                                // �������˶�ĳ�����뱨�ģ�����3������ʵ�ʲ��裬�ɹ����� rtp ,ʧ�ܷ��� RTP_ERROR
rtp remote_three_handshake(const rtp& local,const rtp_addr& addr);          // �ͻ���������� ����3�����ӵ�ʵ�ʲ��躯��

int rtp_send(const rtp& remote, const rtp_data& data);                      // ������Ϣ��ĳ���� || ʵ��Ϊ �����Ϣ�� ���Ͷ��У�Ȼ�����ϵͳ���ͺ���
int rtp_send(const rtp& remote, const char*,const int& len);
rtp_data rtp_recv(const rtp& remote);                                       // ��ĳ���˽�����Ϣ || ʵ��Ϊ ������Ŷ��У����ܷ�ƴ��һ��������Ϣ

int rtp_shutdown(const rtp& ender);                                         // �Ͽ�socket ���� ���ݶ����ڱ��ػ���Զ����Դ��ѡ����ִ�� �رպ����������ͷ���Դ

void local_four_handshake(const rtp& local, const rtp_data& data);          // ����������Զ�� �ر� �������Ӧ��ʵ�ʲ���
void local_four_handshake(const rtp& local, const rtp& remote);
void remote_four_handshake(const rtp& local,const rtp& remote);             // �ͻ��˶�����ر�rtp ���ӵĺ���

// ������rtp_data����
int pack_rtp_data(char* buff, const rtp_data& data, const int& data_len);     // ���һ��rtp_data �����У����ҷ��س���
rtp_data unpack_rtp_data(char* buff, const int& data_len, const rtp_addr&);   // �����н����һ�� rtp_data �����ݣ�У�鹤����udp��� ������ֻ�����
rtp_addr get_rtp_addr_from_addr_in(const SOCKADDR_IN&);

