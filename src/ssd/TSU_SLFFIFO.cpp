#include "TSU_SLFFIFO.h"

namespace SSD_Components
{

	TSU_SLFFIFO::TSU_SLFFIFO(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, unsigned int ChannelCount, unsigned int chip_no_per_channel,
		unsigned int DieNoPerChip, unsigned int PlaneNoPerDie, unsigned int StreamCount,
		sim_time_type WriteReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForWrite,
		bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled)
		: TSU_Base(id, ftl, NVMController, Flash_Scheduling_Type::SLF_FIFO, ChannelCount, chip_no_per_channel, DieNoPerChip, PlaneNoPerDie,
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

		read_unit = new TSU_SLUnit(channel_count * chip_no_per_channel * 1, stream_count);
		write_unit = new TSU_SLUnit(channel_count * chip_no_per_channel * 1, stream_count);
	}

	TSU_SLFFIFO::~TSU_SLFFIFO()
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

		delete read_unit;
		delete write_unit;
	}

	void TSU_SLFFIFO::Start_simulation() {}

	void TSU_SLFFIFO::Validate_simulation_config() {}

	void TSU_SLFFIFO::Execute_simulator_event(MQSimEngine::Sim_Event* event) {}

	void TSU_SLFFIFO::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter)
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

	inline void TSU_SLFFIFO::Prepare_for_transaction_submit()
	{
		opened_scheduling_reqs++;
		if (opened_scheduling_reqs > 1)
			return;
		transaction_receive_slots.clear();
	}

	void TSU_SLFFIFO::Submit_transaction(NVM_Transaction_Flash* transaction)
	{
		transaction_receive_slots.push_back(transaction);
	}

	void TSU_SLFFIFO::handle_transaction_serviced_signal(NVM_Transaction_Flash* transaction)
	{
		/*if (transaction->Source != Transaction_Source_Type::CACHE && transaction->Source != Transaction_Source_Type::USERIO && stream_count == 1) return;
		if (transaction->Type == Transaction_Type::READ) read_unit->feedback(transaction->Stream_id);
		else if (transaction->Type == Transaction_Type::WRITE) write_unit->feedback(transaction->Stream_id);*/
	}

	void TSU_SLFFIFO::Schedule0()
	{
		opened_scheduling_reqs--;
		if (opened_scheduling_reqs > 0)
			return;
		if (opened_scheduling_reqs < 0)
			PRINT_ERROR("TSU_SLFFIFO: Illegal status!");

		if (transaction_receive_slots.size() == 0)
			return;

		for (std::list<NVM_Transaction_Flash*>::iterator it = transaction_receive_slots.begin();
			it != transaction_receive_slots.end(); it++)
		{
			_NVMController->test_transaction_for_conflicting_with_gc(*it);
			if ((*it)->Physical_address_determined)
			{
				flash_channel_ID_type channel_id = (*it)->Address.ChannelID;
				flash_chip_ID_type chip_id = (*it)->Address.ChipID;
				switch ((*it)->Type)
				{
				case Transaction_Type::READ:
					switch ((*it)->Source)
					{
					case Transaction_Source_Type::CACHE:
					case Transaction_Source_Type::USERIO:
						estimate_alone_time(*it, &UserReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID], read_unit->get_buffer((*it)->Stream_id));
						if (stream_count == 1)
							UserReadTRQueue[channel_id][chip_id].push_back((*it));
						else
							read_unit->charging((*it));
						break;
					case Transaction_Source_Type::MAPPING:
						MappingReadTRQueue[channel_id][chip_id].push_back((*it));
						break;
					case Transaction_Source_Type::GC_WL:
						GCReadTRQueue[channel_id][chip_id].push_back((*it));
						break;
					}
					break;
				case Transaction_Type::WRITE:
					switch ((*it)->Source)
					{
					case Transaction_Source_Type::CACHE:
					case Transaction_Source_Type::USERIO:
						estimate_alone_time(*it, &UserWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID], write_unit->get_buffer((*it)->Stream_id));
						if (stream_count == 1)
							UserWriteTRQueue[channel_id][chip_id].push_back((*it));
						else
							write_unit->charging((*it));
						break;
					case Transaction_Source_Type::MAPPING:
						MappingWriteTRQueue[channel_id][chip_id].push_back((*it));
						break;
					case Transaction_Source_Type::GC_WL:
						GCWriteTRQueue[channel_id][chip_id].push_back((*it));
						break;
					}
					break;
				case Transaction_Type::ERASE:
					GCEraseTRQueue[channel_id][chip_id].push_back((*it));
					break;
				default:
					break;
				}
			}
			else
			{
				if (((*it)->Source != Transaction_Source_Type::CACHE && (*it)->Source != Transaction_Source_Type::USERIO) || stream_count == 1)
				{
					std::cout << "wrong in slf tsu  stream count " << stream_count << "  address " << (int)(*it)->Physical_address_determined
						<< "  type " << (int)(*it)->Type << "  source " << (int)(*it)->Source << "\t"
						<< (*it)->Address.ChannelID << (*it)->Address.ChipID << (*it)->Address.DieID << (*it)->Address.PlaneID << "\n";
				}
				else if ((*it)->Type == Transaction_Type::READ)
				{
					estimate_alone_time(*it, read_unit->get_buffer((*it)->Stream_id));
					read_unit->charging((*it));
				}
				else if ((*it)->Type == Transaction_Type::WRITE)
				{
					estimate_alone_time(*it, write_unit->get_buffer((*it)->Stream_id));
					write_unit->charging((*it));
				}
			}
		}


		for (flash_channel_ID_type channelID = 0; channelID < channel_count; channelID++)
		{
			if (_NVMController->Get_channel_status(channelID) == BusChannelStatus::IDLE)
			{
				enqueue_transaction_for_speed_limit_type_tsu();
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

	void TSU_SLFFIFO::estimate_alone_time(NVM_Transaction_Flash* transaction, Flash_Transaction_Queue* buffer)
	{
		sim_time_type chip_busy_time = 0, waiting_last_time = 0;
		NVM_Transaction_Flash* chip_tr = _NVMController->Is_chip_busy_with_stream(transaction);
		if (chip_tr && _NVMController->Expected_finish_time(chip_tr) > Simulator->Time())
		{
			chip_busy_time = _NVMController->Expected_finish_time(chip_tr) - Simulator->Time();
		}
		for (auto tr = buffer->begin(); tr != buffer->end(); ++tr)
		{
			waiting_last_time += _NVMController->Expected_transfer_time(*tr) + _NVMController->Expected_command_time(*tr);
		}
		waiting_last_time = user_alone_time(waiting_last_time, transaction->Type);
		transaction->alone_time = chip_busy_time + waiting_last_time
			+ _NVMController->Expected_transfer_time(transaction) + _NVMController->Expected_command_time(transaction);
	}

	void TSU_SLFFIFO::estimate_alone_time(NVM_Transaction_Flash* transaction, Flash_Transaction_Queue* queue, Flash_Transaction_Queue* buffer)
	{
		sim_time_type chip_busy_time = 0, waiting_last_time = 0;
		NVM_Transaction_Flash* chip_tr = _NVMController->Is_chip_busy_with_stream(transaction);
		if (chip_tr && _NVMController->Expected_finish_time(chip_tr) > Simulator->Time())
		{
			chip_busy_time = _NVMController->Expected_finish_time(chip_tr) - Simulator->Time();
		}
		for (auto tr = queue->begin(); tr != queue->end(); ++tr)
		{
			if ((*tr)->Stream_id == transaction->Stream_id)
			{
				waiting_last_time += _NVMController->Expected_transfer_time(*tr) + _NVMController->Expected_command_time(*tr);
			}
		}
		for (auto tr = buffer->begin(); tr != buffer->end(); ++tr)
		{
			waiting_last_time += _NVMController->Expected_transfer_time(*tr) + _NVMController->Expected_command_time(*tr);
		}
		waiting_last_time = user_alone_time(waiting_last_time, transaction->Type);
		transaction->alone_time = chip_busy_time + waiting_last_time
			+ _NVMController->Expected_transfer_time(transaction) + _NVMController->Expected_command_time(transaction);
	}

	void TSU_SLFFIFO::adjust_alone_time(stream_id_type dispatched_stream_id, sim_time_type adjust_time, Transaction_Type type,
		Transaction_Source_Type source, Flash_Transaction_Queue* queue, Flash_Transaction_Queue* buffer,
		flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
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
		adjust_time /= channel_count * chip_no_per_channel;
		for (auto it = buffer->begin(); it != buffer->end(); ++it)
		{
			(*it)->alone_time += adjust_time;
		}
	}

	size_t TSU_SLFFIFO::GCTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return GCReadTRQueue[channel_id][chip_id].size() + GCWriteTRQueue[channel_id][chip_id].size() + GCEraseTRQueue[channel_id][chip_id].size();
	}

	size_t TSU_SLFFIFO::UserTRQueueSize(stream_id_type gc_stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		//size_t size = read_unit->get_buffer(gc_stream_id)->size() + write_unit->get_buffer(gc_stream_id)->size();
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

	size_t TSU_SLFFIFO::UserTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		size_t size = 0;
		//for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		//{
		//	size += read_unit->get_buffer(stream_id)->size() + write_unit->get_buffer(stream_id)->size();
		//}
		//// address unknown
		//size /= channel_count * chip_no_per_channel;
		size += UserReadTRQueue[channel_id][chip_id].size() + UserWriteTRQueue[channel_id][chip_id].size();
		return size;
	}

	bool TSU_SLFFIFO::service_read_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		Flash_Transaction_Queue* sourceQueue1 = NULL, * sourceQueue2 = NULL;

		if (MappingReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)//Flash transactions that are related to FTL mapping data have the highest priority
		{
			sourceQueue1 = &MappingReadTRQueue[chip->ChannelID][chip->ChipID];
			if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip) && GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			else if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
		}
		else if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first
		{
			if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
				if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
					sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
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
								&UserReadTRQueue[chip->ChannelID][chip->ChipID], read_unit->get_buffer(dispatched_stream_id),
								chip->ChannelID, chip->ChipID);
						}
						adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
							&UserWriteTRQueue[chip->ChannelID][chip->ChipID], write_unit->get_buffer(dispatched_stream_id),
							chip->ChannelID, chip->ChipID);
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
									&UserReadTRQueue[chip->ChannelID][chip->ChipID], read_unit->get_buffer(dispatched_stream_id),
									chip->ChannelID, chip->ChipID);
							}
							adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
								&UserWriteTRQueue[chip->ChannelID][chip->ChipID], write_unit->get_buffer(dispatched_stream_id),
								chip->ChannelID, chip->ChipID);
							continue;
						}
					}
					it++;
				}

			if (transaction_dispatch_slots.size() > 0)
			{
				if (stream_count > 1)
				{
					for (auto tr : transaction_dispatch_slots)
					{
						if (tr->Source == Transaction_Source_Type::CACHE || tr->Source == Transaction_Source_Type::USERIO)
							read_unit->feedback(tr->Stream_id);
					}
				}
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			transaction_dispatch_slots.clear();
			dieID = (dieID + 1) % die_no_per_chip;
		}

		return true;
	}

	bool TSU_SLFFIFO::service_write_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		Flash_Transaction_Queue* sourceQueue1 = NULL, * sourceQueue2 = NULL;

		if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first
		{
			if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
				if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
					sourceQueue2 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
			}
			else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				return false;
			else if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
			else return false;
		}
		else //If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first
		{
			if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
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
								&UserWriteTRQueue[chip->ChannelID][chip->ChipID], write_unit->get_buffer(dispatched_stream_id),
								chip->ChannelID, chip->ChipID);
						}
						adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
							&UserReadTRQueue[chip->ChannelID][chip->ChipID], read_unit->get_buffer(dispatched_stream_id),
							chip->ChannelID, chip->ChipID);
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
									&UserWriteTRQueue[chip->ChannelID][chip->ChipID], write_unit->get_buffer(dispatched_stream_id),
									chip->ChannelID, chip->ChipID);
							}
							adjust_alone_time(dispatched_stream_id, adjust_time, type, source,
								&UserReadTRQueue[chip->ChannelID][chip->ChipID], read_unit->get_buffer(dispatched_stream_id),
								chip->ChannelID, chip->ChipID);
							continue;
						}
					}
					it++;
				}

			if (transaction_dispatch_slots.size() > 0)
			{
				if (stream_count > 1)
				{
					for (auto tr : transaction_dispatch_slots)
					{
						if (tr->Source == Transaction_Source_Type::CACHE || tr->Source == Transaction_Source_Type::USERIO)
							write_unit->feedback(tr->Stream_id);
					}
				}
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			transaction_dispatch_slots.clear();
			dieID = (dieID + 1) % die_no_per_chip;
		}
		return true;
	}

	bool TSU_SLFFIFO::service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip)
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
						&UserReadTRQueue[chip->ChannelID][chip->ChipID], read_unit->get_buffer(dispatched_stream_id),
						chip->ChannelID, chip->ChipID);
					adjust_alone_time(dispatched_stream_id, adjust_time, type, Transaction_Source_Type::GC_WL,
						&UserWriteTRQueue[chip->ChannelID][chip->ChipID], write_unit->get_buffer(dispatched_stream_id),
						chip->ChannelID, chip->ChipID);
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

	void TSU_SLFFIFO::service_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (_NVMController->GetChipStatus(chip) != ChipStatus::IDLE
			|| _NVMController->Get_channel_status(chip->ChannelID) != BusChannelStatus::IDLE)
			return;
		enqueue_transaction_for_speed_limit_type_tsu();
		if (!service_read_transaction(chip))
			if (!service_write_transaction(chip))
				service_erase_transaction(chip);
	}
	void TSU_SLFFIFO::enqueue_transaction_for_speed_limit_type_tsu()
	{
		if (stream_count == 1) return;
		NVM_Transaction_Flash* read = read_unit->discharging();
		while (read)
		{
			if (!read->Physical_address_determined)
			{
				std::cout << "wrong read\t" << read->LPA << "\n";
			}
			UserReadTRQueue[read->Address.ChannelID][read->Address.ChipID].push_back(read);
			read = read_unit->discharging();
		}
		NVM_Transaction_Flash* write = write_unit->discharging();
		while (write)
		{
			if (!write->Physical_address_determined)
			{
				ftl->Address_Mapping_Unit->translate_lpa_to_ppa_for_write_with_slf(write);
			}
			if (write->Physical_address_determined)
			{
				UserWriteTRQueue[write->Address.ChannelID][write->Address.ChipID].push_back(write);
				if (((NVM_Transaction_Flash_WR*)write)->RelatedRead)
				{
					NVM_Transaction_Flash* read = ((NVM_Transaction_Flash_WR*)write)->RelatedRead;
					UserReadTRQueue[read->Address.ChannelID][read->Address.ChipID].push_back(read);
				}
			}
			write = write_unit->discharging();
		}
	}
}
