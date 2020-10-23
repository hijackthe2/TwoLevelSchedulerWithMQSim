#include "TSU_SpeedLimit.h"

namespace SSD_Components
{
	TSU_SpeedLimit::TSU_SpeedLimit(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController,
		unsigned int channel_count, unsigned int chip_count_per_channel, unsigned int die_count_per_chip,
		unsigned int plane_count_per_die, unsigned int StreamCount, sim_time_type WriteReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForRead, sim_time_type EraseReasonableSuspensionTimeForWrite,
		bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled)
		: TSU_Base(id, ftl, NVMController, Flash_Scheduling_Type::SPEED_LIMIT, channel_count, chip_count_per_channel,
			die_count_per_chip, plane_count_per_die, WriteReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForRead,
			EraseReasonableSuspensionTimeForWrite, EraseSuspensionEnabled, ProgramSuspensionEnabled),
		stream_count(StreamCount), min_user_read_arrival_count(channel_count), min_user_write_arrival_count(channel_count),
		max_user_read_arrival_count(channel_count* chip_count_per_channel* die_count_per_chip* plane_count_per_die),
		max_user_write_arrival_count(channel_count* chip_count_per_channel* die_count_per_chip* plane_count_per_die)
	{
		UserReadTRQueue = new Flash_Transaction_Queue * [channel_count];
		UserWriteTRQueue = new Flash_Transaction_Queue * [channel_count];
		GCReadTRQueue = new Flash_Transaction_Queue * [channel_count];
		GCWriteTRQueue = new Flash_Transaction_Queue * [channel_count];
		GCEraseTRQueue = new Flash_Transaction_Queue * [channel_count];
		MappingReadTRQueue = new Flash_Transaction_Queue * [channel_count];
		MappingWriteTRQueue = new Flash_Transaction_Queue * [channel_count];

		transaction_waiting_dispatch_slots = new std::list<std::pair<Transaction_Type, Transaction_Source_Type>> * [channel_count];
		serviced_writes_since_last_GC = new unsigned int** [channel_count];
		remain_read_queue_count = new unsigned long** [channel_count];
		remain_write_queue_count = new unsigned long** [channel_count];

		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			UserReadTRQueue[channel_id] = new Flash_Transaction_Queue[chip_count_per_channel];
			UserWriteTRQueue[channel_id] = new Flash_Transaction_Queue[chip_count_per_channel];
			GCReadTRQueue[channel_id] = new Flash_Transaction_Queue[chip_count_per_channel];
			GCWriteTRQueue[channel_id] = new Flash_Transaction_Queue[chip_count_per_channel];
			GCEraseTRQueue[channel_id] = new Flash_Transaction_Queue[chip_count_per_channel];
			MappingReadTRQueue[channel_id] = new Flash_Transaction_Queue[chip_count_per_channel];
			MappingWriteTRQueue[channel_id] = new Flash_Transaction_Queue[chip_count_per_channel];

			transaction_waiting_dispatch_slots[channel_id] = new std::list<std::pair<Transaction_Type, Transaction_Source_Type>>[chip_count_per_channel];
			serviced_writes_since_last_GC[channel_id] = new unsigned int* [chip_count_per_channel];
			remain_read_queue_count[channel_id] = new unsigned long* [chip_count_per_channel];
			remain_write_queue_count[channel_id] = new unsigned long* [chip_count_per_channel];

			for (unsigned int chip_id = 0; chip_id < chip_count_per_channel; chip_id++)
			{
				std::string str = std::to_string(channel_id) + "@" + std::to_string(chip_id);
				UserReadTRQueue[channel_id][chip_id].Set_id("User_Read_TR_Queue@" + str);
				UserWriteTRQueue[channel_id][chip_id].Set_id("User_Write_TR_Queue@" + str);
				GCReadTRQueue[channel_id][chip_id].Set_id("GC_Read_TR_Queue@" + str);
				MappingReadTRQueue[channel_id][chip_id].Set_id("Mapping_Read_TR_Queue@" + str);
				MappingWriteTRQueue[channel_id][chip_id].Set_id("Mapping_Write_TR_Queue@" + str);
				GCWriteTRQueue[channel_id][chip_id].Set_id("GC_Write_TR_Queue@" + str);
				GCEraseTRQueue[channel_id][chip_id].Set_id("GC_Erase_TR_Queue@" + str);

				transaction_waiting_dispatch_slots[channel_id][chip_id].clear();
				serviced_writes_since_last_GC[channel_id][chip_id] = new unsigned int[stream_count];
				remain_read_queue_count[channel_id][chip_id] = new unsigned long[stream_count];
				remain_write_queue_count[channel_id][chip_id] = new unsigned long[stream_count];
				for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
				{
					serviced_writes_since_last_GC[channel_id][chip_id][stream_id] = 0;
					remain_read_queue_count[channel_id][chip_id][stream_id] = 0;
					remain_write_queue_count[channel_id][chip_id][stream_id] = 0;
				}
			}
		}

