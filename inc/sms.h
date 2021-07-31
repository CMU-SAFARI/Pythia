#ifndef SMS_H
#define SMS_H

#include <vector>
#include <deque>
#include "bitmap.h"
#include "prefetcher.h"

using namespace std;

class FTEntry
{
public:
	uint64_t page;
	uint64_t pc;
	uint32_t trigger_offset;

public:
	void reset()
	{
		page = 0xdeadbeef;
		pc = 0xdeadbeef;
		trigger_offset = 0;
	}
	FTEntry(){reset();}
	~FTEntry(){}
};

class ATEntry
{
public:
	uint64_t page;
	uint64_t pc;
	uint32_t trigger_offset;
	Bitmap pattern;
	uint32_t age;

public:
	void reset()
	{
		page = pc = 0xdeadbeef;
		trigger_offset = 0;
		pattern.reset();
		age = 0;
	}
	ATEntry(){reset();}
	~ATEntry(){}
};

class PHTEntry
{
public:
	uint64_t signature;
	Bitmap pattern;
	uint32_t age;

public:
	void reset()
	{
		signature = 0xdeadbeef;
		pattern.reset();
		age = 0;
	}
	PHTEntry(){reset();}
	~PHTEntry(){}
};

class SMSPrefetcher : public Prefetcher
{
private:
	deque<FTEntry*> filter_table;
	deque<ATEntry*> acc_table;
	vector<deque<PHTEntry*> > pht;
	uint32_t pht_sets;
	deque<uint64_t> pref_buffer;

	struct
	{
		struct
		{
			uint64_t lookup;
			uint64_t hit;
			uint64_t insert;
			uint64_t evict;
		} ft;
		struct
		{
			uint64_t lookup;
			uint64_t hit;
			uint64_t insert;
			uint64_t evict;
		} at;
		struct
		{
			uint64_t lookup;
			uint64_t hit;
			uint64_t insert;
			uint64_t evict;
		} pht;
		struct
		{
			uint64_t called;
			uint64_t pht_miss;
			uint64_t pref_generated;
		} generate_prefetch;
		struct
		{
			uint64_t spilled;
			uint64_t buffered;
			uint64_t issued;
		} pref_buffer;

	} stats;

private:
	void init_knobs();
	void init_stats();

	deque<FTEntry*>::iterator search_filter_table(uint64_t page);
	deque<FTEntry*>::iterator search_victim_filter_table();
	void evict_filter_table(deque<FTEntry*>::iterator victim);
	void insert_filter_table(uint64_t pc, uint64_t page, uint32_t offset);

	deque<ATEntry*>::iterator search_acc_table(uint64_t page);
	deque<ATEntry*>::iterator search_victim_acc_table();
	void evict_acc_table(deque<ATEntry*>::iterator victim);
	void update_age_acc_table(deque<ATEntry*>::iterator current);
	void insert_acc_table(FTEntry *ftentry, uint32_t offset);
	
	deque<PHTEntry*>::iterator search_pht(uint64_t signature, int32_t *set);
	deque<PHTEntry*>::iterator search_victim_pht(int32_t set);
	void evcit_pht(int32_t set, deque<PHTEntry*>::iterator victim);
	void update_age_pht(int32_t set, deque<PHTEntry*>::iterator current);
	void insert_pht_table(ATEntry *atentry);

	uint64_t create_signature(uint64_t pc, uint32_t offset);
	int generate_prefetch(uint64_t pc, uint64_t address, uint64_t page, uint32_t offset, vector<uint64_t> &pref_addr);
	void buffer_prefetch(vector<uint64_t> pref_addr);
	void issue_prefetch(vector<uint64_t> &pref_addr);

public:
	SMSPrefetcher(string type);
	~SMSPrefetcher();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
	void dump_stats();
	void print_config();
};

#endif /* SMS_H */