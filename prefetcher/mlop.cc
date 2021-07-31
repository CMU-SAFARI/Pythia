#include <iostream>
#include "mlop.h"
#include "champsim.h"

namespace knob
{
	extern uint32_t mlop_pref_degree;
	extern uint32_t mlop_num_updates;
	extern float 	mlop_l1d_thresh;
	extern float 	mlop_l2c_thresh;
	extern float 	mlop_llc_thresh;
	extern uint32_t	mlop_debug_level;
}

char state_char[] = {'I', 'A', 'P'};
char getStateChar(MLOP_State state) {return state_char[(int)state];}

string map_to_string(const vector<MLOP_State> &access_map, const vector<int> &prefetch_map) {
    ostringstream oss;
    for (unsigned i = 0; i < access_map.size(); i += 1)
        if (access_map[i] == MLOP_State::PREFTCH) {
            oss << prefetch_map[i];
        } else {
            oss << state_char[access_map[i]];
        }
    return oss.str();
}

void MLOP::init_knobs()
{
	PF_DEGREE = knob::mlop_pref_degree;
	NUM_UPDATES = knob::mlop_num_updates;
	L1D_THRESH = knob::mlop_l1d_thresh * NUM_UPDATES;
	L2C_THRESH = knob::mlop_l2c_thresh * NUM_UPDATES;
	LLC_THRESH = knob::mlop_llc_thresh * NUM_UPDATES;
	debug_level = knob::mlop_debug_level;

	blocks_in_cache = parent->NUM_SET * parent->NUM_WAY;
	blocks_in_zone = PAGE_SIZE/BLOCK_SIZE;
	amt_size = 32 * blocks_in_cache / blocks_in_zone;
	ORIGIN = blocks_in_zone - 1;
	MAX_OFFSET = blocks_in_zone - 1;
	MIN_OFFSET = (-1) * MAX_OFFSET;
	NUM_OFFSETS = 2*blocks_in_zone - 1;
}

void MLOP::init_stats()
{

}

MLOP::MLOP(string type, CACHE *cache) : Prefetcher(type), parent(cache)
{
	init_knobs();
	init_stats();

	/* init data structures */
	access_map_table = new AccessMapTable(amt_size, blocks_in_zone, PF_DEGREE - 1, debug_level);
	pf_offset = vector<vector<int>>(PF_DEGREE, vector<int>());
	pf_level = vector<int>(PF_DEGREE, 0);
	offset_scores = vector<vector<int>>(PF_DEGREE, vector<int>(NUM_OFFSETS, 0));
}

MLOP::~MLOP()
{

}

void MLOP::print_config()
{
	cout << "mlop_pref_degree " << knob::mlop_pref_degree << endl
		<< "mlop_num_updates " << knob::mlop_num_updates << endl
		<< "mlop_l1d_thresh " << knob::mlop_l1d_thresh << endl
		<< "mlop_l2c_thresh " << knob::mlop_l2c_thresh << endl
		<< "mlop_llc_thresh " << knob::mlop_llc_thresh << endl
		<< "mlop_debug_level " << knob::mlop_debug_level << endl
		<< "mlop_blocks_in_cache " << blocks_in_cache << endl
		<< "mlop_blocks_in_zone " << blocks_in_zone << endl
		<< "mlop_amt_size " << amt_size << endl
		<< "mlop_PF_DEGREE " << PF_DEGREE << endl
		<< "mlop_NUM_UPDATES " << NUM_UPDATES << endl
		<< "mlop_L1D_THRESH " << L1D_THRESH << endl
		<< "mlop_L2C_THRESH " << L2C_THRESH << endl
		<< "mlop_LLC_THRESH " << LLC_THRESH << endl
		<< "mlop_ORIGIN " << ORIGIN << endl
		<< "mlop_MAX_OFFSET " << MAX_OFFSET << endl
		<< "mlop_MIN_OFFSET " << MIN_OFFSET << endl
		<< "mlop_ORIGIN " << ORIGIN << endl
		<< "mlop_NUM_OFFSETS " << NUM_OFFSETS << endl
		<< endl;
}

/**
 * Updates MLOP's state based on the most recent trigger access (LOAD miss/prefetch-hit).
 * @param block_number The block address of the most recent trigger access
 */
