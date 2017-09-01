#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::udp;

enum { max_length = 655035 };

int main(int argc, char *argv[])
{
	//const char *host = "192.168.1.103";
	//const char *host = "127.0.0.1";
	//const char *port = "9000";
	if(argc < 3)
		std::cout << "Usage: ./client <host> <port>" << std::endl;
	const char *host = argv[1];
	const char *port = argv[2];
	boost::asio::io_service io_service;

	udp::socket s(io_service, udp::endpoint(udp::v4(), 0));

	udp::resolver resolver(io_service);
	udp::endpoint endpoint = *resolver.resolve({udp::v4(), host, port});

	while(true){
		char request[max_length];
		size_t bytes, count, request_length;
		std::cout << "message ,bytes (if bytes==1 send message else send first byte multipled by bytes) and count: ";
		std::cin >> request >> bytes >> count;
		if (bytes > max_length) {
			std::cout << "too long message" << std::endl;
			continue;
		}
		if (bytes == 1)
			request_length = std::strlen(request);
		else {
			request_length = bytes;
			char ch = request[0];
			std::fill_n(request, request_length, ch);
		}

		for (size_t i=0; i<count; i++)
			s.send_to(boost::asio::buffer(request, request_length), endpoint);
	}
/*
	char reply[max_length];
	udp::endpoint sender_endpoint;
	size_t reply_length = s.receive_from(
			boost::asio::buffer(reply, max_length), sender_endpoint);
	std::cout << "Reply is: ";
	std::cout.write(reply, reply_length);
	std::cout << "\n";
*/
return 0;
}
