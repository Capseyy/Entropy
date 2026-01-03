#pragma once
#include <functional>
#include <mutex>
#include <queue>


class MainThreadQueue {
public:
	void Enqueue(std::function<void()> fn) {
		std::lock_guard<std::mutex> lk(m_);
		q_.push(std::move(fn));
	}
	void Drain() {
		for (;;) {
			std::function<void()> fn;
			{
				std::lock_guard<std::mutex> lk(m_);
				if (q_.empty()) break;
				fn = std::move(q_.front());
				q_.pop();
			}
			fn();
		}
	}
	void RunSlice(size_t maxJobs, int maxMillis) {
		using clock = std::chrono::steady_clock;
		const auto deadline = clock::now() + std::chrono::milliseconds(maxMillis);
		size_t done = 0;
		for (;;) {
			std::function<void()> job;
			{
				std::lock_guard<std::mutex> lk(m_);
				if (q_.empty()) break;
				job = std::move(q_.front()); q_.pop();
			}
			job();                       
			if (++done >= maxJobs) break;
			if (clock::now() >= deadline) break;
		}
	}
private:
	std::mutex m_;
	std::queue<std::function<void()>> q_;
};