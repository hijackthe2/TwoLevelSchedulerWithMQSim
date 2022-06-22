#include <string>
#include "TSU_Base.h"

namespace SSD_Components
{
	TSU_Base* TSU_Base::_my_instance = NULL;

	TSU_Base::TSU_Base(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, Flash_Scheduling_Type Type,
		unsigned int ChannelCount, unsigned int chip_no_per_channel, unsigned int DieNoPerChip, unsigned int PlaneNoPerDie,
		unsigned int StreamCount,
		bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled,
		sim_time_type WriteReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForWrite)
		: Sim_Object(id), ftl(ftl), _NVMController(NVMController), type(Type),
		channel_count(ChannelCount), chip_no_per_channel(chip_no_per_channel), die_no_per_chip(DieNoPerChip), plane_no_per_die(PlaneNoPerDie),
		eraseSuspensionEnabled(EraseSuspensionEnabled), programSuspensionEnabled(ProgramSuspensionEnabled),
		writeReasonableSuspensionTimeForRead(WriteReasonableSuspensionTimeForRead), eraseReasonableSuspensionTimeForRead(EraseReasonableSuspensionTimeForRead),
		eraseReasonableSuspensionTimeForWrite(EraseReasonableSuspensionTimeForWrite), opened_scheduling_reqs(0), stream_count(StreamCount),
		write_confict_when_gc(0), total_serviced_write_when_gc(0), read_conflict_when_gc(0), total_serviced_read_when_gc(0), start_gc(false)
	{
		_my_instance = this;
		Round_robin_turn_of_channel = new flash_chip_ID_type[channel_count];
		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			Round_robin_turn_of_channel[channelID] = 0;
		number_of_gc = new unsigned int[stream_count];
		total_count = new unsigned int[stream_count];
		alone_time = new sim_time_type[stream_count];
		shared_time = new sim_time_type[stream_count];
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			number_of_gc[stream_id] = 0;
			total_count[stream_id] = 0;
			alone_time[stream_id] = 0;
			shared_time[stream_id] = 0;
		}
		chip_level_alone_time = new sim_time_type * *[channel_count];
		chip_level_shared_time = new sim_time_type * *[channel_count];
		chip_level_total_serviced_when_gc = new unsigned int** [channel_count];
		chip_level_conflict_when_gc = new unsigned int** [channel_count];
		gap = new Gap * *[channel_count];
		chip_level_serviced_tr_count = new unsigned int* [channel_count];
		chip_level_latency = new sim_time_type * [channel_count];
		global_rra = new NVM::FlashMemory::Physical_Page_Address * [channel_count];
		for (unsigned int channel_id = 0; channel_id < channel_count; ++channel_id)
		{
			chip_level_alone_time[channel_id] = new sim_time_type * [chip_no_per_channel];
			chip_level_shared_time[channel_id] = new sim_time_type * [chip_no_per_channel];
			chip_level_total_serviced_when_gc[channel_id] = new unsigned int* [chip_no_per_channel];
			chip_level_conflict_when_gc[channel_id] = new unsigned int* [chip_no_per_channel];
			gap[channel_id] = new Gap * [chip_no_per_channel];
			chip_level_serviced_tr_count[channel_id] = new unsigned int[chip_no_per_channel];
			chip_level_latency[channel_id] = new sim_time_type[chip_no_per_channel];
			global_rra[channel_id] = new NVM::FlashMemory::Physical_Page_Address[chip_no_per_channel];
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; ++chip_id)
			{
				chip_level_alone_time[channel_id][chip_id] = new sim_time_type[stream_count];
				chip_level_shared_time[channel_id][chip_id] = new sim_time_type[stream_count];
				chip_level_total_serviced_when_gc[channel_id][chip_id] = new unsigned int[stream_count];
				chip_level_conflict_when_gc[channel_id][chip_id] = new unsigned int[stream_count];
				gap[channel_id][chip_id] = new Gap[stream_count];
				chip_level_serviced_tr_count[channel_id][chip_id] = 0;
				chip_level_latency[channel_id][chip_id] = 0;
				global_rra[channel_id][chip_id].DieID = 0;
				global_rra[channel_id][chip_id].PlaneID = 0;
				for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
				{
					chip_level_alone_time[channel_id][chip_id][stream_id] = 0;
					chip_level_shared_time[channel_id][chip_id][stream_id] = 0;
					chip_level_total_serviced_when_gc[channel_id][chip_id][stream_id] = 0;
					chip_level_conflict_when_gc[channel_id][chip_id][stream_id] = 0;
				}
			}
		}
	}

	TSU_Base::~TSU_Base()
	{
		unsigned int WIDTH = 16;
		std::cout.setf(std::ios::left);
		std::cout << "==========\n";
		std::cout << std::setw(WIDTH) << "stream" << std::setw(WIDTH) << "shared"
			<< std::setw(WIDTH) << "alone" << std::setw(WIDTH) << "slowdown"
			<< std::setw(WIDTH) << "#gc" << std::endl;
		for (stream_id_type stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			std::cout << std::setw(WIDTH) << stream_id
				<< std::setw(WIDTH) << shared_time[stream_id] / (total_count[stream_id] + 1e-8) / 1000
				<< std::setw(WIDTH) << alone_time[stream_id] / (total_count[stream_id] + 1e-8) / 1000
				<< std::setw(WIDTH) << (double)shared_time[stream_id] / (alone_time[stream_id] + 1e-10)
				<< std::setw(WIDTH) << number_of_gc[stream_id]
				<< std::endl;
		}
		std::cout << std::string(WIDTH * 5, '-') << std::endl;
		std::cout << std::string(WIDTH * 3, ' ')
			<< std::setw(WIDTH) << fairness()
			<< std::setw(WIDTH) << Stats::Total_gc_executions
			<< std::endl;
		std::cout << "==========\n";
		std::cout << "following data is collected when performing gc read/write/erase\n";
		std::cout << std::setw(WIDTH) << "type"
			<< std::setw(WIDTH) << "#serviced"
			<< std::setw(WIDTH) << "#conflict gc"
			<< std::setw(WIDTH) << "%conflict gc" << std::endl;
		std::cout << std::setw(WIDTH) << "read"
			<< std::setw(WIDTH) << total_serviced_read_when_gc
			<< std::setw(WIDTH) << read_conflict_when_gc
			<< std::setw(WIDTH) << (double)read_conflict_when_gc / (total_serviced_read_when_gc + 1e-10) << std::endl;
		std::cout << std::setw(WIDTH) << "write"
			<< std::setw(WIDTH) << total_serviced_write_when_gc << std::setw(WIDTH) << write_confict_when_gc
			<< std::setw(WIDTH) << (double)write_confict_when_gc / (total_serviced_write_when_gc + 1e-10) << std::endl;
		std::cout << std::setw(WIDTH) << "read & write"
			<< std::setw(WIDTH) << total_serviced_read_when_gc + total_serviced_write_when_gc
			<< std::setw(WIDTH) << read_conflict_when_gc + write_confict_when_gc
			<< std::setw(WIDTH) << ((double)read_conflict_when_gc + write_confict_when_gc)
			/ ((double)total_serviced_read_when_gc + total_serviced_write_when_gc + 1e-10)
			<< std::endl;
		std::cout << std::string(WIDTH * 5, '-') << std::endl;
		std::cout << "gc execution: " << std::setw(WIDTH / 2) << Stats::Total_gc_executions << "valuable gc execution: "
			<< std::setw(WIDTH / 2) << ftl->GC_and_WL_Unit->Get_valuable_gc() << "proportional: "
			<< std::setw(WIDTH / 2) << ftl->GC_and_WL_Unit->Get_valuable_gc() / (1e-10 + Stats::Total_gc_executions) << "\n";
		std::cout << "==========\n";
		delete[] Round_robin_turn_of_channel;
		delete[] number_of_gc;
		delete[] alone_time;
		delete[] shared_time;
		for (unsigned int channel_id = 0; channel_id < channel_count; ++channel_id)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; ++chip_id)
			{
				delete[] chip_level_alone_time[channel_id][chip_id];
				delete[] chip_level_shared_time[channel_id][chip_id];
				delete[] chip_level_total_serviced_when_gc[channel_id][chip_id];
				delete[] chip_level_conflict_when_gc[channel_id][chip_id];
				delete[] gap[channel_id][chip_id];
				/*std::cout << channel_id << chip_id << "\t"
					<< (double)chip_level_latency[channel_id][chip_id] / chip_level_serviced_tr_count[channel_id][chip_id] / 1000 << "\n";*/
			}
			delete[] chip_level_alone_time[channel_id];
			delete[] chip_level_shared_time[channel_id];
			delete[] chip_level_total_serviced_when_gc[channel_id];
			delete[] chip_level_conflict_when_gc[channel_id];
			delete[] gap[channel_id];
			delete[] chip_level_serviced_tr_count[channel_id];
			delete[] chip_level_latency[channel_id];
			delete[] global_rra[channel_id];
		}
		delete[] chip_level_alone_time;
		delete[] chip_level_shared_time;
		delete[] chip_level_total_serviced_when_gc;
		delete[] chip_level_conflict_when_gc;
		delete[] gap;
		delete[] chip_level_latency;
		delete[] chip_level_serviced_tr_count;
		delete[] global_rra;
	}

	void TSU_Base::Setup_triggers()
	{
		Sim_Object::Setup_triggers();
		_NVMController->ConnectToTransactionServicedSignal(handle_transaction_serviced_signal_from_PHY);
		_NVMController->ConnectToChannelIdleSignal(handle_channel_idle_signal);
		_NVMController->ConnectToChipIdleSignal(handle_chip_idle_signal);
	}

	void TSU_Base::handle_transaction_serviced_signal_from_PHY(NVM_Transaction_Flash* transaction)
	{
		//TSU does nothing. The generator of the transaction will handle it.
		if (transaction->Source == Transaction_Source_Type::CACHE || transaction->Source == Transaction_Source_Type::USERIO)
		{
			_my_instance->alone_time[transaction->Stream_id] += transaction->alone_time;
			_my_instance->shared_time[transaction->Stream_id] += Simulator->Time() - transaction->Issue_time;
			_my_instance->chip_level_alone_time[transaction->Address.ChannelID][transaction->Address.ChipID][transaction->Stream_id] += transaction->alone_time;
			_my_instance->chip_level_shared_time[transaction->Address.ChannelID][transaction->Address.ChipID][transaction->Stream_id] += Simulator->Time() - transaction->Issue_time;
			_my_instance->total_count[transaction->Stream_id]++;
			if (_my_instance->start_gc)
			{
				if (transaction->Type == Transaction_Type::READ)
				{
					_my_instance->total_serviced_read_when_gc++;
					_my_instance->read_conflict_when_gc += (int)transaction->is_conflicting_gc;
				}
				else if (transaction->Type == Transaction_Type::WRITE)
				{
					_my_instance->total_serviced_write_when_gc++;
					_my_instance->write_confict_when_gc += (int)transaction->is_conflicting_gc;
				}
				//_my_instance->chip_level_total_serviced_when_gc[transaction->Address.ChannelID][transaction->Address.ChipID][transaction->Stream_id]++;
				//_my_instance->chip_level_conflict_when_gc[transaction->Address.ChannelID][transaction->Address.ChipID][transaction->Stream_id] += (int)transaction->is_conflicting_gc;
			}
			_my_instance->chip_level_serviced_tr_count[transaction->Address.ChannelID][transaction->Address.ChipID]++;
			_my_instance->chip_level_latency[transaction->Address.ChannelID][transaction->Address.ChipID] += Simulator->Time() - transaction->Issue_time;
		}
		_my_instance->handle_transaction_serviced_signal(transaction);
	}

	void TSU_Base::handle_channel_idle_signal(flash_channel_ID_type channelID)
	{
		for (flash_channel_ID_type channel_id = 0; channel_id < _my_instance->channel_count; channel_id++)
		{
			if (_my_instance->_NVMController->Get_channel_status(channel_id) == BusChannelStatus::IDLE)
			{
				_my_instance->enqueue_transaction_for_speed_limit_type_tsu();
				for (unsigned int i = 0; i < _my_instance->chip_no_per_channel; i++)
				{
					NVM::FlashMemory::Flash_Chip* chip = _my_instance->_NVMController->Get_chip(channel_id, _my_instance->Round_robin_turn_of_channel[channel_id]);
					//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
					_my_instance->service_transaction(chip);
					_my_instance->Round_robin_turn_of_channel[channel_id] = (flash_chip_ID_type)(_my_instance->Round_robin_turn_of_channel[channel_id] + 1) % _my_instance->chip_no_per_channel;
					if (_my_instance->_NVMController->Get_channel_status(chip->ChannelID) != BusChannelStatus::IDLE)
						break;
				}
			}
		}
		return;
		//for (unsigned int i = 0; i < _my_instance->chip_no_per_channel; i++) {
		//	NVM::FlashMemory::Flash_Chip* chip = _my_instance->_NVMController->Get_chip(channelID, _my_instance->Round_robin_turn_of_channel[channelID]);
		//	//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
		//	_my_instance->service_transaction(chip);
		//	_my_instance->Round_robin_turn_of_channel[channelID] = (flash_chip_ID_type)(_my_instance->Round_robin_turn_of_channel[channelID] + 1) % _my_instance->chip_no_per_channel;

		//	//A transaction has been started, so TSU should stop searching for another chip
		//	if (_my_instance->_NVMController->Get_channel_status(chip->ChannelID) == BusChannelStatus::BUSY)
		//		break;
		//}
	}
	
	void TSU_Base::handle_chip_idle_signal(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (_my_instance->_NVMController->Get_channel_status(chip->ChannelID) == BusChannelStatus::IDLE)
		{
			_my_instance->service_transaction(chip);
		}
	}

	sim_time_type TSU_Base::user_alone_time(sim_time_type waiting_time, Transaction_Type type)
	{
		if (type == Transaction_Type::READ)
		{
			waiting_time /= 2;
		}
		else if (type == Transaction_Type::WRITE)
		{
			waiting_time /= 4;
		}
		return waiting_time;
	}

	sim_time_type TSU_Base::gc_alone_time(sim_time_type waiting_time, Transaction_Type type)
	{
		if (type == Transaction_Type::READ)
		{
			waiting_time /*-= waiting_time / 4*//*/= 2*/;
		}
		else if (type == Transaction_Type::WRITE)
		{
			waiting_time /= 2/* = waiting_time * 2 / 5*/;
		}
		else if (type == Transaction_Type::ERASE)
		{
			waiting_time/*+= adjust_time / 2*/;
		}
		return waiting_time;
	}

	sim_time_type TSU_Base::mapping_alone_time(sim_time_type waiting_time, Transaction_Type type)
	{
		return waiting_time;
	}

	void TSU_Base::update_usr_gap(NVM_Transaction_Flash* tr)
	{
		Gap* g = &gap[tr->Address.ChannelID][tr->Address.ChipID][tr->Stream_id];
		if (g->last_arrival_time == 0)
		{
			g->last_arrival_time = Simulator->Time();
		}
		sim_time_type timing_gap = Simulator->Time() - g->last_arrival_time;
		double scalability = 1;
		while (timing_gap > 0)
		{
			timing_gap /= 10;
			scalability /= 10;
		}
		g->transaction_gap = std::pow(lambda, ((double)g->last_arrival_time - Simulator->Time()) * scalability)
			* (g->transaction_gap + Simulator->Time() - g->last_arrival_time);
		g->last_arrival_time = Simulator->Time();
	}

	void TSU_Base::allocate_DP(Flash_Transaction_Queue* queue, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return;
		for (auto it = queue->begin(); it != queue->end(); ++it)
		{
			//round robin
			auto& dp_rra = global_rra[channel_id][chip_id];
			(*it)->Address.DieID = dp_rra.DieID;
			(*it)->Address.PlaneID = dp_rra.PlaneID;
			dp_rra.DieID = (dp_rra.DieID + 1) % 2;
			if (dp_rra.DieID == 0)
			{
				dp_rra.PlaneID = (dp_rra.PlaneID + 1) % 2;
			}
			// static
			//(*it)->Address.DieID = (transaction->LPA / 32) % 2;
			//(*it)->Address.PlaneID = (transaction->LPA / 64) % 2;
		}
	}

	void TSU_Base::Schedule()
	{
		opened_scheduling_reqs--;
		if (opened_scheduling_reqs > 0)
			return;
		if (opened_scheduling_reqs < 0)
			PRINT_ERROR("TSU Schedule function is invoked in an incorrect way!");
		if (transaction_receive_slots.size() == 0)
			return;
		opened_scheduling_reqs++;
		for (auto tr : transaction_receive_slots)
		{
			_NVMController->test_transaction_for_conflicting_with_gc(tr);
			if (tr->Source == Transaction_Source_Type::CACHE || tr->Source == Transaction_Source_Type::USERIO)
			{
				update_usr_gap(tr);
			}
		}
		Schedule0();
	}

	void TSU_Base::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter)
	{
	}
	double TSU_Base::proportional_slowdown(stream_id_type stream_id)
	{
		double min_slowdown = DBL_MAX;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (alone_time[stream_id])
			{
				min_slowdown = std::min(min_slowdown, (double)shared_time[stream_id] / alone_time[stream_id]);
			}
		}
		double slowdown = (double)shared_time[stream_id] / (1e-10 + alone_time[stream_id]);
		return min_slowdown / (1e-10 + slowdown);
	}
	double TSU_Base::fairness()
	{
		double min_slowdown = DBL_MAX, max_slowdown = -1;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (alone_time[stream_id])
			{
				double workload_slowdown = (double)shared_time[stream_id] / alone_time[stream_id];
				min_slowdown = std::min(min_slowdown, workload_slowdown);
				max_slowdown = std::max(max_slowdown, workload_slowdown);
			}
		}
		return max_slowdown < 0 ? 1 : min_slowdown / max_slowdown;
	}
	double TSU_Base::proportional_slowdown(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		double min_slowdown = DBL_MAX;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (chip_level_alone_time[channel_id][chip_id][stream_id])
			{
				min_slowdown = std::min(min_slowdown, (double)chip_level_shared_time[channel_id][chip_id][stream_id]
					/ chip_level_alone_time[channel_id][chip_id][stream_id]);
			}
		}
		double slowdown = (double)chip_level_shared_time[channel_id][chip_id][stream_id]
			/ (1e-10 + chip_level_alone_time[channel_id][chip_id][stream_id]);
		return min_slowdown / (1e-10 + slowdown);
	}
	double TSU_Base::fairness(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		double min_slowdown = DBL_MAX, max_slowdown = -1;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (chip_level_alone_time[channel_id][chip_id][stream_id])
			{
				double workload_slowdown = (double)chip_level_shared_time[channel_id][chip_id][stream_id]
					/ chip_level_alone_time[channel_id][chip_id][stream_id];
				min_slowdown = std::min(min_slowdown, workload_slowdown);
				max_slowdown = std::max(max_slowdown, workload_slowdown);
			}
		}
		return max_slowdown < 0 ? 1 : min_slowdown / max_slowdown;
	}
	double TSU_Base::percentage_of_conflict_gc(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return (double)chip_level_conflict_when_gc[channel_id][chip_id][stream_id]
			/ (1e-10 + chip_level_total_serviced_when_gc[channel_id][chip_id][stream_id]);
	}
	sim_time_type TSU_Base::Get_chip_level_shared_time(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return chip_level_shared_time[channel_id][chip_id][stream_id];
	}
	sim_time_type TSU_Base::Get_chip_level_alone_time(stream_id_type stream_id, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		return chip_level_alone_time[channel_id][chip_id][stream_id];
	}
	double TSU_Base::estimated_gap(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id, stream_id_type stream_id)
	{
		return gap[channel_id][chip_id][stream_id].transaction_gap;
	}
	stream_id_type TSU_Base::stream_with_maximum_slowdown(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		stream_id_type max_stream_id = 0;
		double max_sd = (double)chip_level_shared_time[channel_id][chip_id][0] / (1e-10 + chip_level_alone_time[channel_id][chip_id][0]);
		for (unsigned int stream_id = 1; stream_id < stream_count; ++stream_id)
		{
			double sd = (double)chip_level_shared_time[channel_id][chip_id][stream_id]
				/ (1e-10 + chip_level_alone_time[channel_id][chip_id][stream_id]);
			if (sd > max_sd)
			{
				max_sd = sd;
				max_stream_id = stream_id;
			}
		}
		return max_stream_id;
	}
	void TSU_Base::proportional_slowdown_ordered_list(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id,
		std::vector<double>& v)
	{
		double min_slowdown = DBL_MAX;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (chip_level_alone_time[channel_id][chip_id][stream_id])
			{
				double slowdown = (double)chip_level_shared_time[channel_id][chip_id][stream_id]
					/ chip_level_alone_time[channel_id][chip_id][stream_id];
				min_slowdown = std::min(min_slowdown, slowdown);
				v.emplace_back(slowdown);
			}
		}
		for (unsigned int i = 0, sz = (unsigned int)v.size(); i < sz; ++i)
		{
			v[i] = min_slowdown / v[i];
		}
		v.resize(max_psd_size, 1);
		sort(v.begin(), v.end());
	}
}
