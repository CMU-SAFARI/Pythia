#ifndef BINGO_H
#define BINGO_H


/* Bingo [https://mshakerinava.github.io/papers/bingo-hpca19.pdf] */

#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include "prefetcher.h"
#include "cache.h"
#include "bakshalipour_framework.h"

using namespace std;

class FilterTableData {
public:
   uint64_t pc;
   int offset;
};

class FilterTable : public LRUSetAssociativeCache<FilterTableData> {
   typedef LRUSetAssociativeCache<FilterTableData> Super;

public:
   FilterTable(int size, int debug_level = 0, int num_ways = 16) : Super(size, num_ways, debug_level) {
      // assert(__builtin_popcount(size) == 1);
      if (this->debug_level >= 1)
      cerr << "FilterTable::FilterTable(size=" << size << ", debug_level=" << debug_level
      << ", num_ways=" << num_ways << ")" << dec << endl;
   }

   Entry *find(uint64_t region_number) {
      if (this->debug_level >= 2)
      cerr << "FilterTable::find(region_number=0x" << hex << region_number << ")" << dec << endl;
      uint64_t key = this->build_key(region_number);
      Entry *entry = Super::find(key);
      if (!entry) {
         if (this->debug_level >= 2)
         cerr << "[FilterTable::find] Miss!" << dec << endl;
         return nullptr;
      }
      if (this->debug_level >= 2)
      cerr << "[FilterTable::find] Hit!" << dec << endl;
      Super::set_mru(key);
      return entry;
   }

   void insert(uint64_t region_number, uint64_t pc, int offset) {
      if (this->debug_level >= 2)
      cerr << "FilterTable::insert(region_number=0x" << hex << region_number << ", pc=0x" << pc
      << ", offset=" << dec << offset << ")" << dec << endl;
      uint64_t key = this->build_key(region_number);
      // assert(!Super::find(key));
      Super::insert(key, {pc, offset});
      Super::set_mru(key);
   }

   Entry *erase(uint64_t region_number) {
      uint64_t key = this->build_key(region_number);
      return Super::erase(key);
   }

   string log() {
      vector<string> headers({"Region", "PC", "Offset"});
      return Super::log(headers);
   }

private:
   /* @override */
   void write_data(Entry &entry, Table &table, int row) {
      uint64_t key = hash_index(entry.key, this->index_len);
      table.set_cell(row, 0, key);
      table.set_cell(row, 1, entry.data.pc);
      table.set_cell(row, 2, entry.data.offset);
   }

   uint64_t build_key(uint64_t region_number) {
      uint64_t key = region_number & ((1ULL << 37) - 1);
      return hash_index(key, this->index_len);
   }

   /*==========================================================*/
   /* Entry   = [tag, offset, PC, valid, LRU]                  */
   /* Storage = size * (37 - lg(sets) + 5 + 16 + 1 + lg(ways)) */
   /* 64 * (37 - lg(4) + 5 + 16 + 1 + lg(16)) = 488 Bytes      */
   /*==========================================================*/
};

template <class T> string pattern_to_string(const vector<T> &pattern) {
   ostringstream oss;
   for (unsigned i = 0; i < pattern.size(); i += 1)
   oss << int(pattern[i]);
   return oss.str();
}

class AccumulationTableData {
public:
   uint64_t pc;
   int offset;
   vector<bool> pattern;
};

class AccumulationTable : public LRUSetAssociativeCache<AccumulationTableData> {
   typedef LRUSetAssociativeCache<AccumulationTableData> Super;

public:
   AccumulationTable(int size, int pattern_len, int debug_level = 0, int num_ways = 16)
   : Super(size, num_ways, debug_level), pattern_len(pattern_len) {
      // assert(__builtin_popcount(size) == 1);
      // assert(__builtin_popcount(pattern_len) == 1);
      if (this->debug_level >= 1)
      cerr << "AccumulationTable::AccumulationTable(size=" << size << ", pattern_len=" << pattern_len
      << ", debug_level=" << debug_level << ", num_ways=" << num_ways << ")" << dec << endl;
   }

