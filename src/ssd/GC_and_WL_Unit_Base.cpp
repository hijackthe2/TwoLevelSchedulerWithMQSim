#include "GC_and_WL_Unit_Base.h"

namespace SSD_Components
{
	GC_and_WL_Unit_Base* GC_and_WL_Unit_Base::_my_instance;
	
	GC_and_WL_Unit_Base::GC_and_WL_Unit_Base(const sim_object_id_type& id,
		Address_Mapping_Unit_Base* address_mapping_unit, Flash_Block_Manager_Base* block_manager, TSU_Base* tsu, NVM_PHY_ONFI* flash_controller,
		GC_Block_Selection_Policy_Type block_selection_policy, double gc_threshold, bool preemptible_gc_enabled, double gc_hard_threshold,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block, unsigned int sector_no_per_page, 
		bool use_copyback, double rho, unsigned int max_ongoing_gc_reqs_per_plane, bool dynamic_wearleveling_enabled, bool static_wearleveling_enabled, unsigned int static_wearleveling_threshold, int seed) :
		Sim_Object(id), address_mapping_unit(address_mapping_unit), block_manager(block_manager), tsu(tsu), flash_controller(flash_controller), force_gc(false),
		block_selection_policy(block_selection_policy), gc_threshold(gc_threshold),	use_copyback(use_copyback), 
		preemptible_gc_enabled(preemptible_gc_enabled), gc_hard_threshold(gc_hard_threshold),
		random_generator(seed), max_ongoing_gc_reqs_per_plane(max_ongoing_gc_reqs_per_plane),
		channel_count(channel_count), chip_no_per_channel(chip_no_per_channel), die_no_per_chip(die_no_per_chip), plane_no_per_die(plane_no_per_die),
		block_no_per_plane(block_no_per_plane), pages_no_per_block(page_no_per_block), sector_no_per_page(sector_no_per_page),
		dynamic_wearleveling_enabled(dynamic_wearleveling_enabled), static_wearleveling_enabled(static_wearleveling_enabled), static_wearleveling_threshold(static_wearleveling_threshold)
	{
		_my_instance = this;
		block_pool_gc_threshold = (unsigned int)(gc_threshold * (double)block_no_per_plane);
		if (block_pool_gc_threshold < 1)
		{
			block_pool_gc_threshold = 1;
			this->gc_threshold = (double)block_pool_gc_threshold / block_no_per_plane;
		}
		block_pool_gc_hard_threshold = (unsigned int)(gc_hard_threshold * (double)block_no_per_plane);
		if (block_pool_gc_hard_threshold < 1)
		{
			block_pool_gc_hard_threshold = 1;
			this->gc_hard_threshold = (double)block_pool_gc_threshold / block_no_per_plane;
		}
		random_pp_threshold = (unsigned int)(rho * pages_no_per_block);
		if (block_pool_gc_threshold < max_ongoing_gc_reqs_per_plane)
			block_pool_gc_threshold = max_ongoing_gc_reqs_per_plane;

		unsigned int stream_count = address_mapping_unit->Get_no_of_input_streams();
		unsigned int max_psd_size = tsu->Get_max_psd_size();
		valuable_gc = 0;
		gc_fs.open("out/gc_info.txt", std::fstream::out);
		gc_fs << std::fixed << std::setprecision(5);
		gc_fs << "c\tw\td\tp\t" << "pipb\tpvpb\tpfpb\tpfbb\t" << "bipb\tbvpb\t" << "gtb\tutb\t";
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			gc_fs << "sb" + std::to_string(stream_id) << "\t";
		}
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			gc_fs << "srb" + std::to_string(stream_id) << "\t";
		}
		for (unsigned int stream_id = 0; stream_id < max_psd_size; ++stream_id)
		{
			gc_fs << "psdb" + std::to_string(stream_id) << "\t";
		}
		gc_fs << "fb\t";
		size_t max_size = tsu->Get__NVMController()->Get_max_size() - 1;
		for (unsigned int i = 0; i < max_size; ++i)
		{
			gc_fs << "gap" + std::to_string(i) << "\t";
		}
		gc_fs << "pipa\tpvpa\tpfpa\tpfba\t" << "bipa\tbvpa\t" << "gta\tuta\t";
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			gc_fs << "sa" + std::to_string(stream_id) << "\t";
		}
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			gc_fs << "sra" + std::to_string(stream_id) << "\t";
		}
		for (unsigned int stream_id = 0; stream_id < max_psd_size; ++stream_id)
		{
			gc_fs << "psda" + std::to_string(stream_id) << "\t";
		}
		gc_fs << "fa\t";
		gc_fs << "ca\tcwoa\tid\t" << "GC" << std::endl;
	}

	void GC_and_WL_Unit_Base::Setup_triggers()
	{
		Sim_Object::Setup_triggers();
		flash_controller->ConnectToTransactionServicedSignal(handle_transaction_serviced_signal_from_PHY);
	}

	void GC_and_WL_Unit_Base::handle_transaction_serviced_signal_from_PHY(NVM_Transaction_Flash* transaction)
	{
		if (transaction->Type == Transaction_Type::ERASE)
		{
			std::string key = std::to_string(transaction->Issue_time) + "-" + std::to_string(transaction->Address.ChannelID)
				+ std::to_string(transaction->Address.ChipID) + std::to_string(transaction->Address.DieID)
				+ std::to_string(transaction->Address.PlaneID)
				+ '-' + std::to_string(transaction->Address.BlockID);
			std::unordered_map<std::string, float>& snapshot_before = _my_instance->snapshot[key];
			std::unordered_map<std::string, float> snapshot_after = _my_instance->gc_snapshot(transaction->Address);
			int GC = _my_instance->record_gc_info_when_finished(snapshot_before, snapshot_after, transaction->Address, transaction->Stream_id,
				(NVM_Transaction_Flash_ER *)transaction);
			_my_instance->valuable_gc += GC;
			
			// train
			if (GC == 0)
			{
				float bip = snapshot_before["bip"];
				int gt = (int)snapshot_before["gt"], ut = (int)snapshot_before["ut"];
				float psd = snapshot_before["psd"], f = snapshot_before["f"];
				//if (gc_classifier != NULL) train(gc_classifier, bip, gt, ut, psd, f, 0);
			}

			_my_instance->snapshot.erase(key);

			/*unsigned int stream_count = _my_instance->address_mapping_unit->Get_no_of_input_streams();
			if (Stats::Total_gc_executions >= stream_count * 10000)
			{
				std::cout << "stopped by MQSim with gc " << Stats::Total_gc_executions << "\n";
				Simulator->Stop_simulation();
			}*/
		}
		PlaneBookKeepingType* pbke = &(_my_instance->block_manager->plane_manager[transaction->Address.ChannelID][transaction->Address.ChipID][transaction->Address.DieID][transaction->Address.PlaneID]);

		switch (transaction->Source)
		{
		case Transaction_Source_Type::USERIO:
		case Transaction_Source_Type::MAPPING:
		case Transaction_Source_Type::CACHE:
			switch (transaction->Type)
			{
			case Transaction_Type::READ:
				_my_instance->block_manager->Read_transaction_serviced(transaction->Address);
				break;
			case Transaction_Type::WRITE:
				_my_instance->block_manager->Program_transaction_serviced(transaction->Address);
				break;
			default:
				PRINT_ERROR("Unexpected situation in the GC_and_WL_Unit_Base function!")
			}
			if (_my_instance->block_manager->Block_has_ongoing_gc_wl(transaction->Address))
				if (_my_instance->block_manager->Can_execute_gc_wl(transaction->Address))
				{
					NVM::FlashMemory::Physical_Page_Address gc_wl_candidate_address(transaction->Address);
					Block_Pool_Slot_Type* block = &pbke->Blocks[transaction->Address.BlockID];
					Stats::Total_gc_executions++;
					_my_instance->tsu->Prepare_for_transaction_submit();
					NVM_Transaction_Flash_ER* gc_wl_erase_tr = new NVM_Transaction_Flash_ER(Transaction_Source_Type::GC_WL, block->Stream_id, gc_wl_candidate_address);
					if (block->Current_page_write_index - block->Invalid_page_count > 0)//If there are some valid pages in block, then prepare flash transactions for page movement
					{
						//address_mapping_unit->Lock_physical_block_for_gc(gc_candidate_address);//Lock the block, so no user request can intervene while the GC is progressing
						NVM_Transaction_Flash_RD* gc_wl_read = NULL;
						NVM_Transaction_Flash_WR* gc_wl_write = NULL;
						for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++)
						{
							if (_my_instance->block_manager->Is_page_valid(block, pageID))
							{
								gc_wl_candidate_address.PageID = pageID;
								if (_my_instance->use_copyback)
								{
									gc_wl_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, _my_instance->sector_no_per_page * SECTOR_SIZE_IN_BYTE,
										NO_LPA, _my_instance->address_mapping_unit->Convert_address_to_ppa(gc_wl_candidate_address), NULL, 0, NULL, 0, INVALID_TIME_STAMP);
									gc_wl_write->ExecutionMode = WriteExecutionModeType::COPYBACK;
									_my_instance->tsu->Submit_transaction(gc_wl_write);
								}
								else
								{
									gc_wl_read = new NVM_Transaction_Flash_RD(Transaction_Source_Type::GC_WL, block->Stream_id, _my_instance->sector_no_per_page * SECTOR_SIZE_IN_BYTE,
										NO_LPA, _my_instance->address_mapping_unit->Convert_address_to_ppa(gc_wl_candidate_address), gc_wl_candidate_address, NULL, 0, NULL, 0, INVALID_TIME_STAMP);
									gc_wl_read->Physical_address_determined = true;
									gc_wl_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, _my_instance->sector_no_per_page * SECTOR_SIZE_IN_BYTE,
										NO_LPA, NO_PPA, gc_wl_candidate_address, NULL, 0, gc_wl_read, 0, INVALID_TIME_STAMP);
									gc_wl_write->ExecutionMode = WriteExecutionModeType::SIMPLE;
									gc_wl_write->RelatedErase = gc_wl_erase_tr;
									gc_wl_read->RelatedWrite = gc_wl_write;
									_my_instance->tsu->Submit_transaction(gc_wl_read);//Only the read transaction would be submitted. The Write transaction is submitted when the read transaction is finished and the LPA of the target page is determined
								}
								gc_wl_erase_tr->Page_movement_activities.push_back(gc_wl_write);
							}
						}
					}
					gc_wl_erase_tr->Physical_address_determined = true;
					block->Erase_transaction = gc_wl_erase_tr;
					_my_instance->record_gc_info_when_issued(gc_wl_erase_tr);
					_my_instance->tsu->Schedule();
				}
			return;
		}

		switch (transaction->Type)
		{
		case Transaction_Type::READ:
		{
			PPA_type ppa;
			MPPN_type mppa;
			page_status_type page_status_bitmap;
			if (pbke->Blocks[transaction->Address.BlockID].Holds_mapping_data)
			{
				_my_instance->address_mapping_unit->Get_translation_mapping_info_for_gc(transaction->Stream_id, (MVPN_type)transaction->LPA, mppa, page_status_bitmap);
				if (mppa == transaction->PPA)//There has been no write on the page since GC start, and it is still valid
				{
					_my_instance->tsu->Prepare_for_transaction_submit();
					((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite->write_sectors_bitmap = FULL_PROGRAMMED_PAGE;
					((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite->LPA = transaction->LPA;
					((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite->RelatedRead = NULL;
					_my_instance->address_mapping_unit->Allocate_new_page_for_gc(((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite, pbke->Blocks[transaction->Address.BlockID].Holds_mapping_data);
					_my_instance->tsu->Submit_transaction(((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite);
					_my_instance->tsu->Schedule();
				}
				else
					PRINT_ERROR("Inconsistency found when moving a page for GC/WL!")
			}
			else
			{
				_my_instance->address_mapping_unit->Get_data_mapping_info_for_gc(transaction->Stream_id, transaction->LPA, ppa, page_status_bitmap);
				if (ppa == transaction->PPA)//There has been no write on the page since GC start, and it is still valid
				{
					_my_instance->tsu->Prepare_for_transaction_submit();
					((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite->write_sectors_bitmap = page_status_bitmap;
					((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite->LPA = transaction->LPA;
					((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite->RelatedRead = NULL;
					//_my_instance->address_mapping_unit->allocate_plane_for_gc_write(((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite);
					_my_instance->address_mapping_unit->Allocate_new_page_for_gc(((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite, pbke->Blocks[transaction->Address.BlockID].Holds_mapping_data);
					_my_instance->tsu->Submit_transaction(((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite);
					_my_instance->tsu->Schedule();
				}
				else
				{
					PRINT_ERROR("Inconsistency found when moving a page for GC/WL!")
				}
			}
			break;
		}
		case Transaction_Type::WRITE:
			if (pbke->Blocks[((NVM_Transaction_Flash_WR*)transaction)->RelatedErase->Address.BlockID].Holds_mapping_data)
			{
				_my_instance->address_mapping_unit->Remove_barrier_for_accessing_mvpn(transaction->Stream_id, (MVPN_type)transaction->LPA);
				DEBUG(Simulator->Time() << ": MVPN=" << (MVPN_type)transaction->LPA << " unlocked!!");
			}
			else
			{
				_my_instance->address_mapping_unit->Remove_barrier_for_accessing_lpa(transaction->Stream_id, transaction->LPA);
				DEBUG(Simulator->Time() << ": LPA=" << (MVPN_type)transaction->LPA << " unlocked!!");
			}
			pbke->Blocks[((NVM_Transaction_Flash_WR*)transaction)->RelatedErase->Address.BlockID].Erase_transaction->Page_movement_activities.remove((NVM_Transaction_Flash_WR*)transaction);
			break;
		case Transaction_Type::ERASE:
			pbke->Ongoing_erase_operations.erase(pbke->Ongoing_erase_operations.find(transaction->Address.BlockID));
			_my_instance->block_manager->Add_erased_block_to_pool(transaction->Address);
			_my_instance->block_manager->GC_WL_finished(transaction->Address);
			if (_my_instance->check_static_wl_required(transaction->Address))
				_my_instance->run_static_wearleveling(transaction->Address);
			_my_instance->address_mapping_unit->Start_servicing_writes_for_overfull_plane(transaction->Address);//Must be inovked after above statements since it may lead to flash page consumption for waiting program transactions

			if (_my_instance->Stop_servicing_writes(transaction->Address))
				_my_instance->Check_gc_required(pbke->Get_free_block_pool_size(), transaction->Address);
			break;
		}
	}

	void GC_and_WL_Unit_Base::Start_simulation() {}

	void GC_and_WL_Unit_Base::Validate_simulation_config() {}

	void GC_and_WL_Unit_Base::Execute_simulator_event(MQSimEngine::Sim_Event* ev) {}

	GC_Block_Selection_Policy_Type GC_and_WL_Unit_Base::Get_gc_policy()
	{
		return block_selection_policy;
	}

	unsigned int GC_and_WL_Unit_Base::Get_GC_policy_specific_parameter()
	{
		switch (block_selection_policy)
		{
		case GC_Block_Selection_Policy_Type::RGA:
			return rga_set_size;
		case GC_Block_Selection_Policy_Type::RANDOM_PP:
			return random_pp_threshold;
		}
		return 0;
	}

	unsigned int GC_and_WL_Unit_Base::Get_minimum_number_of_free_pages_before_GC()
	{
		return block_pool_gc_threshold;
		/*if (preemptible_gc_enabled)
			return block_pool_gc_hard_threshold;
		else return block_pool_gc_threshold;*/
	}

	bool GC_and_WL_Unit_Base::Use_dynamic_wearleveling()
	{
		return dynamic_wearleveling_enabled;
	}

	inline bool GC_and_WL_Unit_Base::Use_static_wearleveling()
	{
		return static_wearleveling_enabled;
	}
	
	bool GC_and_WL_Unit_Base::Stop_servicing_writes(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		PlaneBookKeepingType* pbke = &(_my_instance->block_manager->plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID]);
		return block_manager->Get_pool_size(plane_address) < max_ongoing_gc_reqs_per_plane;
	}

	bool GC_and_WL_Unit_Base::is_safe_gc_wl_candidate(const PlaneBookKeepingType* plane_record, const flash_block_ID_type gc_wl_candidate_block_id)
	{
		//The block shouldn't be a current write frontier
		for (unsigned int stream_id = 0; stream_id < address_mapping_unit->Get_no_of_input_streams(); stream_id++)
			if ((&plane_record->Blocks[gc_wl_candidate_block_id]) == plane_record->Data_wf[stream_id]
				|| (&plane_record->Blocks[gc_wl_candidate_block_id]) == plane_record->Translation_wf[stream_id]
				|| (&plane_record->Blocks[gc_wl_candidate_block_id]) == plane_record->GC_wf[stream_id])
				return false;

		//The block shouldn't have an ongoing program request (all pages must already be written)
		if (plane_record->Blocks[gc_wl_candidate_block_id].Ongoing_user_program_count
			+ plane_record->Blocks[gc_wl_candidate_block_id].Ongoing_user_read_count > 0)
			return false;
		if (plane_record->Blocks[gc_wl_candidate_block_id].Ongoing_user_program_count > 0)
			return false;

		if (plane_record->Blocks[gc_wl_candidate_block_id].Has_ongoing_gc_wl)
			return false;

		return true;
	}

	inline bool GC_and_WL_Unit_Base::check_static_wl_required(const NVM::FlashMemory::Physical_Page_Address plane_address)
	{
		return static_wearleveling_enabled && (block_manager->Get_min_max_erase_difference(plane_address) >= static_wearleveling_threshold);
	}

	void GC_and_WL_Unit_Base::run_static_wearleveling(const NVM::FlashMemory::Physical_Page_Address plane_address)
	{
		PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(plane_address);
		flash_block_ID_type wl_candidate_block_id = block_manager->Get_coldest_block_id(plane_address);
		if (!is_safe_gc_wl_candidate(pbke, wl_candidate_block_id))
			return;

		NVM::FlashMemory::Physical_Page_Address wl_candidate_address(plane_address);
		wl_candidate_address.BlockID = wl_candidate_block_id;
		Block_Pool_Slot_Type* block = &pbke->Blocks[wl_candidate_block_id];

		//Run the state machine to protect against race condition
		block_manager->GC_WL_started(wl_candidate_block_id);
		pbke->Ongoing_erase_operations.insert(wl_candidate_block_id);
		address_mapping_unit->Set_barrier_for_accessing_physical_block(wl_candidate_address);//Lock the block, so no user request can intervene while the GC is progressing
		if (block_manager->Can_execute_gc_wl(wl_candidate_address))//If there are ongoing requests targeting the candidate block, the gc execution should be postponed
		{
			Stats::Total_wl_executions++;
			tsu->Prepare_for_transaction_submit();

			NVM_Transaction_Flash_ER* wl_erase_tr = new NVM_Transaction_Flash_ER(Transaction_Source_Type::GC_WL, pbke->Blocks[wl_candidate_block_id].Stream_id, wl_candidate_address);
			if (block->Current_page_write_index - block->Invalid_page_count > 0)//If there are some valid pages in block, then prepare flash transactions for page movement
			{
				NVM_Transaction_Flash_RD* wl_read = NULL;
				NVM_Transaction_Flash_WR* wl_write = NULL;
				for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++)
				{
					if (block_manager->Is_page_valid(block, pageID))
					{
						Stats::Total_page_movements_for_gc;
						wl_candidate_address.PageID = pageID;
						if (use_copyback)
						{
							wl_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
								NO_LPA, address_mapping_unit->Convert_address_to_ppa(wl_candidate_address), NULL, 0, NULL, 0, INVALID_TIME_STAMP);
							wl_write->ExecutionMode = WriteExecutionModeType::COPYBACK;
							tsu->Submit_transaction(wl_write);
						}
						else
						{
							wl_read = new NVM_Transaction_Flash_RD(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
								NO_LPA, address_mapping_unit->Convert_address_to_ppa(wl_candidate_address), wl_candidate_address, NULL, 0, NULL, 0, INVALID_TIME_STAMP);
							wl_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
								NO_LPA, NO_PPA, wl_candidate_address, NULL, 0, wl_read, 0, INVALID_TIME_STAMP);
							wl_write->ExecutionMode = WriteExecutionModeType::SIMPLE;
							wl_write->RelatedErase = wl_erase_tr;
							wl_read->RelatedWrite = wl_write;
							tsu->Submit_transaction(wl_read);//Only the read transaction would be submitted. The Write transaction is submitted when the read transaction is finished and the LPA of the target page is determined
						}
						wl_erase_tr->Page_movement_activities.push_back(wl_write);
					}
				}
			}
			block->Erase_transaction = wl_erase_tr;
			tsu->Submit_transaction(wl_erase_tr);

			tsu->Schedule();
		}
	}

	void GC_and_WL_Unit_Base::record_gc_info_when_issued(const NVM_Transaction_Flash* const erase)
	{
		std::string key = std::to_string(erase->Issue_time) + "-" + std::to_string(erase->Address.ChannelID)
			+ std::to_string(erase->Address.ChipID) + std::to_string(erase->Address.DieID) + std::to_string(erase->Address.PlaneID)
			+ '-' + std::to_string(erase->Address.BlockID);
		snapshot[key] = gc_snapshot(erase->Address);
		tsu->start_execute_gc();
		unsigned int free_block_pool_size = block_manager->Get_plane_bookkeeping_entry(erase->Address)->Get_free_block_pool_size();
		if (Stats::Total_gc_executions % 1000 == 0)
		{
			std::cout << "gc execte\t" << Stats::Total_gc_executions << "\t"
				<< (double)free_block_pool_size / block_no_per_plane << "\t"
				<< Simulator->Time() << "\t" << erase->Stream_id << "\n";
		}
		Stats::Total_gc_executions++;
		tsu->stat_gc_erase(erase->Stream_id);
	}

	std::unordered_map<std::string, float> GC_and_WL_Unit_Base::get_gc_info(const NVM::FlashMemory::Physical_Page_Address& gc_plane_address)
	{
		std::unordered_map<std::string, float> info;
		PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(gc_plane_address);
		Block_Pool_Slot_Type* block = &pbke->Blocks[gc_plane_address.BlockID];
		unsigned int free_block_pool_size = pbke->Get_free_block_pool_size();
		// plane
		info["pip"] = (float)pbke->Invalid_pages_count / pbke->Total_pages_count; // plane invalid page percent
		info["pvp"] = (float)pbke->Valid_pages_count / pbke->Total_pages_count; // plane valid page percent
		info["pfp"] = (float)pbke->Free_pages_count / pbke->Total_pages_count; // plane free page percent
		info["pfb"] = (float)free_block_pool_size / block_no_per_plane; // plane free block percent
		// block
		info["bip"] = (float)block->Invalid_page_count / pages_no_per_block; // block invalid page percent
		info["bvp"] = 1 - info["bip"];
		// proportional slowdown
		info["psd"] = (float)tsu->proportional_slowdown(block->Stream_id, gc_plane_address.ChannelID, gc_plane_address.ChipID);
		std::vector<double> v;
		tsu->proportional_slowdown_ordered_list(gc_plane_address.ChannelID, gc_plane_address.ChipID, v);
		for (size_t i = 0, sz = v.size(); i < sz; ++i)
		{
			info["psd" + std::to_string(i)] = (float)v[i];
		}
		info["f"] = (float)tsu->fairness(gc_plane_address.ChannelID, gc_plane_address.ChipID); // fairness
		// tsu
		info["gt"] = tsu->GCTRQueueSize(gc_plane_address.ChannelID, gc_plane_address.ChipID) > 0; // has gc transaction
		info["ut"] = tsu->UserTRQueueSize(gc_plane_address.ChannelID, gc_plane_address.ChipID) > 0; // has user transaction
		return info;
	}
	std::unordered_map<std::string, float> GC_and_WL_Unit_Base::gc_snapshot(const NVM::FlashMemory::Physical_Page_Address& gc_plane_address)
	{
		std::unordered_map<std::string, float> info;
		PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(gc_plane_address);
		Block_Pool_Slot_Type* block = &pbke->Blocks[gc_plane_address.BlockID];
		unsigned int free_block_pool_size = pbke->Get_free_block_pool_size();
		// plane
		info["pip"] = (float)pbke->Invalid_pages_count / pbke->Total_pages_count;
		info["pvp"] = (float)pbke->Valid_pages_count / pbke->Total_pages_count;
		info["pfp"] = (float)pbke->Free_pages_count / pbke->Total_pages_count;
		info["pfb"] = (float)free_block_pool_size / block_no_per_plane;
		// block
		info["bip"] = (float)block->Invalid_page_count / pages_no_per_block;
		info["bvp"] = 1 - info["bip"];
		unsigned int stream_count = address_mapping_unit->Get_no_of_input_streams();
		// fairness
		info["f"] = (float)tsu->fairness(gc_plane_address.ChannelID, gc_plane_address.ChipID);
		// tsu
		info["gt"] = tsu->GCTRQueueSize(gc_plane_address.ChannelID, gc_plane_address.ChipID) > 0;
		float user_size = (float)tsu->UserTRQueueSize(gc_plane_address.ChannelID, gc_plane_address.ChipID);
		info["ut"] = user_size > 0;
		double total_size = 0;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			std::string str = std::to_string(stream_id);
			float s = (float)tsu->UserTRQueueSize(stream_id, gc_plane_address.ChannelID, gc_plane_address.ChipID);
			// size
			info["s" + str] = s;
			// size rate
			info["sr" + str] = (float)(s / (1e-10 + user_size));
		}
		// proportional slowdown
		std::vector<double> v;
		tsu->proportional_slowdown_ordered_list(gc_plane_address.ChannelID, gc_plane_address.ChipID, v);
		for (size_t i = 0, sz = v.size(); i < sz; ++i)
		{
			info["psd" + std::to_string(i)] = (float)v[i];
		}
		NVM_PHY_ONFI_NVDDR2* controller = tsu->Get__NVMController();
		NVM::FlashMemory::Flash_Chip* flash_chip = controller->Get_chip(gc_plane_address.ChannelID, gc_plane_address.ChipID);
		sim_time_type gc_time = flash_chip->Get_command_execution_latency((int)Transaction_Type::ERASE, 0);
		for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++)
		{
			if (block_manager->Is_page_valid(block, pageID))
			{
				gc_time += flash_chip->Get_command_execution_latency((int)Transaction_Type::READ, pageID);
				gc_time += flash_chip->Get_command_execution_latency((int)Transaction_Type::WRITE, pageID);
				gc_time += 2 * controller->Expected_transfer_time(sector_no_per_page * SECTOR_SIZE_IN_BYTE, gc_plane_address.ChannelID);
			}
		}
		std::queue<sim_time_type> recently_usr_time;
		controller->Get_plane_recently_serviced_usr_time(gc_plane_address, recently_usr_time);
		size_t max_size = std::max(recently_usr_time.size(), controller->Get_max_size()) - 1;
		sim_time_type t = 0;
		if (!recently_usr_time.empty())
		{
			t = recently_usr_time.front();
			recently_usr_time.pop();
		}
		for (unsigned int i = 0; i < max_size; ++i)
		{
			float f = 1;
			if (!recently_usr_time.empty())
			{
				f = (float)(recently_usr_time.front() - t) / gc_time;
				t = recently_usr_time.front();
				recently_usr_time.pop();
			}
			info["gap" + std::to_string(i)] = std::min(f, (float)1);
		}
		return info;
	}
	int GC_and_WL_Unit_Base::record_gc_info_when_finished(const std::unordered_map<std::string, float>& before,
		const std::unordered_map<std::string, float>& after, const NVM::FlashMemory::Physical_Page_Address& address,
		const stream_id_type stream_id, const NVM_Transaction_Flash_ER* const erase)
	{
		std::vector<float> snapshot_vector;
		unsigned int stream_count = address_mapping_unit->Get_no_of_input_streams();
		gc_fs << address.ChannelID << "\t" << address.ChipID << "\t" << address.DieID << "\t" << address.PlaneID << "\t"
			<< before.at("pip") << "\t" << before.at("pvp") << "\t" << before.at("pfp") << "\t" << before.at("pfb") << "\t"
			<< before.at("bip") << "\t" << before.at("bvp") << "\t"
			<< before.at("gt") << "\t" << before.at("ut") << "\t";
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			gc_fs << before.at("s" + std::to_string(stream_id)) << "\t";
		}
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			gc_fs << before.at("sr" + std::to_string(stream_id)) << "\t";
		}
		size_t max_psd_size = tsu->Get_max_psd_size();
		for (unsigned int stream_id = 0; stream_id < max_psd_size; ++stream_id)
		{
			gc_fs << before.at("psd" + std::to_string(stream_id)) << "\t";
		}
		gc_fs << before.at("f") << "\t";
		size_t max_size = tsu->Get__NVMController()->Get_max_size() - 1;
		for (unsigned int i = 0; i < max_size; ++i)
		{
			gc_fs << before.at("gap" + std::to_string(i)) << "\t";
		}
		gc_fs << after.at("pip") << "\t" << after.at("pvp") << "\t" << after.at("pfp") << "\t" << after.at("pfb") << "\t"
			<< after.at("bip") << "\t" << after.at("bvp") << "\t"
			<< after.at("gt") << "\t" << after.at("ut") << "\t";
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			gc_fs << after.at("s" + std::to_string(stream_id)) << "\t";
		}
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			gc_fs << after.at("sr" + std::to_string(stream_id)) << "\t";
		}
		for (unsigned int stream_id = 0; stream_id < max_psd_size; ++stream_id)
		{
			gc_fs << after.at("psd" + std::to_string(stream_id)) << "\t";
		}
		gc_fs << after.at("f") << "\t"
			<< (int)(erase->conflict_with_gc_read_count + erase->conflict_with_gc_write_count + erase->conflict_with_gc_erase_count > 0) << "\t"
			<< (int)(erase->conflict_with_others) << "\t"
			<< stream_id << "\t";
		bool gt_ut = true;
		if (gc_hard_threshold <= before.at("pfb") && before.at("pfb") <= gc_threshold) gt_ut = (before.at("ut") == 0 && before.at("gt") == 0);
		bool f = after.at("f") > before.at("f");
		if (before.at("ut") == 0 && before.at("gt") == 0) f = after.at("f") >= before.at("f");
		bool GC = (f && !erase->conflict_with_others && gt_ut);
		gc_fs << (int)GC << std::endl;
		return (int)GC;
	}
}
