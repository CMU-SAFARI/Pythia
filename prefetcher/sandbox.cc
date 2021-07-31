/* DISCLAIMER: **NOT** the most-efficient optimization of Sandbox prefetcher, Pugsley+ HPCA 2014 */

#include <algorithm>
#include <stdlib.h>
#include <cmath>
#include "sandbox.h"
#include "champsim.h"

namespace knob
{
	extern uint32_t sandbox_pref_degree;
	extern bool     sandbox_enable_stream_detect;
	extern uint32_t sandbox_stream_detect_length;
	extern uint32_t sandbox_num_access_in_phase;
	extern uint32_t sandbox_num_cycle_offsets;
	extern uint32_t sandbox_bloom_filter_size;
	extern uint32_t sandbox_seed;
}

void SandboxPrefetcher::init_knobs()
{
	pref_degree = knob::sandbox_pref_degree; /* minimum prefetch degree is 4 */
}

void SandboxPrefetcher::init_stats()
{
	bzero(&stats, sizeof(stats));
}

SandboxPrefetcher::SandboxPrefetcher(string type) : Prefetcher(type)
{
	init_knobs();
	init_stats();

	/* init bloom filter */
	/* for a given m (bloom filter bit array length) and n (number of elements to be inserted)
	 * the k (number of hash functions) to produce minimal false +ve rate would be m/n*ln(2)
	 * https://en.wikipedia.org/wiki/Bloom_filter#Probability_of_false_positives */
	opt_hash_functions = (uint32_t)ceil(knob::sandbox_bloom_filter_size / knob::sandbox_num_access_in_phase * log(2));
	bf = new bf::basic_bloom_filter(bf::make_hasher(opt_hash_functions, knob::sandbox_seed, true), knob::sandbox_bloom_filter_size);
	assert(bf);

	init_evaluated_offsets();
	init_non_evaluated_offsets();
	reset_eval();
}

SandboxPrefetcher::~SandboxPrefetcher()
{

}

void SandboxPrefetcher::print_config()
{
	cout << "sandbox_pref_degree " << knob::sandbox_pref_degree << endl
		<< "sandbox_enable_stream_detect " << knob::sandbox_enable_stream_detect << endl
		<< "sandbox_stream_detect_length " << knob::sandbox_stream_detect_length << endl
		<< "sandbox_num_access_in_phase " << knob::sandbox_num_access_in_phase << endl
		<< "sandbox_num_cycle_offsets " << knob::sandbox_num_cycle_offsets << endl
		<< "sandbox_bloom_filter_size " << knob::sandbox_bloom_filter_size << endl
		<< "sandbox_bloom_filter_hash_functions " << opt_hash_functions << endl
		<< "sandbox_seed " << knob::sandbox_seed << endl
		;
}

void SandboxPrefetcher::init_evaluated_offsets()
{
	/* select {-8,-1} and {+1,+8} offsets in the beginning */
	for(int32_t index = 1; index <= 8; ++index)
	{
		Score *score = new Score(index);
		evaluated_offsets.push_back(score);
	}
	for(int32_t index = -8; index <= -1; ++index)
	{
		Score *score = new Score(index);
		evaluated_offsets.push_back(score);
	}
}

void SandboxPrefetcher::init_non_evaluated_offsets()
{
	for(int32_t index = 9; index <= 16; ++index)
	{
		non_evaluated_offsets.push_back(index);
	}
	for(int32_t index = -16; index <= -9; ++index)
	{
		non_evaluated_offsets.push_back(index);
	}
}

void SandboxPrefetcher::reset_eval()
{
	eval.curr_ptr = 0;
	eval.total_demand = 0;
	eval.filter_hit = 0;
	bf->clear();
}

