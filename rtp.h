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

//一个将udp封装，增加其可靠性的 数据结构和方法/协议 , rtp
// rtp : 对socket和地址得封装聚合,可以理解成一个 socket
class rtp
{
	friend bool operator==(const rtp& r1, const rtp& r2);
	friend bool operator!=(const rtp& r1, const rtp& r2);
	friend class rtp_system;
	friend void rtp_run(const rtp&);
	friend rtp local_three_handshake(rtp&);

public:
	rtp();                                                                         // 默认初始化，用于建立本地socket/rtp,隐式bind获得端口,默认其为服务器
	rtp(const SOCKADDR_IN& a, const int& sta, bool _sorc);                         // 地址初始化，远程 socket/rtp，析构方式使用
	rtp(const rtp& r) :l_or_r(r.l_or_r), st(r.st), addr(r.addr) {};                // 拷贝复制

	void set_state(const int&) const;
	int state() const;                                                             // 获取状态
	string get_identity() const;                                                   // 获取身份信息， ip+端口 字符串
	rtp_addr get_rpt_addr() const;                                                 // 根据SOCKEADDR_IN 生成一个 rtp_addr 类型数据
	SOCKET get_socket() const { return st; };
	bool is_local()const { return l_or_r; }
	bool is_remote()const { return !l_or_r; }

private:
	bool l_or_r;                                                                   // 该rtp 在本地是客户端还是服务端 1为服务端，影响其析构方式
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

//rtp报文
 
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
	unsigned long long seq;     // 序号

	int len;    // data 长度
	char* data; // data 内容
};

// 消息发送器
class mes_sender
{
public:
	mes_sender() = delete;
	mes_sender(const rtp& sender, const rtp& recver) :sender_rtp(sender), recver_rtp(recver) {};

	void confirm_ack();                    // 确认ack并且调整窗口大小
	void transfer_data(deque<rtp_data>);   // 从缓冲区转移资源到发送区
	unsigned int do_send();                // 实际发送行为   
	int size() { return list.size(); }     // 发送区包的个数
	bool empty() { return list.empty(); }
	bool seq_table_empty() { return seq_table.empty(); }
	unsigned get_wz() { return (int)send_windows_size/1; }

	bool pack_loss = 0;                    // 是否发生了丢包
	bool sw_ad = 0;                        // 是否已调整过窗口大小
	float send_windows_size = 1;           // 发送窗口大小

private:
	rtp sender_rtp;                                       // 发送socket
	rtp recver_rtp;
	deque<rtp_data> list;                                 // 发送区  -> 发送区大小就是 发送窗口大小
	map<unsigned long long, int> fail_times;              // 失败次数记录器
	deque<unsigned long long> seq_table;                  // 优先seq表

};

// 消息缓存类
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

// 发送消息类
class send_mes_list : public mes_list
{
public:
	send_mes_list() = delete;
	send_mes_list(const rtp& sender, const rtp& recver) :sender(sender, recver), countdown_run(0), recorder(0), recorder_high(0), timer(0) { };
	void push_back(const rtp_data& mes, const time_t& t);      // 存储任意内容后启动一个定时函数,控制发送
	void countdown();                                          // 记时函数控制发送行为

private:
	mes_sender sender;                // 消息发送器
	bool countdown_run = 0;           // 是否有计时函数正在运行，有的话，运行一个记时函数即可
	time_t timer;                     // 记录最后push的时间
	unsigned long long recorder;           // 记录器低8位，记录了实际发送数据的长度     MAX = 42亿 * 4G左右
	unsigned int recorder_high;       // 记录器高4位，记录了实际发送数据的长度
};

// 接受消息类
class recv_mes_list : public mes_list
{
public:
	void push_back(const rtp_data& data, const time_t&); // 接受信息,并且发送ack.  根据消息seq ，要么存入队列并且检查缓存队列是否有接下来的消息，要么存入缓存队列
	void push_back_to_chache(const rtp_data& data, const time_t& t);
	void countdown();                                    // 计时函数，60未收到数据
	pair<rtp_data, time_t> front() { return mes_list::front(); };

private:
	map<unsigned long long, pair<rtp_data, time_t>> recv_cache_list;   // 接受缓冲区
	time_t timer;                                                      // 计时器 最后收到消息的时间
	bool countdown_run = 0;
};

