#include <iostream>
#include "cache.h"
#include "champsim.h"
#include "bingo.h"

namespace knob
{
   extern uint32_t bingo_region_size;
	extern uint32_t bingo_pattern_len;
	extern uint32_t bingo_pc_width;
	extern uint32_t bingo_min_addr_width;
	extern uint32_t bingo_max_addr_width;
	extern uint32_t bingo_ft_size;
	extern uint32_t bingo_at_size;
	extern uint32_t bingo_pht_size;
	extern uint32_t bingo_pht_ways;
	extern uint32_t bingo_pf_streamer_size;
	extern uint32_t bingo_debug_level;
   extern float    bingo_l1d_thresh;
   extern float    bingo_l2c_thresh;
   extern float    bingo_llc_thresh;
   extern string   bingo_pc_address_fill_level;
}

void Bingo::init_knobs() {
   assert((knob::bingo_region_size >> LOG2_BLOCK_SIZE) == knob::bingo_pattern_len);
}

void Bingo::init_stats() {

}

Bingo::Bingo(string type, CACHE *cache) :
   Prefetcher(type), parent(cache),
   pattern_len(knob::bingo_pattern_len), filter_table(knob::bingo_ft_size, knob::bingo_debug_level),
   accumulation_table(knob::bingo_at_size, knob::bingo_pattern_len, knob::bingo_debug_level),
   pht(knob::bingo_pht_size, knob::bingo_pattern_len, knob::bingo_min_addr_width, knob::bingo_max_addr_width, knob::bingo_pc_width, knob::bingo_debug_level, knob::bingo_pht_ways),
   pf_streamer(knob::bingo_pf_streamer_size, knob::bingo_pattern_len, knob::bingo_debug_level), debug_level(knob::bingo_debug_level) {
      init_knobs();
      init_stats();
      if(!knob::bingo_pc_address_fill_level.compare("L1"))
         pc_address_fill_level = FILL_L1;
      else if(!knob::bingo_pc_address_fill_level.compare("L2"))
         pc_address_fill_level = FILL_L2;
      else if(!knob::bingo_pc_address_fill_level.compare("LLC"))
         pc_address_fill_level = FILL_LLC;
}

Bingo::~Bingo() {

}

void Bingo::print_config() {
   cout << "bingo_region_size " << knob::bingo_region_size << endl
   << "bingo_pattern_len " << knob::bingo_pattern_len << endl
   << "bingo_pc_width " << knob::bingo_pc_width << endl
   << "bingo_min_addr_width " << knob::bingo_min_addr_width << endl
   << "bingo_max_addr_width " << knob::bingo_max_addr_width << endl
   << "bingo_ft_size " << knob::bingo_ft_size << endl
   << "bingo_at_size " << knob::bingo_at_size << endl
   << "bingo_pht_size " << knob::bingo_pht_size << endl
   << "bingo_pht_ways " << knob::bingo_pht_ways << endl
   << "bingo_pf_streamer_size " << knob::bingo_pf_streamer_size << endl
   << "bingo_debug_level " << knob::bingo_debug_level << endl
   << "bingo_l1d_thresh " << knob::bingo_l1d_thresh << endl
   << "bingo_l2c_thresh " << knob::bingo_l2c_thresh << endl
   << "bingo_llc_thresh " << knob::bingo_llc_thresh << endl
   << "bingo_pc_address_fill_level " << knob::bingo_pc_address_fill_level << endl
   << endl;
}