		user_read_arrival_count = new unsigned int[stream_count];
		user_write_arrival_count = new unsigned int[stream_count];
		UserReadTRCount = new unsigned int[stream_count];
		UserWriteTRCount = new unsigned int[stream_count];
		UserReadTRBuffer = new Flash_Transaction_Queue[stream_count];
		UserWriteTRBuffer = new Flash_Transaction_Queue[stream_count];
		user_read_limit_speed = new unsigned int[stream_count];
		user_write_limit_speed = new unsigned int[stream_count];
		shared_total_time = new sim_time_type[stream_count];
		alone_total_time = new sim_time_type[stream_count];
		total_count = new unsigned long long[stream_count];
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			user_read_arrival_count[stream_id] = 0;
			user_write_arrival_count[stream_id] = 0;
			UserReadTRCount[stream_id] = 0;
			UserWriteTRCount[stream_id] = 0;
			user_read_limit_speed[stream_id] = min_user_read_arrival_count;
			user_write_limit_speed[stream_id] = min_user_write_arrival_count;
			user_read_idx.push_back(stream_id);
			user_write_idx.push_back(stream_id);
			shared_total_time[stream_id] = 0;
			alone_total_time[stream_id] = 0;
			total_count[stream_id] = 0;
		}
	}

	TSU_SpeedLimit::~TSU_SpeedLimit()
	{
		std::cout << "\nestimated values\n";
		for (stream_id_type stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			std::cout << "==========\n";
			std::cout << "shared\t" << shared_total_time[stream_id] / (total_count[stream_id] + 1e-5) / 1000
				<< "\talone\t" << alone_total_time[stream_id] / (total_count[stream_id] + 1e-5) / 1000
				<< "\tslowdown\t" << (double)shared_total_time[stream_id] / alone_total_time[stream_id]
				<< "\n";
		}
		std::cout << "==========\n";
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			delete[] UserReadTRQueue[channel_id];
			delete[] UserWriteTRQueue[channel_id];
			delete[] GCReadTRQueue[channel_id];
			delete[] GCWriteTRQueue[channel_id];
			delete[] GCEraseTRQueue[channel_id];
			delete[] MappingReadTRQueue[channel_id];
			delete[] MappingWriteTRQueue[channel_id];

			delete[] transaction_waiting_dispatch_slots[channel_id];
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; ++chip_id)
			{
				delete[] serviced_writes_since_last_GC[channel_id][chip_id];
				delete[] remain_read_queue_count[channel_id][chip_id];
				delete[] remain_write_queue_count[channel_id][chip_id];
			}
			delete[] serviced_writes_since_last_GC[channel_id];
			delete[] remain_read_queue_count[channel_id];
			delete[] remain_write_queue_count[channel_id];
		}
		delete[] UserReadTRQueue;
		delete[] UserWriteTRQueue;
		delete[] GCReadTRQueue;
		delete[] GCWriteTRQueue;
		delete[] GCEraseTRQueue;
		delete[] MappingReadTRQueue;
		delete[] MappingWriteTRQueue;

		delete[] user_read_arrival_count;
		delete[] user_write_arrival_count;
		delete[] UserReadTRCount;
		delete[] UserWriteTRCount;
		delete[] UserReadTRBuffer;
		delete[] UserWriteTRBuffer;
		delete[] user_read_limit_speed;
		delete[] user_write_limit_speed;

		delete[] transaction_waiting_dispatch_slots;
		delete[] serviced_writes_since_last_GC;
		delete[] shared_total_time;
		delete[] alone_total_time;
		delete[] total_count;
		delete[] remain_read_queue_count;
		delete[] remain_write_queue_count;
	}

	void TSU_SpeedLimit::Start_simulation()
	{
		if (stream_count > 1)
		{
			Simulator->Register_sim_event(1000000, this, 0, (int)Transaction_Type::READ);
			Simulator->Register_sim_event(2000000, this, 0, (int)Transaction_Type::WRITE);
		}
	}
	void TSU_SpeedLimit::Validate_simulation_config() {}

	void TSU_SpeedLimit::Execute_simulator_event(MQSimEngine::Sim_Event* event)
	{
		switch (event->Type)
		{
		case (int)Transaction_Type::READ:
			update(user_read_arrival_count, user_read_limit_speed, max_user_read_arrival_count, max_user_read_arrival_count / 2,
				min_user_read_arrival_count, user_read_idx, 1000000, event->Type);
			break;
		case (int)Transaction_Type::WRITE:
			update(user_write_arrival_count, user_write_limit_speed, max_user_write_arrival_count, max_user_write_arrival_count / 2,
				min_user_write_arrival_count, user_write_idx, 2000000, event->Type);
			break;
		default:
			break;
		}
	}

	void TSU_SpeedLimit::update(unsigned int* arrival_count, unsigned int* limit_speed, unsigned int max_arrival_count,
		unsigned int middle_arrival_count, unsigned int min_arrival_count, std::vector<stream_id_type>& idx,
		const unsigned int interval_time, int type)
	{
		unsigned int min_count = INT_MAX;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (arrival_count[stream_id] > 0)
			{
				min_count = std::min(min_count, arrival_count[stream_id]);
			}
		}
		double* workload_slowdown = new double[stream_count];
		double max_slowdown = -1;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (arrival_count[stream_id] > 0)
			{
				min_count = std::min(min_count, arrival_count[stream_id]);
			}
			workload_slowdown[stream_id] = 1.0;
			if (alone_total_time[stream_id] > 0)
			{
				workload_slowdown[stream_id] = (double)shared_total_time[stream_id] / alone_total_time[stream_id];
				max_slowdown = std::max(max_slowdown, workload_slowdown[stream_id]);
			}
		}
		min_count = min_count > max_arrival_count ? max_arrival_count : std::min(min_count, middle_arrival_count);
		if (type == (int)Transaction_Type::READ)
		{
			double* tmp = new double[stream_count];
			for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
			{
				tmp[stream_id] = (double)arrival_count[stream_id] / min_count;
			}
			for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
			{
				limit_speed[stream_id] = tmp[stream_id] == 0 ? max_arrival_count
					: std::max((unsigned int)(min_count / tmp[stream_id]), min_arrival_count);
				limit_speed[stream_id] *= workload_slowdown[stream_id] / max_slowdown;
				arrival_count[stream_id] = 0;
				idx[stream_id] = stream_id;
			}
			sort(idx.begin(), idx.end(), [=](int a, int b) {return limit_speed[a] > limit_speed[b]; });
			delete[] tmp;
		}
		else if (type == (int)Transaction_Type::WRITE)
		{
			for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
			{
				limit_speed[stream_id] = arrival_count[stream_id] == 0 ? max_arrival_count
					: std::max(min_count, min_arrival_count);
				limit_speed[stream_id] *= workload_slowdown[stream_id] / max_slowdown;
				arrival_count[stream_id] = 0;
			}
		}
		delete[] workload_slowdown;
		Simulator->Register_sim_event(Simulator->Time() + interval_time, this, 0, type);
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

	void TSU_SpeedLimit::handle_transaction_serviced_signal_from_PHY(NVM_Transaction_Flash* transaction)
	{
		if (transaction->Source == Transaction_Source_Type::GC_WL || transaction->Source == Transaction_Source_Type::MAPPING)
			return;
		total_count[transaction->Stream_id]++;
		shared_total_time[transaction->Stream_id] += Simulator->Time() - transaction->Issue_time;
		alone_total_time[transaction->Stream_id] += transaction->alone_time;
	}

	double TSU_SpeedLimit::proportional_slowdown(stream_id_type gc_stream_id)
	{
		if (stream_count) return 0.0;
		double min_slowdown = INT_MAX;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (alone_total_time[stream_id])
			{
				min_slowdown = std::min(min_slowdown, (double)shared_total_time[stream_id] / alone_total_time[stream_id]);
			}
		}
		double slowdown = (double)shared_total_time[gc_stream_id] / (1e-10 + alone_total_time[gc_stream_id]);
		return min_slowdown / (1e-10 + slowdown);
	}

	size_t TSU_SpeedLimit::GCEraseTRQueueSize(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return GCEraseTRQueue[channel_id][chip_id].size();
	}

	void TSU_SpeedLimit::Schedule()
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
			switch ((*it)->Type)
			{
			case Transaction_Type::READ:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					estimate_alone_time(*it, remain_read_queue_count[(*it)->Address.ChannelID][(*it)->Address.ChipID][(*it)->Stream_id]);
					if (stream_count == 1)
						UserReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					else
					{
						user_read_arrival_count[(*it)->Stream_id]++;
						UserReadTRBuffer[(*it)->Stream_id].push_back(*it);
					}
					remain_read_queue_count[(*it)->Address.ChannelID][(*it)->Address.ChipID][(*it)->Stream_id]++;
					break;
				case Transaction_Source_Type::MAPPING:
					if (stream_count > 1)
					{
						//user_read_arrival_count[(*it)->Stream_id]++;
						UserReadTRCount[(*it)->Stream_id]++;
					}
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
					estimate_alone_time(*it, remain_write_queue_count[(*it)->Address.ChannelID][(*it)->Address.ChipID][(*it)->Stream_id]);
					if (stream_count == 1)
						UserWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					else
					{
						user_write_arrival_count[(*it)->Stream_id]++;
						UserWriteTRBuffer[(*it)->Stream_id].push_back(*it);
					}
					remain_write_queue_count[(*it)->Address.ChannelID][(*it)->Address.ChipID][(*it)->Stream_id]++;
					break;
				case Transaction_Source_Type::MAPPING:
					if (stream_count > 1)
					{
						//user_write_arrival_count[(*it)->Stream_id]++;
						//UserWriteTRCount[(*it)->Stream_id]++;
					}
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
		for (flash_channel_ID_type channel_id = 0; channel_id < channel_count; channel_id++)
		{
			if (_NVMController->Get_channel_status(channel_id) == BusChannelStatus::IDLE)
			{
				for (unsigned int i = 0; i < chip_no_per_channel; i++)
				{
					NVM::FlashMemory::Flash_Chip* chip = _NVMController->Get_chip(channel_id, Round_robin_turn_of_channel[channel_id]);
					//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
					service_transaction(chip);
					Round_robin_turn_of_channel[channel_id] = (flash_chip_ID_type)(Round_robin_turn_of_channel[channel_id] + 1) % chip_no_per_channel;
					if (_NVMController->Get_channel_status(chip->ChannelID) != BusChannelStatus::IDLE)
						break;
				}
			}
		}
	}

	void TSU_SpeedLimit::speed_limit(Flash_Transaction_Queue** UserTRQueue, Flash_Transaction_Queue* UserTRBuffer,
		unsigned int* UserTRCount, unsigned int* user_limit_speed, std::vector<stream_id_type>& user_idx)
	{
		if (stream_count == 1) return;

		for (unsigned int id = 0; id < stream_count; ++id)
		{
			stream_id_type stream_id = user_idx[id];
			while (!(UserTRBuffer[stream_id].empty()) && UserTRCount[stream_id] < user_limit_speed[stream_id])
			{
				NVM_Transaction_Flash* it = UserTRBuffer[stream_id].front();
				UserTRBuffer[stream_id].pop_front();
				UserTRQueue[it->Address.ChannelID][it->Address.ChipID].push_back(it);
				UserTRCount[stream_id]++;
			}
		}
	}

	void TSU_SpeedLimit::estimate_alone_time(NVM_Transaction_Flash* transaction, unsigned long long remain_count)
	{
		sim_time_type chip_busy_time = 0, waiting_last_time = 0;
		NVM_Transaction_Flash* chip_tr = _NVMController->Is_chip_busy_with_stream(transaction);
		if (chip_tr && _NVMController->Expected_finish_time(chip_tr) > Simulator->Time())
		{
			chip_busy_time = _NVMController->Expected_finish_time(chip_tr) - Simulator->Time();
		}
		waiting_last_time = remain_count
			* (_NVMController->Expected_transfer_time(transaction) + _NVMController->Expected_command_time(transaction));
		switch (transaction->Type)
		{
		case Transaction_Type::READ:
			waiting_last_time += waiting_last_time / 2;
			break;
		case Transaction_Type::WRITE:
			waiting_last_time /= 4;
			break;
		default:
			break;
		}
		transaction->alone_time = chip_busy_time + waiting_last_time
			+ _NVMController->Expected_transfer_time(transaction) + _NVMController->Expected_command_time(transaction);
	}

	void TSU_SpeedLimit::adjust_alone_time(stream_id_type dispatched_stream_id, sim_time_type adjust_time, Transaction_Type type,
		Transaction_Source_Type source, Flash_Transaction_Queue* queue, Flash_Transaction_Queue* buffer)
	{
		if (source == Transaction_Source_Type::CACHE || source == Transaction_Source_Type::USERIO)
		{
			if (type == Transaction_Type::READ)
			{
				adjust_time += adjust_time / 2;
			}
			else
			{
				adjust_time /= 4;
			}
		}
		else if (source == Transaction_Source_Type::GC_WL)
		{
			if (type == Transaction_Type::READ)
			{
				adjust_time *= 3;
			}
			else if (type == Transaction_Type::WRITE)
			{
				//adjust_time *= 2;
			}
			else if (type == Transaction_Type::ERASE)
			{
				adjust_time *= 2;
			}
		}
		else if (source == Transaction_Source_Type::MAPPING)
		{

		}
		for (auto it = queue->begin(); it != queue->end(); ++it)
		{
			if (dispatched_stream_id == (*it)->Stream_id)
			{
				(*it)->alone_time += adjust_time;
			}
		}
		for (auto it = buffer->begin(); it != buffer->end(); ++it)
		{
			(*it)->alone_time += adjust_time;
		}
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
		if (stream_count > 1)
		{
			speed_limit(UserReadTRQueue, UserReadTRBuffer, UserReadTRCount, user_read_limit_speed, user_read_idx);
		}
		auto info = transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].front();
		std::list<Flash_Transaction_Queue*> queue_list;
		if (!MappingReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_back(&MappingReadTRQueue[chip->ChannelID][chip->ChipID]);
		}
		if (!UserReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_back(&UserReadTRQueue[chip->ChannelID][chip->ChipID]);
		}
		if (info.second == Transaction_Source_Type::USERIO)
		{
			if (queue_list.empty())
				return false;
			if (!GCReadTRQueue[chip->ChannelID][chip->ChipID].empty())
			{
				queue_list.push_back(&GCReadTRQueue[chip->ChannelID][chip->ChipID]);
			}
		}
		else if (info.second == Transaction_Source_Type::GC_WL)
		{
			if (!GCReadTRQueue[chip->ChannelID][chip->ChipID].empty())
			{
				queue_list.push_front(&GCReadTRQueue[chip->ChannelID][chip->ChipID]);
			}
			else return false;
		}
		flash_die_ID_type die_id = queue_list.front()->front()->Address.DieID;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			if (queue_list.empty())
			{
				break;
			}
			transaction_dispatch_slots.clear();
			unsigned int plane_vector = 0;
			flash_page_ID_type page_id = queue_list.front()->front()->Address.PageID;
			for (std::list<Flash_Transaction_Queue*>::iterator queue = queue_list.begin(); queue != queue_list.end(); )
			{
				for (Flash_Transaction_Queue::iterator it = (*queue)->begin(); it != (*queue)->end(); )
				{
					if ((*it)->Address.DieID == die_id && !(plane_vector & 1 << (*it)->Address.PlaneID))
					{
						if (plane_vector == 0)
						{
							page_id = (*it)->Address.PageID;
						}
						if ((*it)->Address.PageID == page_id)
						{
							if ((*it)->Source == Transaction_Source_Type::CACHE
								|| (*it)->Source == Transaction_Source_Type::USERIO
								|| (*it)->Source == Transaction_Source_Type::MAPPING)
							{
								UserReadTRCount[(*it)->Stream_id]--;
								if ((*it)->Source != Transaction_Source_Type::MAPPING)
								{
									remain_read_queue_count[chip->ChannelID][chip->ChipID][(*it)->Stream_id]--;
								}
							}
							stream_id_type dispatched_stream_id = (*it)->Stream_id;
							sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
								+ _NVMController->Expected_command_time(*it);
							Transaction_Source_Type source = (*it)->Source;
							(*it)->SuspendRequired = suspensionRequired;
							plane_vector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							(*queue)->remove(it++);
							if (source == Transaction_Source_Type::MAPPING
								|| source == Transaction_Source_Type::GC_WL)
							{
								adjust_alone_time(dispatched_stream_id, adjust_time, Transaction_Type::READ, source,
									&UserReadTRQueue[chip->ChannelID][chip->ChipID], &UserReadTRBuffer[dispatched_stream_id]);
							}
							adjust_alone_time(dispatched_stream_id, adjust_time, Transaction_Type::READ, source,
								&UserWriteTRQueue[chip->ChannelID][chip->ChipID], &UserWriteTRBuffer[dispatched_stream_id]);
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
				if (transaction_dispatch_slots.size() < plane_no_per_die)
				{
					break;
				}
			}
			if (!transaction_dispatch_slots.empty())
			{
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			die_id = (die_id + 1) % die_no_per_chip;
		}
		return true;
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
		if (stream_count > 1)
		{
			speed_limit(UserWriteTRQueue, UserWriteTRBuffer, UserWriteTRCount, user_write_limit_speed, user_write_idx);
		}
		auto info = transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].front();
		std::list<Flash_Transaction_Queue*> queue_list;
		/*if (!MappingWriteTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_back(&MappingWriteTRQueue[chip->ChannelID][chip->ChipID]);
		}*/
		if (!UserWriteTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_back(&UserWriteTRQueue[chip->ChannelID][chip->ChipID]);
		}
		if (info.second == Transaction_Source_Type::USERIO)
		{
			if (queue_list.empty())
				return false;
			if (!GCWriteTRQueue[chip->ChannelID][chip->ChipID].empty())
			{
				queue_list.push_back(&GCWriteTRQueue[chip->ChannelID][chip->ChipID]);
			}
		}
		else if (info.second == Transaction_Source_Type::GC_WL)
		{
			if (!GCWriteTRQueue[chip->ChannelID][chip->ChipID].empty())
			{
				queue_list.push_front(&GCWriteTRQueue[chip->ChannelID][chip->ChipID]);
			}
			else return false;
		}
		if (queue_list.empty())
			return false;
		flash_die_ID_type die_id = queue_list.front()->front()->Address.DieID;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			if (queue_list.empty())
			{
				break;
			}
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
						if (plane_vector == 0)
						{
							page_id = (*it)->Address.PageID;
						}
						if ((*it)->Address.PageID == page_id)
						{
							if ((*it)->Source == Transaction_Source_Type::CACHE
								|| (*it)->Source == Transaction_Source_Type::USERIO
								/*|| (*it)->Source == Transaction_Source_Type::MAPPING*/)
							{
								UserWriteTRCount[(*it)->Stream_id]--;
								serviced_writes_since_last_GC[chip->ChannelID][chip->ChipID][(*it)->Stream_id]++;
								if ((*it)->Source != Transaction_Source_Type::MAPPING)
								{
									remain_write_queue_count[chip->ChannelID][chip->ChipID][(*it)->Stream_id]--;
								}
							}
							stream_id_type dispatched_stream_id = (*it)->Stream_id;
							sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
								+ _NVMController->Expected_command_time(*it);
							Transaction_Source_Type source = (*it)->Source;
							(*it)->SuspendRequired = suspensionRequired;
							plane_vector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							(*queue)->remove(it++);
							if (source == Transaction_Source_Type::MAPPING
								|| source == Transaction_Source_Type::GC_WL)
							{
								adjust_alone_time(dispatched_stream_id, adjust_time, Transaction_Type::WRITE, source,
									&UserWriteTRQueue[chip->ChannelID][chip->ChipID], &UserWriteTRBuffer[dispatched_stream_id]);
							}
							adjust_alone_time(dispatched_stream_id, adjust_time, Transaction_Type::WRITE, source,
								&UserReadTRQueue[chip->ChannelID][chip->ChipID], &UserReadTRBuffer[dispatched_stream_id]);
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
				if (transaction_dispatch_slots.size() < plane_no_per_die)
				{
					break;
				}
			}
			if (!transaction_dispatch_slots.empty())
			{
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			die_id = (die_id + 1) % die_no_per_chip;
		}
		return true;
	}

	bool TSU_SpeedLimit::service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (_NVMController->GetChipStatus(chip) != ChipStatus::IDLE)
			return false;

		Flash_Transaction_Queue* source_queue = &GCEraseTRQueue[chip->ChannelID][chip->ChipID];
		if (source_queue->size() == 0)
			return false;

		flash_die_ID_type die_id = source_queue->front()->Address.DieID;
		for (unsigned int i = 0; i < die_no_per_chip; i++) {
			if (source_queue->empty())
			{
				break;
			}
			transaction_dispatch_slots.clear();
			unsigned int plane_vector = 0;
			flash_block_ID_type block_id = source_queue->front()->Address.BlockID;

			for (Flash_Transaction_Queue::iterator it = source_queue->begin();
				it != source_queue->end() && transaction_dispatch_slots.size() < plane_no_per_die; )
			{
				if (((NVM_Transaction_Flash_ER*)*it)->Page_movement_activities.size() == 0 && (*it)->Address.DieID == die_id
					&& !(plane_vector & 1 << (*it)->Address.PlaneID))
				{
					if (plane_vector == 0)
					{
						block_id = (*it)->Address.BlockID;
					}
					if ((*it)->Address.BlockID == block_id)
					{
						stream_id_type dispatched_stream_id = (*it)->Stream_id;
						sim_time_type adjust_time = _NVMController->Expected_transfer_time(*it)
							+ _NVMController->Expected_command_time(*it);
						plane_vector |= 1 << (*it)->Address.PlaneID;
						transaction_dispatch_slots.push_back(*it);
						source_queue->remove(it++);
						adjust_alone_time(dispatched_stream_id, adjust_time, Transaction_Type::ERASE, Transaction_Source_Type::GC_WL,
							&UserReadTRQueue[chip->ChannelID][chip->ChipID], &UserReadTRBuffer[dispatched_stream_id]);
						adjust_alone_time(dispatched_stream_id, adjust_time, Transaction_Type::ERASE, Transaction_Source_Type::GC_WL,
							&UserWriteTRQueue[chip->ChannelID][chip->ChipID], &UserWriteTRBuffer[dispatched_stream_id]);
						continue;
					}
				}
				++it;
			}
			if (!transaction_dispatch_slots.empty()) {
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			die_id = (die_id + 1) % die_no_per_chip;
		}
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			serviced_writes_since_last_GC[chip->ChannelID][chip->ChipID][stream_id] = 0;
		}
		return true;
	}

	void TSU_SpeedLimit::service_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (stream_count > 1)
		{
			speed_limit(UserReadTRQueue, UserReadTRBuffer, UserReadTRCount, user_read_limit_speed, user_read_idx);
			speed_limit(UserWriteTRQueue, UserWriteTRBuffer, UserWriteTRCount, user_write_limit_speed, user_write_idx);
		}
		if (!MappingReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].push_front(
				{ Transaction_Type::READ, Transaction_Source_Type::USERIO }
			);
		}
		/*else if (!MappingWriteTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].push_front(
				{ Transaction_Type::WRITE, Transaction_Source_Type::USERIO }
			);
		}*/
		// the number of valid pages for flow f in SSD cannot be calculated in MQSim
		unsigned int GCM = 0;
		NVM_Transaction_Flash_RD* read_slot = get_read_slot(chip->ChannelID, chip->ChipID);
		NVM_Transaction_Flash_WR* write_slot = get_write_slot(chip->ChannelID, chip->ChipID);
		if (!GCReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			unsigned int total_num_writes = 0;
			for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
			{
				total_num_writes += serviced_writes_since_last_GC[chip->ChannelID][chip->ChipID][stream_id];
			}
			if (total_num_writes != 0)
				GCM = (int)((double)GCReadTRQueue[chip->ChannelID][chip->ChipID].size()
					* (double)serviced_writes_since_last_GC[chip->ChannelID][chip->ChipID][write_slot->Stream_id]
					/ total_num_writes);
		}
		double pw_read = 0, pw_write = 0;
		estimate_proportional_wait(read_slot, write_slot, pw_read, pw_write, GCM, chip->ChannelID, chip->ChipID);
		if (pw_read >= 0 && pw_read >= pw_write)
		{
			transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].push_back(
				{ Transaction_Type::READ, Transaction_Source_Type::USERIO });
		}
		else if (pw_write >= 0 && pw_write > pw_read)
		{
			if (ftl->BlockManager->Get_plane_bookkeeping_entry(write_slot->Address)->Free_pages_count < GC_FLIN)
			{
				while (GCM > 0)
				{
					transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].push_back(
						{ Transaction_Type::READ, Transaction_Source_Type::GC_WL });
					transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].push_back(
						{ Transaction_Type::WRITE, Transaction_Source_Type::GC_WL });
					GCM--;
				}
			}
			transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].push_back(
				{ Transaction_Type::WRITE, Transaction_Source_Type::USERIO });
			transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].push_back(
				{ Transaction_Type::ERASE, Transaction_Source_Type::GC_WL });
		}
		bool success = false;
		while (!transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].empty() && !success)
		{
			switch (transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].front().first)
			{
			case Transaction_Type::READ:
				success = service_read_transaction(chip);
				break;
			case Transaction_Type::WRITE:
				success = service_write_transaction(chip);
				break;
			case Transaction_Type::ERASE:
				success = service_erase_transaction(chip);
				break;
			default:
				break;
			}
			transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].pop_front();
		}
	}

	void TSU_SpeedLimit::estimate_proportional_wait(NVM_Transaction_Flash_RD* read_slot,
		NVM_Transaction_Flash_WR* write_slot, double& pw_read, double& pw_write, int GCM, flash_channel_ID_type channel_id,
		flash_chip_ID_type chip_id)
	{
		sim_time_type read_cost = 0;
		sim_time_type write_cost = 0;
		sim_time_type T_erase_memory = 0;
		pw_read = -1;
		pw_write = -1;
		if (read_slot)
		{
			read_cost = _NVMController->Expected_transfer_time(read_slot) + _NVMController->Expected_command_time(read_slot);
		}
		if (write_slot)
		{
			write_cost = _NVMController->Expected_transfer_time(write_slot) + _NVMController->Expected_command_time(write_slot);
		}
		if (!GCEraseTRQueue[channel_id][chip_id].empty())
		{
			T_erase_memory = _NVMController->Expected_command_time(GCEraseTRQueue[channel_id][chip_id].front());
		}
		sim_time_type T_GC = GCM == 0 ? 0 : GCM * (read_cost + write_cost) + T_erase_memory;
		if (read_cost != 0)
		{
			pw_read = (double)(Simulator->Time() - read_slot->Issue_time + write_cost + T_GC) / read_cost;
		}
		if (write_cost != 0)
		{
			pw_write = (double)(Simulator->Time() - write_slot->Issue_time + read_cost + T_GC) / write_cost;
		}
	}

	NVM_Transaction_Flash_RD* TSU_SpeedLimit::get_read_slot(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		NVM_Transaction_Flash* slot = NULL;
		if (MappingReadTRQueue[channel_id][chip_id].empty())
		{
			if (!UserReadTRQueue[channel_id][chip_id].empty())
			{
				slot = UserReadTRQueue[channel_id][chip_id].front();
			}
		}
		else
		{
			slot = MappingReadTRQueue[channel_id][chip_id].front();
		}
		return (NVM_Transaction_Flash_RD*)slot;
	}

	NVM_Transaction_Flash_WR* TSU_SpeedLimit::get_write_slot(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		NVM_Transaction_Flash* slot = NULL;
		if (MappingWriteTRQueue[channel_id][chip_id].empty())
		{
			slot = UserWriteTRQueue[channel_id][chip_id].empty() ? NULL : UserWriteTRQueue[channel_id][chip_id].front();
		}
		/*else
		{
			slot = MappingWriteTRQueue[channel_id][chip_id].front();
		}*/
		return (NVM_Transaction_Flash_WR*)slot;
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
}