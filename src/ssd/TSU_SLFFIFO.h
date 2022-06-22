#ifndef TSU_SLFFIFO_H
#define TSU_SLFFIFO_H

#include <list>
#include "TSU_Base.h"
#include "NVM_Transaction_Flash.h"
#include "NVM_PHY_ONFI_NVDDR2.h"
#include "FTL.h"

namespace SSD_Components
{
	class FTL;

	class TSU_SLFFIFO : public TSU_Base
	{
	public:
		TSU_SLFFIFO(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, unsigned int Channel_no, unsigned int chip_no_per_channel,
			unsigned int DieNoPerChip, unsigned int PlaneNoPerDie, unsigned int StreamCount,
			sim_time_type WriteReasonableSuspensionTimeForRead,
			sim_time_type EraseReasonableSuspensionTimeForRead,
			sim_time_type EraseReasonableSuspensionTimeForWrite,
			bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled);
		~TSU_SLFFIFO();
		void Prepare_for_transaction_submit();
		void Submit_transaction(NVM_Transaction_Flash* transaction);
		void handle_transaction_serviced_signal(NVM_Transaction_Flash* transaction);
		void Schedule0();

		void Start_simulation();
		void Validate_simulation_config();
		void Execute_simulator_event(MQSimEngine::Sim_Event*);
		void Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter);

		size_t GCTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		size_t UserTRQueueSize(stream_id_type gc_stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		size_t UserTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		void queue_insertion(NVM_Transaction_Flash* transaction) {}

	private:
		Flash_Transaction_Queue** UserReadTRQueue;
		Flash_Transaction_Queue** UserWriteTRQueue;
		Flash_Transaction_Queue** GCReadTRQueue;
		Flash_Transaction_Queue** GCWriteTRQueue;
		Flash_Transaction_Queue** GCEraseTRQueue;
		Flash_Transaction_Queue** MappingReadTRQueue;
		Flash_Transaction_Queue** MappingWriteTRQueue;

		void estimate_alone_time(NVM_Transaction_Flash* transaction, Flash_Transaction_Queue* buffer);
		void estimate_alone_time(NVM_Transaction_Flash* transaction, Flash_Transaction_Queue* queue, Flash_Transaction_Queue* buffer);
		void adjust_alone_time(stream_id_type dispatched_stream_id, sim_time_type adjust_time, Transaction_Type type,
			Transaction_Source_Type source, Flash_Transaction_Queue* queue, Flash_Transaction_Queue* buffer,
			flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);

		bool service_read_transaction(NVM::FlashMemory::Flash_Chip* chip);
		bool service_write_transaction(NVM::FlashMemory::Flash_Chip* chip);
		bool service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip);
		// add function
		void service_transaction(NVM::FlashMemory::Flash_Chip* chip);
		void enqueue_transaction_for_speed_limit_type_tsu();

		TSU_SLUnit* read_unit, * write_unit;
	};
}

#endif // TSU_SLFFIFO_H
