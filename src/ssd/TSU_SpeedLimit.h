#ifndef TSU_SPEEDLIMIT_H
#define TSU_SPEEDLIMIT_H

#include "FTL.h"
#include "TSU_Base.h"
#include "NVM_Transaction_Flash.h"
#include "NVM_PHY_ONFI_NVDDR2.h"

namespace SSD_Components
{
	class FTL;

	class TSU_SpeedLimit : public TSU_Base
	{
	public:
		TSU_SpeedLimit(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, unsigned int channel_count,
			unsigned int chip_count_per_channel, unsigned int die_count_per_chip, unsigned int plane_count_per_die,
			unsigned int StreamCount, sim_time_type WriteReasonableSuspensionTimeForRead,
			sim_time_type EraseReasonableSuspensionTimeForRead, sim_time_type EraseReasonableSuspensionTimeForWrite,
			bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled);
		~TSU_SpeedLimit();
		void Prepare_for_transaction_submit();
		void Submit_transaction(NVM_Transaction_Flash* transaction);
		void Schedule();

		void Start_simulation();
		void Validate_simulation_config();
		void Execute_simulator_event(MQSimEngine::Sim_Event* event);
		void Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter);
	private:
		Flash_Transaction_Queue** UserReadTRQueue;
		Flash_Transaction_Queue** UserWriteTRQueue;
		Flash_Transaction_Queue** GCReadTRQueue;
		Flash_Transaction_Queue** GCWriteTRQueue;
		Flash_Transaction_Queue** GCEraseTRQueue;
		Flash_Transaction_Queue** MappingReadTRQueue;
		Flash_Transaction_Queue** MappingWriteTRQueue;

		unsigned int stream_count;
		unsigned int* user_read_limit_speed;
		unsigned int* user_write_limit_speed;
		const unsigned int min_user_read_arrival_count;
		const unsigned int min_user_write_arrival_count;
		const unsigned int max_user_read_arrival_count;
		const unsigned int max_user_write_arrival_count;
		unsigned int* user_read_arrival_count;
		unsigned int* user_write_arrival_count;
		std::vector<stream_id_type> user_read_idx;
		std::vector<stream_id_type> user_write_idx;
		unsigned int* UserReadTRCount;
		unsigned int* UserWriteTRCount;
		Flash_Transaction_Queue* UserReadTRBuffer;
		Flash_Transaction_Queue* UserWriteTRBuffer;
		void update(unsigned int* arrival_count, unsigned int* limit_speed, unsigned int max_arrival_count,
			unsigned int middle_arrival_count, unsigned int min_arrival_count, std::vector<stream_id_type>& idx,
			const unsigned int interval_time, int type);
		void speed_limit(Flash_Transaction_Queue** UserTRQueue, Flash_Transaction_Queue* UserTRBuffer,
			unsigned int* UserTRCount, unsigned int* user_limit_speed, std::vector<stream_id_type>& user_idx);
		void estimate_proportional_wait(NVM_Transaction_Flash_RD* read_slot, NVM_Transaction_Flash_WR* write_slot,
			double& pw_read, double& pw_write, int GCM, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		NVM_Transaction_Flash_RD* get_read_slot(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		NVM_Transaction_Flash_WR* get_write_slot(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		std::list<std::pair<Transaction_Type, Transaction_Source_Type>>** transaction_waiting_dispatch_slots;
		unsigned int*** serviced_writes_since_last_GC;
		const unsigned int GC_FLIN = 1000;

		bool service_read_transaction(NVM::FlashMemory::Flash_Chip* chip);
		bool service_write_transaction(NVM::FlashMemory::Flash_Chip* chip);
		bool service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip);
		void service_transaction(NVM::FlashMemory::Flash_Chip* chip);
	};
}

#endif // !TSU_SPEEDLIMIT_H
