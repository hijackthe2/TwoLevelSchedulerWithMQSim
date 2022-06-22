#include "TSU_ChipLevelBuffer.h"

namespace SSD_Components
{
	TSU_ChipLevelBuffer::TSU_ChipLevelBuffer(unsigned int _channel_count, unsigned int _chip_count_per_channel, unsigned int _stream_count,
		const unsigned int _max_limit): channel_count(_channel_count), chip_count_per_channel(_chip_count_per_channel), stream_count(_stream_count),
		max_limit(_max_limit)
	{
		buffer = new Flash_Transaction_Queue * *[channel_count];
		global_rra = new NVM::FlashMemory::Physical_Page_Address * [channel_count];
		for (unsigned int channel_id = 0; channel_id < channel_count; ++channel_id)
		{
			buffer[channel_id] = new Flash_Transaction_Queue * [chip_count_per_channel];
			global_rra[channel_id] = new NVM::FlashMemory::Physical_Page_Address[chip_count_per_channel];
			for (unsigned int chip_id = 0; chip_id < chip_count_per_channel; ++chip_id)
			{
				buffer[channel_id][chip_id] = new Flash_Transaction_Queue[stream_count];
				global_rra[channel_id][chip_id].DieID = 0;
				global_rra[channel_id][chip_id].PlaneID = 0;
			}
		}
	}
	TSU_ChipLevelBuffer::~TSU_ChipLevelBuffer()
	{
		for (unsigned int channel_id = 0; channel_id < channel_count; ++channel_id)
		{
			for (unsigned int chip_id = 0; chip_id < chip_count_per_channel; ++chip_id)
			{
				delete[] buffer[channel_id][chip_id];
			}
			delete[] buffer[channel_id];
			delete[] global_rra[channel_id];
		}
		delete[] buffer;
		delete[] global_rra;
	}
	void TSU_ChipLevelBuffer::check_in(NVM_Transaction_Flash* transaction)
	{
		buffer[transaction->Address.ChannelID][transaction->Address.ChipID][transaction->Stream_id].push_back(transaction);
	}
	void TSU_ChipLevelBuffer::check_out(Address_Mapping_Unit_Base* amu, TSU_Base* tsu, std::list<Flash_Transaction_Queue*>& queue_list,
		Flash_Transaction_Queue* queue, Flash_Transaction_Queue* related_queue, flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		auto allocate_DP = [&](NVM_Transaction_Flash* transaction) {
			//round robin
			auto& dp_rra = global_rra[channel_id][chip_id];
			transaction->Address.DieID = dp_rra.DieID;
			transaction->Address.PlaneID = dp_rra.PlaneID;
			dp_rra.DieID = (dp_rra.DieID + 1) % 2;
			if (dp_rra.DieID == 0)
			{
				dp_rra.PlaneID = (dp_rra.PlaneID + 1) % 2;
			}
			// static
			//transaction->Address.DieID = (transaction->LPA / 32) % 2;
			//transaction->Address.PlaneID = (transaction->LPA / 64) % 2;
		};


		Flash_Transaction_Queue* buf = buffer[channel_id][chip_id];
		// find buffers which buffer transactions
		size_t total_buffer_size = 0;
		std::vector<stream_id_type> ids;
		for (stream_id_type stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (!buf[stream_id].empty())
			{
				ids.emplace_back(stream_id);
				total_buffer_size += buf[stream_id].size();
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
			while (!buf[stream_id].empty() && queue->size() < max_limit)
			{
				NVM_Transaction_Flash* transaction = buf[stream_id].front();
				//allocate_DP(transaction);
				tsu->queue_insertion(transaction);
				buf[stream_id].pop_front();
				//// allocate DP
				//NVM_Transaction_Flash* transaction = buf[stream_id].front(); buf[stream_id].pop_front();
				//if (!transaction->Physical_address_determined)
				//{
				//	if (transaction->Type != Transaction_Type::WRITE)
				//	{
				//		PRINT_ERROR("check_out get a wrong read which hasn't been allocated!")
				//	}
				//	amu->allocate_DP_for_write(transaction);
				//	if (((NVM_Transaction_Flash_WR*)transaction)->RelatedRead)
				//	{
				//		tsu->queue_insertion(((NVM_Transaction_Flash_WR*)transaction)->RelatedRead);
				//	}
				//}
				//if (transaction->Physical_address_determined)
				//{
				//	tsu->queue_insertion(transaction);
				//}
			}
			queue_list.emplace_back(queue);
			if (!buf[stream_id].empty())
			{
				queue_list.emplace_back(&buf[stream_id]);
			}
			return;
		}
		// calculate probability by buffer size
		std::list<std::pair<double, stream_id_type>> probs;
		for (const stream_id_type& stream_id : ids)
		{
			double prob = (1 - (double)buf[stream_id].size() / total_buffer_size) / ((double)ids.size() - 1);
			probs.emplace_back(prob, stream_id);
		}
		// sort by descending order according to probability
		probs.sort([](const std::pair<double, stream_id_type>& a, const std::pair<double, stream_id_type>& b)
			{
				return a.first > b.first;
			});
		// dispatch transaction according to probability
		double vanished_prob = 0;
		long precision = 10000;
		while (!probs.empty() && queue->size() < max_limit)
		{
			//double prob = (double)(rand() % (long)(vanished_prob * precision)) / precision;
			double prob = (double)(rand() % precision) / precision - vanished_prob;
			for (auto it = probs.begin(); it != probs.end(); ++it)
			{
				if (prob <= it->first)
				{
					NVM_Transaction_Flash* transaction = buf[it->second].front();
					//allocate_DP(transaction);
					tsu->queue_insertion(transaction);
					buf[it->second].pop_front();
					//// allocate DP
					//NVM_Transaction_Flash* transaction = buf[it->second].front(); buf[it->second].pop_front();
					//if (!transaction->Physical_address_determined)
					//{
					//	if (transaction->Type != Transaction_Type::WRITE)
					//	{
					//		PRINT_ERROR("check_out get a wrong read which hasn't been allocated!")
					//	}
					//	amu->allocate_DP_for_write(transaction);
					//	if (((NVM_Transaction_Flash_WR*)transaction)->RelatedRead)
					//	{
					//		tsu->queue_insertion(((NVM_Transaction_Flash_WR*)transaction)->RelatedRead);
					//	}
					//}
					//if (transaction->Physical_address_determined)
					//{
					//	tsu->queue_insertion(transaction);
					//}
					// deal with the situation where buffer may become empty during dipatching
					if (buf[it->second].empty())
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
			queue_list.emplace_back(&buf[prob.second]);
		}
	}
	NVM_Transaction_Flash* TSU_ChipLevelBuffer::someone(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (!buffer[channel_id][chip_id][stream_id].empty())
				return buffer[channel_id][chip_id][stream_id].front();
		}
		return NULL;
	}
	bool TSU_ChipLevelBuffer::empty(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			if (!buffer[channel_id][chip_id][stream_id].empty())
				return false;
		}
		return true;
	}
	size_t TSU_ChipLevelBuffer::size(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id)
	{
		size_t total_size = 0;
		for (unsigned int stream_id = 0; stream_id < stream_count; ++stream_id)
		{
			total_size += buffer[channel_id][chip_id][stream_id].size();
		}
		return total_size;
	}
	size_t TSU_ChipLevelBuffer::size(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id, stream_id_type stream_id)
	{
		return buffer[channel_id][chip_id][stream_id].size();
	}
	Flash_Transaction_Queue* TSU_ChipLevelBuffer::somebuffer(flash_channel_ID_type channel_id, flash_chip_ID_type chip_id, stream_id_type stream_id)
	{
		return &buffer[channel_id][chip_id][stream_id];
	}
}