/**

This program - as all codes in this project - is totally experimental, likelly has many bugs and exploits!
Do not use it.


*/




#include <iostream>
#include <thread>
#include <vector>
#include <system_error>


#include <boost/asio.hpp>
#include <iostream>
#include <chrono>
#include <atomic>
#include <mutex>

#if 1
#define _dbg4(X) {}
#define _dbg1(X) { std::cout<<X<<std::endl; }
#define _note(X) { std::cout<<X<<std::endl; }
#define _mark(X) _note(X);
#else
#define _dbg4(X) { std::cout<<X<<std::endl; }
#define _dbg1(X) {if(0)_dbg4(X);}
#define _note(X) {if(0)_dbg4(X);}
#define _mark(X) {if(0)_dbg4(X);}
#endif

#define _goal(X) { std::cout<<X<<std::endl; }

#define UsePtr(X) (* (X) )
#define addrvoid(X) ( static_cast<const void*>( & (X) ) )

using namespace boost;
using std::vector;
using std::unique_ptr;
using std::make_unique;

template <typename T>
class ThreadObject {
	public:
		template<typename... Args> ThreadObject(Args&&... args) : m_obj(std::forward<Args>(args)...) {}

		T & get() { m_test_data = (m_test_data+1) % 255; return m_obj; }
		const T & get() const { m_test_data = (m_test_data+1) % 255; return m_obj; }

		int get_test_data() { return m_test_data; }

	private:
		T m_obj;
		volatile int m_test_data; ///< write to this to cause (expose) a race condition

		void init_this_object() { m_test_data=0; }
};

template <typename T>
class with_strand {
	public:
		template<typename... Args> with_strand(boost::asio::io_service & ios, Args&&... args)
			: m_obj(std::forward<Args>(args)...) , m_strand(ios) {}

		/**
			* Acccess the object #m_obj
			* @warning the caller guarantees that he is calling from a strand (this functin does NOT assert that),
			* it is UB in other case
			*/
		T & get_unsafe_assume_in_strand() { return m_obj; }
		const T & get_unsafe_assume_in_strand() const { return m_obj; }

		template <typename Lambda> auto wrap(Lambda && lambda) {
			return m_strand.wrap( std::move(lambda) );
		}

	private:
		T m_obj;
		asio::strand m_strand;
};


std::atomic<bool> g_atomic_exit;
std::atomic<long int> g_recv_totall_count;
std::atomic<long int> g_recv_totall_size;

struct t_mytime {
	using t_timevalue = std::chrono::time_point<std::chrono::steady_clock>;
	t_timevalue m_time;
	t_mytime() noexcept { }
	t_mytime(std::chrono::time_point<std::chrono::steady_clock> time_) noexcept { m_time = time_; }
};

std::atomic<t_mytime> g_recv_started;

struct t_inbuf {
	char m_data[1024];
	static size_t size();
	asio::ip::udp::endpoint m_ep;
};
size_t t_inbuf::size() { return std::extent< decltype(m_data) >::value; }

class c_inbuf_tab {
	public:
		vector<unique_ptr<t_inbuf>> m_inbufs;

		c_inbuf_tab(size_t howmany);
		size_t buffers_count();
		char* addr(size_t ix);
		t_inbuf & get(size_t ix);
};
c_inbuf_tab::c_inbuf_tab(size_t howmany) {
	for (size_t i=0; i<howmany; ++i) {
		auto newbuff = make_unique<t_inbuf>();
		_dbg1("");
		_note("newbuff at " << addrvoid( *newbuff) );
		_dbg1("newbuff before move: " << newbuff.get() );
		m_inbufs.push_back( std::move(newbuff) );
		_dbg1("newbuff after move: " << newbuff.get() );
		_dbg1("tab after move: " << m_inbufs.at(i).get() );
	}
}
size_t c_inbuf_tab::buffers_count() { return m_inbufs.size(); }

