#pragma once

#include <boost/asio.hpp>
#include <chrono>

// Single-threaded Asio engine shared by the server targets.
//
// The game loop keeps its synchronous, pulse-based pull architecture:
// every tick calls Poll(), which dispatches the ready completion handlers.
// Socket I/O itself is performed by Asio, so the platform specific
// select()/kqueue() implementations and the raw socket wrappers are no
// longer needed on the network path.
namespace auriga::net
{
	using tcp = boost::asio::ip::tcp;

	inline boost::asio::io_context& Io()
	{
		static boost::asio::io_context s_io;
		return s_io;
	}

	inline void Poll()
	{
		boost::asio::io_context& io = Io();

		if (io.stopped())
			io.restart();

		io.poll();
	}

	template <typename Rep, typename Period>
	inline void RunFor(const std::chrono::duration<Rep, Period>& duration)
	{
		boost::asio::io_context& io = Io();

		if (io.stopped())
			io.restart();

		io.run_for(duration);
	}
}
