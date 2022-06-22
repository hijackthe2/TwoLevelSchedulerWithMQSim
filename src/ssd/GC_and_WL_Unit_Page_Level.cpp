#include <math.h>
#include <vector>
#include <set>
#include "GC_and_WL_Unit_Page_Level.h"
#include "Flash_Block_Manager.h"
#include "FTL.h"

namespace SSD_Components
{

	GC_and_WL_Unit_Page_Level::GC_and_WL_Unit_Page_Level(const sim_object_id_type& id,
		Address_Mapping_Unit_Base* address_mapping_unit, Flash_Block_Manager_Base* block_manager, TSU_Base* tsu, NVM_PHY_ONFI* flash_controller, 
		GC_Block_Selection_Policy_Type block_selection_policy, double gc_threshold, bool preemptible_gc_enabled, double gc_hard_threshold,
		unsigned int ChannelCount, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int Page_no_per_block, unsigned int sectors_per_page, 
		bool use_copyback, double rho, unsigned int max_ongoing_gc_reqs_per_plane, bool dynamic_wearleveling_enabled, bool static_wearleveling_enabled, unsigned int static_wearleveling_threshold, int seed)
		: GC_and_WL_Unit_Base(id, address_mapping_unit, block_manager, tsu, flash_controller, block_selection_policy, gc_threshold, preemptible_gc_enabled, gc_hard_threshold,
		ChannelCount, chip_no_per_channel, die_no_per_chip, plane_no_per_die, block_no_per_plane, Page_no_per_block, sectors_per_page, use_copyback, rho, max_ongoing_gc_reqs_per_plane, 
			dynamic_wearleveling_enabled, static_wearleveling_enabled, static_wearleveling_threshold, seed)
	{
		rga_set_size = (unsigned int)log2(block_no_per_plane);
	}

	GC_and_WL_Unit_Page_Level::~GC_and_WL_Unit_Page_Level()
	{
	}
	
	bool GC_and_WL_Unit_Page_Level::GC_is_in_urgent_mode(const NVM::FlashMemory::Flash_Chip* chip)
	{
		if (!preemptible_gc_enabled)
			return true;

		NVM::FlashMemory::Physical_Page_Address addr;
		addr.ChannelID = chip->ChannelID; addr.ChipID = chip->ChipID;
		for (unsigned int die_id = 0; die_id < die_no_per_chip; die_id++)
			for (unsigned int plane_id = 0; plane_id < plane_no_per_die; plane_id++)
			{
				addr.DieID = die_id; addr.PlaneID = plane_id;
				if (block_manager->Get_pool_size(addr) < block_pool_gc_hard_threshold)
					return true;
			}
		return false;
	}