char* c_inbuf_tab::addr(size_t ix) {
	return m_inbufs.at(ix)->m_data;
}

t_inbuf & c_inbuf_tab::get(size_t ix) {
	return * m_inbufs.at(ix) ;
}

void handler_signal_term(const boost::system::error_code& error , int signal_number)
{
	_goal("Signal! (control-C?) " << signal_number);
	g_atomic_exit = true;
}

void handler_receive(const boost::system::error_code & ec, std::size_t bytes_transferred,
	with_strand< ThreadObject<asio::ip::udp::socket> > & mysocket,
	c_inbuf_tab & inbuf_tab, size_t inbuf_nr,
	std::mutex & mutex_handlerflow_socket)
{
	if (!! ec) {
		_note("Handler hit error, ec="<<ec.message());
		return;
	}

	auto & inbuf = inbuf_tab.get(inbuf_nr);
	_note("handler for inbuf_nr="<<inbuf_nr<<" for tab at " << static_cast<void*>(&inbuf_tab)
		<< " inbuf at " << static_cast<void*>( & inbuf)
		<< " from remote IP " << inbuf.m_ep << " bytes_transferred="<<bytes_transferred
		<< " read: ["<<std::string( & inbuf.m_data[0] , bytes_transferred)<<"]"
	);
	++ g_recv_totall_count;
	g_recv_totall_size += bytes_transferred;
	static const char * marker = "exit";
	static const size_t marker_len = strlen(marker);

	t_mytime time_now( std::chrono::steady_clock::now() );
	t_mytime time_zero;
	g_recv_started.compare_exchange_strong(
		time_zero,
		time_now
	);

	if (std::strncmp( &inbuf.m_data[0] , marker , std::min(bytes_transferred,marker_len) )==0) {
		if (bytes_transferred == marker_len) {
			_note("Message is EXIT, will exit");
			g_atomic_exit=true;
		}
	}

	char* inbuf_data = & inbuf.m_data[0] ;
	auto inbuf_asio = asio::buffer( inbuf_data  , std::extent<decltype(inbuf.m_data)>::value );
	assert( asio::buffer_size( inbuf_asio ) > 0 );
	_dbg4("buffer size is: " << asio::buffer_size( inbuf_asio ) );

	// fake "decrypt/encrypt" operation
	unsigned char bbb=0;
	for (unsigned int j=0; j<10; ++j) {
		unsigned char aaa = j%5;
			for (size_t pos=0; pos<bytes_transferred; ++pos) {
			aaa ^= inbuf.m_data[pos] & inbuf.m_data[ (pos*(j+2))%bytes_transferred ] & j;
		}
		bbb ^= aaa;
	}
	auto volatile rrr = bbb;
	if (rrr==0) _note("rrr="<<rrr);

	// ---

	_dbg4("Restarting async read, on mysocket="<<addrvoid(mysocket));
	{
		// std::lock_guard< std::mutex > lg( mutex_handlerflow_socket ); // *** LOCK ***

		mysocket.get_unsafe_assume_in_strand() // we are called in a handler that should be wrapped, so this is safe
			.get()
			.async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
			mysocket.wrap(
				[&mysocket, &inbuf_tab , inbuf_nr, & mutex_handlerflow_socket](const boost::system::error_code & ec, std::size_t bytes_transferred_again)
				{
					_dbg1("Handler (again), size="<<bytes_transferred_again<<", ec="<<ec.message());
					handler_receive(ec,bytes_transferred_again, mysocket, inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
				}
			)
		);
	}
	_dbg1("Restarting async read - done");
}

namespace nettools {


// void socket_reuse_same_process(


};

