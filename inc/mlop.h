/* Based on Multi-Lookahead Offset Prefetcher (MLOP) - 3rd Data Prefetching Championship */
/* Owners: Mehran Shakerinava and Mohammad Bakhshalipour */


#ifndef MLOP_H
#define MLOP_H

#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include "prefetcher.h"
#include "cache.h"
#include "bakshalipour_framework.h"

/**
 * The access map table records blocks as being in one of 3 general states:
 * ACCESS, PREFETCH, or INIT.
 * The PREFETCH state is actually composed of up to 3 sub-states:
 * L1-PREFETCH, L2-PREFETCH, or L3-PREFETCH.
 * This version of MLOP does not prefetch into L3 so there are 4 states in total (2-bit states).
 */
enum MLOP_State { INIT = 0, ACCESS = 1, PREFTCH = 2 };
char getStateChar(MLOP_State state);
string map_to_string(const vector<MLOP_State> &access_map, const vector<int> &prefetch_map);

class AccessMapData {
  public:
    /* block states are represented with a `MLOP_State` and an `int` in this software implementation but
     * in a hardware implementation, they'd be represented with only 2 bits. */
    vector<MLOP_State> access_map;
    vector<int> prefetch_map;

    deque<int> hist_queue;
};

class AccessMapTable : public LRUSetAssociativeCache<AccessMapData> {
    typedef LRUSetAssociativeCache<AccessMapData> Super;

  public:
    /* NOTE: zones are equivalent to pages (64 blocks) in this implementation */
    AccessMapTable(int size, int blocks_in_zone, int queue_size, int debug_level = 0, int num_ways = 16)
        : Super(size, num_ways, debug_level), blocks_in_zone(blocks_in_zone), queue_size(queue_size) {
        if (this->debug_level >= 1)
            cout << "AccessMapTable::AccessMapTable(size=" << size << ", blocks_in_zone=" << blocks_in_zone
                 << ", queue_size=" << queue_size << ", debug_level=" << debug_level << ", num_ways=" << num_ways << ")"
                 << endl;
    }

    /**
     * Sets specified block to given state. If new state is ACCESS, the block will also be pushed in the zone's queue.
     */
    void set_state(uint64_t block_number, MLOP_State new_state, int new_fill_level = 0) {
        if (this->debug_level >= 2)
            cout << "AccessMapTable::set_state(block_number=0x" << hex << block_number
                 << ", new_state=" << getStateChar(new_state) << ", new_fill_level=" << new_fill_level << ")" << dec
                 << endl;

        // if (new_state != MLOP_State::PREFTCH)
        //     assert(new_fill_level == 0);
        // else
        //     assert(new_fill_level == FILL_L1 || new_fill_level == FILL_L2 || new_fill_level == FILL_LLC);

        uint64_t zone_number = block_number / this->blocks_in_zone;
        int zone_offset = block_number % this->blocks_in_zone;

        uint64_t key = this->build_key(zone_number);
        Entry *entry = Super::find(key);
        if (!entry) {
            // assert(new_state != MLOP_State::PREFTCH);
            if (new_state == MLOP_State::INIT)
                return;
            Super::insert(key, {vector<MLOP_State>(blocks_in_zone, MLOP_State::INIT), vector<int>(blocks_in_zone, 0)});
            entry = Super::find(key);
            // assert(entry->data.hist_queue.empty());
        }

        auto &access_map = entry->data.access_map;
        auto &prefetch_map = entry->data.prefetch_map;
        auto &hist_queue = entry->data.hist_queue;

        if (new_state == MLOP_State::ACCESS) {
            Super::set_mru(key);

            /* insert access into queue */
            hist_queue.push_front(zone_offset);
            if (hist_queue.size() > this->queue_size)
                hist_queue.pop_back();
        }

        MLOP_State old_state = access_map[zone_offset];
        int old_fill_level = prefetch_map[zone_offset];

        vector<MLOP_State> old_access_map = access_map;
        vector<int> old_prefetch_map = prefetch_map;

        access_map[zone_offset] = new_state;
        prefetch_map[zone_offset] = new_fill_level;

        if (new_state == MLOP_State::INIT) {
            /* delete entry if access map is empty (all in state INIT) */
            bool all_init = true;
            for (unsigned i = 0; i < this->blocks_in_zone; i += 1)
                if (access_map[i] != MLOP_State::INIT) {
                    all_init = false;
                    break;
                }
            if (all_init)
                Super::erase(key);
        }

        if (this->debug_level >= 2) {
            cout << "[AccessMapTable::set_state] zone_number=0x" << hex << zone_number << dec
                 << ", zone_offset=" << setw(2) << zone_offset << ": state transition from " << getStateChar(old_state)
                 << " to " << getStateChar(new_state) << endl;
            if (old_state != new_state || old_fill_level != new_fill_level) {
                cout << "[AccessMapTable::set_state] old_access_map=" << map_to_string(old_access_map, old_prefetch_map)
                     << endl;
                cout << "[AccessMapTable::set_state] new_access_map=" << map_to_string(access_map, prefetch_map)
                     << endl;
            }
        }
    }

