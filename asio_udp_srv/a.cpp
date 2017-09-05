/**

This program - as all codes in this project - is totally experimental, likelly has many bugs and exploits!
Do not use it.


Possible ASIO bug (or we did something wrong): see https://svn.boost.org/trac10/ticket/13193


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

bool g_debug = false;

#if 1
#define _dbg4(X) {}
#define _dbg1(X) { if (g_debug) { std::cout<<X<<std::endl; } }
#define _note(X) { _dbg1(X); }
#define _mark(X) _note("****** " << X);
#else
#define _dbg4(X) {if(0){ std::cout<<X<<std::endl;} }
#define _dbg1(X) {if(0)_dbg4(X);}
#define _note(X) {if(0)_dbg4(X);}
#define _mark(X) {if(0)_dbg4(X);}
#endif

#define _goal(X) { std::cout<<"====== " << X<<std::endl; }

#define UsePtr(X) (* (X) )
#define addrvoid(X) ( static_cast<const void*>( & (X) ) )

using namespace boost;
using std::vector;
using std::unique_ptr;
using std::make_unique;
using std::string;
using std::endl;

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

		asio::io_service::strand get_strand() { return m_strand; }

		template <typename Lambda> auto wrap(Lambda && lambda) {
			return m_strand.wrap( std::move(lambda) );
		}

	private:
		T m_obj;
		asio::io_service::strand m_strand;
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
		char* addr(size_t ix); ///< thread-safe
		t_inbuf & get(size_t ix); ///< thread-safe
};
c_inbuf_tab::c_inbuf_tab(size_t howmany) {
	for (size_t i=0; i<howmany; ++i) {
		auto newbuff = make_unique<t_inbuf>();
		_note("newbuff at " << addrvoid( *newbuff) );
	//	_dbg1("newbuff before move: " << newbuff.get() );
		m_inbufs.push_back( std::move(newbuff) );
	//	_dbg1("newbuff after move: " << newbuff.get() );
	//_dbg1("tab after move: " << m_inbufs.at(i).get() );
	}
}
size_t c_inbuf_tab::buffers_count() { return m_inbufs.size(); }

char* c_inbuf_tab::addr(size_t ix) {
	return m_inbufs.at(ix)->m_data;
}

t_inbuf & c_inbuf_tab::get(size_t ix) {
	return * m_inbufs.at(ix) ; // thread-safe: access to vector (read only)
}

void handler_signal_term(const boost::system::error_code& error , int signal_number)
{
	_goal("Signal! (control-C?) " << signal_number);
	g_atomic_exit = true;
}

enum class e_algo_receive {
	after_first_read, after_next_read, // handler when we got the date -, need to read it and use it, decrypt it
	after_processing_done, // handler when we are now ordered to restart the read
};



int cfg_test_crypto_task=0; // global option

void handler_receive(const e_algo_receive algo_step, const boost::system::error_code & ec, std::size_t bytes_transferred,
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

	if ((algo_step==e_algo_receive::after_first_read) || (algo_step==e_algo_receive::after_next_read)) {
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

		// fake "decrypt/encrypt" operation
		if (cfg_test_crypto_task > 0) {
			unsigned char bbb=0;
			for (unsigned int j=0; j<cfg_test_crypto_task; ++j) {
				unsigned char aaa = j%5;
					for (size_t pos=0; pos<bytes_transferred; ++pos) {
					aaa ^= inbuf.m_data[pos] & inbuf.m_data[ (pos*(j+2))%bytes_transferred ] & j;
				}
				bbb ^= aaa;
			}
			auto volatile rrr = bbb;
			if (rrr==0) _note("rrr="<<static_cast<int>(rrr));
		}
		else {
			// nothing. Just avoid warnings / deadcode optimize / unused
			unsigned char volatile xxx;
			xxx=inbuf.m_data[0];
			unsigned char volatile yyy = xxx;
			if (yyy == 0) ++yyy;
		}

		mysocket.get_strand().post(
			mysocket.wrap(
			[&mysocket, &inbuf_tab , inbuf_nr, & mutex_handlerflow_socket]()
			{
				_dbg1("Handler (restart read)");
				handler_receive(e_algo_receive::after_processing_done, boost::system::error_code(),0, mysocket, inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
			}
			)
		);
	} // first_read or next_read

	// ---

	else if (algo_step==e_algo_receive::after_processing_done) {
		_dbg1("Restarting async read, on mysocket="<<addrvoid(mysocket));
		// std::lock_guard< std::mutex > lg( mutex_handlerflow_socket ); // *** LOCK ***

		char* inbuf_data = & inbuf.m_data[0] ;
		auto inbuf_asio = asio::buffer( inbuf_data  , std::extent<decltype(inbuf.m_data)>::value );
		assert( asio::buffer_size( inbuf_asio ) > 0 );
		_dbg4("buffer size is: " << asio::buffer_size( inbuf_asio ) );


		mysocket.get_unsafe_assume_in_strand() // we are called in a handler that should be wrapped, so this is safe
			.get()
			.async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
				[&mysocket, &inbuf_tab , inbuf_nr, & mutex_handlerflow_socket](const boost::system::error_code & ec, std::size_t bytes_transferred_again)
				{
					_dbg1("Handler (again), size="<<bytes_transferred_again<<", ec="<<ec.message());
					handler_receive(e_algo_receive::after_next_read, ec,bytes_transferred_again, mysocket, inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
				}
		);
		_dbg1("Restarting async read - done");
	} // restart_read
	else throw std::runtime_error("Unknown state of algo.");
}

namespace nettools {


// void socket_reuse_same_process(


};

int safe_atoi(const std::string & s) {
	return atoi(s.c_str());
}

string yesno(bool yes) {
	if (yes) return "ENABLED";
	return "no";
}

void asiotest_udpserv(std::vector<std::string> options) {
	g_atomic_exit=false;
	g_recv_totall_count=0;
	g_recv_totall_size=0;
	g_recv_started = t_mytime{};

	if (options.size()<4) {
		std::cout << "\nUsage: program inbuf   socket socket_spread   ios thread_per_ios  crypto_task [OPTIONS]\n"
		<< "OPTIONS can be any of words: mport debug\n"
		<< "See code for more details. socket_spread must be 0 or 1.\n"
		<< "crypto_task must be 0, or >0.\n"
		<< "E.g.: program 32   2 0  4 16\n"
		<< std::endl;
	}

	const int cfg_num_inbuf = safe_atoi(options.at(0)); // 32 ; this is also the number of flows
	const int cfg_num_socket = safe_atoi(options.at(1)); // 2 ; number of sockets
	const int cfg_buf_socket_spread = safe_atoi(options.at(2)); // 0 is: (buf0,sock0),(b1,s1),(b2,s0),(b3,s1),(b4s0) ; 1 is (b0,s0),(b1,s0),(b2,s1),(b3,s1)

	const int cfg_num_ios = safe_atoi(options.at(3)); // 4
	const int cfg_num_thread_per_ios = safe_atoi(options.at(4)); // 16

	bool cfg_port_multiport = false;
	for (const string & arg : options) if (arg=="mport") cfg_port_multiport=true;

	auto func_show_summary = [&]() {
		_goal("Summary: " << endl
			<< "inbufs: " << cfg_num_inbuf << endl
			<< "socket: " << cfg_num_socket << " in spread: " << cfg_buf_socket_spread << endl
			<< "ios: " << cfg_num_ios << " per each there are threads: " << cfg_num_thread_per_ios << endl
			<< "option: " << "mport="<<yesno(cfg_port_multiport)<<" " << endl
		);
	};
	func_show_summary();

	cfg_test_crypto_task = safe_atoi(options.at(5)); // 10

	_goal("Starting test. cfg_test_crypto_task="<<cfg_test_crypto_task);

	std::vector<std::unique_ptr<asio::io_service>> ios;
	for (int i=0; i<cfg_num_ios; ++i) {
		_goal("Creating ios nr "<<i);
		ios.emplace_back( std::make_unique<asio::io_service>() );
	}

	// have any long-term work to do (for ios)
	boost::asio::signal_set signals( * ios.at(0), SIGINT);
	signals.async_wait( handler_signal_term );

	std::this_thread::sleep_for( std::chrono::milliseconds(100) );

	_note("Starting ios worker - ios.run"); // ios.run()
	vector<std::thread> thread_run_tab;
	for (int ios_nr = 0; ios_nr < cfg_num_ios; ++ios_nr) {
		for (int ios_thread=0; ios_thread<cfg_num_thread_per_ios; ++ios_thread) {
			_goal("start worker: " << ios_nr << " " << ios_thread);
			std::thread thread_run(
				[&ios, ios_thread, ios_nr] {
					_goal("ios worker run (ios_thread="<<ios_thread<<" on ios_nr=" << ios_nr << ") - starting");
					ios.at( ios_nr )->run(); // <=== this blocks, for entire main loop, and runs (async) handlers here
					_dbg4("ios worker run (ios_thread="<<ios_thread<<" - COMPLETE");
				}
			);
			thread_run_tab.push_back( std::move( thread_run ) );
		}
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
			for (auto & one_ios : ios) {
				one_ios->stop(); // thread safe, asio::io_service is TS for most functions
			}
		}
	);

	c_inbuf_tab inbuf_tab(cfg_num_inbuf);

	vector<with_strand<ThreadObject<asio::ip::udp::socket>>> mysocket_in_strand;

	std::this_thread::sleep_for( std::chrono::milliseconds(100) );
	for (int nr_sock=0; nr_sock<cfg_num_socket; ++nr_sock) {
		int port_nr = 9000;
		if (cfg_port_multiport) port_nr += nr_sock;
		_note("Creating socket #"<<nr_sock<<" on port " << port_nr);
		//mysocket_in_strand.push_back({ios,ios}); // active udp // <--- TODO why not?
		auto & one_ios = ios.at( nr_sock % ios.size() );
		mysocket_in_strand.push_back( with_strand<ThreadObject<boost::asio::ip::udp::socket>>(*one_ios, *one_ios) );
		_note("bind socket "<<nr_sock);

		boost::asio::ip::udp::socket & thesocket = mysocket_in_strand.back().get_unsafe_assume_in_strand().get();

		thesocket.open( asio::ip::udp::v4() );
		thesocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
		// thesocket.bind( asio::ip::udp::endpoint( asio::ip::address::from_string("127.0.0.1") , 9000 ) );
		thesocket.bind( asio::ip::udp::endpoint( asio::ip::address_v4::any() , port_nr ) );
	}

	_mark("sockets created");

	asio::ip::udp::endpoint remote_ep;

	std::mutex mutex_handlerflow_socket;

	// add first work - handler-flow
	auto func_spawn_flow = [&](int inbuf_nr, int socket_nr_raw) {
		assert(inbuf_nr >= 0);
		assert(socket_nr_raw >= 0);
		int socket_nr = socket_nr_raw % mysocket_in_strand.size(); // spread it (rotate)
		_mark("Creating workflow: buf="<<inbuf_nr<<" socket="<<socket_nr);

		auto inbuf_asio = asio::buffer( inbuf_tab.addr(inbuf_nr) , t_inbuf::size() );
		_dbg1("buffer size is: " << asio::buffer_size( inbuf_asio ) );
		_dbg1("async read, on mysocket="<<addrvoid(mysocket_in_strand));
		{
			// std::lock_guard< std::mutex > lg( mutex_handlerflow_socket ); // LOCK

			auto & this_socket_and_strand = mysocket_in_strand.at(socket_nr);

			this_socket_and_strand.get_unsafe_assume_in_strand().get().async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
					[&this_socket_and_strand, &inbuf_tab , inbuf_nr, &mutex_handlerflow_socket](const boost::system::error_code & ec, std::size_t bytes_transferred) {
						_dbg1("Handler (FIRST), size="<<bytes_transferred);
						handler_receive(e_algo_receive::after_first_read, ec,bytes_transferred, this_socket_and_strand, inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
					}
			); // start async
		}
	} ;

	if (cfg_buf_socket_spread==0) {
		for (size_t inbuf_nr = 0; inbuf_nr<cfg_num_inbuf; ++inbuf_nr) {	func_spawn_flow( inbuf_nr , inbuf_nr); }
	}
	else if (cfg_buf_socket_spread==1) {
		for (size_t inbuf_nr = 0; inbuf_nr<cfg_num_inbuf; ++inbuf_nr) {
			int socket_nr = static_cast<int>( inbuf_nr / static_cast<float>(cfg_num_inbuf ) * cfg_num_socket );
			// int socket_nr = static_cast<int>( inbuf_nr / static_cast<float>(inbuf_tab.buffers_count() ) * mysocket_in_strand.size() );
			func_spawn_flow( inbuf_nr , socket_nr );
		}
	}

	std::this_thread::sleep_for( std::chrono::milliseconds(100) );
	_goal("All started");
	func_show_summary();

	_goal("Waiting for all threads to end");
	thread_stop.join();
	for (auto & thr : thread_run_tab) {
		thr.join();
	}
	_goal("All threads done");

}


void asiotest(std::vector<std::string> options)
{
	_mark("asiotest");

  int port_num = 3456;

  asiotest_udpserv(options);
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



int main(int argc, const char **argv) {
	std::vector< std::string> options;
	for (int i=1; i<argc; ++i) options.push_back(argv[i]);
	for (const string & arg : options) if ((arg=="dbg")||(arg=="debug")||(arg=="d")) g_debug = true;
	_goal("Starting program");
	asiotest(options);
	_goal("Normal exit");
	return 0;
}

