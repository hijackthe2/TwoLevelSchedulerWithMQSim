#ifndef TSU_CHIPLEVELBUFFER_H
#define TSU_CHIPLEVELBUFFER_H

#include "Flash_Transaction_Queue.h"
#include "NVM_Transaction_Flash.h"
#include "TSU_Base.h"
#include "Address_Mapping_Unit_Base.h"
#include <list>
#include <vector>

namespace SSD_Components
{
	class TSU_ChipLevelBuffer
	{
	public:
		TSU_ChipLevelBuffer(unsigned int _channel_count, unsigned int _chip_count_per_channel, unsigned int _stream_count,
			const unsigned int _max_limit);
		~TSU_ChipLevelBuffer();
		void check_in(NVM_Transaction_Flash* transaction);
		void check_out(Address_Mapping_Unit_Base* amu, TSU_Base* tsu, std::list<Flash_Transaction_Queue*>& queue_list,
			Flash_Transaction_Queue* queue, Flash_Transaction_Queue* related_queue, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		NVM_Transaction_Flash* someone(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		bool empty(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		size_t size(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		size_t size(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id, stream_id_type stream_id);
		Flash_Transaction_Queue* somebuffer(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id, stream_id_type stream_id);
	private:
		Flash_Transaction_Queue*** buffer;
		unsigned int channel_count;
		unsigned int chip_count_per_channel;
		unsigned int stream_count;
		const unsigned int max_limit;
		NVM::FlashMemory::Physical_Page_Address** global_rra;
	};
}

#endif // !TSU_CHIPLEVELBUFFER_H