   /**
   * @return False if the tag wasn't found and true if the pattern bit was successfully set
   */
   bool set_pattern(uint64_t region_number, int offset) {
      if (this->debug_level >= 2)
      cerr << "AccumulationTable::set_pattern(region_number=0x" << hex << region_number << ", offset=" << dec
      << offset << ")" << dec << endl;
      uint64_t key = this->build_key(region_number);
      Entry *entry = Super::find(key);
      if (!entry) {
         if (this->debug_level >= 2)
         cerr << "[AccumulationTable::set_pattern] Not found!" << dec << endl;
         return false;
      }
      entry->data.pattern[offset] = true;
      Super::set_mru(key);
      if (this->debug_level >= 2)
      cerr << "[AccumulationTable::set_pattern] OK!" << dec << endl;
      return true;
   }

   /* NOTE: `region_number` is probably truncated since it comes from the filter table */
   Entry insert(uint64_t region_number, uint64_t pc, int offset) {
      if (this->debug_level >= 2)
      cerr << "AccumulationTable::insert(region_number=0x" << hex << region_number << ", pc=0x" << pc
      << ", offset=" << dec << offset << dec << endl;
      uint64_t key = this->build_key(region_number);
      // assert(!Super::find(key));
      vector<bool> pattern(this->pattern_len, false);
      pattern[offset] = true;
      Entry old_entry = Super::insert(key, {pc, offset, pattern});
      Super::set_mru(key);
      return old_entry;
   }

   Entry *erase(uint64_t region_number) {
      uint64_t key = this->build_key(region_number);
      return Super::erase(key);
   }

   string log() {
      vector<string> headers({"Region", "PC", "Offset", "Pattern"});
      return Super::log(headers);
   }

private:
   /* @override */
   void write_data(Entry &entry, Table &table, int row) {
      uint64_t key = hash_index(entry.key, this->index_len);
      table.set_cell(row, 0, key);
      table.set_cell(row, 1, entry.data.pc);
      table.set_cell(row, 2, entry.data.offset);
      table.set_cell(row, 3, pattern_to_string(entry.data.pattern));
   }

   uint64_t build_key(uint64_t region_number) {
      uint64_t key = region_number & ((1ULL << 37) - 1);
      return hash_index(key, this->index_len);
   }

   int pattern_len;

   /*===============================================================*/
   /* Entry   = [tag, map, offset, PC, valid, LRU]                  */
   /* Storage = size * (37 - lg(sets) + 32 + 5 + 16 + 1 + lg(ways)) */
   /* 128 * (37 - lg(8) + 32 + 5 + 16 + 1 + lg(16)) = 1472 Bytes    */
   /*===============================================================*/
};

/**
* There are 3 possible outcomes (here called `Event`) for a PHT lookup:
* PC+Address hit, PC+Offset hit(s), or Miss.
* NOTE: `Event` is only used for gathering stats.
*/
enum Event { PC_ADDRESS = 0, PC_OFFSET = 1, MISS = 2 };

template <class T> vector<T> my_rotate(const vector<T> &x, int n) {
   vector<T> y;
   int len = x.size();
   n = n % len;
   for (int i = 0; i < len; i += 1)
   y.push_back(x[(i - n + len) % len]);
   return y;
}

class PatternHistoryTableData {
public:
   vector<bool> pattern;
};

class PatternHistoryTable : public LRUSetAssociativeCache<PatternHistoryTableData> {
   typedef LRUSetAssociativeCache<PatternHistoryTableData> Super;

public:
   PatternHistoryTable(int size, int pattern_len, int min_addr_width, int max_addr_width, int pc_width, int debug_level = 0, int num_ways = 16)
   : Super(size, num_ways, debug_level), pattern_len(pattern_len), min_addr_width(min_addr_width), max_addr_width(max_addr_width), pc_width(pc_width) {
      // assert(this->pc_width >= 0);
      // assert(this->min_addr_width >= 0);
      // assert(this->max_addr_width >= 0);
      // assert(this->max_addr_width >= this->min_addr_width);
      // assert(this->pc_width + this->min_addr_width > 0);
      // assert(__builtin_popcount(pattern_len) == 1);
      if (this->debug_level >= 1)
      cerr << "PatternHistoryTable::PatternHistoryTable(size=" << size << ", pattern_len=" << pattern_len
      << ", min_addr_width=" << min_addr_width << ", max_addr_width=" << max_addr_width
      << ", pc_width=" << pc_width << ", debug_level=" << debug_level << ", num_ways=" << num_ways << ")"
      << dec << endl;
   }

