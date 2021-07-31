#ifndef IPCP_L1_H
#define IPCP_L1_H

/***************************************************************************
Code taken from
Samuel Pakalapati - samuelpakalapati@gmail.com
Biswabandan Panda - biswap@cse.iitk.ac.in
***************************************************************************/

#include "cache.h"
#include "prefetcher.h"
#include "ipcp_vars.h"

class IP_TABLE_L1
{
  public:
    uint64_t ip_tag;
    uint64_t last_page;                                     // last page seen by IP
    uint64_t last_cl_offset;                                // last cl offset in the 4KB page
    int64_t last_stride;                                    // last delta observed
    uint16_t ip_valid;                                      // Valid IP or not
    int conf;                                               // CS conf
    uint16_t signature;                                     // CPLX signature
    uint16_t str_dir;                                       // stream direction
    uint16_t str_valid;                                     // stream valid
    uint16_t str_strength;                                  // stream strength

    IP_TABLE_L1 ()
    {
        ip_tag = 0;
        last_page = 0;
        last_cl_offset = 0;
        last_stride = 0;
        ip_valid = 0;
        signature = 0;
        conf = 0;
        str_dir = 0;
        str_valid = 0;
        str_strength = 0;
    };
};

class DELTA_PRED_TABLE
{
public:
    int delta;
    int conf;

    DELTA_PRED_TABLE ()
    {
        delta = 0;
        conf = 0;
    };
};

class IPCP_L1 : public Prefetcher
{
public:
   CACHE *m_parent_cache;
   IP_TABLE_L1 trackers_l1[NUM_CPUS][NUM_IP_TABLE_L1_ENTRIES];
   DELTA_PRED_TABLE DPT_l1[NUM_CPUS][4096];
   uint64_t ghb_l1[NUM_CPUS][NUM_GHB_ENTRIES];
   uint64_t prev_cpu_cycle[NUM_CPUS];
   uint64_t num_misses[NUM_CPUS];
   float mpkc[NUM_CPUS] = {0};
   int spec_nl[NUM_CPUS] = {0};

private:
   void init_stats();
   void init_knobs();
   uint16_t update_sig_l1(uint16_t old_sig, int delta);
   uint32_t encode_metadata(int stride, uint16_t type, int spec_nl);
   void check_for_stream_l1(int index, uint64_t cl_addr, uint8_t cpu);
   int update_conf(int stride, int pred_stride, int conf);

public:
   IPCP_L1(std::string type, CACHE *cache);
	~IPCP_L1();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr);
	void dump_stats();
	void print_config();
	void cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
};

#endif /* IPCP_L1_H */