/*
发送消息的几个问题：
1.如何调节窗口大小                            可以使用一个变量
2.如何判断这次是否发生了丢包                  同样使用一个变量

3.发送中如何对待插入队尾的包？需要加锁吗？
这个比较微妙了，因为协议是ipv6格式，限定最大包1500byte.所以向发送队列尾部添加包都是独立包（1个）。
策略：1.使用一个发送缓存队列。发送缓存队列内的信息达到5000条，那么缓存器，调用实际发送函数，将前5000条信息转移
	  2.如果数量没有满，那么如果0.2秒内，消息缓存队列未添加信息，那么缓存队列调用发送函数，将所有信息转移

3.1 实际发送步骤：
	a.发送队列初始化窗口长度1，丢包符为0
	b.每次发送【窗口长度】个包，并在包中加入seq;            窗口长度增长速度：未丢包 收1个ack+1,丢包：收一个ack+【1/窗口长度】
	c.以队列形式发送，如果3秒内未收到ack，那么： 1).首先检查是否发送缓存是否有信息，有则读入回到步骤，无则等待0.2秒后直接重发，并且将丢包符改为1；
	d.重发最大次数为20次，如果某个包20次内未发送成功，那么将其丢弃。并将丢失了的seq 赋予下一个包，如果后面没有包，则发送一个空包，防止阻塞
	e.队列空且发送区无内容后，则将发送窗口重置1，并且 丢包符号置为0

3.2 实际接受步骤：
	a.收到包的seq 合法，则存入队列，发送ack,然后recv_seq++;
	b.收到包不合法，则按照字典序存储在本地缓存队列中，发送ack，并且等待合法包
	c.如果1分钟未收到合法包，则将缓存序列内容全部丢弃
	d.如果收到了合法包，更新计时器，并且查看缓存序列内是否有后续包，有则存储到对位

4.发送的数据总量记录

问题： 发送器是使用全局共享的还是 每个server:client 独立的？
答：使用全局共享的发送器话，可以更好的提升效率，一次性的发送更多的包，资源利用最大化。但是这样的话，每个线程会出现争抢 【推送权限】，导致频繁加锁和去锁。以及需要制定相应的推送策略
	而如果 每个server:client 使用单独的发送器，可以更好的进行推送，检测。
	暂时先采用独立的进行尝试

*/

// ack 缓存类
class a_list
{
public:
	a_list() = default;
	void push_back(const unsigned long long& seq, const time_t& t) { m_data.lock(); seq_list.push_back(seq); time_list.push_back(t); m_data.unlock(); };
	bool empty() { return  seq_list.empty()&&time_list.empty(); };
	pair<unsigned long long, time_t> front();

private:
	deque<unsigned long long> seq_list;              // 消息内容
	deque<time_t> time_list;                         // 发送的时间的list
};

// 全局系统控制器，控制各种缓存器以及发送表
class rtp_system
{
public:
	// *任何本地socket 都是local 型资源
	// *任何非本地 socket 都是 remote 型资源 ； 例如 本地客户端，向远程服务器socket 建立来连接，客户端为本地，服务器为远程
	// *一个客户端用的socket 建立后，在本地也被视为服务端； 类P2P结构


	// 初始化分配及析构资源函数
	void intilized_local(const rtp& server);
	int intilized_remote(const rtp& server, const rtp& client, const unsigned long long & send_seq, const unsigned long long & recv_seq);
	void free_local(const rtp& server);
	void free_remote(const rtp& server, const rtp& client);

	bool local_rtp_table_empty(const rtp& local) const;                         // 某个本地 rtp 已经建立的 远程 rtp 表是否为空
	map<string,const rtp*> get_local_rtp_table(const rtp& local) const;         // 返回某个本地 rtp 对应的 远程 Rtp 集合
	map<string, const rtp*> get_local_rtp_table(const string& local) const;

	const rtp* get_rtp_by_data(const rtp& local, const rtp_data&) const;

	bool is_intilized_local(const string& local) const;                         // 某个本地端是否已经初始化
	bool is_intilized_local(const rtp& server) const;
	bool is_intilized_remote(const rtp& local, const rtp& remote);              // 某个客户端在本地是否已经初始化
	bool is_intilized_remote(const string&, const string&);
	bool is_intilized_remote(const rtp& local, const rtp_data& data);

	bool connect_list_empty(const rtp& server);                                 // 某个本地端的握手队列是否为空
	void connect_list_push(const rtp& server, const rtp_data& data);
	void recv_list_push(const rtp& server, const rtp_data& data);
	pair<rtp, unsigned long long> get_rtp_from_connce_list(const rtp& server);       // 根据握手信息缓存器内容，取出一个内容，并且生成一个临时rtp 及正确发送序号

