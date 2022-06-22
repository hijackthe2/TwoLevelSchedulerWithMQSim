#include "TSU_SpeedLimit.h"

namespace SSD_Components
{
	TSU_SpeedLimit::TSU_SpeedLimit(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, unsigned int ChannelCount, unsigned int chip_no_per_channel,
		unsigned int DieNoPerChip, unsigned int PlaneNoPerDie, unsigned int StreamCount,
		sim_time_type WriteReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForWrite,
		bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled)
		: TSU_Base(id, ftl, NVMController, Flash_Scheduling_Type::SPEED_LIMIT, ChannelCount, chip_no_per_channel, DieNoPerChip, PlaneNoPerDie,
			StreamCount, WriteReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForWrite,
			EraseSuspensionEnabled, ProgramSuspensionEnabled)
	{
		UserReadTRQueue = new Flash_Transaction_Queue * [channel_count];
		UserWriteTRQueue = new Flash_Transaction_Queue * [channel_count];
		GCReadTRQueue = new Flash_Transaction_Queue * [channel_count];
		GCWriteTRQueue = new Flash_Transaction_Queue * [channel_count];
		GCEraseTRQueue = new Flash_Transaction_Queue * [channel_count];
		MappingReadTRQueue = new Flash_Transaction_Queue * [channel_count];
		MappingWriteTRQueue = new Flash_Transaction_Queue * [channel_count];

		read_buffer = new Flash_Transaction_Queue * *[channel_count];
		write_buffer = new Flash_Transaction_Queue * *[channel_count];
		read_intensity = new unsigned int** [channel_count];
		write_intensity = new unsigned int** [channel_count];
		read_recent_serviced = new unsigned** [channel_count];
		write_recent_serviced = new unsigned** [channel_count];

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			UserReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			UserWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCEraseTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];

