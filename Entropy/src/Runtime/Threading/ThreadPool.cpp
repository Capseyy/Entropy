#include "ThreadPool.h"


ThreadPool::ThreadPool(size_t threads) {
	if (threads == 0) threads = 1;
	workers_.reserve(threads);
	for (size_t i = 0; i < threads; ++i) {
		workers_.emplace_back([this] { worker_(); });
	}
}


ThreadPool::~ThreadPool() {
	{
		std::lock_guard<std::mutex> lk(m_);
		stop_ = true;
	}
	cv_.notify_all();
	for (auto& t : workers_) if (t.joinable()) t.join();
}


void ThreadPool::worker_() {
	for (;;) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lk(m_);
			cv_.wait(lk, [this] { return stop_ || !q_.empty(); });
			if (stop_ && q_.empty()) return;
			task = std::move(q_.front());
			q_.pop();
		}
		task();
	}
}