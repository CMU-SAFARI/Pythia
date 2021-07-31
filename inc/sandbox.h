#ifndef SANDBOX_PREFETCHER_H
#define SANDBOX_PREFETCHER_H

#include <vector>
#include <deque>
#include "prefetcher.h"
#include "bf/all.hpp"
using namespace std;

class Score
{
public:
	int32_t offset;
	uint32_t score;
	Score() : offset(0), score(0) {}
	Score(int32_t _offset) : offset(_offset), score(0) {}
	Score(int32_t _offset, uint32_t _score) : offset(_offset), score(_score) {}
	~Score(){}
};

class SandboxPrefetcher : public Prefetcher
{
private:
	deque<Score*> evaluated_offsets;
	deque<int32_t> non_evaluated_offsets;
	uint32_t pref_degree; /* degree per direction */
	struct
	{
		uint32_t curr_ptr;
		uint32_t total_demand;
		uint32_t filter_hit;
	} eval;

	/* Bloom Filter */
	uint32_t opt_hash_functions;
	bf::basic_bloom_filter *bf;

	/* stats */
	struct
	{
		uint64_t called;

		struct
		{
			uint64_t filter_lookup;
			uint64_t filter_hit	;
		} step1;

		struct
		{
			uint64_t filter_add;
		} step2;

		struct
		{
			uint64_t end_of_phase;
			uint64_t end_of_round;
		} step3;

		struct
		{
			uint64_t pref_generated;
			uint64_t pref_generated_pos;
			uint64_t pref_generated_neg;
		} step4;

		uint64_t pref_delta_dist[128];
	} stats;

private:
	void init_knobs();
	void init_stats();
	void init_evaluated_offsets();
	void init_non_evaluated_offsets();
	void reset_eval();
	void get_offset_list_sorted(vector<Score*> &pos_offsets, vector<Score*> &neg_offsets);
	void generate_prefetch(vector<Score*> offset_list, uint32_t pref_degree, uint64_t page, uint32_t offset, vector<uint64_t> &pref_addr);
	void destroy_offset_list(vector<Score*> offset_list);
	uint64_t generate_address(uint64_t page, uint32_t offset, int32_t delta, uint32_t lookahead = 1);
	void end_of_round();
	void filter_add(uint64_t address);
	bool filter_lookup(uint64_t address);
	void record_pref_stats(int32_t offset, uint32_t pref_count);

public:
	SandboxPrefetcher(string type);
	~SandboxPrefetcher();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
	void dump_stats();
	void print_config();
};

#endif /* SANDBOX_PREFETCHER_H */

