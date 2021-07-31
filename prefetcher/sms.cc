#include <iostream>
#include <math.h>
#include <algorithm>
#include <iomanip>
#include "sms.h"
#include "champsim.h"

using namespace std;

namespace knob
{
	extern uint32_t sms_at_size;
	extern uint32_t sms_ft_size;
	extern uint32_t sms_pht_size;
	extern uint32_t sms_pht_assoc;
	extern uint32_t sms_pref_degree;
	extern uint32_t sms_region_size;
	extern uint32_t sms_region_size_log;
	extern bool 	sms_enable_pref_buffer;
	extern uint32_t sms_pref_buffer_size;
}

void SMSPrefetcher::init_knobs()
{
	pht_sets = knob::sms_pht_size/knob::sms_pht_assoc;
}

void SMSPrefetcher::init_stats()
{
	bzero(&stats, sizeof(stats));
}

void SMSPrefetcher::print_config()
{
	cout << "sms_at_size " << knob::sms_at_size << endl
		<< "sms_ft_size " << knob::sms_ft_size << endl
		<< "sms_pht_size " << knob::sms_pht_size << endl
		<< "sms_pht_assoc " << knob::sms_pht_assoc << endl
		<< "sms_pref_degree " << knob::sms_pref_degree << endl
		<< "sms_region_size " << knob::sms_region_size << endl
		<< "sms_region_size_log " << knob::sms_region_size_log << endl
		<< "sms_enable_pref_buffer " << knob::sms_enable_pref_buffer << endl
		<< "sms_pref_buffer_size " << knob::sms_pref_buffer_size << endl
		<< endl;
}

SMSPrefetcher::SMSPrefetcher(string type) : Prefetcher(type)
{
	init_knobs();
	init_stats();
	print_config();

	/* init PHT */
	deque<PHTEntry*> d;
	pht.resize(pht_sets, d);
}

SMSPrefetcher::~SMSPrefetcher()
{

}

void SMSPrefetcher::invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr)
{
	uint64_t page = address >> knob::sms_region_size_log;
	uint32_t offset = (address >> LOG2_BLOCK_SIZE) & ((1ull << (knob::sms_region_size_log - LOG2_BLOCK_SIZE)) - 1);

	// cout << "pc " << hex << setw(16) << pc
	// 	<< " address " << hex << setw(16) << address
	// 	<< " page " << hex << setw(16) << page
	// 	<< " offset " << dec << setw(2) << offset
	// 	<< endl;

	auto at_index = search_acc_table(page);
	stats.at.lookup++;
	if(at_index != acc_table.end())
	{
		/* accumulation table hit */
		stats.at.hit++;
		(*at_index)->pattern[offset] = 1;
		update_age_acc_table(at_index);
	}
	else
	{
		/* search filter table */
		auto ft_index = search_filter_table(page);
		stats.ft.lookup++;
		if(ft_index != filter_table.end())
		{
			/* filter table hit */
			stats.ft.hit++;
			insert_acc_table((*ft_index), offset);
			evict_filter_table(ft_index);
		}
		else
		{
			/* filter table miss. Beginning of new generation. Issue prefetch */
			insert_filter_table(pc, page, offset);
			generate_prefetch(pc, address, page, offset, pref_addr);
			if(knob::sms_enable_pref_buffer)
			{
				buffer_prefetch(pref_addr);
				pref_addr.clear();
			}
		}
	}

	/* slowly inject prefetches at every demand access, if buffer is turned on */
	if(knob::sms_enable_pref_buffer)
	{
		issue_prefetch(pref_addr);
	}
}

/* Functions for Filter table */
deque<FTEntry*>::iterator SMSPrefetcher::search_filter_table(uint64_t page)
{
	return find_if(filter_table.begin(), filter_table.end(), [page](FTEntry *ftentry){return (ftentry->page == page);});
}

void SMSPrefetcher::insert_filter_table(uint64_t pc, uint64_t page, uint32_t offset)
{
	stats.ft.insert++;
	FTEntry *ftentry = NULL;
	if(filter_table.size() >= knob::sms_ft_size)
	{
		auto victim = search_victim_filter_table();
		evict_filter_table(victim);
	}

	ftentry = new FTEntry();
	ftentry->page = page;
	ftentry->pc = pc;
	ftentry->trigger_offset = offset;
	filter_table.push_back(ftentry);
}

