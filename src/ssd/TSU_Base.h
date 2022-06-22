#ifndef TSU_H
#define TSU_H

#include <list>
#include "../sim/Sim_Defs.h"
#include "../sim/Sim_Object.h"
#include "../nvm_chip/flash_memory/Flash_Chip.h"
#include "../sim/Sim_Reporter.h"
#include "FTL.h"
#include "NVM_PHY_ONFI_NVDDR2.h"
#include "Flash_Transaction_Queue.h"
#include <limits.h>
#include <float.h>
#include "../exec/Externer.h"
#include <fstream>
#include <iomanip>
#include "TSU_SLUnit.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace SSD_Components
{
	enum class Flash_Scheduling_Type { OUT_OF_ORDER, // fifo
		FLIN, // original flin
		SIMPLE_FLIN, // single priority flin
		NP_FLIN, // no priority flin, gc prioritized
		SPEED_LIMIT, // speed limit
		SL_FIFO, // speed limit with fifo
		SL_FLIN, // speed limit with simple flin
		SLF_FIFO, // speed limit first, then allocation, tsu is fifo
		SLF_FLIN, // speed limit first, then allocation, tsu is simple flin
		CLB_FIFO, // chip level buffer with fifo, allocate cw then dp
		CLB_FLIN, // chip level buffer with simple flin, allocate cw then dp
		FUZZY_FLIN, // characterise low or high intensity with fuzzy logic
	};
	class FTL;
	class TSU_Base : public MQSimEngine::Sim_Object
	{
	public:
		TSU_Base(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, Flash_Scheduling_Type Type,
			unsigned int Channel_no, unsigned int chip_no_per_channel, unsigned int DieNoPerChip, unsigned int PlaneNoPerDie,
			unsigned int StreamCount,
			bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled,
			sim_time_type WriteReasonableSuspensionTimeForRead,
			sim_time_type EraseReasonableSuspensionTimeForRead,
			sim_time_type EraseReasonableSuspensionTimeForWrite);
		virtual ~TSU_Base();
		void Setup_triggers();


		/*When an MQSim needs to send a set of transactions for execution, the following 
		* three funcitons should be invoked in this order:
		* Prepare_for_transaction_submit()
		* Submit_transaction(transaction)
		* .....
		* Submit_transaction(transaction)
		* Schedule()
		*
		* The above mentioned mechanism helps to exploit die-level and plane-level parallelism.
		* More precisely, first the transactions are queued and then, when the Schedule function
		* is invoked, the TSU has that opportunity to schedule them together to exploit multiplane
		* and die-interleaved execution.
		*/
		virtual void Prepare_for_transaction_submit() = 0;
		virtual void Submit_transaction(NVM_Transaction_Flash* transaction) = 0;
		
		/* Shedules the transactions currently stored in inputTransactionSlots. The transactions could
		* be mixes of reads, writes, and erases.
		*/
		void Schedule();
		virtual void Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter);
		double proportional_slowdown(stream_id_type gc_stream_id);
		virtual size_t GCTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id) = 0;
		virtual size_t UserTRQueueSize(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id) = 0;
		virtual size_t UserTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id) = 0;
		double fairness();
		double proportional_slowdown(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		double fairness(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		double percentage_of_conflict_gc(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		sim_time_type Get_chip_level_shared_time(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		sim_time_type Get_chip_level_alone_time(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		void stat_gc_erase(stream_id_type stream_id) { number_of_gc[stream_id]++; }
		void start_execute_gc() { start_gc = true; }
		NVM_PHY_ONFI_NVDDR2* Get__NVMController() { return _NVMController; }
		double estimated_gap(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id, stream_id_type stream_id);
		stream_id_type stream_with_maximum_slowdown(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		void proportional_slowdown_ordered_list(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id,
			std::vector<double>& v);
		unsigned int Get_max_psd_size() { return max_psd_size; }
		virtual void queue_insertion(NVM_Transaction_Flash* transaction) = 0;
	protected:
		struct Gap
		{
			sim_time_type last_arrival_time = 0;
			double transaction_gap = 0;
		};
		FTL* ftl;
		NVM_PHY_ONFI_NVDDR2* _NVMController;
		Flash_Scheduling_Type type;
		unsigned int channel_count;
		unsigned int chip_no_per_channel;
		unsigned int die_no_per_chip;
		unsigned int plane_no_per_die;
		bool eraseSuspensionEnabled, programSuspensionEnabled;
		sim_time_type writeReasonableSuspensionTimeForRead;
		sim_time_type eraseReasonableSuspensionTimeForRead;//the time period 
		sim_time_type eraseReasonableSuspensionTimeForWrite;
		flash_chip_ID_type* Round_robin_turn_of_channel;//Used for round-robin service of the chips in channels

		static TSU_Base* _my_instance;
		std::list<NVM_Transaction_Flash*> transaction_receive_slots;//Stores the transactions that are received for sheduling
		std::list<NVM_Transaction_Flash*> transaction_dispatch_slots;//Used to submit transactions to the channel controller
		virtual void Schedule0() = 0;
		virtual bool service_read_transaction(NVM::FlashMemory::Flash_Chip* chip) = 0;
		virtual bool service_write_transaction(NVM::FlashMemory::Flash_Chip* chip) = 0;
		virtual bool service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip) = 0;
		// add function
		virtual void service_transaction(NVM::FlashMemory::Flash_Chip* chip) = 0;
		virtual void handle_transaction_serviced_signal(NVM_Transaction_Flash* transaction) = 0;
		virtual void enqueue_transaction_for_speed_limit_type_tsu() = 0;
		static void handle_transaction_serviced_signal_from_PHY(NVM_Transaction_Flash* transaction);
		static void handle_channel_idle_signal(flash_channel_ID_type);
		static void handle_chip_idle_signal(NVM::FlashMemory::Flash_Chip* chip);

		int opened_scheduling_reqs;
		unsigned int stream_count;
		unsigned int* number_of_gc;
		unsigned int read_conflict_when_gc, total_serviced_read_when_gc;
		unsigned int write_confict_when_gc, total_serviced_write_when_gc;
		unsigned int*** chip_level_total_serviced_when_gc, *** chip_level_conflict_when_gc;
		sim_time_type* alone_time, * shared_time;
		sim_time_type*** chip_level_alone_time, *** chip_level_shared_time;
		unsigned int* total_count;
		bool start_gc;
		
		struct Gap*** gap;
		float lambda = 2;
		unsigned int max_psd_size = 5;

		sim_time_type user_alone_time(sim_time_type waiting_time, Transaction_Type type);
		sim_time_type gc_alone_time(sim_time_type waiting_time, Transaction_Type type);
		sim_time_type mapping_alone_time(sim_time_type waiting_time, Transaction_Type type);
		void update_usr_gap(NVM_Transaction_Flash* tr);

		sim_time_type** chip_level_latency;
		unsigned int** chip_level_serviced_tr_count;

		void allocate_DP(Flash_Transaction_Queue* queue, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id);
		NVM::FlashMemory::Physical_Page_Address** global_rra;
	};
}

#endif //TSU_H
