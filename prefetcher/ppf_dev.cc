#include "ppf_dev.h"
#include "champsim.h"
#include "memory_class.h"
using namespace std;
using namespace spp_ppf;

namespace knob
{
    extern int32_t ppf_perc_threshold_hi;
    extern int32_t ppf_perc_threshold_lo;
}

void SPP_PPF_dev::init_knobs()
{

}

void SPP_PPF_dev::init_stats()
{

}

SPP_PPF_dev::SPP_PPF_dev(std::string type, CACHE *cache) : Prefetcher(type), m_parent_cache(cache)
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
         << endl
         << "Initialize PERCEPTRON" << endl
         << "PERC_ENTRIES: " << PERC_ENTRIES << endl
         << "PERC_FEATURES: " << PERC_FEATURES << endl
         << "PERC_THRESH_HI: " << knob::ppf_perc_threshold_hi << endl
         << "PERC_THRESH_LO: " << knob::ppf_perc_threshold_lo << endl
         << endl;

    /* set cross-pointers */
    ST.ghr = &GHR;
    PT.ghr = &GHR;
    PT.perc = &PERC;
    PT.filter = &FILTER;
    FILTER.ghr = &GHR;
    FILTER.perc = &PERC;

}

SPP_PPF_dev::~SPP_PPF_dev()
{

}

void SPP_PPF_dev::print_config()
{

}

