#ifndef SCOOBY_H
#define SCOOBY_H

#include <vector>
#include <unordered_map>
#include "champsim.h"
#include "cache.h"
#include "prefetcher.h"
#include "scooby_helper.h"
#include "learning_engine_basic.h"
#include "learning_engine_featurewise.h"

using namespace std;

#define MAX_ACTIONS 64
#define MAX_REWARDS 16
#define MAX_SCOOBY_DEGREE 16
#define SCOOBY_MAX_IPC_LEVEL 4

/* forward declaration */
class LearningEngine;

class Scooby : public Prefetcher
{
private:
	deque<Scooby_STEntry*> signature_table;
	LearningEngineBasic *brain;
	LearningEngineFeaturewise *brain_featurewise;
	deque<Scooby_PTEntry*> prefetch_tracker;
	Scooby_PTEntry *last_evicted_tracker;
	uint8_t bw_level;
	uint8_t core_ipc;
	uint32_t acc_level;

	/* for workload insights only
	 * has nothing to do with prefetching */
	ScoobyRecorder *recorder;

	/* Data structures for debugging */
	unordered_map<string, uint64_t> target_action_state;

	struct
	{
		struct
		{
			uint64_t lookup;
			uint64_t hit;
			uint64_t evict;
			uint64_t insert;
			uint64_t streaming;
		} st;

		struct
		{
			uint64_t called;
			uint64_t out_of_bounds;
			vector<uint64_t> action_dist;
			vector<uint64_t> issue_dist;
			vector<uint64_t> pred_hit;
			vector<uint64_t> out_of_bounds_dist;
			uint64_t predicted;
			uint64_t multi_deg;
			uint64_t multi_deg_called;
			uint64_t multi_deg_histogram[MAX_SCOOBY_DEGREE+1];
			uint64_t deg_histogram[MAX_SCOOBY_DEGREE+1];
		} predict;

		struct
		{
			uint64_t called;
			uint64_t same_address;
			uint64_t evict;
		} track;

		struct
		{
			struct
			{
				uint64_t called;
				uint64_t pt_not_found;
				uint64_t pt_found;
				uint64_t pt_found_total;
				uint64_t has_reward;
			} demand;

			struct
			{
				uint64_t called;
			} train;

			struct
			{
				uint64_t called;
			} assign_reward;

			struct
			{
				uint64_t dist[MAX_REWARDS][2];
			} compute_reward;
			
			uint64_t correct_timely;
			uint64_t correct_untimely;
			uint64_t no_pref;
			uint64_t incorrect;
			uint64_t out_of_bounds;
			uint64_t tracker_hit;
			uint64_t dist[MAX_ACTIONS][MAX_REWARDS];
		} reward;

		struct
		{
			uint64_t called;
			uint64_t compute_reward;
		} train;

		struct
		{
			uint64_t called;
			uint64_t set;
			uint64_t set_total;
		} register_fill;

		struct
		{
			uint64_t called;
			uint64_t set;
			uint64_t set_total;
		} register_prefetch_hit;

		struct
		{
			uint64_t scooby;
		} pref_issue;

		struct 
		{
			uint64_t epochs;
			uint64_t histogram[DRAM_BW_LEVELS];
		} bandwidth;

		struct 
		{
			uint64_t epochs;
			uint64_t histogram[SCOOBY_MAX_IPC_LEVEL];
		} ipc;

		struct 
		{
			uint64_t epochs;
			uint64_t histogram[CACHE_ACC_LEVELS];
		} cache_acc;
	} stats;

	unordered_map<uint32_t, vector<uint64_t> > state_action_dist;
	unordered_map<std::string, vector<uint64_t> > state_action_dist2;
	unordered_map<int32_t, vector<uint64_t> > action_deg_dist;

private:
	void init_knobs();
	void init_stats();

	void update_global_state(uint64_t pc, uint64_t page, uint32_t offset, uint64_t address);
	Scooby_STEntry* update_local_state(uint64_t pc, uint64_t page, uint32_t offset, uint64_t address);
	uint32_t predict(uint64_t address, uint64_t page, uint32_t offset, State *state, vector<uint64_t> &pref_addr);
	bool track(uint64_t address, State *state, uint32_t action_index, Scooby_PTEntry **tracker);
	void reward(uint64_t address);
	void reward(Scooby_PTEntry *ptentry);
	void assign_reward(Scooby_PTEntry *ptentry, RewardType type);
	int32_t compute_reward(Scooby_PTEntry *ptentry, RewardType type);
	void train(Scooby_PTEntry *curr_evicted, Scooby_PTEntry *last_evicted);
	vector<Scooby_PTEntry*> search_pt(uint64_t address, bool search_all = false);
	void update_stats(uint32_t state, uint32_t action_index, uint32_t pref_degree = 1);
	void update_stats(State *state, uint32_t action_index, uint32_t degree = 1);
	void track_in_st(uint64_t page, uint32_t pred_offset, int32_t pref_offset);
	void gen_multi_degree_pref(uint64_t page, uint32_t offset, int32_t action, uint32_t pref_degree, vector<uint64_t> &pref_addr);
	uint32_t get_dyn_pref_degree(float max_to_avg_q_ratio, uint64_t page = 0xdeadbeef, int32_t action = 0); /* only implemented for CMAC engine 2.0 */
	bool is_high_bw();

public:
	Scooby(string type);
	~Scooby();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
	void register_fill(uint64_t address);
	void register_prefetch_hit(uint64_t address);
	void dump_stats();
	void print_config();
	int32_t getAction(uint32_t action_index);
	void update_bw(uint8_t bw_level);
	void update_ipc(uint8_t ipc);
	void update_acc(uint32_t acc_level);
};

#endif /* SCOOBY_H */

