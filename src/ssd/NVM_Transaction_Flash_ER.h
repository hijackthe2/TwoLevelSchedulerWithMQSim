#ifndef NVM_TRANSACTION_FLASH_ER_H
#define NVM_TRANSACTION_FLASH_ER_H

#include <list>
#include "../nvm_chip/flash_memory/FlashTypes.h"
#include "NVM_Transaction_Flash.h"
#include "NVM_Transaction_Flash_WR.h"

namespace SSD_Components
{
	class NVM_Transaction_Flash_ER : public NVM_Transaction_Flash
	{
	public:
		NVM_Transaction_Flash_ER(Transaction_Source_Type source, stream_id_type streamID, const NVM::FlashMemory::Physical_Page_Address& address);
		std::list<NVM_Transaction_Flash_WR *> Page_movement_activities;
		unsigned int issued_gc_read_write_count = 0;
		unsigned int conflict_with_gc_read_count = 0, conflict_with_gc_write_count = 0, conflict_with_gc_erase_count;
		bool conflict_with_others = false;
	};
}

#endif // !ERASE_TRANSACTION_H