   /* NOTE: In BINGO, address is actually block number. */
   void insert(uint64_t pc, uint64_t address, vector<bool> pattern) {
      if (this->debug_level >= 2)
      cerr << "PatternHistoryTable::insert(pc=0x" << hex << pc << ", address=0x" << address
      << ", pattern=" << pattern_to_string(pattern) << ")" << dec << endl;
      // assert((int)pattern.size() == this->pattern_len);
      int offset = address % this->pattern_len;
      pattern = my_rotate(pattern, -offset);
      uint64_t key = this->build_key(pc, address);
      Super::insert(key, {pattern});
      Super::set_mru(key);
   }

   /**
   * First searches for a PC+Address match. If no match is found, returns all PC+Offset matches.
   * @return All un-rotated patterns if matches were found, returns an empty vector otherwise
   */
   vector<vector<bool>> find(uint64_t pc, uint64_t address) {
      if (this->debug_level >= 2)
      cerr << "PatternHistoryTable::find(pc=0x" << hex << pc << ", address=0x" << address << ")" << dec << endl;
      uint64_t key = this->build_key(pc, address);
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      auto &set = this->entries[index];
      uint64_t min_tag_mask = (1 << (this->pc_width + this->min_addr_width - this->index_len)) - 1;
      uint64_t max_tag_mask = (1 << (this->pc_width + this->max_addr_width - this->index_len)) - 1;
      vector<vector<bool>> matches;
      this->last_event = MISS;
      for (int i = 0; i < this->num_ways; i += 1) {
         if (!set[i].valid)
         continue;
         bool min_match = ((set[i].tag & min_tag_mask) == (tag & min_tag_mask));
         bool max_match = ((set[i].tag & max_tag_mask) == (tag & max_tag_mask));
         vector<bool> &cur_pattern = set[i].data.pattern;
         if (max_match) {
            this->last_event = PC_ADDRESS;
            Super::set_mru(set[i].key);
            matches.clear();
            matches.push_back(cur_pattern);
            break;
         }
         if (min_match) {
            this->last_event = PC_OFFSET;
            matches.push_back(cur_pattern);
         }
      }
      int offset = address % this->pattern_len;
      for (int i = 0; i < (int)matches.size(); i += 1)
      matches[i] = my_rotate(matches[i], +offset);
      return matches;
   }

   Event get_last_event() { return this->last_event; }

   string log() {
      vector<string> headers({"PC", "Offset", "Address", "Pattern"});
      return Super::log(headers);
   }

private:
   /* @override */
   void write_data(Entry &entry, Table &table, int row) {
      uint64_t base_key = entry.key >> (this->pc_width + this->min_addr_width);
      uint64_t index_key = entry.key & ((1 << (this->pc_width + this->min_addr_width)) - 1);
      index_key = hash_index(index_key, this->index_len); /* unhash */
      uint64_t key = (base_key << (this->pc_width + this->min_addr_width)) | index_key;

      /* extract PC, offset, and address */
      uint64_t offset = key & ((1 << this->min_addr_width) - 1);
      key >>= this->min_addr_width;
      uint64_t pc = key & ((1 << this->pc_width) - 1);
      key >>= this->pc_width;
      uint64_t address = (key << this->min_addr_width) + offset;

      table.set_cell(row, 0, pc);
      table.set_cell(row, 1, offset);
      table.set_cell(row, 2, address);
      table.set_cell(row, 3, pattern_to_string(entry.data.pattern));
   }

