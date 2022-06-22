#include "TSU_OutofOrder.h"

namespace SSD_Components
{

	TSU_OutOfOrder::TSU_OutOfOrder(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, unsigned int ChannelCount, unsigned int chip_no_per_channel,
		unsigned int DieNoPerChip, unsigned int PlaneNoPerDie, unsigned int StreamCount,
		sim_time_type WriteReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForWrite, 
		bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled)
		: TSU_Base(id, ftl, NVMController, Flash_Scheduling_Type::OUT_OF_ORDER, ChannelCount, chip_no_per_channel, DieNoPerChip, PlaneNoPerDie,
			StreamCount, WriteReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForWrite,
			EraseSuspensionEnabled, ProgramSuspensionEnabled)
	{
		UserReadTRQueue = new Flash_Transaction_Queue*[channel_count];
		UserWriteTRQueue = new Flash_Transaction_Queue*[channel_count];
		GCReadTRQueue = new Flash_Transaction_Queue*[channel_count];
		GCWriteTRQueue = new Flash_Transaction_Queue*[channel_count];
		GCEraseTRQueue = new Flash_Transaction_Queue*[channel_count];
		MappingReadTRQueue = new Flash_Transaction_Queue*[channel_count];
		MappingWriteTRQueue = new Flash_Transaction_Queue*[channel_count];

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			UserReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			UserWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCEraseTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];

			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				UserReadTRQueue[channelID][chip_cntr].Set_id("User_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				UserWriteTRQueue[channelID][chip_cntr].Set_id("User_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCReadTRQueue[channelID][chip_cntr].Set_id("GC_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				MappingReadTRQueue[channelID][chip_cntr].Set_id("Mapping_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				MappingWriteTRQueue[channelID][chip_cntr].Set_id("Mapping_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCWriteTRQueue[channelID][chip_cntr].Set_id("GC_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCEraseTRQueue[channelID][chip_cntr].Set_id("GC_Erase_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
			}
		}
	}
	
	TSU_OutOfOrder::~TSU_OutOfOrder()
	{
		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			delete[] UserReadTRQueue[channelID];
			delete[] UserWriteTRQueue[channelID];
			delete[] GCReadTRQueue[channelID];
			delete[] GCWriteTRQueue[channelID];
			delete[] GCEraseTRQueue[channelID];
			delete[] MappingReadTRQueue[channelID];
			delete[] MappingWriteTRQueue[channelID];

		}
		delete[] UserReadTRQueue;
		delete[] UserWriteTRQueue;
		delete[] GCReadTRQueue;
		delete[] GCWriteTRQueue;
		delete[] GCEraseTRQueue;
		delete[] MappingReadTRQueue;
		delete[] MappingWriteTRQueue;

	}

	void TSU_OutOfOrder::Start_simulation() {}

	void TSU_OutOfOrder::Validate_simulation_config() {}

	void TSU_OutOfOrder::Execute_simulator_event(MQSimEngine::Sim_Event* event) {}

