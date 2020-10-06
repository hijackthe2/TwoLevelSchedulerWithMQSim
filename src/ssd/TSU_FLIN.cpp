#include "TSU_FLIN.h"
#include "NVM_PHY_ONFI_NVDDR2.h"
#include <stack>
#include <cmath>

namespace SSD_Components
{
	TSU_FLIN::TSU_FLIN(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController,
		const unsigned int channel_count, const unsigned int chip_no_per_channel, const unsigned int die_no_per_chip, const unsigned int plane_no_per_die, unsigned int flash_page_size,
		const sim_time_type flow_classification_epoch, const unsigned int alpha_read, const unsigned int alpha_write,
		const unsigned int no_of_priority_classes, const stream_id_type max_flow_id,
		const unsigned int* stream_count_per_priority_class, stream_id_type** stream_ids_per_priority_class,
		const double f_thr, const unsigned int GC_FLIN,
		const sim_time_type WriteReasonableSuspensionTimeForRead,
		const sim_time_type EraseReasonableSuspensionTimeForRead,
		const sim_time_type EraseReasonableSuspensionTimeForWrite,
		const bool EraseSuspensionEnabled, const bool ProgramSuspensionEnabled)
		: TSU_Base(id, ftl, NVMController, Flash_Scheduling_Type::FLIN, channel_count, chip_no_per_channel, die_no_per_chip, plane_no_per_die,
			WriteReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForWrite,
			EraseSuspensionEnabled, ProgramSuspensionEnabled),
		flow_classification_epoch(flow_classification_epoch),
		no_of_priority_classes(no_of_priority_classes), F_thr(f_thr), GC_flin(GC_FLIN)
	{
		alpha_read_for_epoch = alpha_read / (channel_count * chip_no_per_channel) / flash_page_size;
		alpha_write_for_epoch = alpha_write / (channel_count * chip_no_per_channel) / flash_page_size,
			this->stream_count_per_priority_class = new unsigned int[no_of_priority_classes];
		this->stream_ids_per_priority_class = new stream_id_type * [no_of_priority_classes];
		for (unsigned int i = 0; i < no_of_priority_classes; i++)
		{
			this->stream_count_per_priority_class[i] = stream_count_per_priority_class[i];
			this->stream_ids_per_priority_class[i] = new stream_id_type[stream_count_per_priority_class[i]];
			for (stream_id_type stream_cntr = 0; stream_cntr < stream_count_per_priority_class[i]; stream_cntr++)
				this->stream_ids_per_priority_class[i][stream_cntr] = stream_ids_per_priority_class[i][stream_cntr];
		}
		UserReadTRQueue = new Flash_Transaction_Queue * *[channel_count];
		Read_slot = new NVM_Transaction_Flash_RD * **[channel_count];
		UserWriteTRQueue = new Flash_Transaction_Queue * *[channel_count];
		Write_slot = new NVM_Transaction_Flash_WR * **[channel_count];
		GCReadTRQueue = new Flash_Transaction_Queue * [channel_count];
		GCWriteTRQueue = new Flash_Transaction_Queue * [channel_count];
		GCEraseTRQueue = new Flash_Transaction_Queue * [channel_count];
		MappingReadTRQueue = new Flash_Transaction_Queue * [channel_count];
		MappingWriteTRQueue = new Flash_Transaction_Queue * [channel_count];
		flow_activity_info = new FLIN_Flow_Monitoring_Unit * **[channel_count];
		low_intensity_class_read = new std::set<stream_id_type> * *[channel_count];
		low_intensity_class_write = new std::set<stream_id_type> * *[channel_count];
		head_high_read = new std::list<NVM_Transaction_Flash*>::iterator * *[channel_count];
		head_high_write = new std::list<NVM_Transaction_Flash*>::iterator * *[channel_count];
		transaction_waiting_dispatch_slots = new std::list<std::pair<Transaction_Type, Transaction_Source_Type>> * [channel_count];
		current_turn_read = new int* [channel_count];
		current_turn_write = new int* [channel_count];
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			UserReadTRQueue[channel_id] = new Flash_Transaction_Queue * [chip_no_per_channel];
			Read_slot[channel_id] = new NVM_Transaction_Flash_RD * *[chip_no_per_channel];
			UserWriteTRQueue[channel_id] = new Flash_Transaction_Queue * [chip_no_per_channel];
			Write_slot[channel_id] = new NVM_Transaction_Flash_WR * *[chip_no_per_channel];
			GCReadTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCWriteTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCEraseTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingReadTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingWriteTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			flow_activity_info[channel_id] = new FLIN_Flow_Monitoring_Unit * *[chip_no_per_channel];
			low_intensity_class_read[channel_id] = new std::set<stream_id_type> * [chip_no_per_channel];
			low_intensity_class_write[channel_id] = new std::set<stream_id_type> * [chip_no_per_channel];
			head_high_read[channel_id] = new std::list<NVM_Transaction_Flash*>::iterator * [chip_no_per_channel];
			head_high_write[channel_id] = new std::list<NVM_Transaction_Flash*>::iterator * [chip_no_per_channel];
			transaction_waiting_dispatch_slots[channel_id] = new std::list<std::pair<Transaction_Type, Transaction_Source_Type>>[chip_no_per_channel];
			current_turn_read[channel_id] = new int[chip_no_per_channel];
			current_turn_write[channel_id] = new int[chip_no_per_channel];

			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				UserReadTRQueue[channel_id][chip_id] = new Flash_Transaction_Queue[no_of_priority_classes];
				Read_slot[channel_id][chip_id] = new NVM_Transaction_Flash_RD * [no_of_priority_classes];
				UserWriteTRQueue[channel_id][chip_id] = new Flash_Transaction_Queue[no_of_priority_classes];
				Write_slot[channel_id][chip_id] = new NVM_Transaction_Flash_WR * [no_of_priority_classes];
				flow_activity_info[channel_id][chip_id] = new FLIN_Flow_Monitoring_Unit * [no_of_priority_classes];
				low_intensity_class_read[channel_id][chip_id] = new std::set<stream_id_type>[no_of_priority_classes];
				low_intensity_class_write[channel_id][chip_id] = new std::set<stream_id_type>[no_of_priority_classes];
				head_high_read[channel_id][chip_id] = new std::list<NVM_Transaction_Flash*>::iterator[no_of_priority_classes];
				head_high_write[channel_id][chip_id] = new std::list<NVM_Transaction_Flash*>::iterator[no_of_priority_classes];

				GCReadTRQueue[channel_id][chip_id].Set_id("GC_Read_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id) + "@");
				MappingReadTRQueue[channel_id][chip_id].Set_id("Mapping_Read_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id));
				MappingWriteTRQueue[channel_id][chip_id].Set_id("Mapping_Write_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id));
				GCWriteTRQueue[channel_id][chip_id].Set_id("GC_Write_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id));
				GCEraseTRQueue[channel_id][chip_id].Set_id("GC_Erase_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id));
				for (unsigned int pclass_id = 0; pclass_id < no_of_priority_classes; pclass_id++)
				{
					UserReadTRQueue[channel_id][chip_id][pclass_id].Set_id("User_Read_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id) + "@" + std::to_string(pclass_id));
					UserWriteTRQueue[channel_id][chip_id][pclass_id].Set_id("User_Write_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id) + "@" + std::to_string(pclass_id));
					head_high_read[channel_id][chip_id][pclass_id] = UserReadTRQueue[channel_id][chip_id][pclass_id].end();
					head_high_write[channel_id][chip_id][pclass_id] = UserWriteTRQueue[channel_id][chip_id][pclass_id].end();
					flow_activity_info[channel_id][chip_id][pclass_id] = new FLIN_Flow_Monitoring_Unit[max_flow_id];
					for (int stream_cntr = 0; stream_cntr < max_flow_id; stream_cntr++)
					{
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_read_requests_history = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_read_requests_recent = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_write_requests_history = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_write_requests_recent = 0;

						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_read_requests_total = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_write_requests_total = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Sum_read_slowdown = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Sum_write_slowdown = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].No_of_serviced_writes_since_last_GC = 0;

						low_intensity_class_read[channel_id][chip_id][pclass_id].insert(stream_cntr);
						low_intensity_class_write[channel_id][chip_id][pclass_id].insert(stream_cntr);
					}
				}
			}
		}

		initialize_scheduling_turns();
	}

	TSU_FLIN::~TSU_FLIN()
	{
		delete[] this->stream_count_per_priority_class;
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				delete[] UserReadTRQueue[channel_id][chip_id];
				delete[] Read_slot[channel_id][chip_id];
				delete[] UserWriteTRQueue[channel_id][chip_id];
				delete[] Write_slot[channel_id][chip_id];
				delete[] low_intensity_class_read[channel_id][chip_id];
				delete[] low_intensity_class_write[channel_id][chip_id];
				delete[] head_high_read[channel_id][chip_id];
				delete[] head_high_write[channel_id][chip_id];
			}
			delete[] UserReadTRQueue[channel_id];
			delete[] Read_slot[channel_id];
			delete[] UserWriteTRQueue[channel_id];
			delete[] Write_slot[channel_id];
			delete[] GCReadTRQueue[channel_id];
			delete[] GCWriteTRQueue[channel_id];
			delete[] GCEraseTRQueue[channel_id];
			delete[] MappingReadTRQueue[channel_id];
			delete[] MappingWriteTRQueue[channel_id];
			delete[] low_intensity_class_read[channel_id];
			delete[] low_intensity_class_write[channel_id];
			delete[] head_high_read[channel_id];
			delete[] head_high_write[channel_id];
			delete[] current_turn_read[channel_id];
			delete[] current_turn_write[channel_id];
			delete[] transaction_waiting_dispatch_slots[channel_id];
		}
		delete[] UserReadTRQueue;
		delete[] Read_slot;
		delete[] UserWriteTRQueue;
		delete[] Write_slot;
		delete[] GCReadTRQueue;
		delete[] GCWriteTRQueue;
		delete[] GCEraseTRQueue;
		delete[] MappingReadTRQueue;
		delete[] MappingWriteTRQueue;
		delete[] low_intensity_class_read;
		delete[] low_intensity_class_write;
		delete[] head_high_read;
		delete[] head_high_write;
		delete[] current_turn_read;
		delete[] current_turn_write;
		delete[] transaction_waiting_dispatch_slots;
	}

	void TSU_FLIN::Start_simulation()
	{
		Simulator->Register_sim_event(flow_classification_epoch, this, 0, 0);
	}

	void TSU_FLIN::Validate_simulation_config() {}

	void TSU_FLIN::Execute_simulator_event(MQSimEngine::Sim_Event* event)
	{
		//Flow classification as described in Section 5.1 of FLIN paper in ISCA 2018
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				for (unsigned int pclass_id = 0; pclass_id < no_of_priority_classes; pclass_id++)
				{
					if (stream_count_per_priority_class[pclass_id] < 2)
						continue;

					low_intensity_class_read[channel_id][chip_id][pclass_id].clear();
					low_intensity_class_write[channel_id][chip_id][pclass_id].clear();
					for (unsigned int stream_cntr = 0; stream_cntr < stream_count_per_priority_class[pclass_id]; stream_cntr++)
					{
						if (flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_read_requests_recent < alpha_read_for_epoch)
							low_intensity_class_read[channel_id][chip_id][pclass_id].insert(stream_ids_per_priority_class[pclass_id][stream_cntr]);
						if (flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_write_requests_recent < alpha_write_for_epoch)
							low_intensity_class_write[channel_id][chip_id][pclass_id].insert(stream_ids_per_priority_class[pclass_id][stream_cntr]);
						flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_read_requests_recent = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_write_requests_recent = 0;
					}
				}
			}
		}
		Simulator->Register_sim_event(Simulator->Time() + flow_classification_epoch, this, 0, 0);
	}