	void GC_and_WL_Unit_Page_Level::Check_gc_required(const unsigned int free_block_pool_size, const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		if (free_block_pool_size >= block_pool_gc_threshold) return;
		//if (free_block_pool_size >= block_pool_gc_hard_threshold && tsu->UserTRQueueSize(plane_address.ChannelID, plane_address.ChipID) > 0
			//&& tsu->Get__NVMController()->GetChipStatus(tsu->Get__NVMController()->Get_chip(plane_address.ChannelID, plane_address.ChipID)) == ChipStatus::IDLE) return;

		flash_block_ID_type gc_candidate_block_id = block_manager->Get_coldest_block_id(plane_address);
		PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(plane_address);

		if (pbke->Ongoing_erase_operations.size() >= max_ongoing_gc_reqs_per_plane)
			return;

		switch (block_selection_policy)
		{
			case SSD_Components::GC_Block_Selection_Policy_Type::GREEDY://Find the set of blocks with maximum number of invalid pages and no free pages
			{
				gc_candidate_block_id = 0;
				if (pbke->Ongoing_erase_operations.find(0) != pbke->Ongoing_erase_operations.end())
					gc_candidate_block_id++;
				for (flash_block_ID_type block_id = 1; block_id < block_no_per_plane; block_id++)
				{
					if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
						&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_block
						&& pbke->Blocks[block_id].Invalid_page_count > 0
						&& is_safe_gc_wl_candidate(pbke, block_id))
						gc_candidate_block_id = block_id;
				}
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RGA:
			{
				std::set<flash_block_ID_type> random_set;
				while (random_set.size() < rga_set_size)
				{
					flash_block_ID_type block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
					if (pbke->Ongoing_erase_operations.find(block_id) == pbke->Ongoing_erase_operations.end()
						&& is_safe_gc_wl_candidate(pbke, block_id)
						&& pbke->Blocks[block_id].Invalid_page_count > 0
						&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_block)
						random_set.insert(block_id);
				}
				if (random_set.empty()) return;
				gc_candidate_block_id = *random_set.begin();
				for(auto &block_id : random_set)
					if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
						&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_block)
						gc_candidate_block_id = block_id;
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;
				while (!is_safe_gc_wl_candidate(pbke, gc_candidate_block_id) && repeat++ < block_no_per_plane)//A write frontier block should not be selected for garbage collection
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_P:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;

				//A write frontier block or a block with free pages should not be selected for garbage collection
				while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block || !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
					&& repeat++ < block_no_per_plane)
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_PP:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;

				//The selected gc block should have a minimum number of invalid pages
				while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block 
					|| pbke->Blocks[gc_candidate_block_id].Invalid_page_count < random_pp_threshold
					|| !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
					&& repeat++ < block_no_per_plane)
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::FIFO:
				gc_candidate_block_id = pbke->Block_usage_history.front();
				pbke->Block_usage_history.pop();
				break;
			case SSD_Components::GC_Block_Selection_Policy_Type::FBS:
			{
				unsigned int stream_count = address_mapping_unit->Get_no_of_input_streams();
				flash_block_ID_type* block_ids = new flash_block_ID_type[stream_count];
				unsigned int* invalid_page_counts = new unsigned int[stream_count];
				for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
				{
					block_ids[stream_id] = 0;
					invalid_page_counts[stream_id] = 0;
				}
				// select a block which contains most invalid pages for each workload
				for (flash_block_ID_type block_id = 0; block_id < block_no_per_plane; ++block_id)
				{
					stream_id_type stream_id = pbke->Blocks[block_id].Stream_id;
					if (pbke->Blocks[block_id].Current_page_write_index == pages_no_per_block && is_safe_gc_wl_candidate(pbke, block_id)
						&& pbke->Blocks[block_id].Invalid_page_count > invalid_page_counts[stream_id])
					{
						block_ids[stream_id] = block_id;
						invalid_page_counts[stream_id] = pbke->Blocks[block_id].Invalid_page_count;
					}
				}
				double max_fairness = -1;
				for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
				{
					double min_slowdown = DBL_MAX, max_slowdown = DBL_MIN;
					flash_block_ID_type selected_block_id = block_ids[stream_id];
					Block_Pool_Slot_Type* selected_block = &pbke->Blocks[selected_block_id];
					// caculate gc cost if selected block executes gc
					sim_time_type total_gc_cost = tsu->Get__NVMController()->Get_chip(plane_address.ChannelID, plane_address.ChipID)->Get_command_execution_latency((int)Transaction_Type::ERASE, 0);
					for (flash_page_ID_type pageID = 0; pageID < selected_block->Current_page_write_index; pageID++)
					{
						if (block_manager->Is_page_valid(selected_block, pageID))
						{
							total_gc_cost += tsu->Get__NVMController()->Get_chip(plane_address.ChannelID, plane_address.ChipID)->Get_command_execution_latency((int)Transaction_Type::READ, pageID);
							total_gc_cost += tsu->Get__NVMController()->Get_chip(plane_address.ChannelID, plane_address.ChipID)->Get_command_execution_latency((int)Transaction_Type::WRITE, pageID);
							total_gc_cost += 2 * tsu->Get__NVMController()->Expected_transfer_time(sector_no_per_page * SECTOR_SIZE_IN_BYTE, plane_address.ChannelID);
						}
					}
					// calculate slowdown if selected block executes gc
					for (unsigned int sid = 0; sid < stream_count; ++sid)
					{
						double slowdown = 0;
						if (sid == stream_id)
						{
							slowdown = ((double)tsu->Get_chip_level_shared_time(sid, plane_address.ChannelID, plane_address.ChipID) + total_gc_cost)
								/ (1e-10 + tsu->Get_chip_level_alone_time(sid, plane_address.ChannelID, plane_address.ChipID) + total_gc_cost);
						}
						else
						{
							slowdown = ((double)tsu->Get_chip_level_shared_time(sid, plane_address.ChannelID, plane_address.ChipID) + total_gc_cost)
								/ (1e-10 + tsu->Get_chip_level_alone_time(sid, plane_address.ChannelID, plane_address.ChipID));
						}
						min_slowdown = std::min(min_slowdown, slowdown);
						max_slowdown = std::max(max_slowdown, slowdown);
					}
					// find the most suitable block that can lead to max fairness among all candidated blocks
					if (max_fairness < min_slowdown / (1e-10 + max_slowdown))
					{
						max_fairness = min_slowdown / (1e-10 + max_slowdown);
						gc_candidate_block_id = selected_block_id;
					}
				}
				delete[] block_ids;
				delete[] invalid_page_counts;
				break;
			}
			default:
				break;
		}
		
		if (pbke->Ongoing_erase_operations.find(gc_candidate_block_id) != pbke->Ongoing_erase_operations.end())//This should never happen, but we check it here for safty
			return;
			
		NVM::FlashMemory::Physical_Page_Address gc_candidate_address(plane_address);
		gc_candidate_address.BlockID = gc_candidate_block_id;
		Block_Pool_Slot_Type* block = &pbke->Blocks[gc_candidate_block_id];
		if (block->Current_page_write_index == 0 || block->Invalid_page_count == 0)//No invalid page to erase
			return;
		if (!is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
			return;
		
		std::unordered_map<std::string, float> info = get_gc_info(gc_candidate_address);
		float bip = info["bip"];
		int gt = (int)info["gt"], ut = (int)info["ut"];
		float psd = info["psd"], f = info["f"];
		
		// predict
		// ml
		//if (gc_classifier != NULL && predict(gc_classifier, bip, gt, ut, psd, f) == 0) return;

		// tradition
		/*if (free_block_pool_size >= block_pool_gc_hard_threshold)
		{
			unsigned int stream_count = address_mapping_unit->Get_no_of_input_streams();
			stream_id_type stream_id_min_psd = tsu->stream_with_maximum_slowdown(plane_address.ChannelID, plane_address.ChipID);
			double gap = tsu->estimated_gap(plane_address.ChannelID, plane_address.ChipID, stream_id_min_psd);
			NVM_PHY_ONFI_NVDDR2* controller = tsu->Get__NVMController();
			NVM::FlashMemory::Flash_Chip* flash_chip = controller->Get_chip(plane_address.ChannelID, plane_address.ChipID);
			sim_time_type gc_time = flash_chip->Get_command_execution_latency((int)Transaction_Type::ERASE, 0);
			for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++)
			{
				if (block_manager->Is_page_valid(block, pageID))
				{
					gc_time += flash_chip->Get_command_execution_latency((int)Transaction_Type::READ, pageID);
					gc_time += flash_chip->Get_command_execution_latency((int)Transaction_Type::WRITE, pageID);
					gc_time += 2 * controller->Expected_transfer_time(sector_no_per_page * SECTOR_SIZE_IN_BYTE, plane_address.ChannelID);
				}
			}
			if (gap < gc_time) return;
		}*/
		if (free_block_pool_size >= block_pool_gc_hard_threshold)
		{
			unsigned int stream_count = address_mapping_unit->Get_no_of_input_streams();
			double probability = 1.0;
			size_t total_count = tsu->UserTRQueueSize(plane_address.ChannelID, plane_address.ChipID);
			if (total_count > 0)
			{
				double max_prob = DBL_MIN;
				for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
				{
					double prob = (double)tsu->UserTRQueueSize(stream_id, plane_address.ChannelID, plane_address.ChipID) / total_count;
					max_prob = std::max(max_prob, prob);
				}
				probability = 1.0 - 1.0 / (stream_count * max_prob);
			}
			double prob = (double)(rand() % 10000) / 10000;
			if (prob >= probability) return;
		}

		//Run the state machine to protect against race condition
		block_manager->GC_WL_started(gc_candidate_address);
		pbke->Ongoing_erase_operations.insert(gc_candidate_block_id);
		address_mapping_unit->Set_barrier_for_accessing_physical_block(gc_candidate_address);//Lock the block, so no user request can intervene while the GC is progressing
		//if (block_manager->Can_execute_gc_wl(gc_candidate_address))//If there are ongoing requests targeting the candidate block, the gc execution should be postponed
		tsu->Prepare_for_transaction_submit();

		NVM_Transaction_Flash_ER* gc_erase_tr = new NVM_Transaction_Flash_ER(Transaction_Source_Type::GC_WL, pbke->Blocks[gc_candidate_block_id].Stream_id, gc_candidate_address);
		if (block->Current_page_write_index - block->Invalid_page_count > 0)//If there are some valid pages in block, then prepare flash transactions for page movement
		{
			NVM_Transaction_Flash_RD* gc_read = NULL;
			NVM_Transaction_Flash_WR* gc_write = NULL;
			for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++)
			{
				if (block_manager->Is_page_valid(block, pageID))
				{
					Stats::Total_page_movements_for_gc++;

					gc_candidate_address.PageID = pageID;
					if (use_copyback)
					{
						gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
							NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), NULL, 0, NULL, 0, INVALID_TIME_STAMP);
						gc_write->ExecutionMode = WriteExecutionModeType::COPYBACK;
						tsu->Submit_transaction(gc_write);
					}
					else
					{
						gc_read = new NVM_Transaction_Flash_RD(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
							NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), gc_candidate_address, NULL, 0, NULL, 0, INVALID_TIME_STAMP);
						gc_read->Physical_address_determined = true;
						gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
							NO_LPA, NO_PPA, gc_candidate_address, NULL, 0, gc_read, 0, INVALID_TIME_STAMP);
						gc_write->ExecutionMode = WriteExecutionModeType::SIMPLE;
						gc_write->RelatedErase = gc_erase_tr;
						gc_read->RelatedWrite = gc_write;
						gc_erase_tr->issued_gc_read_write_count += 2;
						tsu->Submit_transaction(gc_read);//Only the read transaction would be submitted. The Write transaction is submitted when the read transaction is finished and the LPA of the target page is determined
					}
					gc_erase_tr->Page_movement_activities.push_back(gc_write);
				}
			}
		}
		gc_erase_tr->Physical_address_determined = true;
		block->Erase_transaction = gc_erase_tr;
		record_gc_info_when_issued(gc_erase_tr);
		tsu->Submit_transaction(gc_erase_tr);
		tsu->Schedule();
	}
}
