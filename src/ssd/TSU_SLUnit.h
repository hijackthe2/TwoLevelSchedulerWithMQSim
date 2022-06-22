#ifndef TSU_SLUNIT_H
#define TSU_SLUNIT_H

#include "Flash_Transaction_Queue.h"
#include "NVM_Transaction_Flash.h"
#include <algorithm>
#include <limits.h>
#include <vector>

namespace SSD_Components
{
	class TSU_SLUnit
	{
	public:
		TSU_SLUnit(unsigned int _minimum_capacity, const unsigned int _stream_count);
		~TSU_SLUnit();
		Flash_Transaction_Queue* get_buffer();
		Flash_Transaction_Queue* get_buffer(stream_id_type stream_id);
		void charging(NVM_Transaction_Flash* transaction);
		NVM_Transaction_Flash* discharging();
		void discharging(Flash_Transaction_Queue** queue);
		void feedback(const stream_id_type stream_id);
	private:
		void update_capacity(size_t& total_buffer_size);
		void assign_capacity(unsigned int left_capacity, unsigned int left_buffer_size, unsigned int start, unsigned int end);
		unsigned int actual_capacity;
		unsigned int minimum_capacity;
		unsigned int stream_count;
		Flash_Transaction_Queue* buffer;
		unsigned int* currently_queuing;
		unsigned int* limited_capacity;
		std::vector<stream_id_type> priority_order;
		unsigned int total_enqueued;
	};
}

#endif // !TSU_SLUNIT_H