void MLOP::access(uint64_t block_number) {
	if (this->debug_level >= 2)
		cout << "MLOP::access(block_number=0x" << hex << block_number << ")" << dec << endl;

	uint64_t zone_number = block_number / this->blocks_in_zone;
	int zone_offset = block_number % this->blocks_in_zone;

	if (this->debug_level >= 2)
		cout << "[MLOP::access] zone_number=0x" << hex << zone_number << dec << ", zone_offset=" << zone_offset
			<< endl;

	// for (int i = 0; i < PF_DEGREE; i += 1)
	//     assert(this->offset_scores[i][ORIGIN] == 0);

	/* update scores */
	AccessMapTable::Entry *entry = this->access_map_table->find(zone_number);
	if (!entry) {
		/* stats */
		this->zone_cnt += 1;
		if (this->zone_cnt == TRACKED_ZONE_CNT) {
			this->tracked_zone_number = zone_number;
			this->tracking = true;
			this->zone_life.push_back(string(this->blocks_in_zone, state_char[MLOP_State::INIT]));
		}
		/* ===== */
		return;
	}
	vector<MLOP_State> access_map = entry->data.access_map;
	if (access_map[zone_offset] == MLOP_State::ACCESS)
		return; /* ignore repeated trigger access */
	this->update_cnt += 1;
	const deque<int> &queue = entry->data.hist_queue;
	for (int d = 0; d <= (int)queue.size(); d += 1) {
		/* unmark latest access to increase prediction depth */
		if (d != 0) {
			int idx = queue[d - 1];
			// assert(0 <= idx && idx < this->blocks_in_zone);
			access_map[idx] = MLOP_State::INIT;
		}
		// cout << "inside access" << endl;
		for (uint32_t i = 0; i < this->blocks_in_zone; i += 1) {
			if (access_map[i] == MLOP_State::ACCESS) {
				int offset = zone_offset - i;
				// cout << "offset " << offset << endl;
				if (offset >= MIN_OFFSET && offset <= MAX_OFFSET && offset != 0)
				{
					// cout << "incrementing offset_score" << endl;
					this->offset_scores[d][ORIGIN + offset] += 1;
				}
			}
		}
	}

	/* update prefetching offsets if round is finished */
	if (this->update_cnt == NUM_UPDATES) {
		if (this->debug_level >= 1)
			cout << "[MLOP::access] Round finished!" << endl;

		/* reset `update_cnt` and clear `pf_level` and `pf_offset` */
		this->update_cnt = 0;
		this->pf_level = vector<int>(PF_DEGREE, 0);
		this->pf_offset = vector<vector<int>>(PF_DEGREE, vector<int>());

		/* calculate maximum score for all degrees */
		vector<int> max_scores(PF_DEGREE, 0);
		for (uint32_t i = 0; i < PF_DEGREE; i += 1) {
			max_scores[i] = *max_element(this->offset_scores[i].begin(), this->offset_scores[i].end());
			/* `max_scores` should be decreasing */
			// if (i > 0)
			//     assert(max_scores[i] <= max_scores[i - 1]);
		}

		int fill_level = 0;
		vector<bool> pf_offset_map(NUM_OFFSETS, false);
		for (int d = PF_DEGREE - 1; d >= 0; d -= 1) {
			// cout << "d " << d << " max_scores[d] " << max_scores[d] << endl;
			/* check thresholds */
			// if (max_scores[d] >= (int)L1D_THRESH)
			// 	fill_level = FILL_L1;
			if (max_scores[d] >= (int)L2C_THRESH)
				fill_level = FILL_L2;
			else if (max_scores[d] >= (int)LLC_THRESH)
				fill_level = FILL_LLC;
			else
				continue;

			/* select offsets with highest score */
			vector<int> best_offsets;
			for (int i = MIN_OFFSET; i <= MAX_OFFSET; i += 1) {
				int &cur_score = this->offset_scores[d][ORIGIN + i];
				// assert(0 <= cur_score && cur_score <= NUM_UPDATES);
				if (cur_score == max_scores[d] && !pf_offset_map[ORIGIN + i])
					best_offsets.push_back(i);
			}

			// cout << "came here too!" << endl;
			this->pf_level[d] = fill_level;
			this->pf_offset[d] = best_offsets;

			/* mark in `pf_offset_map` to avoid duplicate prefetch offsets */
			for (int i = 0; i < (int)best_offsets.size(); i += 1)
				pf_offset_map[ORIGIN + best_offsets[i]] = true;
		}

		/* reset `offset_scores` */
		this->offset_scores = vector<vector<int>>(PF_DEGREE, vector<int>(NUM_OFFSETS, 0));

		/* print selected prefetching offsets if debug is on */
		if (this->debug_level >= 1) {
			for (uint32_t d = 0; d < PF_DEGREE; d += 1) {
				cout << "[MLOP::access] Degree=" << setw(2) << d + 1 << ", Level=";

				if (this->pf_level[d] == 0)
					cout << "No";
				if (this->pf_level[d] == FILL_L1)
					cout << "L1";
				if (this->pf_level[d] == FILL_L2)
					cout << "L2";
				if (this->pf_level[d] == FILL_LLC)
					cout << "L3";

				cout << ", Offsets: ";
				if (this->pf_offset[d].size() == 0)
					cout << "None";
				for (int i = 0; i < (int)this->pf_offset[d].size(); i += 1) {
					cout << this->pf_offset[d][i];
					if (i < (int)this->pf_offset[d].size() - 1)
						cout << ", ";
				}
				cout << endl;
			}
		}

		/* stats */
		this->round_cnt += 1;

		int cur_pf_degree = 0;
		for (const bool &x : pf_offset_map)
			cur_pf_degree += (x ? 1 : 0);
		this->pf_degree_sum += cur_pf_degree;
		this->pf_degree_sqr_sum += square(cur_pf_degree);

		uint64_t max_score_le = max_scores[PF_DEGREE - 1];
		uint64_t max_score_ri = max_scores[0];
		this->max_score_le_sum += max_score_le;
		this->max_score_ri_sum += max_score_ri;
		this->max_score_le_sqr_sum += square(max_score_le);
		this->max_score_ri_sqr_sum += square(max_score_ri);
		/* ===== */
	}
}