/**
* Updates BINGO's state based on the most recent LOAD access.
* @param block_number The block address of the most recent LOAD access
* @param pc           The PC of the most recent LOAD access
*/
void Bingo::access(uint64_t block_number, uint64_t pc) {
   if (this->debug_level >= 2)
   cerr << "[Bingo] access(block_number=0x" << hex << block_number << ", pc=0x" << pc << ")" << dec << endl;
   uint64_t region_number = block_number / this->pattern_len;
   int region_offset = block_number % this->pattern_len;
   bool success = this->accumulation_table.set_pattern(region_number, region_offset);
   if (success)
   return;
   FilterTable::Entry *entry = this->filter_table.find(region_number);
   if (!entry) {
      /* trigger access */
      this->filter_table.insert(region_number, pc, region_offset);
      vector<int> pattern = this->find_in_pht(pc, block_number);
      if (pattern.empty()) {
         /* nothing to prefetch */
         return;
      }
      /* give pattern to `pf_streamer` */
      // assert((int)pattern.size() == this->pattern_len);
      this->pf_streamer.insert(region_number, pattern);
      return;
   }
   if (entry->data.offset != region_offset) {
      /* move from filter table to accumulation table */
      uint64_t region_number = hash_index(entry->key, this->filter_table.get_index_len());
      AccumulationTable::Entry victim =
      this->accumulation_table.insert(region_number, entry->data.pc, entry->data.offset);
      this->accumulation_table.set_pattern(region_number, region_offset);
      this->filter_table.erase(region_number);
      if (victim.valid) {
         /* move from accumulation table to PHT */
         this->insert_in_pht(victim);
      }
   }
}

void Bingo::eviction(uint64_t block_number) {
   if (this->debug_level >= 2)
      cerr << "[Bingo] eviction(block_number=" << block_number << ")" << dec << endl;
   /* end of generation: footprint must now be stored in PHT */
   uint64_t region_number = block_number / this->pattern_len;
   this->filter_table.erase(region_number);
   AccumulationTable::Entry *entry = this->accumulation_table.erase(region_number);
   if (entry) {
      /* move from accumulation table to PHT */
      this->insert_in_pht(*entry);
   }
}

int Bingo::prefetch(uint64_t block_number) {
   int pf_issued = this->pf_streamer.prefetch(parent, block_number);
   if (this->debug_level >= 2)
      cerr << "[Bingo::prefetch] pf_issued=" << pf_issued << dec << endl;
   return pf_issued;
}

void Bingo::set_debug_level(int debug_level) {
   this->filter_table.set_debug_level(debug_level);
   this->accumulation_table.set_debug_level(debug_level);
   this->pht.set_debug_level(debug_level);
   this->pf_streamer.set_debug_level(debug_level);
   this->debug_level = debug_level;
}

void Bingo::log() {
   cerr << "Filter Table:" << dec << endl;
   cerr << this->filter_table.log();

   cerr << "Accumulation Table:" << dec << endl;
   cerr << this->accumulation_table.log();

   cerr << "Pattern History Table:" << dec << endl;
   cerr << this->pht.log();

   cerr << "Prefetch Streamer:" << dec << endl;
   cerr << this->pf_streamer.log();
}

/*========== stats ==========*/
/* NOTE: the BINGO code submitted for DPC3 (this code) does not call any of these methods. */

Event Bingo::get_event(uint64_t block_number) {
   uint64_t region_number = block_number / this->pattern_len;
   // assert(this->pht_events.count(region_number) == 1);
   return this->pht_events[region_number];
}

void Bingo::add_prefetch(uint64_t block_number) {
   Event ev = this->get_event(block_number);
   // assert(ev != MISS);
   this->prefetch_cnt[ev] += 1;
}

void Bingo::add_useful(uint64_t block_number, Event ev) {
   // assert(ev != MISS);
   this->useful_cnt[ev] += 1;
}

void Bingo::add_useless(uint64_t block_number, Event ev) {
   // assert(ev != MISS);
   this->useless_cnt[ev] += 1;
}

void Bingo::reset_stats() {
   this->pht_access_cnt = 0;
   this->pht_pc_address_cnt = 0;
   this->pht_pc_offset_cnt = 0;
   this->pht_miss_cnt = 0;

   for (int i = 0; i < 2; i += 1) {
      this->prefetch_cnt[i] = 0;
      this->useful_cnt[i] = 0;
      this->useless_cnt[i] = 0;
   }

   this->pref_level_cnt.clear();
   this->region_pref_cnt = 0;

   this->voter_sum = 0;
   this->vote_cnt = 0;
}

