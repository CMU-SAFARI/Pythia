#ifndef SPP_DEV2_H
#define SPP_DEV2_H

#include <string>
#include <vector>
#include <unordered_map>
#include "prefetcher.h"
#include "spp_dev2_helper.h"
#include "cache.h"
//using namespace spp;

/* SPP Prefetcher */
class SPP_dev2 : public Prefetcher
{
private:
	CACHE *m_parent_cache;
	SIGNATURE_TABLE ST;
	PATTERN_TABLE   PT;
	PREFETCH_FILTER FILTER;
	GLOBAL_REGISTER GHR;

	/* stats by rbera */
	struct
	{
		struct
		{
			uint64_t total;
			uint64_t at_L2;
			uint64_t at_LLC;
		} pref;

		struct
		{
			uint64_t count;
			uint64_t total;
			uint64_t max;
			uint64_t min;
		} depth;

		struct
		{
			uint64_t count;
			uint64_t total;
			uint64_t max;
			uint64_t min;
		} breadth;
	} stats;

	unordered_map<int32_t, uint64_t> delta_histogram;
	unordered_map<uint32_t, unordered_map<int32_t, uint64_t> > depth_delta_histogram;

private:
	void init_knobs();
	void init_stats();

public:
	SPP_dev2(std::string type, CACHE *cache);
	~SPP_dev2();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr);
	void dump_stats();
	void print_config();
	void cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
};

#endif /* SPP_DEV2_H */

