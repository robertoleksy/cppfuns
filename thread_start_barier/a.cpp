#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <system_error>
#include <condition_variable>

void SL(int d) {
	std::this_thread::sleep_for( std::chrono::milliseconds(d) );
}


int main() {
	const uint32_t worker_count=3; // in runtime
	// workers notify main that they are ready:
	uint32_t worker_ready_countdown; // how many remain and are NOT yet ready
	std::condition_variable worker_ready_cv; // will notify via this cv
	std::mutex worker_ready_mu; // the lock
	// main notifies workers that it's time to start working
	bool worker_start_flag; // should worker start working now?
	std::condition_variable worker_start_cv; // will notify via this cv
	std::mutex worker_start_mu; // the lock

	std::vector<std::thread> worker_thread;

	worker_ready_countdown = worker_count; // will need this many threads to be ready before start
	worker_start_flag = false; // do not start yet

	for (uint32_t worker_nr=0; worker_nr<worker_count; ++worker_nr) {
		SL(100);
		std::thread thr( [ worker_nr , & worker_ready_cv, & worker_ready_countdown, & worker_ready_mu,
			& worker_start_cv, & worker_start_flag, & worker_start_mu
		  ](){
			{ // signal -> main
				SL(300);
				std::unique_lock<std::mutex> lg( worker_ready_mu );
				SL(300);
				--worker_ready_countdown; // I am ready, so one less. Safe: under lock
				SL(300);
				worker_ready_cv.notify_one(); // tell main to check
				SL(300);
				std::cout<<"#" << std::setw(3) << worker_nr << " Told main that I'm ready\n";
			}
			{ // wait <- main
				SL(300);
				std::unique_lock<std::mutex> lg( worker_start_mu );
				worker_start_cv.wait(lg, [ & worker_start_flag ](){ return worker_start_flag; });
			}
			std::cout<<"#" << std::setw(3) << worker_nr << " - I AM WORKING NOW\n";
			SL(1000);
			std::cout<<"#" << std::setw(3) << worker_nr << " - done, exiting now\n";
			SL(100);
		} );
		std::cout<<"MAIN, I started in background #" << std::setw(3) << worker_nr << "\n";
		SL(100);
		worker_thread.push_back( std::move(thr) );
		SL(100);
	}
	// workers are now starting, some (or all) maybe even decreased worker_ready_countdown already

	{
		// waiting untill countdown is 0, checking when signalled via cv by workers
		std::unique_lock<std::mutex> lg( worker_ready_mu );
		worker_ready_cv.wait(lg, [ & worker_ready_countdown ](){ return worker_ready_countdown==0; });
	}
	// all workers said they are ready

	// tell all workers to start then
	{
		SL(800);
		std::cout<<"MAIN, I will tell them to start\n";
		std::unique_lock<std::mutex> lg( worker_start_mu );
		worker_start_flag = true;
		std::cout<<"MAIN, I will tell them to start ... notify...\n";
		worker_start_cv.notify_all();
		std::cout<<"MAIN, I will tell them to start ... notify... OK\n";
		SL(100);
	}

	std::cout<<"MAIN, joining... \n";
	// wait/join all workers back
	for (auto & thr : worker_thread) thr.join();
	std::cout<<"MAIN, join DONE\n";
}