void Bingo::print_stats() {
   cout << "[Bingo] PHT Access: " << this->pht_access_cnt << endl;
   cout << "[Bingo] PHT Hit PC+Addr: " << this->pht_pc_address_cnt << endl;
   cout << "[Bingo] PHT Hit PC+Offs: " << this->pht_pc_offset_cnt << endl;
   cout << "[Bingo] PHT Miss: " << this->pht_miss_cnt << endl;

   cout << "[Bingo] Prefetch PC+Addr: " << this->prefetch_cnt[PC_ADDRESS] << endl;
   cout << "[Bingo] Prefetch PC+Offs: " << this->prefetch_cnt[PC_OFFSET] << endl;

   cout << "[Bingo] Useful PC+Addr: " << this->useful_cnt[PC_ADDRESS] << endl;
   cout << "[Bingo] Useful PC+Offs: " << this->useful_cnt[PC_OFFSET] << endl;

   cout << "[Bingo] Useless PC+Addr: " << this->useless_cnt[PC_ADDRESS] << endl;
   cout << "[Bingo] Useless PC+Offs: " << this->useless_cnt[PC_OFFSET] << endl;

   double l1_pref_per_region = 1.0 * this->pref_level_cnt[FILL_L1] / this->region_pref_cnt;
   double l2_pref_per_region = 1.0 * this->pref_level_cnt[FILL_L2] / this->region_pref_cnt;
   double l3_pref_per_region = 1.0 * this->pref_level_cnt[FILL_LLC] / this->region_pref_cnt;
   double no_pref_per_region = (double)this->pattern_len - (l1_pref_per_region + l2_pref_per_region + l3_pref_per_region);

   cout << "[Bingo] L1 Prefetch per Region: " << l1_pref_per_region << endl;
   cout << "[Bingo] L2 Prefetch per Region: " << l2_pref_per_region << endl;
   cout << "[Bingo] L3 Prefetch per Region: " << l3_pref_per_region << endl;
   cout << "[Bingo] No Prefetch per Region: " << no_pref_per_region << endl;

   double voter_mean = 1.0 * this->voter_sum / this->vote_cnt;
   double voter_sqr_mean = 1.0 * this->voter_sqr_sum / this->vote_cnt;
   double voter_sd = sqrt(voter_sqr_mean - square(voter_mean));
   cout << "[Bingo] Number of Voters Mean: " << voter_mean << endl;
   cout << "[Bingo] Number of Voters SD: " << voter_sd << endl;
   cout << endl;
}

/**
* Performs a PHT lookup and computes a prefetching pattern from the result.
* @return The appropriate prefetch level for all blocks based on PHT output or an empty vector
*         if no blocks should be prefetched
*/
vector<int> Bingo::find_in_pht(uint64_t pc, uint64_t address) {
   if (this->debug_level >= 2) {
      cerr << "[Bingo] find_in_pht(pc=0x" << hex << pc << ", address=0x" << address << ")" << dec << endl;
   }
   vector<vector<bool>> matches = this->pht.find(pc, address);
   this->pht_access_cnt += 1;
   Event pht_last_event = this->pht.get_last_event();
   uint64_t region_number = address / this->pattern_len;
   if (pht_last_event != MISS)
   this->pht_events[region_number] = pht_last_event;
   vector<int> pattern;
   if (pht_last_event == PC_ADDRESS) {
      this->pht_pc_address_cnt += 1;
      // assert(matches.size() == 1); /* there can only be 1 PC+Address match */
      // assert(matches[0].size() == (unsigned)this->pattern_len);
      pattern.resize(this->pattern_len, 0);
      for (int i = 0; i < this->pattern_len; i += 1)
      if (matches[0][i])
         pattern[i] = pc_address_fill_level;
   } else if (pht_last_event == PC_OFFSET) {
      this->pht_pc_offset_cnt += 1;
      pattern = this->vote(matches);
   } else if (pht_last_event == MISS) {
      this->pht_miss_cnt += 1;
   } else {
      /* error: unknown event! */
      // assert(0);
   }
   /* stats */
   if (pht_last_event != MISS) {
      this->region_pref_cnt += 1;
      for (int i = 0; i < (int)pattern.size(); i += 1)
      if (pattern[i] != 0)
      this->pref_level_cnt[pattern[i]] += 1;
      // assert(this->pref_level_cnt.size() <= 3); /* L1, L2, L3 */
   }
   /* ===== */
   return pattern;
}

