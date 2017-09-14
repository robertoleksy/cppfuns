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

bool g_debug = true;

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

#define _erro(X) { std::cout<<"###### ERRROR: " << X<<std::endl; }
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
std::atomic<int> g_running_tuntap_jobs;
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

enum class t_mt_method {
	mt_unset=0, // not set
	mt_strand=1, // strands, wrap
	mt_mutex=2, // mutex, lock
};
t_mt_method cfg_mt_method = t_mt_method::mt_unset;

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

		// [asioflow]
		if (cfg_mt_method == t_mt_method::mt_strand) {
			mysocket.get_strand().post(
				mysocket.wrap(
				[&mysocket, &inbuf_tab , inbuf_nr, & mutex_handlerflow_socket]()
				{
					_dbg1("Handler (restart read)");
					handler_receive(e_algo_receive::after_processing_done, boost::system::error_code(),0, mysocket, inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
				}
				)
			);
		}
		else if (cfg_mt_method == t_mt_method::mt_mutex) {
			mysocket.get_strand().post(
				[&mysocket, &inbuf_tab , inbuf_nr, & mutex_handlerflow_socket]()
				{
					_dbg1("Handler (restart read)");
					handler_receive(e_algo_receive::after_processing_done, boost::system::error_code(),0, mysocket, inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
				}
			);
		}
		else throw std::runtime_error("unsupported mt");
	} // first_read or next_read

	// ---

	else if (algo_step==e_algo_receive::after_processing_done) {
		_dbg1("Restarting async read, on mysocket="<<addrvoid(mysocket));
		char* inbuf_data = & inbuf.m_data[0] ;
		auto inbuf_asio = asio::buffer( inbuf_data  , std::extent<decltype(inbuf.m_data)>::value );
		assert( asio::buffer_size( inbuf_asio ) > 0 );
		_dbg4("buffer size is: " << asio::buffer_size( inbuf_asio ) );

		if (cfg_mt_method == t_mt_method::mt_mutex) {
			std::lock_guard< std::mutex > lg( mutex_handlerflow_socket ); // *** LOCK ***
			// [asioflow]
			mysocket.get_unsafe_assume_in_strand() // in this block, we are using LOCKING so this is safe.
				.get()
				.async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
					[&mysocket, &inbuf_tab , inbuf_nr, & mutex_handlerflow_socket](const boost::system::error_code & ec, std::size_t bytes_transferred_again)
					{
						_dbg1("Handler (again), size="<<bytes_transferred_again<<", ec="<<ec.message());
						handler_receive(e_algo_receive::after_next_read, ec,bytes_transferred_again, mysocket, inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
					}
			);
		}
		else if (cfg_mt_method == t_mt_method::mt_strand) {
			// [asioflow]
			mysocket.get_unsafe_assume_in_strand() // we are called in a handler that should be wrapped, so this is safe
				.get()
				.async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
					[&mysocket, &inbuf_tab , inbuf_nr, & mutex_handlerflow_socket](const boost::system::error_code & ec, std::size_t bytes_transferred_again)
					{
						_dbg1("Handler (again), size="<<bytes_transferred_again<<", ec="<<ec.message());
						handler_receive(e_algo_receive::after_next_read, ec,bytes_transferred_again, mysocket, inbuf_tab,inbuf_nr, mutex_handlerflow_socket);
					}
			);
		}
		else throw std::runtime_error("unsupported mt");

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
	// the main "loop"

	g_atomic_exit=false;
	g_running_tuntap_jobs=0;

	g_recv_totall_count=0;
	g_recv_totall_size=0;
	g_recv_started = t_mytime{};

	auto func_show_usage = []() {
		std::cout << "\nUsage: program    inbuf   socket socket_spread   ios thread_per_ios  crypto_task  [OPTIONS]\n"
		<< "OPTIONS can be any of words: \n"
		<< "  mt_strand/mt_mutex\n"
		<< "  tuntap_block/tuntap_async\n"
		<< "  mport debug \n"
		<< "See code for more details. socket_spread must be 0 or 1.\n"
		<< "crypto_task must be 0, or >0.\n"
		<< "E.g.: program 32   2 0  4 16\n"
		<< std::endl;
	};

	if (options.size()<4) {
		func_show_usage();
		return;
	}

	const int cfg_num_inbuf = safe_atoi(options.at(0)); // 32 ; this is also the number of flows (p2p)
	const int cfg_num_socket_wire = safe_atoi(options.at(1)); // 2 ; number of sockets - wire (p2p)
	const int cfg_buf_socket_spread = safe_atoi(options.at(2)); // 0 is: (buf0,sock0),(b1,s1),(b2,s0),(b3,s1),(b4s0) ; 1 is (b0,s0),(b1,s0),(b2,s1),(b3,s1)

	const int cfg_port_faketuntap = 2345;

	const int cfg_num_ios = safe_atoi(options.at(3)); // 4
	const int cfg_num_thread_per_ios = safe_atoi(options.at(4)); // 16
	cfg_test_crypto_task = safe_atoi(options.at(5)); // 10

	const int cfg_num_weld_tuntap = safe_atoi(options.at(6)); // 16?
	const int cfg_num_socket_tuntap = safe_atoi(options.at(7)); // 4?

	vector<asio::ip::udp::endpoint> peer_pegs;
	//peer_pegs.emplace_back( asio::ip::address_v4::from_string("127.0.0.1") , 9000 );
	_note("Adding peer");
	peer_pegs.emplace_back(
		asio::ip::address_v4::from_string(options.at(8)) ,
		safe_atoi(options.at(9)  ));
	_note("Got peer(s) " << peer_pegs.size());


	bool tuntap_set=false;
	bool cfg_tuntap_blocking=false;
	for (const string & arg : options) if (arg=="tuntap_block")  {  tuntap_set=true;  cfg_tuntap_blocking=true;  }
	for (const string & arg : options) if (arg=="tuntap_async")  {  tuntap_set=true; cfg_tuntap_blocking=false; }
	if (!tuntap_set) {
		func_show_usage();
		std::cerr << endl << "ERROR: You must add option either tuntap_block or tuntap_async (or other option of this family, see source)" << endl;
		throw std::runtime_error("Must set tuntap method");
	}


	for (const string & arg : options) if (arg=="mt_strand")
		{ assert(cfg_mt_method==t_mt_method::mt_unset); cfg_mt_method=t_mt_method::mt_strand; }
	for (const string & arg : options) if (arg=="mt_mutex")
		{ assert(cfg_mt_method==t_mt_method::mt_unset); cfg_mt_method=t_mt_method::mt_mutex; }

	if (cfg_mt_method == t_mt_method::mt_unset) {
		func_show_usage();
		std::cerr << endl << "ERROR: You must add option either mt_strand or mt_mutex (or other option of this family, see source)" << endl;
		throw std::runtime_error("Must set mt method");
	}


	bool cfg_port_multiport = false;
	for (const string & arg : options) if (arg=="mport") cfg_port_multiport=true;

	auto func_show_summary = [&]() {
		std::ostringstream oss;
		oss<<"Summary: " << endl
			<< "  inbufs: " << cfg_num_inbuf << endl
			<< "  socket: " << cfg_num_socket_wire << " in spread: " << cfg_buf_socket_spread << endl
			<< "  ios: " << cfg_num_ios << " per each there are threads: " << cfg_num_thread_per_ios << endl
			<< "  option: " << "mport="<<yesno(cfg_port_multiport)<<" " << endl
			<< endl
			<< "Program functions:" << endl
			<< "  reading UDP P2P: yes (async multithread as above)" << endl
			<< "    decrypt E2E: faked (task=" << cfg_test_crypto_task << ")" << endl
			<< "    decrypt P2P: equals E2E" << endl
			<< "    re-route received P2P: no, throw away" << endl
			<< "    re-route received P2P, do P2P-crypto: no" << endl
			<< "    consume received P2P into our endpoint TUN: no, throw away" << endl
			<< "  reading local TUNTAP: *TODO* faked as UDP localhost port=" << cfg_port_faketuntap << endl
			<< "    using weld buffers: " << cfg_num_weld_tuntap << endl
			<< "    using sockets: " << cfg_num_socket_tuntap << " that are: " << (cfg_tuntap_blocking ? "BLOCKING" : "async") << endl
			<< "    encrypt E2E: no" << endl
			<< "    encrypt P2P: equals E2E" << endl
			<< "    send out endpoint data from TUN: YES, to peers: ";
		for (const auto &peg : peer_pegs) oss << peg << " ";
		oss << endl;
		_goal(oss.str());
	};
	func_show_summary();


	_goal("Starting test. cfg_test_crypto_task="<<cfg_test_crypto_task);

	std::vector<std::unique_ptr<asio::io_service>> ios;
	for (int i=0; i<cfg_num_ios; ++i) {
		_goal("Creating ios nr "<<i);
		ios.emplace_back( std::make_unique<asio::io_service>() );
	}

	int cfg_num_ios_tuntap = cfg_num_ios;

	std::vector<std::unique_ptr<asio::io_service>> ios_tuntap;
	for (int i=0; i<cfg_num_ios_tuntap; ++i) {
		_goal("Creating ios (TUNTAP) nr "<<i);
		ios_tuntap.emplace_back( std::make_unique<asio::io_service>() );
	}

	// have any long-term work to do (for ios)
	boost::asio::signal_set signals( * ios.at(0), SIGINT);
	signals.async_wait( handler_signal_term );

	std::this_thread::sleep_for( std::chrono::milliseconds(100) );

	vector<std::thread> thread_iosrun_tab; // threads for ios_run

	_note("Starting ios worker - ios.run"); // ios.run()
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
			thread_iosrun_tab.push_back( std::move( thread_run ) );
		}
	}


	// --- tuntap classes / data ---
	const int cfg_size_tuntap_maxread=9000;
	const int cfg_size_tuntap_buf=cfg_size_tuntap_maxread * 2;

	static const size_t fragment_pos_max=32;
	struct c_weld {
		unsigned char m_buf[cfg_size_tuntap_buf];

		uint16_t m_fragment_pos[fragment_pos_max]; ///< positions of ends fragments,
		/// as constant-size array, with separate index in it
		/// if array has values {5,10,11} then fragments are ranges: [0..4], [5..9], [10]

		uint16_t m_fragment_pos_ix; ///< index in above array

		size_t m_pos; ///< if ==0, then nothing yet is written; If ==10 then 0..9 is written, 10..end is free
		bool m_reserved; ///< do some thread now read this now?

		c_weld() { clear(); }
		void clear() {
			m_reserved=false;
			m_pos=0;
			m_fragment_pos_ix=0;
		}

		void add_fragment(uint16_t size) {
			assert(m_fragment_pos_ix < fragment_pos_max); // can not add anything since no place to store indexes

			// e.g. buf=10 (array is 0..9), m_pos=0 (nothing used), size=10 -> allowed
			assert(size <= cfg_size_tuntap_buf - m_pos);

			m_pos += size;
			m_fragment_pos[ m_fragment_pos_ix ] = m_pos;
			++m_fragment_pos_ix;
		}
		size_t space_left() const {
			if (m_fragment_pos_ix>=fragment_pos_max) return 0; // can not add anything since no place to store indexes
			return cfg_size_tuntap_buf - m_pos;
		}
		unsigned char * addr_at_pos() { return & m_buf[m_pos]; }
		unsigned char * addr_all() { return & m_buf[0]; }
	};

	// --- welds var ---
	vector<c_weld> welds;
	std::mutex welds_mutex;

	// stop / stats
	_dbg1("The stop thread"); // exit flag --> ios.stop()
	std::thread thread_stop(
		[&ios, &welds, &welds_mutex] {
			for (int i=0; true; ++i) {
				std::this_thread::sleep_for( std::chrono::seconds(1) );

				auto time_now = std::chrono::steady_clock::now();
				auto now_recv_totall_size = g_recv_totall_size.load();
				double now_recv_ellapsed_sec = ( std::chrono::duration_cast<std::chrono::milliseconds>(time_now - g_recv_started.load().m_time) ).count() / 1000.;
				double now_recv_speed = now_recv_totall_size / now_recv_ellapsed_sec; // B/s
				std::ostringstream oss;
				oss << "Loop, i="<<i<<" recv count=" << g_recv_totall_count << ", size=" << now_recv_totall_size
					<< " speed="<< (now_recv_speed/1000000) <<" MB/s";
				oss << " Welds: ";
				{
					std::lock_guard<std::mutex> lg(welds_mutex);
					for (const auto & weld : welds) {
						oss << "[" << weld.space_left() << " left; " << (weld.m_reserved ? "RESERVED" : "not-resv") << "]";
					}
				}
				_goal(oss.str());
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


	std::this_thread::sleep_for( std::chrono::milliseconds(100) ); // ---

	// sockets for (fake-)TUN connections:
	vector<with_strand<ThreadObject<asio::ip::udp::socket>>> tuntap_socket;
	for (int nr_sock=0; nr_sock<cfg_num_socket_tuntap; ++nr_sock) {
		int port_nr = cfg_port_faketuntap;
		_note("Creating TUNTAP socket #"<<nr_sock<<" on port " << port_nr);
		auto & one_ios = ios.at( 0 ); // same IOS for all TUN ;  nr_sock % ios.size() );

		auto & socket_array = tuntap_socket;
		socket_array.push_back( with_strand<ThreadObject<boost::asio::ip::udp::socket>>(*one_ios, *one_ios) );
		boost::asio::ip::udp::socket & thesocket = socket_array.back().get_unsafe_assume_in_strand().get();

		thesocket.open( asio::ip::udp::v4() );
		thesocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
		//thesocket.bind( asio::ip::udp::endpoint( asio::ip::address_v4::from_string("127.0.0.1") , port_nr ) );
		thesocket.bind( asio::ip::udp::endpoint( asio::ip::address_v4::any() , port_nr ) );
	}

	// sockets for p2p connections:
	vector<with_strand<ThreadObject<asio::ip::udp::socket>>> wire_socket;
	c_inbuf_tab inbuf_tab(cfg_num_inbuf);

	for (int nr_sock=0; nr_sock<cfg_num_socket_wire; ++nr_sock) {
		int port_nr = 9000;
		if (cfg_port_multiport) port_nr += nr_sock;
		_note("Creating wire (P2P) socket #"<<nr_sock<<" on port " << port_nr);
		//wire_socket.push_back({ios,ios}); // active udp // <--- TODO why not?
		auto & one_ios = ios.at( nr_sock % ios.size() );

		auto & socket_array = wire_socket;
		socket_array.push_back( with_strand<ThreadObject<boost::asio::ip::udp::socket>>(*one_ios, *one_ios) );
		boost::asio::ip::udp::socket & thesocket = socket_array.back().get_unsafe_assume_in_strand().get();

		thesocket.open( asio::ip::udp::v4() );
		thesocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
		// thesocket.bind( asio::ip::udp::endpoint( asio::ip::address::from_string("127.0.0.1") , 9000 ) );
		thesocket.bind( asio::ip::udp::endpoint( asio::ip::address_v4::any() , port_nr ) );
	}

	_mark("wire sockets created");

	asio::ip::udp::endpoint remote_ep;

	std::mutex mutex_handlerflow_socket_wire;


	std::this_thread::sleep_for( std::chrono::milliseconds(100) );

	// ---> tuntap: blocking version seems faster <---


	for (size_t i=0; i<cfg_num_weld_tuntap; ++i) welds.push_back( c_weld() );

	vector<std::thread> thread_tuntap_tab;

	// tuntap: DO WORK
	for (size_t tuntap_socket_nr=0; tuntap_socket_nr<cfg_num_socket_tuntap; ++tuntap_socket_nr) {
		_mark("Creating workflow (blocking - thread) for tuntap, socket="<<tuntap_socket_nr);


		std::thread thr = std::thread(
			[tuntap_socket_nr, &tuntap_socket, &welds, &welds_mutex, &wire_socket, &peer_pegs]()
			{
				++g_running_tuntap_jobs;

				while (!g_atomic_exit) {
					auto & one_socket = tuntap_socket.at(tuntap_socket_nr);
					_note("TUNTAP reading");

					size_t found_ix=0;
					bool found_any=false;

					{ // lock to find and reserve buffer a weld
						std::lock_guard<std::mutex> lg(welds_mutex);

						for (size_t i=0; i<welds.size(); ++i) {
							if (! welds.at(i).m_reserved) {
								if (welds.at(i).space_left() >= cfg_size_tuntap_maxread) {
									found_ix=i; found_any=true;
									break;
								}
							}
						}

						if (found_any) {
							auto & weld = welds.at(found_ix);
							weld.m_reserved=true; // we are using it now
						}
						else {
							_erro("No free tuntap buffers!");
							break ; // <---
						}
					} // lock operations on welds

					auto & found_weld = welds.at(found_ix);
					size_t receive_size = found_weld.space_left();
					assert(receive_size>0);
					void * buf_ptr = reinterpret_cast<void*>(found_weld.addr_at_pos());
					assert(buf_ptr);
					auto buf_asio = asio::buffer( buf_ptr , receive_size );

					int my_random = (tuntap_socket_nr*437213)%38132 + std::rand();
					try {
						//one_socket.get_unsafe_assume_in_strand().get().open( asio::ip::udp::v4() ); // set it to IPv4 mode
						//asio::ip::udp::endpoint ep;

						_note("TUNTAP read, on tuntap_socket_nr="<<tuntap_socket_nr<<" socket="<<addrvoid(one_socket)<<" "
							<<"into weld "<<found_ix<<" "
							<< "buffer size is: " << asio::buffer_size( buf_asio ) << " buf_ptr="<<buf_ptr);
						asio::ip::udp::endpoint ep;

						// [asioflow] read ***
						auto read_size = size_t { one_socket.get_unsafe_assume_in_strand().get().receive_from(buf_asio, ep) };

						_goal("TUNTAP socket="<<tuntap_socket_nr<<": got data from ep="<<ep<<" read_size="<<read_size
						<<" weld "<<found_ix);

						{
								auto & mysocket = wire_socket.at(0);
								mysocket.get_strand().post(
									mysocket.wrap(
										[]() { _goal("TEST - WRAPPED posted - running inside callback.. OK...."); }
									)
								);
								_goal("TEST - WRAPPED posted (should run now!");
						}

						// process data, and un-reserve it so that others can add more to it
						{ // lock
							std::lock_guard<std::mutex> lg(welds_mutex);
							c_weld & the_weld = welds.at(found_ix); // optimize: no need for mutex for this one
							the_weld.add_fragment(read_size);

							bool should_send = ! (the_weld.space_left() >= cfg_size_tuntap_maxread) ;
							_note("TUNTAP (weld "<<found_ix<<") decided to: " << (should_send ? "SEND-NOW" : "not-send-yet")
								<< " space left " << the_weld.space_left() << " vs needed space " << cfg_size_tuntap_maxread);

							if (should_send) { // almost full
								// send it
								// wire selection

								// select wire
								int wire_socket_nr = ((my_random*4823)%4913) % wire_socket.size(); // TODO better pseudo-random
								++my_random;

								_goal("TUNTAP sending out the data from tuntap socket="<<tuntap_socket_nr
									<<" via wire_socket_nr="<<wire_socket_nr);

								// [thread] this is SAFE probably, as we read-only access the peer_pegs, and that vector
								// is not changing
								// select peg
								asio::ip::udp::endpoint peer_peg = peer_pegs.at(0);


								/*
								// [asioflow]
								_erro("WARNING this is probably UB (thread race)"); // XXX FIXME [thread]
								this_wire_socket_and_strand.get_unsafe_assume_in_strand().get().async_send_to( // <-- THREAD I am NOT in strang, not allowed to use this !!! XXX
									send_buf_asio,
									peer_peg,
									[wire_socket_nr, &welds, &welds_mutex, found_ix](const boost::system::error_code & ec, std::size_t bytes_transferred) {
										_note("TUNTAP: data passed on and sent to peer. wire_socket_nr="<<wire_socket_nr
											<<" ec="<<ec.message()
										);
										{
											_goal("TUNTAP - DONE SENDING ... taking lock");
											std::lock_guard<std::mutex> lg(welds_mutex);
											auto & weld = welds.at(found_ix);
											_goal("TUNTAP - DONE SENDING - clearing weld " << found_ix);
											weld.clear();
										} // lock
									}
								);
								*/


								auto & mysocket = wire_socket.at(wire_socket_nr);
								mysocket.get_strand().post(
									mysocket.wrap(
										[wire_socket_nr, &wire_socket, &welds, &welds_mutex, found_ix, peer_peg]() {
											_note("(wrapped) - starting TUNTAP->WIRE transfer, wire "<<wire_socket_nr<<" weld " <<found_ix
											<<" XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");

											auto & wire = wire_socket.at(wire_socket_nr);

											auto send_buf_asio = asio::buffer( welds.at(found_ix).addr_all() , welds.at(found_ix).m_pos );

											wire.get_unsafe_assume_in_strand().get().async_send_to(
												send_buf_asio,
												peer_peg,
												[wire_socket_nr, &welds, &welds_mutex, found_ix](const boost::system::error_code & ec, std::size_t bytes_transferred) {
													_dbg1("TUNTAP: data passed on and sent to peer. wire_socket_nr="<<wire_socket_nr
														<<" ec="<<ec.message()
													);
													{
														_dbg1("TUNTAP - DONE SENDING (weld "<<found_ix<<") ... taking lock");
														std::lock_guard<std::mutex> lg(welds_mutex);
														auto & weld = welds.at(found_ix);
														_dbg1("TUNTAP - DONE SENDING - clearing weld " << found_ix);
														weld.clear();
													} // lock
												}
											);
											_note("(wrapped) - starting TUNTAP->WIRE transfer, wire "<<wire_socket_nr<<" weld " <<found_ix << " - ASYNC STARTED");


										} // delayed TUNTAP->WIRE
									) // wrap
								); // start(post) handler: TUNTAP->WIRE start

								_note("TUNTAP->Wire work is started(post) by weld " << found_ix << " to P2P socket="<<wire_socket_nr);

							} // send the full weld
							else { // do not send. weld extended with data
								_note("Removing reservation on weld " << found_ix);
								the_weld.m_reserved=false;
							}
						} // lock to un-reserve

						/*
						one_socket.get_unsafe_assume_in_strand().get().async_receive(buf_asio
							, [](const boost::system::error_code & ec, std::size_t bytes_transferred) {
								_goal("TUNTAP received, bytes_transferred="<<bytes_transferred<<" ec="<<ec.message());
							}
						);
						break; // exit loop
						*/
					}
					catch (std::exception &ex) {
						_erro("Error in TUNTAP lambda: "<<ex.what());
						std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
					}
					catch (...) {
						_erro("Error in TUNTAP lambda - unknown");
						std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
					}

					// process the TUN read data TODO

				} // loop forever

				--g_running_tuntap_jobs;
			} // the lambda
		);

		thread_tuntap_tab.push_back( std::move(thr) );

/*
		auto inbuf_asio = asio::buffer( inbuf_tab.addr(inbuf_nr) , t_inbuf::size() );
		_dbg1("buffer size is: " << asio::buffer_size( inbuf_asio ) );
		_dbg1("async read, on mysocket="<<addrvoid(wire_socket));
		{
			// std::lock_guard< std::mutex > lg( mutex_handlerflow_socket ); // LOCK

			auto & this_socket_and_strand = wire_socket.at(socket_nr);

			// [asioflow]
			this_socket_and_strand.get_unsafe_assume_in_strand().get().async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
					[&this_socket_and_strand, &inbuf_tab , inbuf_nr, &mutex_handlerflow_socket_wire](const boost::system::error_code & ec, std::size_t bytes_transferred) {
						_dbg1("Handler (FIRST), size="<<bytes_transferred);
						handler_receive(e_algo_receive::after_first_read, ec,bytes_transferred, this_socket_and_strand, inbuf_tab,inbuf_nr, mutex_handlerflow_socket_wire);
					}
			); // start async
		}
		*/
	};
	_mark("TUNTAP work started");


	std::this_thread::sleep_for( std::chrono::milliseconds(100) );

	// wire P2P: add first work - handler-flow
	auto func_spawn_flow_wire = [&](int inbuf_nr, int socket_nr_raw) {
		assert(inbuf_nr >= 0);
		assert(socket_nr_raw >= 0);
		int socket_nr = socket_nr_raw % wire_socket.size(); // spread it (rotate)
		_mark("Creating workflow: buf="<<inbuf_nr<<" socket="<<socket_nr);

		auto inbuf_asio = asio::buffer( inbuf_tab.addr(inbuf_nr) , t_inbuf::size() );
		_dbg1("buffer size is: " << asio::buffer_size( inbuf_asio ) );
		_dbg1("async read, on mysocket="<<addrvoid(wire_socket));
		{
			// std::lock_guard< std::mutex > lg( mutex_handlerflow_socket ); // LOCK

			auto & this_socket_and_strand = wire_socket.at(socket_nr);

			// [asioflow]
			this_socket_and_strand.get_unsafe_assume_in_strand().get().async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
					[&this_socket_and_strand, &inbuf_tab , inbuf_nr, &mutex_handlerflow_socket_wire](const boost::system::error_code & ec, std::size_t bytes_transferred) {
						_dbg1("Handler (FIRST), size="<<bytes_transferred);
						handler_receive(e_algo_receive::after_first_read, ec,bytes_transferred, this_socket_and_strand, inbuf_tab,inbuf_nr, mutex_handlerflow_socket_wire);
					}
			); // start async
		}
	} ;

	if (cfg_buf_socket_spread==0) {
		for (size_t inbuf_nr = 0; inbuf_nr<cfg_num_inbuf; ++inbuf_nr) {	func_spawn_flow_wire( inbuf_nr , inbuf_nr); }
	}
	else if (cfg_buf_socket_spread==1) {
		for (size_t inbuf_nr = 0; inbuf_nr<cfg_num_inbuf; ++inbuf_nr) {
			int socket_nr = static_cast<int>( inbuf_nr / static_cast<float>(cfg_num_inbuf ) * cfg_num_socket_wire );
			// int socket_nr = static_cast<int>( inbuf_nr / static_cast<float>(inbuf_tab.buffers_count() ) * wire_socket.size() );
			func_spawn_flow_wire( inbuf_nr , socket_nr );
		}
	}

	std::this_thread::sleep_for( std::chrono::milliseconds(100) );

	_goal("All started");
	func_show_summary();

	_goal("Waiting for all threads to end");
	_goal("Join stop threads");
	thread_stop.join();

	_goal("Stopping tuntap threads - unblocking them with some self-sent data");
	while (g_running_tuntap_jobs>0) {
		_note("Sending data to unblock...");

		asio::io_service ios_local;
		auto & one_ios = ios_local;

		asio::ip::udp::socket socket_local(one_ios);
		boost::asio::ip::udp::socket & thesocket = socket_local;
		thesocket.open( asio::ip::udp::v4() );
		thesocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
		unsigned char data[1]; data[0]=0;
		auto buff = asio::buffer( reinterpret_cast<void*>(&data[0]), 1 );
		auto dst = asio::ip::udp::endpoint( asio::ip::address::from_string("127.0.0.1") , cfg_port_faketuntap  );
		_note("Sending to dst=" << dst);
		thesocket.send_to( buff , dst );

		std::this_thread::sleep_for( std::chrono::milliseconds(10) );
	}
	_goal("Join tuntap threads");
	for (auto & thr : thread_tuntap_tab) {
		thr.join();
	}

	_goal("Join iosrun threads");
	for (auto & thr : thread_iosrun_tab) {
		thr.join();
	}

	_goal("All threads done");

}



int main(int argc, const char **argv) {
	std::vector< std::string> options;
	for (int i=1; i<argc; ++i) options.push_back(argv[i]);
	for (const string & arg : options) if ((arg=="dbg")||(arg=="debug")||(arg=="d")) g_debug = true;
	_goal("Starting program");
  asiotest_udpserv(options);
	_goal("Normal exit");
	return 0;
}



// ========================================================
// unused - example
void old_tests() {
  int port_num = 3456;

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

