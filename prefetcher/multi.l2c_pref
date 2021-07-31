#include <string>
#include <assert.h>
#include "cache.h"
#include "prefetcher.h"

/* Supported prefetchers at L2 */
#include "sms.h"
#include "scooby.h"
#include "next_line.h"
#include "bop.h"
#include "sandbox.h"
#include "dspatch.h"
#include "spp_dev2.h"
#include "ppf_dev.h"
#include "mlop.h"
#include "bingo.h"
#include "stride.h"
#include "ipcp_L2.h"
#include "ampm.h"
#include "streamer.h"
#include "pref_power7.h"

using namespace std;

namespace knob
{
	extern vector<string> l2c_prefetcher_types;
}

// vector<Prefetcher*> prefetchers;

void CACHE::l2c_prefetcher_initialize()
{
	for(uint32_t index = 0; index < knob::l2c_prefetcher_types.size(); ++index)
	{
		if(!knob::l2c_prefetcher_types[index].compare("none"))
		{
			cout << "adding L2C_PREFETCHER: NONE" << endl;
		}
		else if(!knob::l2c_prefetcher_types[index].compare("sms"))
		{
			cout << "adding L2C_PREFETCHER: SMS" << endl;
			SMSPrefetcher *pref_sms = new SMSPrefetcher(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_sms);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("bop"))
		{
			cout << "adding L2C_PREFETCHER: BOP" << endl;
			BOPrefetcher *pref_bop = new BOPrefetcher(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_bop);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("dspatch"))
		{
			cout << "adding L2C_PREFETCHER: DSPatch" << endl;
			DSPatch *pref_dspatch = new DSPatch(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_dspatch);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("scooby"))
		{
			cout << "adding L2C_PREFETCHER: Scooby" << endl;
			Scooby *pref_scooby = new Scooby(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_scooby);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("next_line"))
		{
			cout << "adding L2C_PREFETCHER: next_line" << endl;
			NextLinePrefetcher *pref_nl = new NextLinePrefetcher(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_nl);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("sandbox"))
		{
			cout << "adding L2C_PREFETCHER: Sandbox" << endl;
			SandboxPrefetcher *pref_sandbox = new SandboxPrefetcher(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_sandbox);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("spp_dev2"))
		{
			cout << "adding L2C_PREFETCHER: SPP_dev2" << endl;
			SPP_dev2 *pref_spp_dev2 = new SPP_dev2(knob::l2c_prefetcher_types[index], this);
			prefetchers.push_back(pref_spp_dev2);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("spp_ppf_dev"))
		{
			cout << "adding L2C_PREFETCHER: SPP_PPF_dev" << endl;
			SPP_PPF_dev *pref_spp_ppf_dev = new SPP_PPF_dev(knob::l2c_prefetcher_types[index], this);
			prefetchers.push_back(pref_spp_ppf_dev);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("mlop"))
		{
			cout << "adding L2C_PREFETCHER: MLOP" << endl;
			MLOP *pref_mlop = new MLOP(knob::l2c_prefetcher_types[index], this);
			prefetchers.push_back(pref_mlop);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("bingo"))
		{
			cout << "adding L2C_PREFETCHER: Bingo" << endl;
			Bingo *pref_bingo = new Bingo(knob::l2c_prefetcher_types[index], this);
			prefetchers.push_back(pref_bingo);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("stride"))
		{
			cout << "adding L2C_PREFETCHER: Stride" << endl;
			StridePrefetcher *pref_stride = new StridePrefetcher(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_stride);
		}
		else if (!knob::l2c_prefetcher_types[index].compare("streamer"))
		{
			cout << "adding L2C_PREFETCHER: streamer" << endl;
			Streamer *pref_streamer = new Streamer(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_streamer);
		}
		else if (!knob::l2c_prefetcher_types[index].compare("power7"))
		{
			cout << "adding L2C_PREFETCHER: POWER7" << endl;
			POWER7_Pref *pref_power7 = new POWER7_Pref(knob::l2c_prefetcher_types[index], this);
			prefetchers.push_back(pref_power7);
		}
		else if(!knob::l2c_prefetcher_types[index].compare("ipcp"))
		{
			cout << "adding L2C_PREFETCHER: IPCP" << endl;
			IPCP_L2 *pref_ipcp_L2 = new IPCP_L2(knob::l2c_prefetcher_types[index], this);
			prefetchers.push_back(pref_ipcp_L2);
		}
		else if (!knob::l2c_prefetcher_types[index].compare("ampm"))
		{
			cout << "adding L2C_PREFETCHER: AMPM" << endl;
			AMPM *pref_ampm = new AMPM(knob::l2c_prefetcher_types[index]);
			prefetchers.push_back(pref_ampm);
		}
		else
		{
			cout << "unsupported prefetcher type " << knob::l2c_prefetcher_types[index] << endl;
			exit(1);
		}
	}

	assert(knob::l2c_prefetcher_types.size() == prefetchers.size() || !knob::l2c_prefetcher_types[0].compare("none"));
}