void Bingo::insert_in_pht(const AccumulationTable::Entry &entry) {
   uint64_t pc = entry.data.pc;
   uint64_t region_number = hash_index(entry.key, this->accumulation_table.get_index_len());
   uint64_t address = region_number * this->pattern_len + entry.data.offset;
   if (this->debug_level >= 2) {
      cerr << "[Bingo] insert_in_pht(pc=0x" << hex << pc << ", address=0x" << address << ")" << dec << endl;
   }
   const vector<bool> &pattern = entry.data.pattern;
   this->pht.insert(pc, address, pattern);
}

/**
* Uses a voting mechanism to produce a prefetching pattern from a set of footprints.
* @param x The patterns obtained from all PC+Offset matches
* @return  The appropriate prefetch level for all blocks based on BINGO's voting thresholds or
*          an empty vector if no blocks should be prefetched
*/
vector<int> Bingo::vote(const vector<vector<bool>> &x) {
   if (this->debug_level >= 2)
   cerr << "Bingo::vote(...)" << endl;
   int n = x.size();
   if (n == 0) {
      if (this->debug_level >= 2)
      cerr << "[Bingo::vote] There are no voters." << endl;
      return vector<int>();
   }
   /* stats */
   this->vote_cnt += 1;
   this->voter_sum += n;
   this->voter_sqr_sum += square(n);
   /* ===== */
   if (this->debug_level >= 2) {
      cerr << "[Bingo::vote] Taking a vote among:" << endl;
      for (int i = 0; i < n; i += 1)
      cerr << "<" << setw(3) << i + 1 << "> " << pattern_to_string(x[i]) << endl;
   }
   bool pf_flag = false;
   vector<int> res(this->pattern_len, 0);
   for (int i = 0; i < n; i += 1)
   // assert((int)x[i].size() == this->pattern_len);
   for (int i = 0; i < this->pattern_len; i += 1) {
      int cnt = 0;
      for (int j = 0; j < n; j += 1)
      if (x[j][i])
      cnt += 1;
      double p = 1.0 * cnt / n;
      if (p >= knob::bingo_l1d_thresh)
         res[i] = FILL_L1;
      else if (p >= knob::bingo_l2c_thresh)
         res[i] = FILL_L2;
      else if (p >= knob::bingo_llc_thresh)
         res[i] = FILL_LLC;
      else
         res[i] = 0;
      if (res[i] != 0)
         pf_flag = true;
   }
   if (this->debug_level >= 2) {
      cerr << "<res> " << pattern_to_string(res) << endl;
   }
   if (!pf_flag)
   return vector<int>();
   return res;
}

/* Base-class virtual function */
void Bingo::invoke_prefetcher(uint64_t pc, uint64_t addr, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr)
{
   if (debug_level >= 2) {
      cerr << "CACHE::l1d_prefetcher_operate(addr=0x" << hex << addr << ", PC=0x" << pc << ", cache_hit=" << dec
      << (int)cache_hit << ", type=" << (int)type << ")" << dec << endl;
   }

   if (type != LOAD)
   return;

   uint64_t block_number = addr >> LOG2_BLOCK_SIZE;

   /* update BINGO with most recent LOAD access */
   access(block_number, pc);

   /* issue prefetches */
   prefetch(block_number);

   if (debug_level >= 3) {
      log();
      cerr << "=======================================" << dec << endl;
   }
}

void Bingo::register_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr) {
   uint64_t evicted_block_number = evicted_addr >> LOG2_BLOCK_SIZE;

   if (parent->block[set][way].valid == 0)
   return; /* no eviction */

   /* inform all sms modules of the eviction */
   /* RBERA: original code was to send eviction signal to Bingo in every core
   * modified it to make the signal local */
   eviction(evicted_block_number);
}

void Bingo::dump_stats() {
   print_stats();
}