void SandboxPrefetcher::invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr)
{
	uint64_t page = address >> LOG2_PAGE_SIZE;
	uint32_t offset = (address >> LOG2_BLOCK_SIZE) & ((1ull << (LOG2_PAGE_SIZE - LOG2_BLOCK_SIZE)) - 1);

	assert(evaluated_offsets.size() == 16);
	assert(eval.curr_ptr >= 0 && eval.curr_ptr < evaluated_offsets.size());

	stats.called++;

	/* ======================  Four step process  ======================
	 * Step 1: check demand access in bloom filter to manipulate score.
	 * Step 2: generate pseudo prefetch request and add to bloom filter.
	 * Step 3: check the end of current phase or the end of current round. 
	 * Step 4: generate actual prefetch reuqests based on the scores. */

	/* Step 1: check demand access in bloom filter to manipulate score */
	eval.total_demand++;
	stats.step1.filter_lookup++;
	bool lookup = filter_lookup(address);
	if(lookup)
	{
		stats.step1.filter_hit++;
		eval.filter_hit++;
		/* increment score */
		evaluated_offsets[eval.curr_ptr]->score++;
		if(knob::sandbox_enable_stream_detect)
		{
			/* RBERA: TODO */
			for(uint32_t index = 1; index <= knob::sandbox_stream_detect_length; ++index)
			{
				int32_t stream_offset = offset - (evaluated_offsets[eval.curr_ptr]->offset * index);
				if(stream_offset >= 0 && stream_offset < 64)
				{
					uint64_t stream_addr = (page << LOG2_PAGE_SIZE) + (stream_offset << LOG2_BLOCK_SIZE);
					if(filter_lookup(stream_addr))
					{
						evaluated_offsets[eval.curr_ptr]->score++;
					}
				}
			}
		}
	}

	/* Step 2: generate pseudo prefetch request and add to bloom filter */
	uint32_t pref_offset = offset + evaluated_offsets[eval.curr_ptr]->offset;
	if(pref_offset >= 0 && pref_offset < 64)
	{
		uint64_t pseudo_pref_addr = (page << LOG2_PAGE_SIZE) + (pref_offset << LOG2_BLOCK_SIZE);
		filter_add(pseudo_pref_addr);
		stats.step2.filter_add++;
	}

	/* Step 3: check the end of current phase or the end of current round */
	/* Phase end := the current offset has been evaluated for a number of cache accesses
	 * Round end := all 16 offsets has been evaluated */
	if(eval.total_demand == knob::sandbox_num_access_in_phase)
	{
		stats.step3.end_of_phase++;
		uint32_t next_ptr = eval.curr_ptr + 1;
		if(next_ptr < evaluated_offsets.size())
		{
			reset_eval();
			eval.curr_ptr = next_ptr;
		}
		else
		{
			/* end of current round */
			stats.step3.end_of_round++;
			end_of_round();
			reset_eval();
			eval.curr_ptr = 0;
		}
	}

	/* Step 4: generate actual prefetch reuqests based on the scores */
	vector<Score*> pos_offsets, neg_offsets;
	/* Following function does two job:
	 * 1. Creates two separate lists of Scores based on posetive and negative offsets
	 * 2. Sorts both offset lists based on ABSOLUTE offset value, as Snadbox prefers smaller offsets for prefetching */
	get_offset_list_sorted(pos_offsets, neg_offsets);
	/* generate prefetches */
	generate_prefetch(pos_offsets, pref_degree, page, offset, pref_addr);
	uint32_t pos_pref = pref_addr.size();
	generate_prefetch(neg_offsets, pref_degree, page, offset, pref_addr);
	uint32_t neg_pref = pref_addr.size() - pos_pref;
	/* destroy temporary lists */
	destroy_offset_list(pos_offsets);
	destroy_offset_list(neg_offsets);

	stats.step4.pref_generated += pref_addr.size();
	stats.step4.pref_generated_pos += pos_pref;
	stats.step4.pref_generated_neg += neg_pref;
}

bool sort_function(Score *score1, Score *score2)
{
	return (abs(score1->offset) < abs(score2->offset));
}

void SandboxPrefetcher::get_offset_list_sorted(vector<Score*> &pos_offsets, vector<Score*> &neg_offsets)
{
	for(uint32_t index = 0; index < evaluated_offsets.size(); ++index)
	{
		assert(evaluated_offsets[index]->offset != 0);
		if(evaluated_offsets[index]->offset > 0)
		{
			pos_offsets.push_back(new Score(evaluated_offsets[index]->offset, evaluated_offsets[index]->score));
		}
		else
		{
			neg_offsets.push_back(new Score(evaluated_offsets[index]->offset, evaluated_offsets[index]->score));			
		}
	}

	std::sort(pos_offsets.begin(), pos_offsets.end(), [](Score *score1, Score *score2){return abs(score1->offset) < abs(score2->offset);});
	std::sort(neg_offsets.begin(), neg_offsets.end(), [](Score *score1, Score *score2){return abs(score1->offset) < abs(score2->offset);});
}