	// ack 函数
	bool inspection_ack(const rtp& local, const rtp& remote, const unsigned long long & sent_seq);  // 单独检查某个rtp_data的消息 对方是否收到，3秒内未收到，则超时
	bool inspection_ack(const string&, const string&, const unsigned long long & seq);
	unsigned long long client_inspection_syn(const rtp& local, const rtp& remote);                           // 客户端确认是否收到服务端的syn，如果收到了，则返回对方发来的 seq
	unsigned long long client_inspection_fin(const rtp& local, const rtp& remote);                           // 客户端确认是否收到服务端的fin, 则返回对方发来的 seq
	void client_push_syn_fin(const rtp& local, const rtp& remote, const rtp_data& data);               // 将一个 【第二次握手】的报文存放到队列中 
	void client_push_syn_fin(const string& local, const string& remote, const rtp_data& data);
	void clear_syn_fin(const rtp& local, const rtp& remote);

	void ack_list_push(const rtp& local, const rtp_data& data);
	unsigned long long get_right_recv_seq(const rtp& local, const rtp& remote);     // 获取某个客户端下一个正确的seq
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

	// 发送
	int send_mes(const rtp& ender, const rtp_data& data);                      // 发送消息的实际步骤，对于发送队列的内容进行发送
	const rtp* get_send_rtp(const string& s) const;                            // 获取发送者rtp

	rtp_data recv_mes(const string&, const string&);
	rtp_data recv_mes(const rtp& local, const rtp& remote);                    // rtp 系统检查 信息收集队列，判断是否存在一条 信息              

private:

	// 缓存器
	map<string, map<string, recv_mes_list*>> recv_list;              // 接受信息缓存器  <服务器ip+端口：<发送ip+端口+socket，内容>>
	map<string, map<string, send_mes_list*>> send_list;              // 发送缓存器
	map<string, map<string, a_list*>>  ack_list;                     // ack 专用缓存器
	map<string, map<string, mes_list*>> syn_fin_list;                // 存储 syn/fin 缓存器
	map<string, mes_list*> connect_list;                             // 连接请求缓存器  

	// 已建立连接对象 序号表
	map <string, map<string, unsigned long long>> seq_send_table;         // 发送信息的正确序号
	map <string, map<string, unsigned long long>> seq_recv_table;         // 接受信息的正确序号

	map<string,int> state_table;                                          // SOCKET 状态表

	// 消息发送表
	map<string, const rtp*> sender_table;                                   // 发送消息时，根据发送表选择 使用哪个rtp来发送
	map<string, map<string,const rtp*>> local_rtp_table;                    // 记录所有的 本地 rpt 和 已经建立连接 远程 rtp
};

// 全局功能 函数
void rtp_run_server(const rtp& server);
void rtp_run(const rtp& server);                                            // 信息接收器函数，listen后开始监听发送到本端口得内容，并存储到相应缓存队列
rtp rtp_accept(const rtp& server);                                          // 服务端进行握手操作，10秒超时，如果无人发送握手请求，返回 RTP_ERROR
rtp rtp_connect(const rtp& client, const rtp_addr& addr);                   // 客户都安向服务端发起连接，进行验证和握手，成功后返rtp ，失败则 RTP_ERROR
rtp local_three_handshake(const rtp& local);                                // 服务器端对某个申请报文，进行3次握手实际步骤，成功返回 rtp ,失败返回 RTP_ERROR
rtp remote_three_handshake(const rtp& local,const rtp_addr& addr);          // 客户端向服务器 发起3次连接的实际步骤函数

int rtp_send(const rtp& remote, const rtp_data& data);                      // 发送消息到某个端 || 实质为 添加消息到 发送队列，然后调用系统发送函数
int rtp_send(const rtp& remote, const char*,const int& len);
rtp_data rtp_recv(const rtp& remote);                                       // 从某个端接收消息 || 实质为 检查收信队列，看能否拼凑一条完整消息

int rtp_shutdown(const rtp& ender);                                         // 断开socket 流程 根据端属于本地还是远程资源，选择型执行 关闭函数，并且释放资源

void local_four_handshake(const rtp& local, const rtp_data& data);          // 服务器对于远程 关闭 请求的响应的实际步骤
void local_four_handshake(const rtp& local, const rtp& remote);
void remote_four_handshake(const rtp& local,const rtp& remote);             // 客户端端请求关闭rtp 连接的函数

// 打包解包rtp_data函数
int pack_rtp_data(char* buff, const rtp_data& data, const int& data_len);     // 打包一个rtp_data 到流中，并且返回长度
rtp_data unpack_rtp_data(char* buff, const int& data_len, const rtp_addr&);   // 从流中解包出一个 rtp_data 类数据，校验工作由udp完成 本函数只做解包
rtp_addr get_rtp_addr_from_addr_in(const SOCKADDR_IN&);

