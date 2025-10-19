#pragma once
#include <memory>
#include <future>
#include <chrono>


template<typename T>
struct AssetHandle {
	std::shared_future<std::shared_ptr<T>> future;
	bool Ready() const { return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
	std::shared_ptr<T> Get() const { return future.get(); }
};