#include <algorithm>
#include "spp_dev2.h"
#include "champsim.h"
#include "memory_class.h"
using namespace std;
// using namespace spp;

namespace knob
{
    extern uint32_t spp_dev2_fill_threshold;
    extern uint32_t spp_dev2_pf_threshold;
}

void SPP_dev2::init_knobs()
{

}

void SPP_dev2::init_stats()
{
    bzero(&stats, sizeof(stats));
}

SPP_dev2::SPP_dev2(std::string type, CACHE *cache) : Prefetcher(type), m_parent_cache(cache)
{
    cout << "Initialize SIGNATURE TABLE" << endl
        << "ST_SET: " << ST_SET << endl
        << "ST_WAY: " << ST_WAY << endl
        << "ST_TAG_BIT: " << ST_TAG_BIT << endl
        << "ST_TAG_MASK: " << hex << ST_TAG_MASK << dec << endl
        << endl 
        << "Initialize PATTERN TABLE" << endl
        << "PT_SET: " << PT_SET << endl
        << "PT_WAY: " << PT_WAY << endl
        << "SIG_DELTA_BIT: " << SIG_DELTA_BIT << endl
        << "C_SIG_BIT: " << C_SIG_BIT << endl
        << "C_DELTA_BIT: " << C_DELTA_BIT << endl
        << endl 
        << "Initialize PREFETCH FILTER" << endl
        << "FILTER_SET: " << FILTER_SET << endl
        << "fill_threshold: " << knob::spp_dev2_fill_threshold << endl
        << "pf_threshold: " << knob::spp_dev2_pf_threshold << endl
        << endl;
}

SPP_dev2::~SPP_dev2()
{

}

void SPP_dev2::print_config()
{

}