void SPP_PPF_dev::invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr)
{
    uint64_t page = addr >> LOG2_PAGE_SIZE;
    uint32_t page_offset = (addr >> LOG2_BLOCK_SIZE) & (PAGE_SIZE / BLOCK_SIZE - 1),
             last_sig = 0,
             curr_sig = 0,
             confidence_q[100*L2C_MSHR_SIZE],
             depth = 0;

    int32_t  delta = 0,
             delta_q[100*L2C_MSHR_SIZE],
             perc_sum_q[100*L2C_MSHR_SIZE];

    for (uint32_t i = 0; i < 100*L2C_MSHR_SIZE; i++){
        confidence_q[i] = 0;
        delta_q[i] = 0;
        perc_sum_q[i] = 0;
    }
    confidence_q[0] = 100;
    GHR.global_accuracy = GHR.pf_issued ? ((100 * GHR.pf_useful) / GHR.pf_issued)  : 0;
    
    for (int i = PAGES_TRACKED-1; i>0; i--) { // N down to 1
        GHR.page_tracker[i] = GHR.page_tracker[i-1];
    }
    GHR.page_tracker[0] = page;

    int distinct_pages = 0;
    uint8_t num_pf = 0;
    for (int i=0; i < PAGES_TRACKED; i++) {
        int j;
        for (j=0; j<i; j++) {
            if (GHR.page_tracker[i] == GHR.page_tracker[j])
                break;
        }
        if (i==j)
            distinct_pages++;
    }
    //cout << "Distinct Pages: " << distinct_pages << endl;
    
    SPP_DP (
        cout << endl << "[ChampSim] " << __func__ << " addr: " << hex << addr << " cache_line: " << (addr >> LOG2_BLOCK_SIZE);
        cout << " page: " << page << " page_offset: " << dec << page_offset << endl;
    );

    // Stage 1: Read and update a sig stored in ST
    // last_sig and delta are used to update (sig, delta) correlation in PT
    // curr_sig is used to read prefetch candidates in PT 
    ST.read_and_update_sig(page, page_offset, last_sig, curr_sig, delta);
    
    // Also check the prefetch filter in parallel to update global accuracy counters 
    FILTER.check(addr, 0, 0, L2C_DEMAND, 0, 0, 0, 0, 0, 0); 

    // Stage 2: Update delta patterns stored in PT
    if (last_sig) PT.update_pattern(last_sig, delta);

    // Stage 3: Start prefetching
    uint64_t base_addr = addr;
    uint64_t curr_ip = ip;
    uint32_t lookahead_conf = 100,
             pf_q_head = 0, 
             pf_q_tail = 0;
    uint8_t  do_lookahead = 0;
    int32_t  prev_delta = 0;

    uint64_t train_addr  = addr;
    int32_t  train_delta = 0;

    GHR.ip_3 = GHR.ip_2;
    GHR.ip_2 = GHR.ip_1;
    GHR.ip_1 = GHR.ip_0;
    GHR.ip_0 = ip;

#ifdef LOOKAHEAD_ON
    do {
#endif
        uint32_t lookahead_way = PT_WAY;

        train_addr  = addr; train_delta = prev_delta;
        // Remembering the original addr here and accumulating the deltas in lookahead stages
        
        // Read the PT. Also passing info required for perceptron inferencing as PT calls perc_predict()
        PT.read_pattern(curr_sig, delta_q, confidence_q, perc_sum_q, lookahead_way, lookahead_conf, pf_q_tail, depth, addr, base_addr, train_addr, curr_ip, train_delta, last_sig, m_parent_cache->PQ.occupancy, m_parent_cache->PQ.SIZE, m_parent_cache->MSHR.occupancy, m_parent_cache->MSHR.SIZE);

        do_lookahead = 0;
        for (uint32_t i = pf_q_head; i < pf_q_tail; i++) {

            uint64_t pf_addr = (base_addr & ~(BLOCK_SIZE - 1)) + (delta_q[i] << LOG2_BLOCK_SIZE);
            int32_t perc_sum   = perc_sum_q[i];

            SPP_DP(
                cout << "[ChampSim] State of features: \nTrain addr: " << train_addr << "\tCurr IP: " << curr_ip << "\tIP_1: " << GHR.ip_1 << "\tIP_2: " << GHR.ip_2 << "\tIP_3: " << GHR.ip_3 << "\tDelta: " << train_delta + delta_q[i] << "\t:LastSig " << last_sig << "\t:CurrSig " << curr_sig << "\t:Conf " << confidence_q[i] << "\t:Depth " << depth << "\tSUM: "<< perc_sum  << endl;
            );
            FILTER_REQUEST fill_level = (perc_sum >= knob::ppf_perc_threshold_hi) ? SPP_L2C_PREFETCH : SPP_LLC_PREFETCH;
            
            if ((addr & ~(PAGE_SIZE - 1)) == (pf_addr & ~(PAGE_SIZE - 1))) { // Prefetch request is in the same physical page
                
                // Filter checks for redundancy and returns FALSE if redundant
                // Else it returns TRUE and logs the features for future retrieval 
                if ( num_pf < ceil(((m_parent_cache->PQ.SIZE)/distinct_pages)) ) {                  
                    if (FILTER.check(pf_addr, train_addr, curr_ip, fill_level, train_delta + delta_q[i], last_sig, curr_sig, confidence_q[i], perc_sum, (depth-1))) {

                        //[DO NOT TOUCH]:   
                        // Use addr (not base_addr) to obey the same physical page boundary
                        m_parent_cache->prefetch_line(ip, addr, pf_addr, ((fill_level == SPP_L2C_PREFETCH) ? FILL_L2 : FILL_LLC),5); 
                        num_pf++;
                        
                        //FILTER.valid_reject[quotient] = 0;
                        if (fill_level == SPP_L2C_PREFETCH) {
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
                            cout << " depth: " << i << " fill_level: " << ((fill_level == SPP_L2C_PREFETCH) ? FILL_L2 : FILL_LLC) << endl;
                        );
                    }
                }   
            } else { // Prefetch request is crossing the physical page boundary
#ifdef GHR_ON
                    // Store this prefetch request in GHR to bootstrap SPP learning when we see a ST miss (i.e., accessing a new page)
                    GHR.update_entry(curr_sig, confidence_q[i], (pf_addr >> LOG2_BLOCK_SIZE) & 0x3F, delta_q[i]); 
#endif
            }
            do_lookahead = 1;
            pf_q_head++;
        }

        // Update base_addr and curr_sig
        if (lookahead_way < PT_WAY) {
            uint32_t set = spp_ppf::get_hash(curr_sig) % PT_SET;
            base_addr += (PT.delta[set][lookahead_way] << LOG2_BLOCK_SIZE);
            prev_delta += PT.delta[set][lookahead_way]; 

            // PT.delta uses a 7-bit sign magnitude representation to generate sig_delta
            //int sig_delta = (PT.delta[set][lookahead_way] < 0) ? ((((-1) * PT.delta[set][lookahead_way]) & 0x3F) + 0x40) : PT.delta[set][lookahead_way];
            int sig_delta = (PT.delta[set][lookahead_way] < 0) ? (((-1) * PT.delta[set][lookahead_way]) + (1 << (SIG_DELTA_BIT - 1))) : PT.delta[set][lookahead_way];
            curr_sig = ((curr_sig << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
        }

        SPP_DP (
            cout << "Looping curr_sig: " << hex << curr_sig << " base_addr: " << base_addr << dec;
            cout << " pf_q_head: " << pf_q_head << " pf_q_tail: " << pf_q_tail << " depth: " << depth << endl;
        );
#ifdef LOOKAHEAD_ON
    } while (do_lookahead);
#endif
}

void SPP_PPF_dev::cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
#ifdef FILTER_ON
    SPP_DP (cout << endl;);
    FILTER.check(evicted_addr, 0, 0, L2C_EVICT, 0, 0, 0, 0, 0, 0);
#endif
}

void SPP_PPF_dev::dump_stats()
{

}