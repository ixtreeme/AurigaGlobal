#include "stdafx.h"
#include "asio_engine.h"

#include <atomic>
#include <cassert>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace auriga::net
{
	namespace
	{
		struct Engine
		{
			boost::asio::io_context io;
			boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard;
			std::thread thread;

			std::mutex completionMutex;
			std::deque<CompletionFn> completions;

			std::atomic<bool> running { false };
			std::thread::id gameThreadId;

			Engine()
				: guard(boost::asio::make_work_guard(io))
			{
			}

			~Engine()
			{
				guard.reset();
				io.stop();

				if (thread.joinable() && thread.get_id() != std::this_thread::get_id())
					thread.join();
			}
		};

		Engine& State()
		{
			static Engine s_engine;
			return s_engine;
		}
	}

	boost::asio::io_context& Io()
	{
		return State().io;
	}

	void Start()
	{
		Engine& engine = State();

		bool expected = false;

		if (!engine.running.compare_exchange_strong(expected, true))
			return;

		if (engine.thread.joinable())
			engine.thread.join();

		if (engine.io.stopped())
			engine.io.restart();

		engine.gameThreadId = std::this_thread::get_id();
		engine.thread = std::thread([]() { State().io.run(); });
	}

	void Stop()
	{
		Engine& engine = State();

		if (!engine.running.exchange(false))
			return;

		engine.guard.reset();
		engine.io.stop();

		if (engine.thread.joinable() && engine.thread.get_id() != std::this_thread::get_id())
			engine.thread.join();
	}

	void PostIo(CompletionFn fn)
	{
		boost::asio::post(State().io, std::move(fn));
	}

	void PostCompletion(CompletionFn fn)
	{
		Engine& engine = State();

		std::lock_guard<std::mutex> lock(engine.completionMutex);
		engine.completions.push_back(std::move(fn));
	}

	void DrainCompletions()
	{
		Engine& engine = State();

		assert(IsGameThread() && "DrainCompletions must run on the game thread");

		std::deque<CompletionFn> ready;

		{
			std::lock_guard<std::mutex> lock(engine.completionMutex);
			ready.swap(engine.completions);
		}

		for (CompletionFn& fn : ready)
			fn();
	}

	bool IsGameThread()
	{
		Engine& engine = State();

		if (!engine.running.load(std::memory_order_acquire))
			return true;

		return engine.gameThreadId == std::this_thread::get_id();
	}
}