deque<FTEntry*>::iterator SMSPrefetcher::search_victim_filter_table()
{
	return filter_table.begin();
}

void SMSPrefetcher::evict_filter_table(deque<FTEntry*>::iterator victim)
{
	stats.ft.evict++;
	FTEntry *ftentry = (*victim);
	filter_table.erase(victim);
	delete ftentry;
}

/* Functions for Accumulation Table */
deque<ATEntry*>::iterator SMSPrefetcher::search_acc_table(uint64_t page)
{
	return find_if(acc_table.begin(), acc_table.end(), [page](ATEntry *atentry){return (atentry->page == page);});	
}

void SMSPrefetcher::insert_acc_table(FTEntry *ftentry, uint32_t offset)
{
	stats.at.insert++;
	ATEntry *atentry = NULL;
	if(acc_table.size() >= knob::sms_at_size)
	{
		auto victim = search_victim_acc_table();
		evict_acc_table(victim);
	}

	atentry = new ATEntry();
	atentry->pc = ftentry->pc;
	atentry->page = ftentry->page;
	atentry->trigger_offset = ftentry->trigger_offset;
	atentry->pattern[ftentry->trigger_offset] = 1;
	atentry->pattern[offset] = 1;
	atentry->age = 0;
	for(uint32_t index = 0; index < acc_table.size(); ++index) acc_table[index]->age++;
	acc_table.push_back(atentry);
}

deque<ATEntry*>::iterator SMSPrefetcher::search_victim_acc_table()
{
	uint32_t max_age = 0;
	deque<ATEntry*>::iterator it, victim;
	for(it = acc_table.begin(); it != acc_table.end(); ++it)
	{
		if((*it)->age >= max_age)
		{
			max_age	 = (*it)->age;
			victim = it;
		}
	}
	return victim;
}

void SMSPrefetcher::evict_acc_table(deque<ATEntry*>::iterator victim)
{
	stats.at.evict++;
	ATEntry *atentry = (*victim);
	insert_pht_table(atentry);
	acc_table.erase(victim);

	// cout << "[PHT_INSERT] pc " << hex << setw(10) << atentry->pc
	// 	<< " page " << hex << setw(10) << atentry->page
	// 	<< " offset " << dec << setw(3) << atentry->trigger_offset
	// 	<< " pattern " << BitmapHelper::to_string(atentry->pattern)
	// 	<< endl;

	delete(atentry);
}

void SMSPrefetcher::update_age_acc_table(deque<ATEntry*>::iterator current)
{
	for(auto it = acc_table.begin(); it != acc_table.end(); ++it)
	{
		(*it)->age++;
	}
	(*current)->age = 0;
}

/* Functions for Pattern History Table */
void SMSPrefetcher::insert_pht_table(ATEntry *atentry)
{
	stats.pht.lookup++;
	uint64_t signature = create_signature(atentry->pc, atentry->trigger_offset);

	// cout << "signature " << hex << setw(20) << signature << dec
	// 	<< " pattern " << BitmapHelper::to_string(atentry->pattern)
	// 	<< endl;

	int32_t set = -1;
	auto pht_index = search_pht(signature, &set);
	assert(set != -1);
	if(pht_index != pht[set].end())
	{
		/* PHT hit */
		stats.pht.hit++;
		(*pht_index)->pattern = atentry->pattern;
		update_age_pht(set, pht_index);
	}
	else
	{
		/* PHT miss */
		assert(set != -1);
		if(pht[set].size() >= knob::sms_pht_assoc)
		{
			auto victim = search_victim_pht(set);
			evcit_pht(set, victim);
		}

		stats.pht.insert++;
		PHTEntry *phtentry = new PHTEntry();
		phtentry->signature = signature;
		phtentry->pattern = atentry->pattern;
		phtentry->age = 0;
		for(uint32_t index = 0; index < pht[set].size(); ++index) pht[set][index]->age = 0;
		pht[set].push_back(phtentry);
	}
}

deque<PHTEntry*>::iterator SMSPrefetcher::search_pht(uint64_t signature, int32_t *set)
{
	(*set) = signature % pht_sets;
	return find_if(pht[(*set)].begin(), pht[(*set)].end(), [signature](PHTEntry *phtentry){return (phtentry->signature == signature);});	
}