void asiotest_udpserv() {
	g_atomic_exit=false;
	g_recv_totall_count=0;
	g_recv_totall_size=0;
	g_recv_started = t_mytime{};

	asio::io_service ios;

	// TODO XXX
	_goal("WARNING this is NOT THREAD SAFE (race on the socket accsssed from handlers across different handler-flows");
	const int cfg_num_inbuf = 16;
	const int cfg_num_thread_per_ios = 16;
	const int cfg_num_socket = 2;

	// have any long-term work to do (for ios)
	boost::asio::signal_set signals(ios, SIGINT);
	signals.async_wait( handler_signal_term );

	_note("Starting ios run"); // ios.run()
	vector<std::thread> thread_run_tab;
	for (int ios_thread=0; ios_thread<cfg_num_thread_per_ios; ++ios_thread) {
		std::thread thread_run(
			[&ios, ios_thread] {
				_note("ios run (ios_thread="<<ios_thread<<" - starting");
				ios.run(); // <=== this blocks, for entire main loop, and runs (async) handlers here
				_note("ios run (ios_thread="<<ios_thread<<" - COMPLETE");
			}
		);
		thread_run_tab.push_back( std::move( thread_run ) );
	}

	_dbg1("The stop thread"); // exit flag --> ios.stop()
	std::thread thread_stop(
		[&ios] {
			for (int i=0; true; ++i) {
				std::this_thread::sleep_for( std::chrono::seconds(1) );

				auto time_now = std::chrono::steady_clock::now();
				auto now_recv_totall_size = g_recv_totall_size.load();
				double now_recv_ellapsed_sec = ( std::chrono::duration_cast<std::chrono::milliseconds>(time_now - g_recv_started.load().m_time) ).count() / 1000.;
				double now_recv_speed = now_recv_totall_size / now_recv_ellapsed_sec; // B/s
				_goal("Waiting, i="<<i<<" so far recv count=" << g_recv_totall_count << ", size=" << now_recv_totall_size
					<< " speed="<< (now_recv_speed/1000000) <<" MB/s"
				);
				if (g_atomic_exit) {
					_note("Exit flag is set, exiting loop and will stop program");
					break;
				}
			}
			ios.stop(); // thread safe, asio::io_service is TS for most functions
		}
	);

	c_inbuf_tab inbuf_tab(16);

	vector<with_strand<ThreadObject<asio::ip::udp::socket>>> mysocket_in_strand;


	_note("Example with reuse...");
	auto listen_address = boost::asio::ip::address_v4::from_string("127.0.0.1");
	int multicast_port = 8001;
	boost::asio::ip::udp::endpoint listen_endpoint(
	    listen_address, multicast_port);

	boost::asio::io_service ioservice;
	boost::asio::ip::udp::socket socket_(ioservice);
	boost::asio::ip::udp::socket socket2_(ioservice);
	// == important part ==
	socket_.open(listen_endpoint.protocol());
	socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
	socket_.bind(listen_endpoint);

	socket2_.open(listen_endpoint.protocol());
	socket2_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
	socket2_.bind(listen_endpoint);

	// == important part ==
/*	boost::asio::ip::udp::endpoint remote_endpoint;
	std::array<char, 2000> recvBuffer;
	socket_.receive_from(boost::asio::buffer(recvBuffer), remote_endpoint);
	socket2_.receive_from(boost::asio::buffer(recvBuffer), remote_endpoint);*/
	_note("Example with reuse... - done");


//	boost::asio::ip::udp::socket socket3(ioservice);
//	boost::asio::ip::udp::socket socket4(ioservice);
//	vector< with_strand<ThreadObject<boost::asio::ip::udp::socket>> > socket_tab;

	for (int nr_sock=0; nr_sock<cfg_num_socket; ++nr_sock) {
		_note("Creating socket #"<<nr_sock);
		//mysocket_in_strand.push_back({ios,ios}); // active udp // <--- TODO why not?
		mysocket_in_strand.push_back( with_strand<ThreadObject<boost::asio::ip::udp::socket>>(ios,ios) );
		_note("bind");

		boost::asio::ip::udp::socket & thesocket = mysocket_in_strand.back().get_unsafe_assume_in_strand().get();

		thesocket.open( asio::ip::udp::v4() );
		thesocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
		thesocket.bind( asio::ip::udp::endpoint( asio::ip::address::from_string("127.0.0.1") , 9000 ) );
	}

	_mark("all ok");
	std::string s;
	getline(std::cin,s);
	return ;

	asio::ip::udp::endpoint remote_ep;

	std::mutex mutex_handlerflow_socket;

	// add first work - handler-flow
	for (size_t inbuf_nr = 0; inbuf_nr<cfg_num_inbuf; ++inbuf_nr) {
		auto inbuf_asio = asio::buffer( inbuf_tab.addr(inbuf_nr) , t_inbuf::size() );
		_dbg1("buffer size is: " << asio::buffer_size( inbuf_asio ) );
		_dbg1("async read, on mysocket="<<addrvoid(mysocket_in_strand));
		{
			// std::lock_guard< std::mutex > lg( mutex_handlerflow_socket ); // LOCK
			mysocket_in_strand.at(0).get_unsafe_assume_in_strand().get().async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
				mysocket_in_strand.at(0).wrap(
					[&mysocket_in_strand, &inbuf_tab , inbuf_nr, &mutex_handlerflow_socket](const boost::system::error_code & ec, std::size_t bytes_transferred) {
						_dbg1("Handler (FIRST), size="<<bytes_transferred);
						handler_receive(ec,bytes_transferred, mysocket_in_strand.at(0),inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
					}
				)
			); // start async
		}
	}


	thread_stop.join();
	for (auto & thr : thread_run_tab) {
		thr.join();
	}

}