uint32_t CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
	vector<uint64_t> pref_addr;
	for(uint32_t index = 0; index < prefetchers.size(); ++index)
	{
		if(knob::l2c_prefetcher_types[index].compare("ipcp"))
		{
			prefetchers[index]->invoke_prefetcher(ip, addr, cache_hit, type, pref_addr);
		}
		else /* means IPCP */
		{
			IPCP_L2 *pref_ipcp_L2 = (IPCP_L2*)prefetchers[index];
			pref_ipcp_L2->invoke_prefetcher(ip, addr, cache_hit, type, metadata_in, pref_addr);
		}
		if(knob::l2c_prefetcher_types[index].compare("spp_dev2")
			&& knob::l2c_prefetcher_types[index].compare("spp_ppf_dev")
			&& knob::l2c_prefetcher_types[index].compare("mlop")
			&& knob::l2c_prefetcher_types[index].compare("bingo")
			&& knob::l2c_prefetcher_types[index].compare("ipcp")
			&& !pref_addr.empty())
		{
			for(uint32_t addr_index = 0; addr_index < pref_addr.size(); ++addr_index)
			{
				prefetch_line(ip, addr, pref_addr[addr_index], FILL_L2, 0);
			}
		}
		pref_addr.clear();
	}

	return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
	if(prefetch)
	{
		for(uint32_t index = 0; index < prefetchers.size(); ++index)
		{
			if(!prefetchers[index]->get_type().compare("scooby"))
			{
				Scooby *pref_scooby = (Scooby*)prefetchers[index];
				pref_scooby->register_fill(addr);
			}
			if(!prefetchers[index]->get_type().compare("next_line"))
			{
				NextLinePrefetcher *pref_nl = (NextLinePrefetcher*)prefetchers[index];
				pref_nl->register_fill(addr);
			}
			if(!prefetchers[index]->get_type().compare("bop"))
			{
				BOPrefetcher *pref_bop = (BOPrefetcher*)prefetchers[index];
				pref_bop->register_fill(addr);
			}
			if(!prefetchers[index]->get_type().compare("spp_dev2"))
			{
				SPP_dev2 *pref_spp_dev2 = (SPP_dev2*)prefetchers[index];
				pref_spp_dev2->cache_fill(addr, set, way, prefetch, evicted_addr);
			}
			//if(!prefetchers[index]->get_type().compare("spp_ppf_dev"))
			//{
			//	SPP_PPF_dev *pref_spp_ppf_dev = (SPP_PPF_dev*)prefetchers[index];
			//	pref_spp_ppf_dev->cache_fill(addr, set, way, prefetch, evicted_addr);
			//}
			if(!prefetchers[index]->get_type().compare("mlop"))
			{
				MLOP *pref_mlop = (MLOP*)prefetchers[index];
				pref_mlop->register_fill(addr, set, way, prefetch, evicted_addr);
			}
			if(!prefetchers[index]->get_type().compare("bingo"))
			{
				Bingo *pref_bingo = (Bingo*)prefetchers[index];
				pref_bingo->register_fill(addr, set, way, prefetch, evicted_addr);
			}
		}
	}

	return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_prefetch_hit(uint64_t addr, uint64_t ip, uint32_t metadata_in)
{
	for(uint32_t index = 0; index < prefetchers.size(); ++index)
	{
		if(!prefetchers[index]->get_type().compare("scooby"))
		{
			Scooby *pref_scooby = (Scooby*)prefetchers[index];
			pref_scooby->register_prefetch_hit(addr);
		}
	}

    return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats()
{
	for(uint32_t index = 0; index < prefetchers.size(); ++index)
	{
		prefetchers[index]->dump_stats();
	}
}

void CACHE::l2c_prefetcher_print_config()
{
	for(uint32_t index = 0; index < prefetchers.size(); ++index)
	{
		prefetchers[index]->print_config();
	}
}

void CACHE::l2c_prefetcher_broadcast_bw(uint8_t bw_level)
{
	for(uint32_t index = 0; index < prefetchers.size(); ++index)
	{
		if(!prefetchers[index]->get_type().compare("scooby"))
		{
			Scooby *pref_scooby = (Scooby*)prefetchers[index];
			pref_scooby->update_bw(bw_level);
		}
		if(!prefetchers[index]->get_type().compare("dspatch"))
		{
			DSPatch *pref_dspatch = (DSPatch*)prefetchers[index];
			pref_dspatch->update_bw(bw_level);
		}
	}
}

void CACHE::l2c_prefetcher_broadcast_ipc(uint8_t ipc)
{
	for(uint32_t index = 0; index < prefetchers.size(); ++index)
	{
		if(!prefetchers[index]->get_type().compare("scooby"))
		{
			Scooby *pref_scooby = (Scooby*)prefetchers[index];
			pref_scooby->update_ipc(ipc);
		}
	}
}

void CACHE::l2c_prefetcher_broadcast_acc(uint32_t acc_level)
{
	for(uint32_t index = 0; index < prefetchers.size(); ++index)
	{
		if(!prefetchers[index]->get_type().compare("scooby"))
		{
			Scooby *pref_scooby = (Scooby*)prefetchers[index];
			pref_scooby->update_acc(acc_level);
		}
	}
}
