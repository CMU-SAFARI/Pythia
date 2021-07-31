#ifndef NEXT_LINE
#define NEXT_LINE

#include <deque>
#include <random>
#include "prefetcher.h"
using namespace std;

#define MAX_DELTAS 16

class NL_PTEntry
{
public:
	uint64_t address;
	bool fill;
	int32_t timely;

	NL_PTEntry(uint64_t addr, bool f) : address(addr), fill(f), timely(-1) {}
	~NL_PTEntry(){}
};

class NextLinePrefetcher : public Prefetcher
{
private:
	deque<NL_PTEntry*> prefetch_tracker;
	vector<float> delta_probability;
	default_random_engine generator;
	uniform_real_distribution<float> *deltagen;

	uint64_t trace_timestamp;
	uint32_t trace_interval;
	FILE *trace;

	struct
	{
		struct
		{
			uint64_t select[MAX_DELTAS];
			uint64_t issue[MAX_DELTAS];
			uint64_t out_of_bounds[MAX_DELTAS];
			uint64_t tracker_hit[MAX_DELTAS];
		} predict;

		struct
		{
			uint64_t called;
			uint64_t pt_miss;
			uint64_t evict;
			uint64_t insert;
			uint64_t pt_hit;
		} track;

		struct
		{
			uint64_t called;
			uint64_t fill;
		} register_fill;

		struct
		{
			uint64_t called;
			uint64_t hit;
			uint64_t timely;
			uint64_t untimely;
		} record_demand;

		struct
		{
			uint64_t total;
			uint64_t timely;
			uint64_t untimely;
			uint64_t incorrect;
		} pref;

	} stats;

private:
	void init_knobs();
	void init_stats();
	bool track(uint64_t address);
	NL_PTEntry* search_pt(uint64_t address);
	void measure_stats(NL_PTEntry *ptentry);
	void record_demand(uint64_t address);
	uint32_t gen_delta();

public:
	NextLinePrefetcher(string type);
	~NextLinePrefetcher();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
	void register_fill(uint64_t address);
	void dump_stats();
	void print_config();
};


#endif /* NEXT_LINE */