/**
 * @param block_number The block address of the most recent LOAD access
 */
void MLOP::prefetch(CACHE *cache, uint64_t block_number) {
	if (this->debug_level >= 2) {
		cout << "MLOP::prefetch(cache=" << cache->NAME << "-" << cache->cpu << ", block_number=0x" << hex
			<< block_number << dec << ")" << endl;
	}
	int pf_issued = 0;
	uint64_t zone_number = block_number / this->blocks_in_zone;
	int zone_offset = block_number % this->blocks_in_zone;
	AccessMapTable::Entry *entry = this->access_map_table->find(zone_number);
	// assert(entry); /* I expect `mark` to have been called before `prefetch` */
	const vector<MLOP_State> &access_map = entry->data.access_map;
	const vector<int> &prefetch_map = entry->data.prefetch_map;
	if (this->debug_level >= 2) {
		cout << "[MLOP::prefetch] old_access_map=" << map_to_string(access_map, prefetch_map) << endl;
	}
	for (uint32_t d = 0; d < PF_DEGREE; d += 1) {
		for (auto &cur_pf_offset : this->pf_offset[d]) {
			// assert(this->pf_level[d] > 0);
			int offset_to_prefetch = zone_offset + cur_pf_offset;

			/* use `access_map` to filter prefetches */
			if (access_map[offset_to_prefetch] == MLOP_State::ACCESS)
				continue;
			if (access_map[offset_to_prefetch] == MLOP_State::PREFTCH &&
					prefetch_map[offset_to_prefetch] <= this->pf_level[d])
				continue;

			if (this->is_inside_zone(offset_to_prefetch) && cache->PQ.occupancy < cache->PQ.SIZE &&
					cache->PQ.occupancy + cache->MSHR.occupancy < cache->MSHR.SIZE - 1) {
				uint64_t pf_block_number = block_number + cur_pf_offset;
				uint64_t base_addr = block_number << LOG2_BLOCK_SIZE;
				uint64_t pf_addr = pf_block_number << LOG2_BLOCK_SIZE;
				cache->prefetch_line(0, base_addr, pf_addr, this->pf_level[d], 0);
				// assert(ok == 1);
				this->mark(pf_block_number, MLOP_State::PREFTCH, this->pf_level[d]);
				pf_issued += 1;
			}
		}
	}
	if (this->debug_level >= 2) {
		cout << "[MLOP::prefetch] new_access_map=" << map_to_string(access_map, prefetch_map) << endl;
		cout << "[MLOP::prefetch] issued " << pf_issued << " prefetch(es)" << endl;
	}
}

void MLOP::mark(uint64_t block_number, MLOP_State state, int fill_level) {
	this->access_map_table->set_state(block_number, state, fill_level);
}

void MLOP::set_debug_level(int debug_level) {
	this->debug_level = debug_level;
	this->access_map_table->set_debug_level(debug_level);
}

string MLOP::log_offset_scores() {
	Table table(1 + PF_DEGREE, this->offset_scores.size() + 1);
	vector<string> headers = {"Offset"};
	for (uint32_t d = 0; d < PF_DEGREE; d += 1) {
		ostringstream oss;
		oss << "Score[d=" << d + 1 << "]";
		headers.push_back(oss.str());
	}
	table.set_row(0, headers);
	for (uint32_t i = -(this->blocks_in_zone - 1); i <= +(this->blocks_in_zone - 1); i += 1) {
		table.set_cell(i + this->blocks_in_zone, 0, (int)i);
		for (uint32_t d = 0; d < PF_DEGREE; d += 1)
			table.set_cell(i + this->blocks_in_zone, d + 1, (int)this->offset_scores[d][ORIGIN + i]);
	}
	return table.to_string();
}