   uint64_t build_key(uint64_t pc, uint64_t address) {
      pc &= (1 << this->pc_width) - 1;            /* use `pc_width` bits from pc */
      address &= (1 << this->max_addr_width) - 1; /* use `addr_width` bits from address */
      uint64_t offset = address & ((1 << this->min_addr_width) - 1);
      uint64_t base = (address >> this->min_addr_width);
      /* key = base + hash_index( pc + offset )
      * The index must be computed from only PC+Offset to ensure that all entries with the same
      * PC+Offset end up in the same set */
      uint64_t index_key = hash_index((pc << this->min_addr_width) | offset, this->index_len);
      uint64_t key = (base << (this->pc_width + this->min_addr_width)) | index_key;
      return key;
   }

   int pattern_len;
   int min_addr_width, max_addr_width, pc_width;
   Event last_event;

   /*======================================================*/
   /* Entry   = [tag, map, valid, LRU]                     */
   /* Storage = size * (32 - lg(sets) + 32 + 1 + lg(ways)) */
   /* 8K * (32 - lg(512) + 32 + 1 + lg(16)) = 60K Bytes    */
   /*======================================================*/
};

class PrefetchStreamerData {
public:
   /* contains the prefetch fill level for each block of spatial region */
   vector<int> pattern;
};

class PrefetchStreamer : public LRUSetAssociativeCache<PrefetchStreamerData> {
   typedef LRUSetAssociativeCache<PrefetchStreamerData> Super;

public:
   PrefetchStreamer(int size, int pattern_len, int debug_level = 0, int num_ways = 16)
   : Super(size, num_ways, debug_level), pattern_len(pattern_len) {
      if (this->debug_level >= 1)
      cerr << "PrefetchStreamer::PrefetchStreamer(size=" << size << ", pattern_len=" << pattern_len
      << ", debug_level=" << debug_level << ", num_ways=" << num_ways << ")" << dec << endl;
   }

   void insert(uint64_t region_number, vector<int> pattern) {
      if (this->debug_level >= 2)
      cerr << "PrefetchStreamer::insert(region_number=0x" << hex << region_number
      << ", pattern=" << pattern_to_string(pattern) << ")" << dec << endl;
      uint64_t key = this->build_key(region_number);
      Super::insert(key, {pattern});
      Super::set_mru(key);
   }

   int prefetch(CACHE *cache, uint64_t block_address) {
      if (this->debug_level >= 2) {
         cerr << "PrefetchStreamer::prefetch(cache=" << cache->NAME << ", block_address=0x" << hex << block_address
         << ")" << dec << endl;
         cerr << "[PrefetchStreamer::prefetch] " << cache->PQ.occupancy << "/" << cache->PQ.SIZE
         << " PQ entries occupied." << dec << endl;
         cerr << "[PrefetchStreamer::prefetch] " << cache->MSHR.occupancy << "/" << cache->MSHR.SIZE
         << " MSHR entries occupied." << dec << endl;
      }
      uint64_t base_addr = block_address << LOG2_BLOCK_SIZE;
      int region_offset = block_address % this->pattern_len;
      uint64_t region_number = block_address / this->pattern_len;
      uint64_t key = this->build_key(region_number);
      Entry *entry = Super::find(key);
      if (!entry) {
         if (this->debug_level >= 2)
         cerr << "[PrefetchStreamer::prefetch] No entry found." << dec << endl;
         return 0;
      }
      Super::set_mru(key);
      int pf_issued = 0;
      vector<int> &pattern = entry->data.pattern;
      pattern[region_offset] = 0; /* accessed block will be automatically fetched if necessary (miss) */
      int pf_offset;
      /* prefetch blocks that are close to the recent access first (locality!) */
      for (int d = 1; d < this->pattern_len; d += 1) {
         /* prefer positive strides */
         for (int sgn = +1; sgn >= -1; sgn -= 2) {
            pf_offset = region_offset + sgn * d;
            if (0 <= pf_offset && pf_offset < this->pattern_len && pattern[pf_offset] > 0) {
               uint64_t pf_address = (region_number * this->pattern_len + pf_offset) << LOG2_BLOCK_SIZE;
               if (cache->PQ.occupancy + cache->MSHR.occupancy < cache->MSHR.SIZE - 1 && cache->PQ.occupancy < cache->PQ.SIZE) {
                  cache->prefetch_line(0, base_addr, pf_address, pattern[pf_offset], 0);
                  pf_issued += 1;
                  pattern[pf_offset] = 0;
               } else {
                  /* prefetching limit is reached */
                  return pf_issued;
               }
            }
         }
      }
      /* all prefetches done for this spatial region */
      Super::erase(key);
      return pf_issued;
   }

