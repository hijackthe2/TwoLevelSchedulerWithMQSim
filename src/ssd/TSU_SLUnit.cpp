#include "TSU_SLUnit.h"

namespace SSD_Components
{
	TSU_SLUnit::TSU_SLUnit(unsigned int _minimum_capacity, const unsigned int _stream_count):
		actual_capacity(_minimum_capacity), stream_count(_stream_count), total_enqueued(0), minimum_capacity(_minimum_capacity)
	{
		buffer = new Flash_Transaction_Queue[stream_count];
		currently_queuing = new unsigned int[stream_count];
		limited_capacity = new unsigned int[stream_count];
		priority_order.resize(stream_count);
		unsigned int avg_capacity = (unsigned int)((float)actual_capacity / stream_count + 0.5);
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			currently_queuing[stream_id] = 0;
			limited_capacity[stream_id] = avg_capacity;
			priority_order[stream_id] = stream_id;
		}
	}
	TSU_SLUnit::~TSU_SLUnit()
	{
		delete[] buffer;
		delete[] currently_queuing;
		delete[] limited_capacity;
	}

	Flash_Transaction_Queue* TSU_SLUnit::get_buffer()
	{
		return this->buffer;
	}

	Flash_Transaction_Queue* TSU_SLUnit::get_buffer(stream_id_type stream_id)
	{
		return &this->buffer[stream_id];
	}

	void TSU_SLUnit::charging(NVM_Transaction_Flash* transaction)
	{
		buffer[transaction->Stream_id].push_back(transaction);
	}

	NVM_Transaction_Flash* TSU_SLUnit::discharging()
	{
		for (unsigned int i = 0; i < stream_count; ++i)
		{
			stream_id_type stream_id = priority_order[i];
			if (!buffer[stream_id].empty() && currently_queuing[stream_id] < limited_capacity[stream_id])
			{
				NVM_Transaction_Flash* transaction = buffer[stream_id].front(); buffer[stream_id].pop_front();
				++currently_queuing[stream_id];
				return transaction;
			}
		}
		return NULL;
	}

	void TSU_SLUnit::discharging(Flash_Transaction_Queue** queue)
	{
		for (unsigned int i = 0; i < stream_count; ++i)
		{
			stream_id_type stream_id = priority_order[i];
			while (buffer[stream_id].size() && currently_queuing[stream_id] < limited_capacity[stream_id])
			{
				NVM_Transaction_Flash* transaction = buffer[stream_id].front(); buffer[stream_id].pop_front();
				queue[transaction->Address.ChannelID][transaction->Address.ChipID].push_back(transaction);
				++currently_queuing[stream_id];
			}
		}
	}

	void TSU_SLUnit::feedback(const stream_id_type stream_id)
	{
		--currently_queuing[stream_id];
		++total_enqueued;
		if (total_enqueued < actual_capacity) return;
		total_enqueued = 0;
		size_t total_buffer_size = 0;
		update_capacity(total_buffer_size);
		// ascending order by buffer size
		sort(priority_order.begin(), priority_order.end(), [=](stream_id_type a, stream_id_type b) {return buffer[a].size() < buffer[b].size(); });
		assign_capacity(actual_capacity, (unsigned int)total_buffer_size, 0, stream_count - 1);
		// ascending order by limited capacity
		sort(priority_order.begin(), priority_order.end(),
			[=](stream_id_type a, stream_id_type b) {return limited_capacity[a] < limited_capacity[b]; });
	}

	void TSU_SLUnit::update_capacity(size_t& total_buffer_size)
	{
		size_t min_buffer_size = INT_MAX; // record minimum buffer size that is greater than zero
		unsigned int live_stream_count = 0; // count live stream
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (!buffer[stream_id].empty())
			{
				min_buffer_size = std::min(min_buffer_size, buffer[stream_id].size());
				total_buffer_size += buffer[stream_id].size();
				live_stream_count++;
			}
			// reordering priority
			priority_order[stream_id] = stream_id;
		}
		if (live_stream_count == 0)
			actual_capacity = stream_count * minimum_capacity;
		else if (live_stream_count == 1)
			actual_capacity = std::max((unsigned int)total_buffer_size, stream_count * minimum_capacity);
		else
		{
			unsigned int maximum_capacity = stream_count * minimum_capacity;
			unsigned int desired_capacity = stream_count * (unsigned int)min_buffer_size;
			actual_capacity = std::min(maximum_capacity, std::max(minimum_capacity, desired_capacity));
		}
		//live_stream_count = std::max(live_stream_count, (unsigned int)1);
		//live_stream_count = stream_count;
		//unsigned int maximum_capacity = live_stream_count * minimum_capacity;
		//unsigned int desired_capacity = live_stream_count * (unsigned int)min_buffer_size;
		//// minimum_capacity <= actual_capacity <= maximum capacity
		//actual_capacity = std::min(maximum_capacity, std::max(minimum_capacity, desired_capacity));
	}

	void TSU_SLUnit::assign_capacity(unsigned int left_capacity, unsigned int left_buffer_size, unsigned int start, unsigned int end)
	{
		if (start > end || start < 0 || end >= stream_count) return;
		int cnt = end - start + 1;
		unsigned int avg_capacity = (unsigned int)((float)left_capacity / cnt + 0.5);
		if (buffer[priority_order[start]].size() >= avg_capacity || buffer[priority_order[end]].size() <= avg_capacity) // every workload can get avg_capacity
		{
			for (unsigned int i = start; i <= end; ++i)
			{
				limited_capacity[priority_order[i]] = avg_capacity;
			}
		}
		else if (left_buffer_size <= left_capacity) // workloads with larger buffer size are prioritized to assign
		{
			unsigned int i = end;
			while (i >= start && buffer[priority_order[i]].size() >= avg_capacity)
			{
				unsigned int size = (unsigned int)buffer[priority_order[i]].size();
				limited_capacity[priority_order[i]] = size;
				left_capacity -= size;
				left_buffer_size -= size;
				--i;
			}
			assign_capacity(left_capacity, left_buffer_size, start, i);
		}
		else // workloads with smaller buffer size are prioritized to assign
		{
			unsigned int i = start;
			while (i <= end && buffer[priority_order[i]].size() <= avg_capacity)
			{
				unsigned int size = (unsigned int)buffer[priority_order[i]].size();
				limited_capacity[priority_order[i]] = size;
				left_capacity -= size;
				left_buffer_size -= size;
				++i;
			}
			assign_capacity(left_capacity, left_buffer_size, i, end);
		}
	}
}