void SandboxPrefetcher::generate_prefetch(vector<Score*> offset_list, uint32_t pref_degree, uint64_t page, uint32_t offset, vector<uint64_t> &pref_addr)
{
	uint32_t count = 0;
	for(uint32_t index = 0; index < offset_list.size(); ++index)
	{
		for(uint32_t lookahead = 1; lookahead <= knob::sandbox_stream_detect_length+1; ++lookahead)
		{
			if(lookahead > 1 && !knob::sandbox_enable_stream_detect)
			{
				break;
			}

			if(offset_list[index]->score >= lookahead*knob::sandbox_num_access_in_phase)
			{
				uint64_t addr = generate_address(page, offset, offset_list[index]->offset, lookahead);
				if(addr != 0xdeadbeef)
				{
					pref_addr.push_back(addr);
					count++;
					record_pref_stats(offset_list[index]->offset, 1);
				}
			}
		}
		
		if(count > pref_degree)
		{
			break;
		}
	}
}

void SandboxPrefetcher::destroy_offset_list(vector<Score*> offset_list)
{
	for(uint32_t index = 0; index < offset_list.size(); ++index)
	{
		delete offset_list[index];
	}
}

uint64_t SandboxPrefetcher::generate_address(uint64_t page, uint32_t offset, int32_t delta, uint32_t lookahead)
{
	int32_t pref_offset = offset + delta * lookahead;
	if(pref_offset >= 0 && pref_offset < 64)
	{
		return ((page << LOG2_PAGE_SIZE) + (pref_offset << LOG2_BLOCK_SIZE));
	}
	else
	{
		return 0xdeadbeef;
	}
}

void SandboxPrefetcher::end_of_round()
{
	/* sort evaluated offset list based on score */
	std::sort(evaluated_offsets.begin(), evaluated_offsets.end(), [](Score *score1, Score *score2){return score1->score > score2->score;});

	/* cycle-out n lowest performing offsets */
	for(uint32_t count = 0; count < knob::sandbox_num_cycle_offsets; ++count)
	{
		if(evaluated_offsets.empty())
		{
			break;
		}
		Score *score = evaluated_offsets.back();
		evaluated_offsets.pop_back();
		non_evaluated_offsets.push_back(score->offset);
		delete score;
	}

	/* cycle-in next n non_evaluated_offsets */
	for(uint32_t count = 0; count < knob::sandbox_num_cycle_offsets; ++count)
	{
		int32_t offset = non_evaluated_offsets.front();
		non_evaluated_offsets.pop_front();
		Score *score = new Score(offset);
		evaluated_offsets.push_back(score);
	}
}

void SandboxPrefetcher::filter_add(uint64_t address)
{
	bf->add(address);
}

bool SandboxPrefetcher::filter_lookup(uint64_t address)
{
	return bf->lookup(address);
}

void SandboxPrefetcher::record_pref_stats(int32_t offset, uint32_t pref_count)
{
	uint32_t index = offset > 0 ? offset : 64 + abs(offset);
	stats.pref_delta_dist[index] += pref_count;
}

void SandboxPrefetcher::dump_stats()
{
	cout << "sandbox_called " << stats.called << endl
		<< "sandbox_step1_filter_lookup " << stats.step1.filter_lookup << endl
		<< "sandbox_step1_filter_hit " << stats.step1.filter_hit << endl
		<< "sandbox_step2_filter_add " << stats.step2.filter_add << endl
		<< "sandbox_step3_end_of_phase " << stats.step3.end_of_phase << endl
		<< "sandbox_step3_end_of_round " << stats.step3.end_of_round << endl
		<< "sandbox_step4_pref_generated " << stats.step4.pref_generated << endl
		<< "sandbox_step4_pref_generated_pos " << stats.step4.pref_generated_pos << endl
		<< "sandbox_step4_pref_generated_neg " << stats.step4.pref_generated_neg << endl
		<< endl;

	for(uint32_t index = 0; index < 128; ++index)
	{
		if(!stats.pref_delta_dist[index])
		{
			continue;
		}
		cout << "sandbox_offset_";
		if(index >= 64)
		{
			cout << (int32_t)(64 - index);
		}
		else
		{
			cout << index;
		}
		cout << " " << stats.pref_delta_dist[index] << endl;
	}
	cout << endl;
}