   string log() {
      vector<string> headers({"Region", "Pattern"});
      return Super::log(headers);
   }

private:
   /* @override */
   void write_data(Entry &entry, Table &table, int row) {
      uint64_t key = hash_index(entry.key, this->index_len);
      table.set_cell(row, 0, key);
      table.set_cell(row, 1, pattern_to_string(entry.data.pattern));
   }

   uint64_t build_key(uint64_t region_number) { return hash_index(region_number, this->index_len); }

   int pattern_len;

   /*======================================================*/
   /* Entry   = [tag, map, valid, LRU]                     */
   /* Storage = size * (53 - lg(sets) + 64 + 1 + lg(ways)) */
   /* 128 * (53 - lg(8) + 64 + 1 + lg(16)) = 1904 Bytes    */
   /*======================================================*/
};

class Bingo : public Prefetcher {
public:
   Bingo(string type, CACHE *cache);
   ~Bingo();
   void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr);
   void register_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
   void dump_stats();
   void print_config();

   /**
   * Updates BINGO's state based on the most recent LOAD access.
   * @param block_number The block address of the most recent LOAD access
   * @param pc           The PC of the most recent LOAD access
   */
   void access(uint64_t block_number, uint64_t pc);
   void eviction(uint64_t block_number);
   int prefetch(uint64_t block_number);
   void set_debug_level(int debug_level);
   void log();

   /*========== stats ==========*/
   /* NOTE: the BINGO code submitted for DPC3 (this code) does not call any of these methods. */
   Event get_event(uint64_t block_number);
   void add_prefetch(uint64_t block_number);
   void add_useful(uint64_t block_number, Event ev);
   void add_useless(uint64_t block_number, Event ev);
   void reset_stats();
   void print_stats();


private:
   /**
   * Performs a PHT lookup and computes a prefetching pattern from the result.
   * @return The appropriate prefetch level for all blocks based on PHT output or an empty vector
   *         if no blocks should be prefetched
   */
   vector<int> find_in_pht(uint64_t pc, uint64_t address);

   void insert_in_pht(const AccumulationTable::Entry &entry);

   /**
   * Uses a voting mechanism to produce a prefetching pattern from a set of footprints.
   * @param x The patterns obtained from all PC+Offset matches
   * @return  The appropriate prefetch level for all blocks based on BINGO's voting thresholds or
   *          an empty vector if no blocks should be prefetched
   */
   vector<int> vote(const vector<vector<bool>> &x);

   void init_knobs();
   void init_stats();

   /*======================*/
   CACHE *parent = NULL;
   int pattern_len;
   FilterTable filter_table;
   AccumulationTable accumulation_table;
   PatternHistoryTable pht;
   PrefetchStreamer pf_streamer;
   int debug_level = 0;
   uint32_t pc_address_fill_level;

   /* stats */
   unordered_map<uint64_t, Event> pht_events;

   uint64_t pht_access_cnt = 0;
   uint64_t pht_pc_address_cnt = 0;
   uint64_t pht_pc_offset_cnt = 0;
   uint64_t pht_miss_cnt = 0;

   uint64_t prefetch_cnt[2] = {0};
   uint64_t useful_cnt[2] = {0};
   uint64_t useless_cnt[2] = {0};

   unordered_map<int, uint64_t> pref_level_cnt;
   uint64_t region_pref_cnt = 0;

   uint64_t vote_cnt = 0;
   uint64_t voter_sum = 0;
   uint64_t voter_sqr_sum = 0;
};

#endif /* BINGO_H */
