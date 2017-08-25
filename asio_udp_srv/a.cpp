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

#if 0
#define _dbg4(X) {}
#define _dbg1(X) { std::cout<<X<<std::endl; }
#define _note(X) { std::cout<<X<<std::endl; }
#define _mark(X) _note(X);
#define _goal(X) _note(X);
#else
#define _dbg4(X) {}
#define _dbg1(X) {}
#define _note(X) {}
#define _mark(X) {}
#define _goal(X) _note(X);
#endif

#define UsePtr(X) (* (X) )
#define addrvoid(X) ( static_cast<const void*>( & (X) ) )

using namespace boost;
using std::vector;
using std::unique_ptr;
using std::make_unique;

std::atomic<bool> g_atomic_exit;

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
	_note("Signal! (control-C?) " << signal_number);
	g_atomic_exit = true;
}

void handler_receive(const boost::system::error_code & ec, std::size_t bytes_transferred,
	asio::ip::udp::socket & mysocket,
	c_inbuf_tab & inbuf_tab, size_t inbuf_nr)
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
	static const char * marker = "exit";
	static const size_t marker_len = strlen(marker);
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

	unsigned char bbb=0;
	for (int j=0; j<100; ++j) {
		unsigned char aaa=0;
			for (size_t pos=0; pos<bytes_transferred; ++pos) {
			aaa ^= inbuf.m_data[pos] & inbuf.m_data[ (pos*(j+2))%bytes_transferred ];
		}
		bbb ^= aaa;
	}

	_dbg4("Restarting async read, on mysocket="<<addrvoid(mysocket));
	mysocket.async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
		[&mysocket, &inbuf_tab , inbuf_nr](const boost::system::error_code & ec, std::size_t bytes_transferred_again) {
			_dbg1("Handler (again), size="<<bytes_transferred_again<<", ec="<<ec.message());
			handler_receive(ec,bytes_transferred_again, mysocket, inbuf_tab,inbuf_nr);
		}
	);
	_dbg1("Restarting async read - done");
}

void asiotest_udpserv() {
	g_atomic_exit=false;

	asio::io_service ios;

	// TODO XXX
	_goal("WARNING this is NOT THREAD SAFE (race on the socket accsssed from handlers across different handler-flows");
	const int cfg_num_inbuf = 16;
	const int cfg_num_thread_per_ios = 16;

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
				_note("Waiting, i="<<i);
				if (g_atomic_exit) {
					_note("Exit flag is set, exiting loop and will stop program");
					break;
				}
			}
			ios.stop(); // thread safe, asio::io_service is TS for most functions
		}
	);

	c_inbuf_tab inbuf_tab(16);

	asio::ip::udp::socket mysocket(ios); // active udp
	_note("bind");
	mysocket.open( asio::ip::udp::v4() );
	mysocket.bind( asio::ip::udp::endpoint( asio::ip::address_v4::any() , 9000 ) );
	asio::ip::udp::endpoint remote_ep;

	// add first work - handler-flow
	for (size_t inbuf_nr = 0; inbuf_nr<cfg_num_inbuf; ++inbuf_nr) {
		auto inbuf_asio = asio::buffer( inbuf_tab.addr(inbuf_nr) , t_inbuf::size() );
		_dbg1("buffer size is: " << asio::buffer_size( inbuf_asio ) );
		_dbg1("async read, on mysocket="<<addrvoid(mysocket));
		mysocket.async_receive_from( inbuf_asio , inbuf_tab.get(inbuf_nr).m_ep ,
			[&mysocket, &inbuf_tab , inbuf_nr](const boost::system::error_code & ec, std::size_t bytes_transferred) {
				_dbg1("Handler (FIRST), size="<<bytes_transferred);
				handler_receive(ec,bytes_transferred, mysocket,inbuf_tab,inbuf_nr);
			}
		);
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
	_note("Normal exit");
	return 0;
}