	inline void TSU_FLIN::Prepare_for_transaction_submit()
	{
		opened_scheduling_reqs++;
		if (opened_scheduling_reqs > 1)
			return;
		transaction_receive_slots.clear();
	}

	inline void TSU_FLIN::Submit_transaction(NVM_Transaction_Flash* transaction)
	{
		transaction_receive_slots.push_back(transaction);
	}

	void TSU_FLIN::Schedule()
	{
		opened_scheduling_reqs--;
		if (opened_scheduling_reqs > 0)
			return;
		if (opened_scheduling_reqs < 0)
			PRINT_ERROR("TSU Schedule function is invoked in an incorrect way!");

		if (transaction_receive_slots.size() == 0)
			return;


		for (std::list<NVM_Transaction_Flash*>::iterator it = transaction_receive_slots.begin();
			it != transaction_receive_slots.end(); it++)
		{
			flash_channel_ID_type channel_id = (*it)->Address.ChannelID;
			flash_chip_ID_type chip_id = (*it)->Address.ChipID;
			unsigned int priority_class = 1;
			if ((*it)->Source == Transaction_Source_Type::CACHE || (*it)->Source == Transaction_Source_Type::USERIO)
			{
				priority_class = ((int)(*it)->UserIORequest->Priority_class) - 1;
				if (priority_class >= no_of_priority_classes)
				{
					priority_class = 1;
				}
			}
			stream_id_type stream_id = (*it)->Stream_id;
			switch ((*it)->Type)
			{
			case Transaction_Type::READ:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					if (stream_count_per_priority_class[priority_class] < 2)
					{
						UserReadTRQueue[channel_id][chip_id][priority_class].push_back(*it);
						break;
					}
					flow_activity_info[channel_id][chip_id][priority_class][stream_id].Serviced_read_requests_recent++;
					if (low_intensity_class_read[channel_id][chip_id][priority_class].find(stream_id) == low_intensity_class_read[channel_id][chip_id][priority_class].end())
					{
						UserReadTRQueue[channel_id][chip_id][priority_class].push_back(*it);//Insert(TRnew after Q.Tail)
						auto tail = UserReadTRQueue[channel_id][chip_id][priority_class].end();
						tail--;
						auto copy_tail = tail;
						if (head_high_read[channel_id][chip_id][priority_class] == UserReadTRQueue[channel_id][chip_id][priority_class].end())
						{
							head_high_read[channel_id][chip_id][priority_class] = tail;
						}
						estimate_alone_waiting_time(&UserReadTRQueue[channel_id][chip_id][priority_class], tail);//EstimateAloneWaitingTime(Q, TRnew)
						stream_id_type flow_with_max_average_slowdown;
						double f = fairness_based_on_average_slowdown(&UserReadTRQueue[channel_id][chip_id][priority_class],
							head_high_read[channel_id][chip_id][priority_class],
							stream_ids_per_priority_class[priority_class], stream_count_per_priority_class[priority_class],
							flow_with_max_average_slowdown);//FairnessBasedOnAverageSlowdown(Q)
						if (f < F_thr && stream_id == flow_with_max_average_slowdown)
							move_forward(&UserReadTRQueue[channel_id][chip_id][priority_class],
								tail,
								head_high_read[channel_id][chip_id][priority_class]);//MoveForward(from Q.Tail up to Q.Taillow + 1)
						else reorder_for_fairness(&UserReadTRQueue[channel_id][chip_id][priority_class],
							head_high_read[channel_id][chip_id][priority_class], tail);//ReorderForFairness(from Q.Tail to Q.Taillow + 1)
					}
					else
					{
						UserReadTRQueue[channel_id][chip_id][priority_class].insert(head_high_read[channel_id][chip_id][priority_class], *it);//Insert(TRnew after Q.Taillow)
						auto tail_low = head_high_read[channel_id][chip_id][priority_class];
						if (tail_low != UserReadTRQueue[channel_id][chip_id][priority_class].begin())
							tail_low--;
						estimate_alone_waiting_time(&UserReadTRQueue[channel_id][chip_id][priority_class], tail_low);//EstimateAloneWaitingTime(Q, TRnew)
						reorder_for_fairness(&UserReadTRQueue[channel_id][chip_id][priority_class],
							UserReadTRQueue[channel_id][chip_id][priority_class].begin(), tail_low);//ReorderForFairness(from Q.Taillow to Q.Head)
					}
					break;
				case Transaction_Source_Type::MAPPING:
					MappingReadTRQueue[channel_id][chip_id].push_back(*it);
					break;
				case Transaction_Source_Type::GC_WL:
					GCReadTRQueue[channel_id][chip_id].push_back(*it);
					break;
				default:
					PRINT_ERROR("TSU_FLIN: Unhandled source type four read transaction!")
				}
				break;
			case Transaction_Type::WRITE:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					if (stream_count_per_priority_class[priority_class] < 2)
					{
						UserWriteTRQueue[channel_id][chip_id][priority_class].push_back(*it);
						break;
					}
					flow_activity_info[channel_id][chip_id][priority_class][stream_id].Serviced_write_requests_recent++;
					if (low_intensity_class_write[channel_id][chip_id][priority_class].find(stream_id) == low_intensity_class_write[channel_id][chip_id][priority_class].end())
					{
						UserWriteTRQueue[channel_id][chip_id][priority_class].push_back(*it);//Insert(TRnew after Q.Tail)
						auto tail = UserWriteTRQueue[channel_id][chip_id][priority_class].end();
						tail--;
						if (head_high_write[channel_id][chip_id][priority_class] == UserWriteTRQueue[channel_id][chip_id][priority_class].end())
						{
							head_high_write[channel_id][chip_id][priority_class] = tail;
						}
						estimate_alone_waiting_time(&UserWriteTRQueue[channel_id][chip_id][priority_class], tail);//EstimateAloneWaitingTime(Q, TRnew)
						stream_id_type flow_with_max_average_slowdown;
						double f = fairness_based_on_average_slowdown(&UserWriteTRQueue[channel_id][chip_id][priority_class],
							head_high_write[channel_id][chip_id][priority_class],
							stream_ids_per_priority_class[priority_class],
							stream_count_per_priority_class[priority_class], flow_with_max_average_slowdown);//FairnessBasedOnAverageSlowdown(Q)
						if (f < F_thr && stream_id == flow_with_max_average_slowdown)
							move_forward(&UserWriteTRQueue[channel_id][chip_id][priority_class],
								tail,
								head_high_write[channel_id][chip_id][priority_class]);//MoveForward(from Q.Tail up to Q.Taillow + 1)
						else reorder_for_fairness(&UserWriteTRQueue[channel_id][chip_id][priority_class],
							head_high_write[channel_id][chip_id][priority_class], tail);//ReorderForFairness(from Q.Tail to Q.Taillow + 1)
					}
					else
					{
						UserWriteTRQueue[channel_id][chip_id][priority_class].insert(head_high_write[channel_id][chip_id][priority_class], *it);
						auto tail_low = head_high_write[channel_id][chip_id][priority_class];
						if (tail_low != UserWriteTRQueue[channel_id][chip_id][priority_class].begin())
							tail_low--;
						estimate_alone_waiting_time(&UserWriteTRQueue[channel_id][chip_id][priority_class], tail_low);
						reorder_for_fairness(&UserWriteTRQueue[channel_id][chip_id][priority_class],
							UserWriteTRQueue[channel_id][chip_id][priority_class].begin(), tail_low);
					}
					break;
				case Transaction_Source_Type::MAPPING:
					MappingWriteTRQueue[channel_id][chip_id].push_back(*it);
					break;
				case Transaction_Source_Type::GC_WL:
					GCWriteTRQueue[channel_id][chip_id].push_back(*it);
					break;
				default:
					PRINT_ERROR("TSU_FLIN: Unhandled source type four write transaction!")
				}
				break;
			case Transaction_Type::ERASE:
				GCEraseTRQueue[channel_id][chip_id].push_back(*it);
				break;
			default:
				break;
			}
		}


		for (flash_channel_ID_type channel_id = 0; channel_id < channel_count; channel_id++)
		{
			if (_NVMController->Get_channel_status(channel_id) == BusChannelStatus::IDLE)
			{
				for (unsigned int i = 0; i < chip_no_per_channel; i++) {
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

	void TSU_FLIN::reorder_for_fairness(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator start, std::list<NVM_Transaction_Flash*>::iterator end)
	{
		if (start == end)
		{
			return;
		}
		std::list<NVM_Transaction_Flash*>::iterator itr = queue->begin();
		sim_time_type time_to_finish = 0;
		if (_NVMController->Is_chip_busy(*itr))
			if (_NVMController->Expected_finish_time(*itr) > Simulator->Time())
				time_to_finish = _NVMController->Expected_finish_time(*itr) - Simulator->Time();//T^chip_busy

		while (itr != start)
		{
			time_to_finish += _NVMController->Expected_transfer_time(*itr) + _NVMController->Expected_command_time(*itr);
			itr++;
		}

		//First pass: from start to end to estimate the slowdown of each transaction in its current position
		std::stack<double> min_slowdown_list, max_slowdown_list;
		double slowdown_min = 10000000000000000000, slowdown_max = 0;
		while (itr != std::next(end))
		{
			time_to_finish += _NVMController->Expected_transfer_time(*itr) + _NVMController->Expected_command_time(*itr);
			sim_time_type T_TR_shared = time_to_finish + (Simulator->Time() - (*itr)->Issue_time);
			double slowdown = (double)T_TR_shared / (double)((*itr)->Estimated_alone_waiting_time
				+ _NVMController->Expected_transfer_time(*itr) + _NVMController->Expected_command_time(*itr));
			if (slowdown < slowdown_min)
				slowdown_min = slowdown;
			if (slowdown > slowdown_max)
				slowdown_max = slowdown;
			min_slowdown_list.push(slowdown_min);
			max_slowdown_list.push(slowdown_max);
			itr++;
		}
		double fairness_max = slowdown_min / slowdown_max;

		//Second pass: from end to start to find a position for TR_new sitting at end, that maximizes fairness
		sim_time_type T_new_alone = (*end)->Estimated_alone_waiting_time
			+ _NVMController->Expected_transfer_time(*end) + _NVMController->Expected_command_time(*end);
		sim_time_type T_new_shared_before = time_to_finish;

		auto final_position = end;
		auto traverser = std::prev(end);
		time_to_finish -= _NVMController->Expected_transfer_time(*end) + _NVMController->Expected_command_time(*end);

		while (traverser != std::prev(start) && (*traverser)->Stream_id != (*end)->Stream_id)
		{
			sim_time_type T_pos_alone = (*traverser)->Estimated_alone_waiting_time
				+ _NVMController->Expected_transfer_time(*traverser) + _NVMController->Expected_command_time(*traverser);

			sim_time_type T_pos_shared_after = time_to_finish + _NVMController->Expected_transfer_time(*end) + _NVMController->Expected_command_time(*end);
			double slowdown_pos_after = (double)T_pos_shared_after / T_pos_alone;

			sim_time_type T_new_shared_after = time_to_finish - _NVMController->Expected_transfer_time(*traverser) + _NVMController->Expected_command_time(*traverser);
			double slowdown_new_after = (double)T_new_shared_after / T_new_alone;

			double slowdown_min = min_slowdown_list.top();
			min_slowdown_list.pop();
			double slowdown_max = max_slowdown_list.top();
			max_slowdown_list.pop();
			if (slowdown_pos_after > slowdown_max)
				slowdown_max = slowdown_pos_after;
			if (slowdown_pos_after < slowdown_min)
				slowdown_min = slowdown_pos_after;

			if (slowdown_new_after > slowdown_max)
				slowdown_max = slowdown_new_after;
			if (slowdown_new_after < slowdown_min)
				slowdown_min = slowdown_new_after;

			double fairness_after = (double)slowdown_min / slowdown_max;
			if (fairness_after > fairness_max)
			{
				fairness_max = fairness_after;
				final_position = traverser;
			}

			time_to_finish -= _NVMController->Expected_transfer_time(*traverser) + _NVMController->Expected_command_time(*traverser);
			traverser--;
		}

		if (final_position != end)
		{
			NVM_Transaction_Flash* tr = *end;
			queue->remove(end);
			queue->insert(final_position, tr);
		}
	}

	void TSU_FLIN::estimate_alone_waiting_time(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator position)
	{
		sim_time_type chip_busy_time = 0, expected_last_time = 0;
		NVM_Transaction_Flash* chip_tr = _NVMController->Is_chip_busy_with_stream(*position);
		if (chip_tr && _NVMController->Expected_finish_time(chip_tr) > Simulator->Time())
		{
			chip_busy_time = _NVMController->Expected_finish_time(chip_tr) - Simulator->Time();
		}
		if (position != queue->begin())
		{
			auto itr = position;
			--itr;
			while (itr != queue->begin() && (*itr)->Stream_id != (*position)->Stream_id)
			{
				--itr;
			}
			if ((*itr)->Stream_id == (*position)->Stream_id)
			{
				expected_last_time = (*itr)->Estimated_alone_waiting_time
					+ _NVMController->Expected_transfer_time(*position) + _NVMController->Expected_command_time(*position);
				if (expected_last_time + (*itr)->Issue_time > Simulator->Time())
				{
					expected_last_time = expected_last_time + (*itr)->Issue_time - Simulator->Time();
				}
			}
		}
		(*position)->Estimated_alone_waiting_time = chip_busy_time + expected_last_time;
	}

	double TSU_FLIN::fairness_based_on_average_slowdown(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator start,
		stream_id_type* priority_stream_id, unsigned int priority_stream_count, stream_id_type& flow_with_max_average_slowdown)
	{
		std::unordered_map<stream_id_type, double> sum_slowdown;
		std::unordered_map<stream_id_type, unsigned int> transaction_count;
		for (unsigned int i = 0; i < priority_stream_count; i++)
		{
			sum_slowdown[priority_stream_id[i]] = 0;
			transaction_count[priority_stream_id[i]] = 0;
		}
		sim_time_type total_finish_time = 0;
		auto itr = queue->begin();
		while (itr != start)
		{
			total_finish_time += _NVMController->Expected_transfer_time(*itr) + _NVMController->Expected_command_time(*itr);
			++itr;
		}
		while (itr != queue->end())
		{
			total_finish_time += _NVMController->Expected_transfer_time(*itr) + _NVMController->Expected_command_time(*itr);
			sim_time_type transaction_alone_time = (*itr)->Estimated_alone_waiting_time
				+ _NVMController->Expected_transfer_time(*itr) + _NVMController->Expected_command_time(*itr);
			sim_time_type transaction_shared_time = total_finish_time + (Simulator->Time() - (*itr)->Issue_time);
			sum_slowdown[(*itr)->Stream_id] += (double)transaction_shared_time / transaction_alone_time;
			++transaction_count[(*itr)->Stream_id];
			++itr;
		}

		double slowdown_max = 0, slowdown_min = 10000000000000000000.0;
		int stream_count = 0;
		for (unsigned int i = 0; i < priority_stream_count; i++)
		{
			if (transaction_count[i] == 0)
				continue;
			stream_count++;
			double average_slowdown = sum_slowdown[i] / transaction_count[i];
			if (average_slowdown > slowdown_max)
			{
				slowdown_max = average_slowdown;
				flow_with_max_average_slowdown = priority_stream_id[i];
			}
			if (average_slowdown < slowdown_min)
				slowdown_min = average_slowdown;
		}
		if (stream_count == 1)
		{
			flow_with_max_average_slowdown = -1;
		}
		return (double)slowdown_min / slowdown_max;
	}

	void TSU_FLIN::move_forward(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator TRnew_pos, std::list<NVM_Transaction_Flash*>::iterator ultimate_posistion)
	{
		if (TRnew_pos == ultimate_posistion)
			return;
		auto Tnew_final_pos = TRnew_pos;
		Tnew_final_pos--;
		while ((*Tnew_final_pos)->Stream_id != (*TRnew_pos)->Stream_id && !(*Tnew_final_pos)->FLIN_Barrier && Tnew_final_pos != ultimate_posistion)
			Tnew_final_pos--;

		if (Tnew_final_pos == ultimate_posistion
			&& (*Tnew_final_pos)->Stream_id != (*ultimate_posistion)->Stream_id
			&& !(*TRnew_pos)->FLIN_Barrier)
		{
			NVM_Transaction_Flash* tr = *TRnew_pos;
			queue->remove(TRnew_pos);
			queue->insert(Tnew_final_pos, tr);
			tr->FLIN_Barrier = true;//According to FLIN: When TRnew is moved forward, it is tagged so that no future arriving flash transaction of the high-intensity flows jumps ahead of it
		}
		else
		{
			NVM_Transaction_Flash* tr = *TRnew_pos;
			queue->remove(TRnew_pos);
			Tnew_final_pos--;
			queue->insert(Tnew_final_pos, tr);
			tr->FLIN_Barrier = true;//According to FLIN: When TRnew is moved forward, it is tagged so that no future arriving flash transaction of the high-intensity flows jumps ahead of it
		}
	}

	void TSU_FLIN::initialize_scheduling_turns()
	{
		unsigned int total_turns = (int)std::pow(2, no_of_priority_classes) - 1;
		for (unsigned int i = 0; i < total_turns; i++)
		{
			scheduling_turn_assignments_read.push_back(i);
			scheduling_turn_assignments_write.push_back(i);
		}

		int priority_class = 0;

		while (priority_class <= int(no_of_priority_classes))
		{
			unsigned int step = (unsigned int)std::pow(2, priority_class + 1);
			for (unsigned int i = (unsigned int)std::pow(2, priority_class) - 1; i < total_turns; i += step)
			{
				scheduling_turn_assignments_read[i] = priority_class;
				scheduling_turn_assignments_write[i] = priority_class;
			}
			priority_class++;
		}
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				current_turn_read[channel_id][chip_id] = 0;
				current_turn_write[channel_id][chip_id] = 0;
			}
		}
	}

	bool TSU_FLIN::service_read_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		int read_turn = current_turn_read[chip->ChannelID][chip->ChipID];
		unsigned int read_pclass = scheduling_turn_assignments_read[read_turn];
		auto info = transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].front();
		std::list<Flash_Transaction_Queue*> queue_list;
		if (!MappingReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_back(&MappingReadTRQueue[chip->ChannelID][chip->ChipID]);
		}
		if (!UserReadTRQueue[chip->ChannelID][chip->ChipID][read_pclass].empty())
		{
			queue_list.push_back(&UserReadTRQueue[chip->ChannelID][chip->ChipID][read_pclass]);
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
		flash_die_ID_type die_id = queue_list.front()->front()->Address.DieID;
		bool dispathed_user_transaction = false;
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
								|| (*it)->Source == Transaction_Source_Type::USERIO)
							{
								if (it == head_high_read[chip->ChannelID][chip->ChipID][read_pclass])
								{
									++head_high_read[chip->ChannelID][chip->ChipID][read_pclass];
								}
								dispathed_user_transaction = true;
							}
							(*it)->SuspendRequired = suspensionRequired;
							plane_vector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							(*queue)->remove(it++);
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
		if (dispathed_user_transaction)
		{
			current_turn_read[chip->ChannelID][chip->ChipID]
				= (current_turn_read[chip->ChannelID][chip->ChipID] + 1) % scheduling_turn_assignments_read.size();
		}
		return true;
	}

	bool TSU_FLIN::service_write_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		int write_turn = current_turn_write[chip->ChannelID][chip->ChipID];
		unsigned int write_pclass = scheduling_turn_assignments_write[write_turn];
		auto info = transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].front();
		std::list<Flash_Transaction_Queue*> queue_list;
		/*if (!MappingWriteTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			queue_list.push_back(&MappingWriteTRQueue[chip->ChannelID][chip->ChipID]);
		}*/
		if (!UserWriteTRQueue[chip->ChannelID][chip->ChipID][write_pclass].empty())
		{
			queue_list.push_back(&UserWriteTRQueue[chip->ChannelID][chip->ChipID][write_pclass]);
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
		flash_die_ID_type die_id = queue_list.front()->front()->Address.DieID;
		bool dispathed_user_transaction = false;
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
								|| (*it)->Source == Transaction_Source_Type::USERIO)
							{
								flow_activity_info[chip->ChannelID][chip->ChipID][write_pclass][(*it)->Stream_id].No_of_serviced_writes_since_last_GC++;
								if (it == head_high_write[chip->ChannelID][chip->ChipID][write_pclass])
								{
									++head_high_write[chip->ChannelID][chip->ChipID][write_pclass];
								}
								dispathed_user_transaction = true;
							}
							(*it)->SuspendRequired = suspensionRequired;
							plane_vector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							(*queue)->remove(it++);
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
		if (dispathed_user_transaction)
		{
			current_turn_write[chip->ChannelID][chip->ChipID]
				= (current_turn_write[chip->ChannelID][chip->ChipID] + 1) % scheduling_turn_assignments_write.size();
		}
		return true;
	}

	bool TSU_FLIN::service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip)
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
			flash_block_ID_type block_id;
			block_id = source_queue->front()->Address.BlockID;

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
						plane_vector |= 1 << (*it)->Address.PlaneID;
						transaction_dispatch_slots.push_back(*it);
						source_queue->remove(it++);
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
		int write_turn = (current_turn_write[chip->ChannelID][chip->ChipID] - 1) % scheduling_turn_assignments_write.size();
		unsigned int write_pclass = scheduling_turn_assignments_write[write_turn];
		for (unsigned int i = 0; i < stream_count_per_priority_class[write_pclass]; ++i)
		{
			stream_id_type stream_id = stream_ids_per_priority_class[write_pclass][i];
			flow_activity_info[chip->ChannelID][chip->ChipID][write_pclass][stream_id].No_of_serviced_writes_since_last_GC = 0;
		}
		return true;
	}

	void TSU_FLIN::service_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		/*if (!service_read_transaction(chip))
			if (!service_write_transaction(chip))
				service_erase_transaction(chip);
		return;*/
		// the number of valid pages for flow f in SSD cannot be calculated in MQSim
		unsigned int GCM = 0;
		NVM_Transaction_Flash_RD* read_slot = get_read_slot(chip->ChannelID, chip->ChipID);
		NVM_Transaction_Flash_WR* write_slot = get_write_slot(chip->ChannelID, chip->ChipID);
		int read_turn = current_turn_read[chip->ChannelID][chip->ChipID];
		int write_turn = current_turn_write[chip->ChannelID][chip->ChipID];
		unsigned int read_pclass = scheduling_turn_assignments_read[read_turn];
		unsigned int write_pclass = scheduling_turn_assignments_write[write_turn];
		if (!GCReadTRQueue[chip->ChannelID][chip->ChipID].empty())
		{
			unsigned int total_num_writes = 0;
			for (unsigned int i = 0; i < stream_count_per_priority_class[write_pclass]; ++i)
			{
				stream_id_type stream_id = stream_ids_per_priority_class[write_pclass][i];
				total_num_writes += flow_activity_info[chip->ChannelID][chip->ChipID][write_pclass][stream_id].No_of_serviced_writes_since_last_GC;
			}
			if (total_num_writes != 0)
				GCM = (unsigned int)((double)GCReadTRQueue[chip->ChannelID][chip->ChipID].size()
					* (double)flow_activity_info[chip->ChannelID][chip->ChipID][write_pclass][write_slot->Stream_id].No_of_serviced_writes_since_last_GC
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
			if (ftl->BlockManager->Get_plane_bookkeeping_entry(write_slot->Address)->Free_pages_count < GC_flin)
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
		/*if (Simulator->Time() > 10000000000)
		{
			std::cout << Simulator->Time() << "\t" << chip->ChannelID << "\t" << chip->ChipID
				<< "\t" << (int)_NVMController->GetChipStatus(chip) << "\t"
				<< (int)transaction_waiting_dispatch_slots[chip->ChannelID][chip->ChipID].front().first << "\n";
		}*/
	}

	void TSU_FLIN::estimate_proportional_wait(NVM_Transaction_Flash_RD* read_slot, NVM_Transaction_Flash_WR* write_slot,
		double& pw_read, double& pw_write, unsigned int GCM, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		sim_time_type read_cost = 0;
		sim_time_type write_cost = 0;
		sim_time_type T_erase_memory = 0;
		pw_read = -1;
		pw_write = -1;
		if (read_slot)
		{
			read_cost = _NVMController->Expected_transfer_time(read_slot) + _NVMController->Expected_finish_time(read_slot);
		}
		if (write_slot)
		{
			write_cost = _NVMController->Expected_transfer_time(write_slot) + _NVMController->Expected_finish_time(write_slot);
		}
		if (!GCEraseTRQueue[channel_id][chip_id].empty())
		{
			T_erase_memory = _NVMController->Expected_finish_time(GCEraseTRQueue[channel_id][chip_id].front());
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

	NVM_Transaction_Flash_RD* TSU_FLIN::get_read_slot(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		NVM_Transaction_Flash* slot = NULL;
		if (MappingReadTRQueue[channel_id][chip_id].empty())
		{
			int temp_turn = current_turn_read[channel_id][chip_id];
			int cntr = 0;
			while (cntr < scheduling_turn_assignments_read.size())
			{
				if (UserReadTRQueue[channel_id][chip_id][scheduling_turn_assignments_read[temp_turn]].size() > 0)
				{
					slot = UserReadTRQueue[channel_id][chip_id][scheduling_turn_assignments_read[temp_turn]].front();
					current_turn_read[channel_id][chip_id] = temp_turn;
					break;
				}
				temp_turn = (temp_turn + 1) % scheduling_turn_assignments_read.size();
				cntr++;
			}
		}
		else
		{
			slot = MappingReadTRQueue[channel_id][chip_id].front();
		}
		return (NVM_Transaction_Flash_RD*)slot;
	}

	NVM_Transaction_Flash_WR* TSU_FLIN::get_write_slot(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		NVM_Transaction_Flash* slot = NULL;
		if (MappingWriteTRQueue[channel_id][chip_id].empty())
		{
			int temp_turn = current_turn_write[channel_id][chip_id];
			int cntr = 0;
			while (cntr < scheduling_turn_assignments_write.size())
			{
				if (UserWriteTRQueue[channel_id][chip_id][scheduling_turn_assignments_write[temp_turn]].size() > 0)
				{
					slot = UserWriteTRQueue[channel_id][chip_id][scheduling_turn_assignments_write[temp_turn]].front();
					if (((NVM_Transaction_Flash_WR*)slot)->RelatedRead == NULL)
						current_turn_write[channel_id][chip_id] = temp_turn;
					else
						slot = NULL;
					break;
				}
				temp_turn = (temp_turn + 1) % scheduling_turn_assignments_write.size();
				cntr++;
			}
		}
		/*else
		{
			slot = MappingWriteTRQueue[channel_id][chip_id].front();
		}*/
		return (NVM_Transaction_Flash_WR*)slot;
	}

	void TSU_FLIN::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter) {}

}