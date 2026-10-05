#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <functional>

// Threaded Asio engine shared by the server targets.
//
// The engine owns a dedicated io thread: Start() spawns it and keeps the
// io_context alive with an executor_work_guard, Stop() resets the guard and
// joins. Socket I/O and every asynchronous operation run on that thread.
//
// Game logic stays single-threaded: the io thread never calls into the game
// directly. Completion handlers hand their result over through a thread-safe
// completion queue (PostCompletion), which the game loop drains once per tick
// with DrainCompletions(). Work that has to touch the socket (write starts,
// socket closes, connector setup) is posted onto the io thread from the game
// thread with PostIo().
namespace auriga::net
{
	using tcp = boost::asio::ip::tcp;
	using CompletionFn = std::function<void()>;

	boost::asio::io_context& Io();

	// Starts the dedicated io thread. Idempotent; safe to call again after
	// Stop() (the previous thread is joined first).
	void Start();

	// Resets the work guard, stops the io_context and joins the io thread.
	// Idempotent. Outstanding handlers are abandoned, which is intentional
	// on the shutdown path.
	void Stop();

	// Game thread -> io thread. Thread-safe.
	void PostIo(CompletionFn fn);

	// io thread -> game-thread completion queue. Thread-safe.
	void PostCompletion(CompletionFn fn);

	// Game thread: runs every completion queued since the previous drain.
	void DrainCompletions();

	// True when the calling thread is the one that called Start() (or when
	// the engine was never started, e.g. in unit tests).
	bool IsGameThread();
}
