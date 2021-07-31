#include <string>
#include <assert.h>
#include "cache.h"
#include "prefetcher.h"
#include "next_line.h"
#include "stride.h"
#include "ipcp_L1.h"

using namespace std;

namespace knob
{
	extern vector<string> l1d_prefetcher_types;
}

void CACHE::l1d_prefetcher_initialize()
{
	for(uint32_t index = 0; index < knob::l1d_prefetcher_types.size(); ++index)
	{
		if(!knob::l1d_prefetcher_types[index].compare("none"))
		{
			cout << "adding L1D_PREFETCHER: NONE" << endl;
		}
		else if(!knob::l1d_prefetcher_types[index].compare("next_line"))
		{
			cout << "adding L1D_PREFETCHER: next_line" << endl;
			NextLinePrefetcher *pref_nl = new NextLinePrefetcher(knob::l1d_prefetcher_types[index]);
			l1d_prefetchers.push_back(pref_nl);
		}
		else if(!knob::l1d_prefetcher_types[index].compare("stride"))
		{
			cout << "adding L1D_PREFETCHER: Stride" << endl;
			StridePrefetcher *pref_stride = new StridePrefetcher(knob::l1d_prefetcher_types[index]);
			l1d_prefetchers.push_back(pref_stride);
		}
      else if(!knob::l1d_prefetcher_types[index].compare("ipcp"))
		{
			cout << "adding L1D_PREFETCHER: IPCP" << endl;
			IPCP_L1 *pref_ipcp_l1 = new IPCP_L1(knob::l1d_prefetcher_types[index], this);
			l1d_prefetchers.push_back(pref_ipcp_l1);
		}
		else
		{
			cout << "unsupported prefetcher type " << knob::l1d_prefetcher_types[index] << endl;
			exit(1);
		}
	}

	assert(knob::l1d_prefetcher_types.size() == l1d_prefetchers.size() || !knob::l1d_prefetcher_types[0].compare("none"));
}

void CACHE::l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
	vector<uint64_t> pref_addr;
	for(uint32_t index = 0; index < l1d_prefetchers.size(); ++index)
	{
		l1d_prefetchers[index]->invoke_prefetcher(ip, addr, cache_hit, type, pref_addr);
		if(knob::l1d_prefetcher_types[index].compare("ipcp")
         && !pref_addr.empty())
		{
			for(uint32_t addr_index = 0; addr_index < pref_addr.size(); ++addr_index)
			{
				prefetch_line(ip, addr, pref_addr[addr_index], FILL_L1, 0);
			}
		}
		pref_addr.clear();
	}
}

void CACHE::l1d_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
	if(prefetch)
	{
		for(uint32_t index = 0; index < l1d_prefetchers.size(); ++index)
		{
			if(!l1d_prefetchers[index]->get_type().compare("next_line"))
			{
				NextLinePrefetcher *pref_nl = (NextLinePrefetcher*)l1d_prefetchers[index];
				pref_nl->register_fill(addr);
			}
		}
	}
}

uint32_t CACHE::l1d_prefetcher_prefetch_hit(uint64_t addr, uint64_t ip, uint32_t metadata_in)
{
    return metadata_in;
}

void CACHE::l1d_prefetcher_final_stats()
{
	for(uint32_t index = 0; index < l1d_prefetchers.size(); ++index)
	{
		l1d_prefetchers[index]->dump_stats();
	}
}

void CACHE::l1d_prefetcher_print_config()
{
	for(uint32_t index = 0; index < l1d_prefetchers.size(); ++index)
	{
		l1d_prefetchers[index]->print_config();
	}
}

void CACHE::l1d_prefetcher_broadcast_bw(uint8_t bw_level)
{

}

void CACHE::l1d_prefetcher_broadcast_ipc(uint8_t ipc)
{

}

void CACHE::l1d_prefetcher_broadcast_acc(uint32_t acc_level)
{

}
