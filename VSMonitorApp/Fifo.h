#pragma once
#include <queue>
#include <mutex>

template<typename T>
class Fifo
{
public:
	Fifo(uint32_t maxSize) : m_MaxSize{ maxSize }
	{
	};

	T Pop()
	{
		std::unique_lock<std::mutex> lk{ m_Mutex };
		while (m_Queue.empty()) // wait for new data to arrive. NOTE: this might not work if notify is called before wait!
		{
			m_NewDataCondition.wait(lk);
		}
		T elem = std::move(m_Queue.front()); // can be faster than making copy
		m_Queue.pop();

		return elem;
	}

	bool Push(const T& element)
	{
		std::unique_lock<std::mutex> lk{ m_Mutex };
		if (m_Queue.size() >= m_MaxSize) return false;
		m_Queue.push(element);
		lk.unlock();

		m_NewDataCondition.notify_one();

		return true;
	}

	bool IsEmpty()
	{
		std::scoped_lock<std::mutex> lk{ m_Mutex };
		return m_Queue.empty();
	}

private:
	uint32_t m_MaxSize;
	std::queue<T> m_Queue;
	std::condition_variable m_NewDataCondition;
	mutable std::mutex m_Mutex;
};