void MLOP::log() {
	cout << "Access Map Table:" << dec << endl;
	cout << this->access_map_table->log();

	cout << "Offset Scores:" << endl;
	cout << this->log_offset_scores();
}

/*========== stats ==========*/

void MLOP::track(uint64_t block_number) {
	uint64_t zone_number = block_number / this->blocks_in_zone;
	if (this->tracking && zone_number == this->tracked_zone_number) {
		AccessMapTable::Entry *entry = this->access_map_table->find(zone_number);
		if (!entry) {
			this->tracking = false; /* end of zone lifetime, stop tracking */
			this->zone_life.push_back(string(this->blocks_in_zone, state_char[MLOP_State::INIT]));
			return;
		}
		const vector<MLOP_State> &access_map = entry->data.access_map;
		const vector<int> &prefetch_map = entry->data.prefetch_map;
		string s = map_to_string(access_map, prefetch_map);
		if (s != this->zone_life.back())
			this->zone_life.push_back(s);
	}
}

void MLOP::reset_stats() {
	this->tracking = false;
	this->zone_cnt = 0;
	this->zone_life.clear();

	this->round_cnt = 0;
	this->pf_degree_sum = 0;
	this->pf_degree_sqr_sum = 0;
	this->max_score_le_sum = 0;
	this->max_score_le_sqr_sum = 0;
	this->max_score_ri_sum = 0;
	this->max_score_ri_sqr_sum = 0;
}

void MLOP::print_stats() {
	cout << "[MLOP] History of tracked zone:" << endl;
	for (auto &x : this->zone_life)
		cout << x << endl;

	double pf_degree_mean = 1.0 * this->pf_degree_sum / this->round_cnt;
	double pf_degree_sqr_mean = 1.0 * this->pf_degree_sqr_sum / this->round_cnt;
	double pf_degree_sd = sqrt(pf_degree_sqr_mean - square(pf_degree_mean));
	cout << "[MLOP] Prefetch Degree Mean: " << pf_degree_mean << endl;
	cout << "[MLOP] Prefetch Degree SD: " << pf_degree_sd << endl;

	double max_score_le_mean = 1.0 * this->max_score_le_sum / this->round_cnt;
	double max_score_le_sqr_mean = 1.0 * this->max_score_le_sqr_sum / this->round_cnt;
	double max_score_le_sd = sqrt(max_score_le_sqr_mean - square(max_score_le_mean));
	cout << "[MLOP] Max Score Left Mean (%): " << 100.0 * max_score_le_mean / NUM_UPDATES << endl;
	cout << "[MLOP] Max Score Left SD (%): " << 100.0 * max_score_le_sd / NUM_UPDATES << endl;

	double max_score_ri_mean = 1.0 * this->max_score_ri_sum / this->round_cnt;
	double max_score_ri_sqr_mean = 1.0 * this->max_score_ri_sqr_sum / this->round_cnt;
	double max_score_ri_sd = sqrt(max_score_ri_sqr_mean - square(max_score_ri_mean));
	cout << "[MLOP] Max Score Right Mean (%): " << 100.0 * max_score_ri_mean / NUM_UPDATES << endl;
	cout << "[MLOP] Max Score Right SD (%): " << 100.0 * max_score_ri_sd / NUM_UPDATES << endl;
	cout << endl;
}

void MLOP::dump_stats()
{
	print_stats();
}

/* Base-class virtual function */
void MLOP::invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr)
{
    if (type != LOAD)
        return;

    uint64_t block_number = address >> LOG2_BLOCK_SIZE;

    /* check prefetch hit */
    bool prefetch_hit = false;
    if (cache_hit == 1) {
        uint32_t set = parent->get_set(block_number);
        uint32_t way = parent->get_way(block_number, set);
        if (parent->block[set][way].prefetch == 1)
            prefetch_hit = true;
    }

    /* check trigger access */
    bool trigger_access = false;
    if (cache_hit == 0 || prefetch_hit)
        trigger_access = true;

    if (trigger_access)
        /* update MLOP with most recent trigger access */
        access(block_number);

    /* issue prefetches */
    mark(block_number, MLOP_State::ACCESS);
    prefetch(parent, block_number);

    if (knob::mlop_debug_level >= 3) {
        log();
        cout << "=======================================" << dec << endl;
    }

    /* stats */
    track(block_number);
}

void MLOP::register_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
	if (parent->block[set][way].valid == 0)
		return; /* no eviction */

	uint64_t evicted_block_number = evicted_addr >> LOG2_BLOCK_SIZE;
	mark(evicted_block_number, MLOP_State::INIT);

	/* stats */
	track(evicted_block_number);
}
