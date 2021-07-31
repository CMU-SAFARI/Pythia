#ifndef PPF_DEV_H
#define PPF_DEV_H

#include "prefetcher.h"
#include "ppf_dev_helper.h"
#include "cache.h"

using namespace spp_ppf;

/* SPP+PPF imnplementation */
class SPP_PPF_dev : public Prefetcher
{
public:
	CACHE *m_parent_cache;
	spp_ppf::SIGNATURE_TABLE ST;
	spp_ppf::PATTERN_TABLE PT;
	spp_ppf::PREFETCH_FILTER FILTER;
	spp_ppf::GLOBAL_REGISTER GHR;
	spp_ppf::PERCEPTRON PERC;

private:
	void init_knobs();
	void init_stats();

public:
	SPP_PPF_dev(std::string type, CACHE *cache);
	~SPP_PPF_dev();
	void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr);
	void dump_stats();
	void print_config();
	void cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
};

#endif /* PPF_DEV_H */