void asiotest()
{
	_mark("asiotest");

  int port_num = 3456;

  asiotest_udpserv();
  return; // !!!

	asio::io_service ios;
  asio::ip::tcp protocol_tcp = asio::ip::tcp::v4();
  asio::ip::udp protocol_udp = asio::ip::udp::v4();

	if (1) { // UDP server/client - anyway active socket
		char inbuf_data[64];
		auto inbuf_asio = asio::buffer( inbuf_data , std::extent<decltype(inbuf_data)>::value );
		asio::ip::udp::socket mysocket(ios); // active udp
		_note("bind");
		mysocket.open( protocol_udp );
		mysocket.bind( asio::ip::udp::endpoint( asio::ip::address_v4::any() , 9000 ) );
		asio::ip::udp::endpoint remote_ep;
		_note("receive");
		size_t read_size = mysocket.receive_from( inbuf_asio , remote_ep ); // ***
		_note("got data from remote " << remote_ep);
		_note("read: ["<<std::string(&inbuf_data[0],read_size)<<"]");
	}

	if (0) { // TCP server - part
		asio::ip::address ip_address = asio::ip::address_v4::any();
		asio::ip::tcp::endpoint ep(ip_address, port_num);

		asio::ip::tcp::socket mysocket(ios);

		asio::ip::tcp::acceptor myacceptor(ios);

		mysocket.open(protocol_tcp);
		myacceptor.open(protocol_tcp);
	}

	if (0) { // TCP client
		asio::ip::tcp::endpoint ep_dst_tcp( asio::ip::address::from_string("127.0.0.1") , 80 );
		asio::ip::tcp::socket tcpsend( ios );
		_note("open");
		tcpsend.open(protocol_tcp);
		_note("connect");
		tcpsend.connect( ep_dst_tcp ); // waits for ACK here
		asio::write( tcpsend , asio::buffer( std::string("GET /index.html HTTP/1.1\r\n\r\n") ) );

		char inbuf_data[64];
		auto inbuf = asio::buffer( inbuf_data , std::extent<decltype(inbuf_data)>::value );
		_note("read");
		auto read_size = size_t{ asio::read( tcpsend , inbuf ) };
		_note("read size " << read_size);
		_note("read: ["<<std::string(&inbuf_data[0],read_size)<<"]");
	}

}



int main() {
	asiotest();
	_goal("Normal exit");
	return 0;
}