    Entry *find(uint64_t zone_number) {
        if (this->debug_level >= 2)
            cout << "AccessMapTable::find(zone_number=0x" << hex << zone_number << ")" << dec << endl;
        uint64_t key = this->build_key(zone_number);
        return Super::find(key);
    }

    string log() {
        vector<string> headers({"Zone", "Access Map"});
        return Super::log(headers);
    }

  private:
    /* @override */
    void write_data(Entry &entry, Table &table, int row) {
        uint64_t zone_number = hash_index(entry.key, this->index_len);
        table.set_cell(row, 0, zone_number);
        table.set_cell(row, 1, map_to_string(entry.data.access_map, entry.data.prefetch_map));
    }

    uint64_t build_key(uint64_t zone_number) {
        uint64_t key = zone_number; /* no truncation (52 bits) */
        return hash_index(key, this->index_len);
    }

    unsigned blocks_in_zone;
    unsigned queue_size;

    /*===================================================================*/
    /* Entry   = [tag, map, queue, valid, LRU]                           */
    /* Storage = size * (52 - lg(sets) + 64 * 2 + 15 * 6 + 1 + lg(ways)) */
    /* L1D: 256 * (52 - lg(16) + 128 + 90 + 1 + lg(16)) = 8672 Bytes     */
    /*===================================================================*/
};

class MLOP : public Prefetcher
{
private:
    inline bool is_inside_zone(uint32_t zone_offset) { return (0 <= zone_offset && zone_offset < this->blocks_in_zone); }

	uint32_t PF_DEGREE;
	uint32_t NUM_UPDATES;
	uint32_t L1D_THRESH;
	uint32_t L2C_THRESH;
	uint32_t LLC_THRESH;
	uint32_t ORIGIN;
	uint32_t NUM_OFFSETS;
    CACHE *parent;

	int32_t MAX_OFFSET, MIN_OFFSET;
    uint32_t blocks_in_cache, blocks_in_zone, amt_size;
    AccessMapTable *access_map_table;

    /**
     * Contains best offsets for each degree of prefetching. A degree will have several offsets if
     * they all had maximum score (thus, a vector of vectors). A degree won't have any offsets if
     * all best offsets were redundant (already selected in previous degrees).
     */
    vector<vector<int>> pf_offset;

    vector<vector<int>> offset_scores;
    vector<int> pf_level; /* the prefetching level for each degree of prefetching offsets */
    uint32_t update_cnt = 0;  /* tracks the number of score updates, round is over when `update_cnt == NUM_UPDATES` */

    uint32_t debug_level = 0;

    /* stats */
    const uint64_t TRACKED_ZONE_CNT = 100;
    bool tracking = false;
    uint64_t tracked_zone_number = 0;
    uint64_t zone_cnt = 0;
    vector<string> zone_life;

    uint64_t round_cnt = 0;
    uint64_t pf_degree_sum = 0, pf_degree_sqr_sum = 0;
    uint64_t max_score_le_sum = 0, max_score_le_sqr_sum = 0;
    uint64_t max_score_ri_sum = 0, max_score_ri_sqr_sum = 0;

private:
	void init_knobs();
	void init_stats();

public:
	MLOP(string type, CACHE *cache);
	~MLOP();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr);
	void register_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
	void dump_stats();
	void print_config();

	void access(uint64_t block_number);
	void prefetch(CACHE *cache, uint64_t block_number);
    void mark(uint64_t block_number, MLOP_State state, int fill_level = 0);
    void set_debug_level(int debug_level);
    string log_offset_scores();
    void log();
    void track(uint64_t block_number);
    void reset_stats();
    void print_stats();
};

# endif /* MLOP_H */
