#ifndef BOP_H
#define BOP_H

#include <vector>
#include <deque>
#include "prefetcher.h"
using namespace std;

class BOPrefetcher : public Prefetcher
{
private:
	deque<uint64_t> rr;
	vector<uint32_t> scores;
	deque<uint64_t> pref_buffer;

	uint32_t round_counter;
	uint32_t candidate_ptr;
	vector<int32_t> best_offsets;

	struct
	{
		struct
		{
			uint64_t max_round;
			uint64_t max_score;
		} end_phase;

		struct
		{
			uint64_t called;
			uint64_t insert_rr;
		} fill;

		struct
		{
			uint64_t hit;
			uint64_t evict;
			uint64_t insert;
		} insert_rr;

		struct
		{
			uint64_t buffered;
			uint64_t spilled;
			uint64_t issued;
		} pref_buffer;

		uint64_t total_phases;
		uint64_t pref_issued;
	} stats;

private:
	void init_knobs();
	void init_stats();

	bool check_end_of_phase();
	void phase_end();
	bool search_rr(uint64_t address);
	void buffer_prefetch(vector<uint64_t> pref_addr);
	void issue_prefetch(vector<uint64_t> &pref_addr);
	void insert_rr(uint64_t address);

public:
	BOPrefetcher(string type);
	~BOPrefetcher();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
	void register_fill(uint64_t address);
	void dump_stats();
	void print_config();
};

#endif /* BOP_H */