deque<PHTEntry*>::iterator SMSPrefetcher::search_victim_pht(int32_t set)
{
	uint32_t max_age = 0;
	deque<PHTEntry*>::iterator it, victim;
	for(it = pht[set].begin(); it != pht[set].end(); ++it)
	{
		if((*it)->age >= max_age)
		{
			max_age	 = (*it)->age;
			victim = it;
		}
	}
	return victim;
}

void SMSPrefetcher::update_age_pht(int32_t set, deque<PHTEntry*>::iterator current)
{
	for(auto it = pht[set].begin(); it != pht[set].end(); ++it)
	{
		(*it)->age++;
	}
	(*current)->age = 0;
}

void SMSPrefetcher::evcit_pht(int32_t set, deque<PHTEntry*>::iterator victim)
{
	stats.pht.evict++;
	PHTEntry *phtentry = (*victim);
	pht[set].erase(victim);
	delete phtentry;
}

uint64_t SMSPrefetcher::create_signature(uint64_t pc, uint32_t offset)
{
	uint64_t signature = pc;
	signature = (signature << (knob::sms_region_size_log - LOG2_BLOCK_SIZE));
	signature += (uint64_t)offset;
	return signature;
}

int SMSPrefetcher::generate_prefetch(uint64_t pc, uint64_t address, uint64_t page, uint32_t offset, vector<uint64_t> &pref_addr)
{
	stats.generate_prefetch.called++;
	uint64_t signature = create_signature(pc, offset);
	int32_t set = -1;
	auto pht_index = search_pht(signature, &set);
	assert(set != -1);
	if(pht_index == pht[set].end())
	{
		stats.generate_prefetch.pht_miss++;
		return 0;
	}

	PHTEntry *phtentry = (*pht_index);
	for(uint32_t index = 0; index < BITMAP_MAX_SIZE; ++index)
	{
		if(phtentry->pattern[index] && offset != index)
		{
			uint64_t addr = (page << knob::sms_region_size_log) + (index << LOG2_BLOCK_SIZE);
			pref_addr.push_back(addr);
		}
	}
	update_age_pht(set, pht_index);
	stats.generate_prefetch.pref_generated += pref_addr.size();
	return pref_addr.size();
}

void SMSPrefetcher::buffer_prefetch(vector<uint64_t> pref_addr)
{
	// cout << "buffering " << pref_addr.size() << " already present " << pref_buffer.size() << endl;
	uint32_t count = 0;
	for(uint32_t index = 0; index < pref_addr.size(); ++index)
	{
		if(pref_buffer.size() >= knob::sms_pref_buffer_size)
		{
			break;
		}
		pref_buffer.push_back(pref_addr[index]);
		count++;
	}
	stats.pref_buffer.buffered += count;
	stats.pref_buffer.spilled += (pref_addr.size() - count);
}

void SMSPrefetcher::issue_prefetch(vector<uint64_t> &pref_addr)
{
	uint32_t count = 0;
	while(!pref_buffer.empty() && count < knob::sms_pref_degree)
	{
		pref_addr.push_back(pref_buffer.front());
		pref_buffer.pop_front();
		count++;
	}
	stats.pref_buffer.issued += pref_addr.size();
}

void SMSPrefetcher::dump_stats()
{
	cout << "sms.ft.lookup " << stats.ft.lookup << endl
	<< "sms.ft.hit " << stats.ft.hit << endl
	<< "sms.ft.insert " << stats.ft.insert << endl
	<< "sms.ft.evict " << stats.ft.evict << endl
	<< "sms.at.lookup " << stats.at.lookup << endl
	<< "sms.at.hit " << stats.at.hit << endl
	<< "sms.at.insert " << stats.at.insert << endl
	<< "sms.at.evict " << stats.at.evict << endl
	<< "sms.pht.lookup " << stats.pht.lookup << endl
	<< "sms.pht.hit " << stats.pht.hit << endl
	<< "sms.pht.insert " << stats.pht.insert << endl
	<< "sms.pht.evict " << stats.pht.evict << endl
	<< "sms.generate_prefetch.called " << stats.generate_prefetch.called << endl
	<< "sms.generate_prefetch.pht_miss " << stats.generate_prefetch.pht_miss << endl
	<< "sms.generate_prefetch.pref_generated " << stats.generate_prefetch.pref_generated << endl
	<< "sms.pref_buffer.buffered " << stats.pref_buffer.buffered << endl
	<< "sms.pref_buffer.spilled " << stats.pref_buffer.spilled << endl
	<< "sms.pref_buffer.issued " << stats.pref_buffer.issued << endl
	<< endl;
}