	void TSU_OutOfOrder::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter)
	{
		name_prefix = name_prefix + +".TSU";
		xmlwriter.Write_open_tag(name_prefix);

		TSU_Base::Report_results_in_XML(name_prefix, xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				UserReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".User_Read_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				UserWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".User_Write_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				MappingReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".Mapping_Read_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				MappingWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".Mapping_Write_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				GCReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Read_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				GCWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Write_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				GCEraseTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Erase_TR_Queue", xmlwriter);
	
		xmlwriter.Write_close_tag();
	}

	inline void TSU_OutOfOrder::Prepare_for_transaction_submit()
	{
		opened_scheduling_reqs++;
		if (opened_scheduling_reqs > 1)
			return;
		transaction_receive_slots.clear();
	}

	inline void TSU_OutOfOrder::Submit_transaction(NVM_Transaction_Flash* transaction)
	{
		transaction_receive_slots.push_back(transaction);
	}

	void TSU_OutOfOrder::handle_transaction_serviced_signal(NVM_Transaction_Flash* transaction)
	{
		unsigned int channel = transaction->Address.ChannelID, chip = transaction->Address.ChipID;
		unsigned int die = transaction->Address.DieID, plane = transaction->Address.PlaneID;
		/*if (Simulator->Time() - transaction->Issue_time > 5215134)
			std::cout << (int)transaction->Type << " " << Simulator->Time() - transaction->Issue_time
			<< " " << channel << chip << die << plane << " " << transaction->STAT_transfer_time + transaction->STAT_execution_time
			<< " " << ((int)transaction->Type == 1 ? UserWriteTRQueue[channel][chip].size() : UserReadTRQueue[channel][chip].size()) << "\n";*/
	}

	void TSU_OutOfOrder::Schedule0()
	{
		opened_scheduling_reqs--;
		if (opened_scheduling_reqs > 0)
			return;
		if (opened_scheduling_reqs < 0)
			PRINT_ERROR("TSU_OutOfOrder: Illegal status!");

		if (transaction_receive_slots.size() == 0)
			return;

		for (std::list<NVM_Transaction_Flash*>::iterator it = transaction_receive_slots.begin();
			it != transaction_receive_slots.end(); it++)
		{
			_NVMController->test_transaction_for_conflicting_with_gc(*it);
			switch ((*it)->Type)
			{
			case Transaction_Type::READ:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					estimate_alone_time(*it, &UserReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID]);
					UserReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::MAPPING:
					MappingReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::GC_WL:
					GCReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				default:
					PRINT_ERROR("TSU_OutOfOrder: unknown source type for a read transaction!")
				}
				break;
			case Transaction_Type::WRITE:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					estimate_alone_time(*it, &UserWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID]);
					UserWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::MAPPING:
					MappingWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::GC_WL:
					GCWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				default:
					PRINT_ERROR("TSU_OutOfOrder: unknown source type for a write transaction!")
				}
				break;
			case Transaction_Type::ERASE:
				GCEraseTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
				break;
			default:
				break;
			}
		}


		for (flash_channel_ID_type channelID = 0; channelID < channel_count; channelID++)
		{
			if (_NVMController->Get_channel_status(channelID) == BusChannelStatus::IDLE)
			{
				for (unsigned int i = 0; i < chip_no_per_channel; i++) {
					NVM::FlashMemory::Flash_Chip* chip = _NVMController->Get_chip(channelID, Round_robin_turn_of_channel[channelID]);
					//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
					service_transaction(chip);
					Round_robin_turn_of_channel[channelID] = (flash_chip_ID_type)(Round_robin_turn_of_channel[channelID] + 1) % chip_no_per_channel;
					if (_NVMController->Get_channel_status(chip->ChannelID) != BusChannelStatus::IDLE)
						break;
				}
			}
		}
	}
	
	void TSU_OutOfOrder::estimate_alone_time(NVM_Transaction_Flash* transaction, Flash_Transaction_Queue* queue)
	{
		sim_time_type chip_busy_time = 0, waiting_last_time = 0;
		NVM_Transaction_Flash* chip_tr = _NVMController->Is_chip_busy_with_stream(transaction);
		if (chip_tr && _NVMController->Expected_finish_time(chip_tr) > Simulator->Time())
		{
			chip_busy_time = _NVMController->Expected_finish_time(chip_tr) - Simulator->Time();
		}
		if (queue->size())
		{
			auto itr_it = queue->end();
			do
			{
				--itr_it;
				if ((*itr_it)->Stream_id == transaction->Stream_id)
				{
					waiting_last_time += _NVMController->Expected_command_time(*itr_it) + _NVMController->Expected_transfer_time(*itr_it);
				}
			} while (itr_it != queue->begin());
		}
		waiting_last_time = user_alone_time(waiting_last_time, transaction->Type);
		transaction->alone_time = chip_busy_time + waiting_last_time
			+ _NVMController->Expected_transfer_time(transaction) + _NVMController->Expected_command_time(transaction);
	}

	void TSU_OutOfOrder::adjust_alone_time(stream_id_type dispatched_stream_id, sim_time_type adjust_time, Transaction_Type type,
		Transaction_Source_Type source, Flash_Transaction_Queue* queue)
	{
		if (source == Transaction_Source_Type::CACHE || source == Transaction_Source_Type::USERIO)
		{
			adjust_time = user_alone_time(adjust_time, type);
		}
		else if (source == Transaction_Source_Type::GC_WL)
		{
			adjust_time = gc_alone_time(adjust_time, type);
		}
		else if (source == Transaction_Source_Type::MAPPING)
		{
			adjust_time = mapping_alone_time(adjust_time, type);
		}
		for (auto it = queue->begin(); it != queue->end(); ++it)
		{
			if (dispatched_stream_id == (*it)->Stream_id)
			{
				(*it)->alone_time += adjust_time;
			}
		}
	}

	size_t TSU_OutOfOrder::GCTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return GCReadTRQueue[channel_id][chip_id].size() + GCWriteTRQueue[channel_id][chip_id].size() + GCEraseTRQueue[channel_id][chip_id].size();
	}

	size_t TSU_OutOfOrder::UserTRQueueSize(stream_id_type gc_stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		size_t size = 0;
		for (auto& it : UserWriteTRQueue[channel_id][chip_id])
		{
			if ((*it).Stream_id == gc_stream_id) ++size;
		}
		for (auto& it : UserReadTRQueue[channel_id][chip_id])
		{
			if ((*it).Stream_id == gc_stream_id) ++size;
		}
		return size;
	}

	size_t TSU_OutOfOrder::UserTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return UserReadTRQueue[channel_id][chip_id].size() + UserWriteTRQueue[channel_id][chip_id].size();
	}

	bool TSU_OutOfOrder::service_read_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		Flash_Transaction_Queue *sourceQueue1 = NULL, *sourceQueue2 = NULL;

		if (MappingReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)//Flash transactions that are related to FTL mapping data have the highest priority
		{
			sourceQueue1 = &MappingReadTRQueue[chip->ChannelID][chip->ChipID];
			if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip) && GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			else if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				allocate_DP(&UserReadTRQueue[chip->ChannelID][chip->ChipID], chip->ChannelID, chip->ChipID);
				sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
			}
		}
		else if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first
		{
			if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
				if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				{
					allocate_DP(&UserReadTRQueue[chip->ChannelID][chip->ChipID], chip->ChannelID, chip->ChipID);
					sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
				}
			}
			else if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				return false;
			else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				return false;
			else if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
			else return false;
		} 
		else //If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first
		{
			if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				allocate_DP(&UserReadTRQueue[chip->ChannelID][chip->ChipID], chip->ChannelID, chip->ChipID);
				sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
				if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
					sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			}
			else if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
					return false;
			else if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			else return false;
		}

		bool suspensionRequired = false;
		ChipStatus cs = _NVMController->GetChipStatus(chip);
		switch (cs)
		{
		case ChipStatus::IDLE:
			break;
		case ChipStatus::WRITING:
			if (!programSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
				return false;
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < writeReasonableSuspensionTimeForRead)
				return false;
			suspensionRequired = true;
		case ChipStatus::ERASING:
			if (!eraseSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
				return false;
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < eraseReasonableSuspensionTimeForRead)
				return false;
			suspensionRequired = true;
		default:
			return false;
		}
		
		flash_die_ID_type dieID = sourceQueue1->front()->Address.DieID;
		flash_page_ID_type pageID = sourceQueue1->front()->Address.PageID;
		Transaction_Type type = sourceQueue1->front()->Type;
		unsigned int planeVector = 0;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			transaction_dispatch_slots.clear();
			planeVector = 0;

			for (Flash_Transaction_Queue::iterator it = sourceQueue1->begin(); it != sourceQueue1->end();)
			{
				if ((*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID) /*&& (*it)->Type == type*/)
				{
					if (planeVector == 0 || (*it)->Address.PageID == pageID)//Check for identical pages when running multiplane command
					{
						stream_id_type dispatched_stream_id = (*it)->Stream_id;
						sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
							+ _NVMController->Expected_command_time(*it);
						Transaction_Type type = (*it)->Type;
						Transaction_Source_Type source = (*it)->Source;
						(*it)->SuspendRequired = suspensionRequired;
						planeVector |= 1 << (*it)->Address.PlaneID;
						transaction_dispatch_slots.push_back(*it);
						sourceQueue1->remove(it++);
						if (source == Transaction_Source_Type::MAPPING
							|| source == Transaction_Source_Type::GC_WL)
						{
							adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
								&UserReadTRQueue[chip->ChannelID][chip->ChipID]);
						}
						adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
							&UserWriteTRQueue[chip->ChannelID][chip->ChipID]);
						continue;
					}
				}
				it++;
			}

			if (sourceQueue2 != NULL && transaction_dispatch_slots.size() < plane_no_per_die)
				for (Flash_Transaction_Queue::iterator it = sourceQueue2->begin(); it != sourceQueue2->end();)
				{
					if ((*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID) /*&& (*it)->Type == type*/)
					{
						if (planeVector == 0 || (*it)->Address.PageID == pageID)//Check for identical pages when running multiplane command
						{
							stream_id_type dispatched_stream_id = (*it)->Stream_id;
							sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
								+ _NVMController->Expected_command_time(*it);
							Transaction_Type type = (*it)->Type;
							Transaction_Source_Type source = (*it)->Source;
							(*it)->SuspendRequired = suspensionRequired;
							planeVector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							sourceQueue2->remove(it++);
							if (source == Transaction_Source_Type::MAPPING
								|| source == Transaction_Source_Type::GC_WL)
							{
								adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
									&UserReadTRQueue[chip->ChannelID][chip->ChipID]);
							}
							adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
								&UserWriteTRQueue[chip->ChannelID][chip->ChipID]);
							continue;
						}
					}
					it++;
				}

			if (transaction_dispatch_slots.size() > 0)
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			transaction_dispatch_slots.clear();
			dieID = (dieID + 1) % die_no_per_chip;
		}

		return true;
	}

	bool TSU_OutOfOrder::service_write_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		Flash_Transaction_Queue *sourceQueue1 = NULL, *sourceQueue2 = NULL;

		if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first
		{
			if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
				if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				{
					allocate_DP(&UserWriteTRQueue[chip->ChannelID][chip->ChipID], chip->ChannelID, chip->ChipID);
					sourceQueue2 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
				}
			}
			else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				return false;
			else if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				allocate_DP(&UserWriteTRQueue[chip->ChannelID][chip->ChipID], chip->ChannelID, chip->ChipID);
				sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
			}
			else return false;
		}
		else //If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first
		{
			if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				allocate_DP(&UserWriteTRQueue[chip->ChannelID][chip->ChipID], chip->ChannelID, chip->ChipID);
				sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
				if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
					sourceQueue2 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
			}
			else if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
			else return false;
		}


		bool suspensionRequired = false;
		ChipStatus cs = _NVMController->GetChipStatus(chip);
		switch (cs)
		{
		case ChipStatus::IDLE:
			break;
		case ChipStatus::ERASING:
			if (!eraseSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
				return false;
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < eraseReasonableSuspensionTimeForWrite)
				return false;
			suspensionRequired = true;
		default:
			return false;
		}

		flash_die_ID_type dieID = sourceQueue1->front()->Address.DieID;
		flash_page_ID_type pageID = sourceQueue1->front()->Address.PageID;
		unsigned int planeVector = 0;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			transaction_dispatch_slots.clear();
			planeVector = 0;

			for (Flash_Transaction_Queue::iterator it = sourceQueue1->begin(); it != sourceQueue1->end(); )
			{
				if (((NVM_Transaction_Flash_WR*)*it)->RelatedRead == NULL && (*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
				{
					if (planeVector == 0 || (*it)->Address.PageID == pageID)//Check for identical pages when running multiplane command
					{
						stream_id_type dispatched_stream_id = (*it)->Stream_id;
						sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
							+ _NVMController->Expected_command_time(*it);
						Transaction_Type type = (*it)->Type;
						Transaction_Source_Type source = (*it)->Source;
						(*it)->SuspendRequired = suspensionRequired;
						planeVector |= 1 << (*it)->Address.PlaneID;
						transaction_dispatch_slots.push_back(*it);
						sourceQueue1->remove(it++);
						if (source == Transaction_Source_Type::MAPPING
							|| source == Transaction_Source_Type::GC_WL)
						{
							adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
								&UserWriteTRQueue[chip->ChannelID][chip->ChipID]);
						}
						adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
							&UserReadTRQueue[chip->ChannelID][chip->ChipID]);
						continue;
					}
				}
				it++;
			}

			if (sourceQueue2 != NULL)
				for (Flash_Transaction_Queue::iterator it = sourceQueue2->begin(); it != sourceQueue2->end(); )
				{
					if (((NVM_Transaction_Flash_WR*)*it)->RelatedRead == NULL && (*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
					{
						if (planeVector == 0 || (*it)->Address.PageID == pageID)//Check for identical pages when running multiplane command
						{
							stream_id_type dispatched_stream_id = (*it)->Stream_id;
							sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
								+ _NVMController->Expected_command_time(*it);
							Transaction_Type type = (*it)->Type;
							Transaction_Source_Type source = (*it)->Source;
							(*it)->SuspendRequired = suspensionRequired;
							planeVector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							sourceQueue2->remove(it++);
							if (source == Transaction_Source_Type::MAPPING
								|| source == Transaction_Source_Type::GC_WL)
							{
								adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
									&UserWriteTRQueue[chip->ChannelID][chip->ChipID]);
							}
							adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
								&UserReadTRQueue[chip->ChannelID][chip->ChipID]);
							continue;
						}
					}
					it++;
				}

			if (transaction_dispatch_slots.size() > 0)
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			transaction_dispatch_slots.clear();
			dieID = (dieID + 1) % die_no_per_chip;
		}
		return true;
	}

	bool TSU_OutOfOrder::service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (_NVMController->GetChipStatus(chip) != ChipStatus::IDLE)
			return false;

		Flash_Transaction_Queue* source_queue = &GCEraseTRQueue[chip->ChannelID][chip->ChipID];
		if (source_queue->size() == 0)
			return false;

		flash_die_ID_type dieID = source_queue->front()->Address.DieID;
		unsigned int planeVector = 0;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			transaction_dispatch_slots.clear();
			planeVector = 0;

			for (Flash_Transaction_Queue::iterator it = source_queue->begin(); it != source_queue->end(); )
			{
				if (((NVM_Transaction_Flash_ER*)*it)->Page_movement_activities.size() == 0 && (*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
				{
					stream_id_type dispatched_stream_id = (*it)->Stream_id;
					sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
						+ _NVMController->Expected_command_time(*it);
					Transaction_Type type = (*it)->Type;
					planeVector |= 1 << (*it)->Address.PlaneID;
					transaction_dispatch_slots.push_back(*it);
					source_queue->remove(it++);
					adjust_alone_time(dispatched_stream_id, adjust_time, type, Transaction_Source_Type::GC_WL,
						&UserReadTRQueue[chip->ChannelID][chip->ChipID]);
					adjust_alone_time(dispatched_stream_id, adjust_time, type, Transaction_Source_Type::GC_WL,
						&UserWriteTRQueue[chip->ChannelID][chip->ChipID]);
					continue;
				}
				it++;
			}
			if (transaction_dispatch_slots.size() > 0)
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			transaction_dispatch_slots.clear();
			dieID = (dieID + 1) % die_no_per_chip;
		}
		return true;
	}

	void TSU_OutOfOrder::service_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (_NVMController->GetChipStatus(chip) != ChipStatus::IDLE)
			return;
		/*if (!UserWriteTRQueue[chip->ChannelID][chip->ChipID].empty() && !UserReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].front()->Issue_time
				< UserReadTRQueue[chip->ChannelID][chip->ChipID].front()->Issue_time)
			{
				if (service_write_transaction(chip)) return;
			}
		}*/
		
		if (!service_read_transaction(chip))
			if (!service_write_transaction(chip))
				service_erase_transaction(chip);
		/*if (!service_write_transaction(chip))
			if (!service_read_transaction(chip))
				service_erase_transaction(chip);*/
	}
}
