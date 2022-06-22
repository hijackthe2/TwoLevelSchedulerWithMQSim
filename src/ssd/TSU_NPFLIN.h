#ifndef TSU_NP_FLIN_H
#define TSU_NP_FLIN_H

#include <list>
#include <set>
#include "TSU_Base.h"
#include "NVM_Transaction_Flash.h"
#include "NVM_PHY_ONFI_NVDDR2.h"
#include "FTL.h"
#include <stack>

namespace SSD_Components
{
	class TSU_NPFLIN : public TSU_Base
	{
	public:
		TSU_NPFLIN(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController,
			const unsigned int Channel_no, const unsigned int chip_no_per_channel, const unsigned int die_no_per_chip, const unsigned int plane_no_per_die, unsigned int flash_page_size,
			const sim_time_type flow_classification_epoch, const unsigned int alpha_read, const unsigned int alpha_write,
			const stream_id_type max_flow_id, const double f_thr, const unsigned int GC_FLIN,
			const sim_time_type WriteReasonableSuspensionTimeForRead, const sim_time_type EraseReasonableSuspensionTimeForRead,
			const sim_time_type EraseReasonableSuspensionTimeForWrite,
			const bool EraseSuspensionEnabled, const bool ProgramSuspensionEnabled);
		~TSU_NPFLIN();
		void Prepare_for_transaction_submit();
		void Submit_transaction(NVM_Transaction_Flash* transaction);
		void Schedule0();

		void Start_simulation();
		void Validate_simulation_config();
		void Execute_simulator_event(MQSimEngine::Sim_Event*);
		void Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter);
		size_t GCTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		size_t UserTRQueueSize(stream_id_type gc_stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		size_t UserTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		void handle_transaction_serviced_signal(NVM_Transaction_Flash* transaction);
		void queue_insertion(NVM_Transaction_Flash* transaction) {}
	private:
		struct FLIN_Unit
		{
			unsigned int Serviced_read_requests_recent = 0;
			unsigned int Serviced_write_requests_recent = 0;
			unsigned int No_of_serviced_writes_since_last_GC = 0;
		};
	private:
		struct FLIN_Unit*** flow_activity_info;
		sim_time_type flow_classification_epoch;
		unsigned int alpha_read_for_epoch, alpha_write_for_epoch;
		double F_thr;
		unsigned int GC_flin;
		std::set<stream_id_type>** low_intensity_class_read, ** low_intensity_class_write;
		std::list<NVM_Transaction_Flash*>::iterator** head_high_read;
		std::list<NVM_Transaction_Flash*>::iterator** head_high_write;
		Flash_Transaction_Queue** UserReadTRQueue;
		Flash_Transaction_Queue** UserWriteTRQueue;
		Flash_Transaction_Queue** GCReadTRQueue;
		Flash_Transaction_Queue** GCWriteTRQueue;
		Flash_Transaction_Queue** GCEraseTRQueue;
		Flash_Transaction_Queue** MappingReadTRQueue;
		Flash_Transaction_Queue** MappingWriteTRQueue;
		std::list<std::pair<Transaction_Type, Transaction_Source_Type>>** transaction_waiting_dispatch_slots;

		void reorder_for_fairness(Flash_Transaction_Queue* queue,
			std::list<NVM_Transaction_Flash*>::iterator start, std::list<NVM_Transaction_Flash*>::iterator end);
		void estimate_alone_waiting_time(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator position);
		double fairness_based_on_average_slowdown(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator start,
			stream_id_type& flow_with_max_average_slowdown);
		void move_forward(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator TRnew_pos,
			std::list<NVM_Transaction_Flash*>::iterator ultimate_posistion);
		void estimate_proportional_wait(NVM_Transaction_Flash_RD* read_slot, NVM_Transaction_Flash_WR* write_slot,
			double& pw_read, double& pw_write, unsigned int GCM, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		NVM_Transaction_Flash_RD* get_read_slot(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		NVM_Transaction_Flash_WR* get_write_slot(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		void estimate_alone_time(NVM_Transaction_Flash* transaction, Flash_Transaction_Queue* queue);
		void move_alone_time(NVM_Transaction_Flash* forward_transaction, NVM_Transaction_Flash* backward_transaction);
		void adjust_alone_time(stream_id_type dispatched_stream_id, sim_time_type adjust_time, Transaction_Type type,
			Transaction_Source_Type source, Flash_Transaction_Queue* queue);
		bool is_dominated_by_one_stream(Flash_Transaction_Queue* queue);

		bool service_read_transaction(NVM::FlashMemory::Flash_Chip* chip);
		bool service_write_transaction(NVM::FlashMemory::Flash_Chip* chip);
		bool service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip);
		void service_transaction(NVM::FlashMemory::Flash_Chip* chip);
		void enqueue_transaction_for_speed_limit_type_tsu() {};
	};
}

#endif // !TSU_NP_FLIN_H
