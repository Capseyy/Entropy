#pragma once
#include <unordered_map>
#include <memory>
#include <future>
#include <mutex>






template<typename T>
class AssetCache {
public:
	using Ptr = std::shared_ptr<T>;
	using Future = std::shared_future<Ptr>;


	template<typename Loader>
	Future GetOrLoad(uint32_t id, Loader&& loader) {
		{
			std::lock_guard<std::mutex> lk(m_);
			if (auto it = loaded_.find(id); it != loaded_.end()) {
				if (auto sp = it->second.lock()) return make_ready_(sp);
			}
			if (auto it = inflight_.find(id); it != inflight_.end()) return it->second;
		}


		std::promise<Ptr> p;
		auto fut = p.get_future().share();
		{
			std::lock_guard<std::mutex> lk(m_);
			if (auto it = inflight_.find(id); it != inflight_.end()) return it->second;
			inflight_[id] = fut;
		}


		auto start = [this, id, pr = std::move(p), loader = std::forward<Loader>(loader)]() mutable {
			try {
				Ptr ptr = loader();
				{
					std::lock_guard<std::mutex> lk(m_);
					loaded_[id] = ptr;
					inflight_.erase(id);
				}
				pr.set_value(std::move(ptr));
			}
			catch (...) {
				std::lock_guard<std::mutex> lk(m_);
				inflight_.erase(id);
				pr.set_exception(std::current_exception());
			}
			};
		start();
		return fut;
	}


	void Clear() {
		std::lock_guard<std::mutex> lk(m_);
		loaded_.clear();
		inflight_.clear();
	}


private:
	Future make_ready_(const Ptr& sp) {
		std::promise<Ptr> p; p.set_value(sp);
		return p.get_future().share();
	}


	std::unordered_map<uint32_t, std::weak_ptr<T>> loaded_;
	std::unordered_map<uint32_t, Future> inflight_;
	std::mutex m_;
};