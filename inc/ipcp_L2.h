#ifndef IPCP_L2_H
#define IPCP_L2_H

/*****************************************************
Code taken from
Samuel Pakalapati - pakalapatisamuel@gmail.com
Biswabandan Panda - biswap@cse.iitk.ac.in
******************************************************/

#include "cache.h"
#include "prefetcher.h"
#include "ipcp_vars.h"

class IP_TRACKER
{
  public:
    uint64_t ip_tag;
    uint16_t ip_valid;
    uint32_t pref_type;                                     // prefetch class type
    int stride;                                             // last stride sent by metadata

    IP_TRACKER () {
        ip_tag = 0;
        ip_valid = 0;
        pref_type = 0;
        stride = 0;
    };
};

class IPCP_L2 : public Prefetcher
{
public:
   CACHE *m_parent_cache;
   uint32_t spec_nl_l2[NUM_CPUS] = {0};
   IP_TRACKER trackers[NUM_CPUS][NUM_IP_TABLE_L2_ENTRIES];

private:
   void init_knobs();
   void init_stats();
   int decode_stride(uint32_t metadata);

public:
   IPCP_L2(std::string type, CACHE *cache);
	~IPCP_L2();
   void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr) {}
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, uint32_t metadata_in, std::vector<uint64_t> &pref_addr);
	void dump_stats();
	void print_config();
	void cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
};

#endif /* IPCP_L2_H */
