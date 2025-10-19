#pragma once
#include <future>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <type_traits>


class ThreadPool {
public:
	explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());
	~ThreadPool();


	template<class F, class... Args>
	auto Submit(F&& f, Args&&... args)
		-> std::future<std::invoke_result_t<F, Args...>> {
		using Ret = std::invoke_result_t<F, Args...>;
		auto task = std::make_shared<std::packaged_task<Ret()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);
		std::future<Ret> fut = task->get_future();
		{
			std::lock_guard<std::mutex> lk(m_);
			q_.emplace([task]() { (*task)(); });
		}
		cv_.notify_one();
		return fut;
	}


private:
	void worker_();


	std::vector<std::thread> workers_;
	std::queue<std::function<void()>> q_;
	std::mutex m_;
	std::condition_variable cv_;
	bool stop_ = false;
};