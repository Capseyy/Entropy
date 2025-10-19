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
private:
	std::mutex m_;
	std::queue<std::function<void()>> q_;
};