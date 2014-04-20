#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class BlockingQueue
{
public:
	BlockingQueue(): totalSize(0), totalSizeLimit(static_cast<size_t>(-1))
	{
	}

	explicit BlockingQueue(size_t limit): totalSize(0), totalSizeLimit(limit)
	{
	}

	void push(const T& value, size_t size = 0)
	{
		unique_lock<mutex> lock(itemsMutex);

		itemsNotFull.wait(lock, [&]() { return !(totalSize != 0 && totalSize + size > totalSizeLimit); });

		Item item = {value, size};
		items.push(item);
		totalSize += size;

		lock.unlock();
		itemsNotEmpty.notify_one();
	}

	T pop()
	{
		unique_lock<mutex> lock(itemsMutex);

		itemsNotEmpty.wait(lock, [&]() { return !items.empty(); });
        
        return popInternal(lock);
	}
    
    bool pop(T& value)
    {
		unique_lock<mutex> lock(itemsMutex);
        
        if (!items.empty())
        {
            value = popInternal(lock);
            
            return true;
        }
        else
        {
            return false;
        }
    }

private:
	T popInternal(unique_lock<mutex>& lock)
	{
		Item item = items.front();
		items.pop();

		if (item.size > 0)
		{
			assert(totalSize >= item.size);
			totalSize -= item.size;

			lock.unlock();
			itemsNotFull.notify_all();
		}

		return item.value;
	}
    
	struct Item
	{
		T value;
		size_t size;
	};

	mutex itemsMutex;
	condition_variable itemsNotEmpty;
	condition_variable itemsNotFull;

	queue<Item> items;
	size_t totalSize;
	size_t totalSizeLimit;
};