			read_buffer[channelID] = new Flash_Transaction_Queue * [chip_no_per_channel];
			write_buffer[channelID] = new Flash_Transaction_Queue * [chip_no_per_channel];
			read_intensity[channelID] = new unsigned int* [chip_no_per_channel];
			write_intensity[channelID] = new unsigned int* [chip_no_per_channel];
			read_recent_serviced[channelID] = new unsigned int* [chip_no_per_channel];
			write_recent_serviced[channelID] = new unsigned int* [chip_no_per_channel];

			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				UserReadTRQueue[channelID][chip_cntr].Set_id("User_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				UserWriteTRQueue[channelID][chip_cntr].Set_id("User_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCReadTRQueue[channelID][chip_cntr].Set_id("GC_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				MappingReadTRQueue[channelID][chip_cntr].Set_id("Mapping_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				MappingWriteTRQueue[channelID][chip_cntr].Set_id("Mapping_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCWriteTRQueue[channelID][chip_cntr].Set_id("GC_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCEraseTRQueue[channelID][chip_cntr].Set_id("GC_Erase_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				
				read_buffer[channelID][chip_cntr] = new Flash_Transaction_Queue[stream_count];
				write_buffer[channelID][chip_cntr] = new Flash_Transaction_Queue[stream_count];
				read_intensity[channelID][chip_cntr] = new unsigned int[stream_count];
				write_intensity[channelID][chip_cntr] = new unsigned int[stream_count];
				read_recent_serviced[channelID][chip_cntr] = new unsigned[stream_count];
				write_recent_serviced[channelID][chip_cntr] = new unsigned[stream_count];
				for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
				{
					read_intensity[channelID][chip_cntr][stream_id] = 0;
					write_intensity[channelID][chip_cntr][stream_id] = 0;
					read_recent_serviced[channelID][chip_cntr][stream_id] = 0;
					write_recent_serviced[channelID][chip_cntr][stream_id] = 0;
				}
			}
		}
	}

	TSU_SpeedLimit::~TSU_SpeedLimit()
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
			
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; ++chip_id)
			{
				delete[] read_buffer[channelID][chip_id];
				delete[] write_buffer[channelID][chip_id];
				delete[] read_intensity[channelID][chip_id];
				delete[] write_intensity[channelID][chip_id];
				delete[] read_recent_serviced[channelID][chip_id];
				delete[] write_recent_serviced[channelID][chip_id];
			}
			delete[] read_buffer[channelID];
			delete[] write_buffer[channelID];
			delete[] read_intensity[channelID];
			delete[] write_intensity[channelID];
			delete[] read_recent_serviced[channelID];
			delete[] write_recent_serviced[channelID];
		}
		delete[] UserReadTRQueue;
		delete[] UserWriteTRQueue;
		delete[] GCReadTRQueue;
		delete[] GCWriteTRQueue;
		delete[] GCEraseTRQueue;
		delete[] MappingReadTRQueue;
		delete[] MappingWriteTRQueue;

		delete[] read_buffer;
		delete[] write_buffer;
		delete[] read_intensity;
		delete[] write_intensity;
		delete[] read_recent_serviced;
		delete[] write_recent_serviced;

	}

	void TSU_SpeedLimit::Start_simulation()
	{
		for (unsigned int channel_id = 0; channel_id < channel_count; ++channel_id)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; ++chip_id)
			{
				Simulator->Register_sim_event(interval, this, 0, 0);
			}
		}
	}

	void TSU_SpeedLimit::Validate_simulation_config() {}

	void TSU_SpeedLimit::Execute_simulator_event(MQSimEngine::Sim_Event* event)
	{
		for (unsigned int channel_id = 0; channel_id < channel_count; ++channel_id)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; ++chip_id)
			{
				for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
				{
					read_intensity[channel_id][chip_id][stream_id] = read_recent_serviced[channel_id][chip_id][stream_id];
					read_recent_serviced[channel_id][chip_id][stream_id] = 0;
					write_intensity[channel_id][chip_id][stream_id] = write_recent_serviced[channel_id][chip_id][stream_id];
					write_recent_serviced[channel_id][chip_id][stream_id] = 0;
				}
			}
		}
		Simulator->Register_sim_event(Simulator->Time() + interval, this, 0, 0);
	}

	void TSU_SpeedLimit::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter)
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

	inline void TSU_SpeedLimit::Prepare_for_transaction_submit()
	{
		opened_scheduling_reqs++;
		if (opened_scheduling_reqs > 1)
			return;
		transaction_receive_slots.clear();
	}

	inline void TSU_SpeedLimit::Submit_transaction(NVM_Transaction_Flash* transaction)
	{
		transaction_receive_slots.push_back(transaction);
	}

	void TSU_SpeedLimit::handle_transaction_serviced_signal(NVM_Transaction_Flash* transaction)
	{
	}

	void TSU_SpeedLimit::Schedule0()
	{
		opened_scheduling_reqs--;
		if (opened_scheduling_reqs > 0)
			return;
		if (opened_scheduling_reqs < 0)
			PRINT_ERROR("TSU_SpeedLimit: Illegal status!");

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
					//UserReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					read_buffer[(*it)->Address.ChannelID][(*it)->Address.ChipID][(*it)->Stream_id].push_back(*it);
					read_recent_serviced[(*it)->Address.ChannelID][(*it)->Address.ChipID][(*it)->Stream_id]++;
					break;
				case Transaction_Source_Type::MAPPING:
					MappingReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::GC_WL:
					GCReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				default:
					PRINT_ERROR("TSU_SpeedLimit: unknown source type for a read transaction!")
				}
				break;
			case Transaction_Type::WRITE:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					estimate_alone_time(*it, &UserWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID]);
					//UserWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					write_buffer[(*it)->Address.ChannelID][(*it)->Address.ChipID][(*it)->Stream_id].push_back(*it);
					write_recent_serviced[(*it)->Address.ChannelID][(*it)->Address.ChipID][(*it)->Stream_id]++;
					break;
				case Transaction_Source_Type::MAPPING:
					MappingWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::GC_WL:
					GCWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				default:
					PRINT_ERROR("TSU_SpeedLimit: unknown source type for a write transaction!")
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

	void TSU_SpeedLimit::estimate_alone_time(NVM_Transaction_Flash* transaction, Flash_Transaction_Queue* queue)
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

	void TSU_SpeedLimit::adjust_alone_time(stream_id_type dispatched_stream_id, sim_time_type adjust_time, Transaction_Type type,
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

	size_t TSU_SpeedLimit::GCTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return GCReadTRQueue[channel_id][chip_id].size() + GCWriteTRQueue[channel_id][chip_id].size() + GCEraseTRQueue[channel_id][chip_id].size();
	}

	size_t TSU_SpeedLimit::UserTRQueueSize(stream_id_type gc_stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
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

	size_t TSU_SpeedLimit::UserTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return UserReadTRQueue[channel_id][chip_id].size() + UserWriteTRQueue[channel_id][chip_id].size();
	}

	bool TSU_SpeedLimit::service_read_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
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

		std::list<Flash_Transaction_Queue*> queue_list;
		if (!MappingReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_back(&MappingReadTRQueue[chip->ChannelID][chip->ChipID]);
		}
		release(queue_list, &UserReadTRQueue[chip->ChannelID][chip->ChipID], read_buffer[chip->ChannelID][chip->ChipID],
			read_intensity[chip->ChannelID][chip->ChipID]);
		if (!GCReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_front(&GCReadTRQueue[chip->ChannelID][chip->ChipID]);
		}
		if (queue_list.empty()) return false;
		flash_die_ID_type die_id = queue_list.front()->front()->Address.DieID;
		bool dispathed_user_transaction = false;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			if (queue_list.empty()) break;
			transaction_dispatch_slots.clear();
			unsigned int plane_vector = 0;
			flash_page_ID_type page_id = queue_list.front()->front()->Address.PageID;
			for (std::list<Flash_Transaction_Queue*>::iterator queue = queue_list.begin(); queue != queue_list.end(); )
			{
				for (Flash_Transaction_Queue::iterator it = (*queue)->begin(); it != (*queue)->end(); )
				{
					if ((*it)->Address.DieID == die_id && !(plane_vector & 1 << (*it)->Address.PlaneID))
					{
						if (plane_vector == 0) page_id = (*it)->Address.PageID;
						if ((*it)->Address.PageID == page_id)
						{
							stream_id_type dispatched_stream_id = (*it)->Stream_id;
							sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
								+ _NVMController->Expected_command_time(*it);
							Transaction_Type type = (*it)->Type;
							Transaction_Source_Type source = (*it)->Source;
							(*it)->SuspendRequired = suspensionRequired;
							plane_vector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							(*queue)->remove(it++);
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
					++it;
				}
				if ((*queue)->empty())
				{
					queue_list.erase(queue++);
					continue;
				}
				++queue;
				if (transaction_dispatch_slots.size() >= plane_no_per_die) break;
			}
			if (!transaction_dispatch_slots.empty())
			{
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			die_id = (die_id + 1) % die_no_per_chip;
		}
		return true;
		/*release(&UserReadTRQueue[chip->ChannelID][chip->ChipID], read_buffer[chip->ChannelID][chip->ChipID],
			read_intensity[chip->ChannelID][chip->ChipID]);*/
	}

	bool TSU_SpeedLimit::service_write_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
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

		std::list<Flash_Transaction_Queue*> queue_list;
		/*if (!MappingWriteTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_back(&MappingWriteTRQueue[chip->ChannelID][chip->ChipID]);
		}*/
		release(queue_list, &UserWriteTRQueue[chip->ChannelID][chip->ChipID], write_buffer[chip->ChannelID][chip->ChipID],
			write_intensity[chip->ChannelID][chip->ChipID]);
		if (!GCWriteTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_front(&GCWriteTRQueue[chip->ChannelID][chip->ChipID]);
		}
		if (queue_list.empty()) return false;
		flash_die_ID_type die_id = queue_list.front()->front()->Address.DieID;
		bool dispathed_user_transaction = false;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			if (queue_list.empty()) break;
			transaction_dispatch_slots.clear();
			unsigned int plane_vector = 0;
			flash_page_ID_type page_id = queue_list.front()->front()->Address.PageID;
			for (std::list<Flash_Transaction_Queue*>::iterator queue = queue_list.begin(); queue != queue_list.end(); )
			{
				for (Flash_Transaction_Queue::iterator it = (*queue)->begin(); it != (*queue)->end(); )
				{
					if ((*it)->Address.DieID == die_id && !(plane_vector & 1 << (*it)->Address.PlaneID)
						&& ((NVM_Transaction_Flash_WR*)*it)->RelatedRead == NULL)
					{
						if (plane_vector == 0) page_id = (*it)->Address.PageID;
						if ((*it)->Address.PageID == page_id)
						{
							stream_id_type dispatched_stream_id = (*it)->Stream_id;
							sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
								+ _NVMController->Expected_command_time(*it);
							Transaction_Type type = (*it)->Type;
							Transaction_Source_Type source = (*it)->Source;
							(*it)->SuspendRequired = suspensionRequired;
							plane_vector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							(*queue)->remove(it++);
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
					++it;
				}
				if ((*queue)->empty())
				{
					queue_list.erase(queue++);
					continue;
				}
				++queue;
				if (transaction_dispatch_slots.size() >= plane_no_per_die) break;
			}
			if (!transaction_dispatch_slots.empty())
			{
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			die_id = (die_id + 1) % die_no_per_chip;
		}
		return true;
		/*release(&UserWriteTRQueue[chip->ChannelID][chip->ChipID], write_buffer[chip->ChannelID][chip->ChipID],
			write_intensity[chip->ChannelID][chip->ChipID]);*/
	}

	bool TSU_SpeedLimit::service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip)
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

	void TSU_SpeedLimit::service_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (_NVMController->GetChipStatus(chip) != ChipStatus::IDLE)
			return;
		if (!service_read_transaction(chip))
			if (!service_write_transaction(chip))
				service_erase_transaction(chip);
	}

	void TSU_SpeedLimit::release(Flash_Transaction_Queue* queue, Flash_Transaction_Queue* buffer, unsigned int* intensity)
	{
		size_t total_size = 0;
		std::vector<stream_id_type> ids;
		for (stream_id_type stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			ids.emplace_back(stream_id);
			total_size += buffer[stream_id].size();
		}
		if (total_size == 0) return;
		sort(ids.begin(), ids.end(), [=](const stream_id_type& a, const stream_id_type& b) { return buffer[a].size() < buffer[b].size(); });
		int cnt = std::min((int)total_size, 4);
		unsigned int idx = 0;
		while (cnt > 0)
		{
			while (idx < stream_count && buffer[ids[idx]].empty()) ++idx;
			stream_id_type stream_id = ids[idx];
			queue->push_back(buffer[stream_id].front());
			buffer[stream_id].pop_front();
			--cnt;
		}
	}
	void TSU_SpeedLimit::release(std::list<Flash_Transaction_Queue*>& queue_list, Flash_Transaction_Queue* buffer, unsigned int* intensity)
	{
		std::vector<stream_id_type> ids;
		for (stream_id_type stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (!buffer[stream_id].empty()) ids.emplace_back(stream_id);
		}
		if (ids.empty()) return;
		if (ids.size() > 1)
			sort(ids.begin(), ids.end(), [=](const stream_id_type& a, const stream_id_type& b) { return buffer[a].size() < buffer[b].size(); });
		for (stream_id_type id : ids)
		{
			queue_list.emplace_back(&buffer[id]);
		}
	}
	void TSU_SpeedLimit::release(std::list<Flash_Transaction_Queue*>& queue_list, Flash_Transaction_Queue* queue, Flash_Transaction_Queue* buffer,
		unsigned int* intensity)
	{
		const unsigned int limit = 4;
		// find buffers which buffer transactions
		size_t total_buffer_size = 0;
		std::vector<stream_id_type> ids;
		for (stream_id_type stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (!buffer[stream_id].empty())
			{
				ids.emplace_back(stream_id);
				total_buffer_size += buffer[stream_id].size();
			}
		}
		if (ids.empty())
		{
			if (!queue->empty())
			{
				queue_list.emplace_back(queue);
			}
			return;
		}
		if (ids.size() == 1)
		{
			stream_id_type stream_id = ids.front();
			while (!buffer[stream_id].empty() && queue->size() < limit)
			{
				queue->push_back(buffer[stream_id].front());
				buffer[stream_id].pop_front();
			}
			queue_list.emplace_back(queue);
			if (!buffer[stream_id].empty())
			{
				queue_list.emplace_back(&buffer[stream_id]);
			}
			return;
		}
		// calculate probability by buffer size
		std::list<std::pair<double, stream_id_type>> probs;
		for (const stream_id_type& stream_id : ids)
		{
			double prob = (1 - (double)buffer[stream_id].size() / total_buffer_size) / ((double)ids.size() - 1);
			probs.emplace_back(prob, stream_id);
		}
		// sort by descending order according to probability
		probs.sort([](const std::pair<double, stream_id_type>& a, const std::pair<double, stream_id_type>& b)
			{
				return a.first > b.first;
			});
		// dispatch transaction according to probability
		double vanished_prob = 0;
		while (!probs.empty() && queue->size() < limit)
		{
			double prob = (double)(rand() % 10000) / 10000 - vanished_prob;
			for (auto it = probs.begin(); it != probs.end(); ++it)
			{
				if (prob <= it->first)
				{
					queue->push_back(buffer[it->second].front());
					buffer[it->second].pop_front();
					// deal with the situation where buffer may become empty during dipatching
					if (buffer[it->second].empty())
					{
						vanished_prob += it->first;
						probs.erase(it);
					}
					break;
				}
				prob -= it->first;
			}
		}
		queue_list.emplace_back(queue);
		// parallelism in die- and plane- level
		for (const auto& prob : probs)
		{
			queue_list.emplace_back(&buffer[prob.second]);
		}
	}
}