void SPP_dev2::invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr)
{
    uint64_t page = addr >> LOG2_PAGE_SIZE;
    uint32_t page_offset = (addr >> LOG2_BLOCK_SIZE) & (PAGE_SIZE / BLOCK_SIZE - 1),
             last_sig = 0,
             curr_sig = 0,
             confidence_q[L2C_MSHR_SIZE],
             depth = 0;

    int32_t  delta = 0,
             delta_q[L2C_MSHR_SIZE];

    for (uint32_t i = 0; i < L2C_MSHR_SIZE; i++){
        confidence_q[i] = 0;
        delta_q[i] = 0;
    }
    confidence_q[0] = 100;
    GHR.global_accuracy = GHR.pf_issued ? ((100 * GHR.pf_useful) / GHR.pf_issued)  : 0;
    
    SPP_DP (
        cout << endl << "[ChampSim] " << __func__ << " addr: " << hex << addr << " cache_line: " << (addr >> LOG2_BLOCK_SIZE);
        cout << " page: " << page << " page_offset: " << dec << page_offset << endl;
    );

    // Stage 1: Read and update a sig stored in ST
    // last_sig and delta are used to update (sig, delta) correlation in PT
    // curr_sig is used to read prefetch candidates in PT 
    ST.read_and_update_sig(page, page_offset, last_sig, curr_sig, delta, GHR);

    // Also check the prefetch filter in parallel to update global accuracy counters 
    FILTER.check(addr, L2C_DEMAND, GHR); 

    // Stage 2: Update delta patterns stored in PT
    if (last_sig) PT.update_pattern(last_sig, delta);

    // Stage 3: Start prefetching
    uint64_t base_addr = addr;
    uint32_t lookahead_conf = 100,
             pf_q_head = 0, 
             pf_q_tail = 0;
    uint8_t  do_lookahead = 0;
    uint32_t breadth = 0;

#ifdef LOOKAHEAD_ON
    do {
#endif
        uint32_t lookahead_way = PT_WAY;
        PT.read_pattern(curr_sig, delta_q, confidence_q, lookahead_way, lookahead_conf, pf_q_tail, depth, GHR);

        do_lookahead = 0;
        breadth = 0;
        for (uint32_t i = pf_q_head; i < pf_q_tail; i++) {
            if (confidence_q[i] >= knob::spp_dev2_pf_threshold) {
                uint64_t pf_addr = (base_addr & ~(BLOCK_SIZE - 1)) + (delta_q[i] << LOG2_BLOCK_SIZE);

                if ((addr & ~(PAGE_SIZE - 1)) == (pf_addr & ~(PAGE_SIZE - 1))) { // Prefetch request is in the same physical page
                    if (FILTER.check(pf_addr, confidence_q[i] >= knob::spp_dev2_fill_threshold ? SPP_L2C_PREFETCH : SPP_LLC_PREFETCH, GHR)) {
                        m_parent_cache->prefetch_line(ip, addr, pf_addr, ((confidence_q[i] >= knob::spp_dev2_fill_threshold) ? FILL_L2 : FILL_LLC), 0); // Use addr (not base_addr) to obey the same physical page boundary
                        
                        stats.pref.total++;
                        if(confidence_q[i] >= knob::spp_dev2_fill_threshold) 
                            stats.pref.at_L2++;
                        else
                            stats.pref.at_LLC++;

                        if (confidence_q[i] >= knob::spp_dev2_fill_threshold) {
                            GHR.pf_issued++;
                            if (GHR.pf_issued > GLOBAL_COUNTER_MAX) {
                                GHR.pf_issued >>= 1;
                                GHR.pf_useful >>= 1;
                            }
                            SPP_DP (cout << "[ChampSim] SPP L2 prefetch issued GHR.pf_issued: " << GHR.pf_issued << " GHR.pf_useful: " << GHR.pf_useful << endl;);
                        }

                        SPP_DP (
                            cout << "[ChampSim] " << __func__ << " base_addr: " << hex << base_addr << " pf_addr: " << pf_addr;
                            cout << " pf_cache_line: " << (pf_addr >> LOG2_BLOCK_SIZE);
                            cout << " prefetch_delta: " << dec << delta_q[i] << " confidence: " << confidence_q[i];
                            cout << " depth: " << i << " FILL_THRESH: " << knob::spp_dev2_fill_threshold << " fill_level: " << ((confidence_q[i] >= knob::spp_dev2_fill_threshold) ? FILL_L2 : FILL_LLC) << endl;
                        );
                    }

                    breadth++;
                    delta_histogram[delta_q[i]]++;
                    depth_delta_histogram[depth][delta_q[i]]++;
                } else { // Prefetch request is crossing the physical page boundary
#ifdef GHR_ON
                    // Store this prefetch request in GHR to bootstrap SPP learning when we see a ST miss (i.e., accessing a new page)
                    GHR.update_entry(curr_sig, confidence_q[i], (pf_addr >> LOG2_BLOCK_SIZE) & 0x3F, delta_q[i]); 
#endif
                }

                do_lookahead = 1;
                pf_q_head++;
            }
        }

        // Update base_addr and curr_sig
        if (lookahead_way < PT_WAY) {
            uint32_t set = get_hash(curr_sig) % PT_SET;
            base_addr += (PT.delta[set][lookahead_way] << LOG2_BLOCK_SIZE);

            // PT.delta uses a 7-bit sign magnitude representation to generate sig_delta
            //int sig_delta = (PT.delta[set][lookahead_way] < 0) ? ((((-1) * PT.delta[set][lookahead_way]) & 0x3F) + 0x40) : PT.delta[set][lookahead_way];
            int sig_delta = (PT.delta[set][lookahead_way] < 0) ? (((-1) * PT.delta[set][lookahead_way]) + (1 << (SIG_DELTA_BIT - 1))) : PT.delta[set][lookahead_way];
            curr_sig = ((curr_sig << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
        }

        SPP_DP (
            cout << "Looping curr_sig: " << hex << curr_sig << " base_addr: " << base_addr << dec;
            cout << " pf_q_head: " << pf_q_head << " pf_q_tail: " << pf_q_tail << " depth: " << depth << endl;
        );

        stats.breadth.count++;
        stats.breadth.total += breadth;
        if(breadth >= stats.breadth.max) stats.breadth.max = breadth;
        if(breadth <= stats.breadth.min) stats.breadth.min = breadth;
#ifdef LOOKAHEAD_ON
    } while (do_lookahead);
#endif

    stats.depth.count++;
    stats.depth.total += depth;
    if(depth >= stats.depth.max) stats.depth.max = depth;
    if(depth <= stats.depth.min) stats.depth.min = depth;
}

void SPP_dev2::cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
#ifdef FILTER_ON
    SPP_DP (cout << endl;);
    FILTER.check(evicted_addr, L2C_EVICT, GHR);
#endif
}

void SPP_dev2::dump_stats()
{
    uint32_t avg_breadth = (float)stats.breadth.total/stats.breadth.count;
    uint32_t avg_depth = (float)stats.depth.total/stats.depth.count;

     cout << "spp.pref.total " << stats.pref.total << endl
        << "spp.pref.at_L2 " << stats.pref.at_L2 << endl
        << "spp.pref.at_LLC " << stats.pref.at_LLC << endl
        << endl
        << "spp.depth.max " << stats.depth.max << endl
        << "spp.depth.min " << stats.depth.min << endl
        << "spp.depth.avg " << avg_depth << endl
        << endl
        << "spp.breadth.max " << stats.breadth.max << endl
        << "spp.breadth.min " << stats.breadth.min << endl
        << "spp.breadth.avg " << avg_breadth << endl
        << endl;

    vector<pair<int32_t, uint64_t> > pairs;
    for(auto it = delta_histogram.begin(); it != delta_histogram.end(); ++it)
        pairs.push_back(*it);
    sort(pairs.begin(), pairs.end(), [](pair<int32_t, uint64_t>& a, pair<int32_t, uint64_t>& b){return a.second > b.second;});
    for(uint32_t index = 0; index < pairs.size(); ++index)
    {
        cout << "delta_" << pairs[index].first << " " << pairs[index].second << endl;
    }
    cout << endl;

    vector<pair<uint32_t, unordered_map<int32_t, uint64_t> > > pairs2;
    for(auto it = depth_delta_histogram.begin(); it != depth_delta_histogram.end(); ++it)
        pairs2.push_back(*it);
    sort(pairs2.begin(), pairs2.end(), [](pair<uint32_t, unordered_map<int32_t, uint64_t> >& a, pair<uint32_t, unordered_map<int32_t, uint64_t> >& b){return a.first < b.first;});
    for(uint32_t index = 0; index < pairs2.size(); ++index)
    {
        vector<pair<int32_t, uint64_t> > pairs3;
        for(auto it = pairs2[index].second.begin(); it != pairs2[index].second.end(); ++it)
            pairs3.push_back(*it);
        sort(pairs3.begin(), pairs3.end(), [](pair<int32_t, uint64_t>& a, pair<int32_t, uint64_t>& b){return a.second > b.second;});

        for(uint32_t index2 = 0; index2 < pairs3.size(); ++index2)
        {
            cout << "depth_" << pairs2[index].first << "_delta_" << pairs3[index2].first << " " << pairs3[index2].second << endl;
        }
    }
    cout << endl;
}
