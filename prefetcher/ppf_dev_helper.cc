#include <iostream>
#include "ppf_dev_helper.h"

using namespace spp_ppf;

namespace knob
{
    extern int32_t ppf_perc_threshold_hi;
    extern int32_t ppf_perc_threshold_lo;
}

// TODO: Find a good 64-bit hash function
uint64_t spp_ppf::get_hash(uint64_t key)
{
    // Robert Jenkins' 32 bit mix function
    key += (key << 12);
    key ^= (key >> 22);
    key += (key << 4);
    key ^= (key >> 9);
    key += (key << 10);
    key ^= (key >> 2);
    key += (key << 7);
    key ^= (key >> 12);

    // Knuth's multiplicative method
    key = (key >> 3) * 2654435761;

    return key;
}

void SIGNATURE_TABLE::read_and_update_sig(uint64_t page, uint32_t page_offset, uint32_t &last_sig, uint32_t &curr_sig, int32_t &delta)
{

    uint32_t set = spp_ppf::get_hash(page) % ST_SET,
             match = ST_WAY,
             partial_page = page & ST_TAG_MASK;
    uint8_t  ST_hit = 0;
    int      sig_delta = 0;

    SPP_DP (cout << "[ST] " << __func__ << " page: " << hex << page << " partial_page: " << partial_page << dec << endl;);

    // Case 1: Hit
    for (match = 0; match < ST_WAY; match++) {
        if (valid[set][match] && (tag[set][match] == partial_page)) {
            last_sig = sig[set][match];
            delta = page_offset - last_offset[set][match];

            if (delta) {
                // Build a new sig based on 7-bit sign magnitude representation of delta
                //sig_delta = (delta < 0) ? ((((-1) * delta) & 0x3F) + 0x40) : delta;
                sig_delta = (delta < 0) ? (((-1) * delta) + (1 << (SIG_DELTA_BIT - 1))) : delta;
                sig[set][match] = ((last_sig << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
                curr_sig = sig[set][match];
                last_offset[set][match] = page_offset;

                SPP_DP (
                    cout << "[ST] " << __func__ << " hit set: " << set << " way: " << match;
                    cout << " valid: " << valid[set][match] << " tag: " << hex << tag[set][match];
                    cout << " last_sig: " << last_sig << " curr_sig: " << curr_sig;
                    cout << " delta: " << dec << delta << " last_offset: " << page_offset << endl;
                );
            } else last_sig = 0; // Hitting the same cache line, delta is zero

            ST_hit = 1;
            break;
        }
    }

    // Case 2: Invalid
    if (match == ST_WAY) {
        for (match = 0; match < ST_WAY; match++) {
            if (valid[set][match] == 0) {
                valid[set][match] = 1;
                tag[set][match] = partial_page;
                sig[set][match] = 0;
                curr_sig = sig[set][match];
                last_offset[set][match] = page_offset;

                SPP_DP (
                    cout << "[ST] " << __func__ << " invalid set: " << set << " way: " << match;
                    cout << " valid: " << valid[set][match] << " tag: " << hex << partial_page;
                    cout << " sig: " << sig[set][match] << " last_offset: " << dec << page_offset << endl;
                );

                break;
            }
        }
    }

    // Case 3: Miss
    if (match == ST_WAY) {
        for (match = 0; match < ST_WAY; match++) {
            if (lru[set][match] == ST_WAY - 1) { // Find replacement victim
                tag[set][match] = partial_page;
                sig[set][match] = 0;
                curr_sig = sig[set][match];
                last_offset[set][match] = page_offset;

                SPP_DP (
                    cout << "[ST] " << __func__ << " miss set: " << set << " way: " << match;
                    cout << " valid: " << valid[set][match] << " victim tag: " << hex << tag[set][match] << " new tag: " << partial_page;
                    cout << " sig: " << sig[set][match] << " last_offset: " << dec << page_offset << endl;
                );

                break;
            }
        }

        #ifdef SPP_SANITY_CHECK
        // Assertion
        if (match == ST_WAY) {
            cout << "[ST] Cannot find a replacement victim!" << endl;
            assert(0);
        }
        #endif
    }

#ifdef GHR_ON
    if (ST_hit == 0) {
        uint32_t GHR_found = ghr->check_entry(page_offset);
        if (GHR_found < MAX_GHR_ENTRY) {
            sig_delta = (ghr->delta[GHR_found] < 0) ? (((-1) * ghr->delta[GHR_found]) + (1 << (SIG_DELTA_BIT - 1))) : ghr->delta[GHR_found];
            sig[set][match] = ((ghr->sig[GHR_found] << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
            curr_sig = sig[set][match];
        }
    }
#endif

    // Update LRU
    for (uint32_t way = 0; way < ST_WAY; way++) {
        if (lru[set][way] < lru[set][match]) {
            lru[set][way]++;

            #ifdef SPP_SANITY_CHECK
            // Assertion
            if (lru[set][way] >= ST_WAY) {
                cout << "[ST] LRU value is wrong! set: " << set << " way: " << way << " lru: " << lru[set][way] << endl;
                assert(0);
            }
            #endif
        }
    }
    lru[set][match] = 0; // Promote to the MRU position
}

void PATTERN_TABLE::update_pattern(uint32_t last_sig, int curr_delta)
{
    // Update (sig, delta) correlation
    uint32_t set = spp_ppf::get_hash(last_sig) % PT_SET,
             match = 0;

    // Case 1: Hit
    for (match = 0; match < PT_WAY; match++) {
        if (delta[set][match] == curr_delta) {
            c_delta[set][match]++;
            c_sig[set]++;
            if (c_sig[set] > C_SIG_MAX) {
                for (uint32_t way = 0; way < PT_WAY; way++)
                    c_delta[set][way] >>= 1;
                c_sig[set] >>= 1;
            }

            SPP_DP (
                cout << "[PT] " << __func__ << " hit sig: " << hex << last_sig << dec << " set: " << set << " way: " << match;
                cout << " delta: " << delta[set][match] << " c_delta: " << c_delta[set][match] << " c_sig: " << c_sig[set] << endl;
            );

            break;
        }
    }

    // Case 2: Miss
    if (match == PT_WAY) {
        uint32_t victim_way = PT_WAY,
                 min_counter = C_SIG_MAX;

        for (match = 0; match < PT_WAY; match++) {
            if (c_delta[set][match] < min_counter) { // Select an entry with the minimum c_delta
                victim_way = match;
                min_counter = c_delta[set][match];
            }
        }

        delta[set][victim_way] = curr_delta;
        c_delta[set][victim_way] = 0;
        c_sig[set]++;
        if (c_sig[set] > C_SIG_MAX) {
            for (uint32_t way = 0; way < PT_WAY; way++)
                c_delta[set][way] >>= 1;
            c_sig[set] >>= 1;
        }

        SPP_DP (
            cout << "[PT] " << __func__ << " miss sig: " << hex << last_sig << dec << " set: " << set << " way: " << victim_way;
            cout << " delta: " << delta[set][victim_way] << " c_delta: " << c_delta[set][victim_way] << " c_sig: " << c_sig[set] << endl;
        );

        #ifdef SPP_SANITY_CHECK
        // Assertion
        if (victim_way == PT_WAY) {
            cout << "[PT] Cannot find a replacement victim!" << endl;
            assert(0);
        }
        #endif
    }
}

void PATTERN_TABLE::read_pattern(uint32_t curr_sig, int *delta_q, uint32_t *confidence_q, int32_t *perc_sum_q, uint32_t &lookahead_way, uint32_t &lookahead_conf, uint32_t &pf_q_tail, uint32_t &depth, uint64_t addr, uint64_t base_addr, uint64_t train_addr, uint64_t curr_ip, int32_t train_delta, uint32_t last_sig, uint32_t pq_occupancy, uint32_t pq_SIZE, uint32_t mshr_occupancy, uint32_t mshr_SIZE)
{
    // Update (sig, delta) correlation
    uint32_t set = spp_ppf::get_hash(curr_sig) % PT_SET,
             local_conf = 0,
             pf_conf = 0,
             max_conf = 0;
	
	bool found_candidate = false;

    if (c_sig[set]) {
        for (uint32_t way = 0; way < PT_WAY; way++) {
            local_conf = (100 * c_delta[set][way]) / c_sig[set];
            pf_conf = depth ? (ghr->global_accuracy * c_delta[set][way] / c_sig[set] * lookahead_conf / 100) : local_conf;

			int32_t perc_sum = perc->perc_predict(train_addr, curr_ip, ghr->ip_1, ghr->ip_2, ghr->ip_3, train_delta + delta[set][way], last_sig, curr_sig, pf_conf, depth);
			bool do_pf = (perc_sum >= knob::ppf_perc_threshold_lo) ? 1 : 0;
			bool fill_l2 = (perc_sum >= knob::ppf_perc_threshold_hi) ? 1 : 0;

			if (fill_l2 && (mshr_occupancy >= mshr_SIZE || pq_occupancy >= pq_SIZE) )
				continue;
			// Now checking against the L2C_MSHR_SIZE
			// Saving some slots in the internal PF queue by checking against do_pf
            if (pf_conf && do_pf && pf_q_tail < 100 ) {

				confidence_q[pf_q_tail] = pf_conf;
            	delta_q[pf_q_tail] = delta[set][way];
				perc_sum_q[pf_q_tail] = perc_sum;
				//cout << "WAY:  "<< way << "\tPF_CONF: " << pf_conf <<  "\tIndex: " << pf_q_tail << endl;
				SPP_DP (
					cout << "[PT] State of Features: \nTrain addr: " << train_addr << "\tCurr IP: " << curr_ip << "\tIP_1: " << ghr->ip_1 << "\tIP_2: " << ghr->ip_2 << "\tIP_3: " << ghr->ip_3 << "\tDelta: " << train_delta + delta[set][way] << "\tLastSig: " << last_sig << "\tCurrSig: " << curr_sig << "\tConf: " << pf_conf << "\tDepth: " << depth << "\tSUM: "<< perc_sum  << endl;
				);
            	// Lookahead path follows the most confident entry
            	if (pf_conf > max_conf) {
            	    lookahead_way = way;
            	    max_conf = pf_conf;
            	}
            	pf_q_tail++;
				found_candidate = true;
                
                SPP_DP (
                    cout << "[PT] " << __func__ << " HIGH CONF: " << pf_conf << " sig: " << hex << curr_sig << dec << " set: " << set << " way: " << way;
                    cout << " delta: " << delta[set][way] << " c_delta: " << c_delta[set][way] << " c_sig: " << c_sig[set];
                    cout << " conf: " << local_conf << " pf_q_tail: " << (pf_q_tail-1) << " depth: " << depth << endl;
                );
            } else {
                SPP_DP (
                    cout << "[PT] " << __func__ << "  LOW CONF: " << pf_conf << " sig: " << hex << curr_sig << dec << " set: " << set << " way: " << way;
                    cout << " delta: " << delta[set][way] << " c_delta: " << c_delta[set][way] << " c_sig: " << c_sig[set];
                    cout << " conf: " << local_conf << " pf_q_tail: " << (pf_q_tail) << " depth: " << depth << endl;
                );
            }
			// Recording Perc negatives
            if (pf_conf && pf_q_tail < L2C_MSHR_SIZE && (perc_sum < knob::ppf_perc_threshold_hi) ) {
				// Note: Using knob::ppf_perc_threshold_hi as the decising factor for negative case
				// Because 'trueness' of a prefetch is decisded based on the feedback from L2C
				// So even though LLC prefetches go through, they are treated as false wrt L2C in this case
				uint64_t pf_addr = (base_addr & ~(BLOCK_SIZE - 1)) + (delta[set][way] << LOG2_BLOCK_SIZE);
    			
                if ((addr & ~(PAGE_SIZE - 1)) == (pf_addr & ~(PAGE_SIZE - 1))) { // Prefetch request is in the same physical page
                	filter->check(pf_addr, train_addr, curr_ip, SPP_PERC_REJECT, train_delta + delta[set][way], last_sig, curr_sig, pf_conf, perc_sum, depth);
				}
			}
        }
        lookahead_conf = max_conf;
        if (found_candidate) depth++;

        SPP_DP (cout << "global_accuracy: " << ghr->global_accuracy << " lookahead_conf: " << lookahead_conf << endl;);
    } else confidence_q[pf_q_tail] = 0;
}

bool PREFETCH_FILTER::check(uint64_t check_addr, uint64_t base_addr, uint64_t ip, FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth)
{
    uint64_t cache_line = check_addr >> LOG2_BLOCK_SIZE,
             hash = spp_ppf::get_hash(cache_line);
	
	//MAIN FILTER
	uint64_t quotient = (hash >> REMAINDER_BIT) & ((1 << QUOTIENT_BIT) - 1),
             remainder = hash % (1 << REMAINDER_BIT);
	
	//REJECT FILTER
	uint64_t quotient_reject = (hash >> REMAINDER_BIT_REJ) & ((1 << QUOTIENT_BIT_REJ) - 1),
             remainder_reject = hash % (1 << REMAINDER_BIT_REJ);

    SPP_DP (
        cout << "[FILTER] check_addr: " << hex << check_addr << " check_cache_line: " << (check_addr >> LOG2_BLOCK_SIZE);
		cout << " request type: " << filter_request;
        cout << " hash: " << hash << dec << " quotient: " << quotient << " remainder: " << remainder << endl;
    );

    switch (filter_request) {
		
		case SPP_PERC_REJECT: // To see what would have been the prediction given perceptron has rejected the PF
            if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) { 
				// We want to check if the prefetch would have gone through had perc not rejected
				// So even in perc reject case, I'm checking in the accept filter for redundancy
                SPP_DP (
                    cout << "[FILTER] " << __func__ << " line is already in the filter check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
                    cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
                );
                return false; // False return indicates "Do not prefetch"
            } else {
				valid_reject[quotient_reject] = 1;
				remainder_tag_reject[quotient_reject] = remainder_reject;

				// Logging perc features
				address_reject[quotient_reject] = base_addr;
				pc_reject[quotient_reject] = ip;
				pc_1_reject[quotient_reject] = ghr->ip_1;
				pc_2_reject[quotient_reject] = ghr->ip_2;
				pc_3_reject[quotient_reject] = ghr->ip_3;
				delta_reject[quotient_reject] = cur_delta;
				perc_sum_reject[quotient_reject] = sum;
				last_signature_reject[quotient_reject] = last_sig;
				cur_signature_reject[quotient_reject] = curr_sig;
				confidence_reject[quotient_reject] = conf;
				la_depth_reject[quotient_reject] = depth;

				SPP_DP (
                    cout << "[FILTER] " << __func__ << " PF rejected by perceptron. Set valid_reject for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
                    cout << " quotient: " << quotient << " remainder_tag: " << remainder_tag_reject[quotient_reject] << endl; 
					cout << " More Recorded Metadata: Addr: " << hex << address_reject[quotient_reject] << dec << " PC: " << pc_reject[quotient_reject] << " Delta: " << delta_reject[quotient_reject] << " Last Signature: " << last_signature_reject[quotient_reject] << " Current Signature: " << cur_signature_reject[quotient_reject] << " Confidence: " << confidence_reject[quotient_reject] << endl;
                );
			}
			break;
		
		case SPP_L2C_PREFETCH:
            if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) { 
                SPP_DP (
                    cout << "[FILTER] " << __func__ << " line is already in the filter check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
                    cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
                );

                return false; // False return indicates "Do not prefetch"
            } else {

                valid[quotient] = 1;  // Mark as prefetched
                useful[quotient] = 0; // Reset useful bit
                remainder_tag[quotient] = remainder;

				// Logging perc features
				delta[quotient] = cur_delta;
				pc[quotient] = ip;
				pc_1[quotient] = ghr->ip_1;
				pc_2[quotient] = ghr->ip_2;
				pc_3[quotient] = ghr->ip_3;
				last_signature[quotient] = last_sig; 
				cur_signature[quotient] = curr_sig;
				confidence[quotient] = conf;
				address[quotient] = base_addr; 
				perc_sum[quotient] = sum;
				la_depth[quotient] = depth;
				
				SPP_DP (
                    cout << "[FILTER] " << __func__ << " set valid for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
                    cout << " quotient: " << quotient << " remainder_tag: " << remainder_tag[quotient] << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
					cout << " More Recorded Metadata: Addr:" << hex << address[quotient] << dec << " PC: " << pc[quotient] << " Delta: " << delta[quotient] << " Last Signature: " << last_signature[quotient] << " Current Signature: " << cur_signature[quotient] << " Confidence: " << confidence[quotient] << endl;
                );
            }
            break;

        case SPP_LLC_PREFETCH:
            if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) { 
                SPP_DP (
                    cout << "[FILTER] " << __func__ << " line is already in the filter check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
                    cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
                );

                return false; // False return indicates "Do not prefetch"
            } else {
                // NOTE: SPP_LLC_PREFETCH has relatively low confidence 
                // Therefore, it is safe to prefetch this cache line in the large LLC and save precious L2C capacity
                // If this prefetch request becomes more confident and SPP eventually issues SPP_L2C_PREFETCH,
                // we can get this cache line immediately from the LLC (not from DRAM)
                // To allow this fast prefetch from LLC, SPP does not set the valid bit for SPP_LLC_PREFETCH
				
				SPP_DP (
                    cout << "[FILTER] " << __func__ << " don't set valid for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
                    cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
                );
            }
            break;

        case L2C_DEMAND:
            if ((remainder_tag[quotient] == remainder) && (useful[quotient] == 0)) {
                useful[quotient] = 1;
                if (valid[quotient]) {
					ghr->pf_useful++; // This cache line was prefetched by SPP and actually used in the program
				}

                SPP_DP (
                    cout << "[FILTER] " << __func__ << " set useful for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
                    cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient];
                    cout << " ghr->pf_issued: " << ghr->pf_issued << " ghr->pf_useful: " << ghr->pf_useful << endl; 
					if (valid[quotient])
						cout << " Calling Perceptron Update (INC) as L2C_DEMAND was useful" << endl;
                );

                if (valid[quotient]) {
					// Prefetch leads to a demand hit
					perc->perc_update(address[quotient], pc[quotient], pc_1[quotient], pc_2[quotient], pc_3[quotient], delta[quotient], last_signature[quotient], cur_signature[quotient], confidence[quotient], la_depth[quotient], 1, perc_sum[quotient]);
				}
            }
			//If NOT Prefetched
			if (!(valid[quotient] && remainder_tag[quotient] == remainder)) {
				// AND If Rejected by Perc
				if (valid_reject[quotient_reject] && remainder_tag_reject[quotient_reject] == remainder_reject) {
               		 SPP_DP (
               		     cout << "[FILTER] " << __func__ << " not doing anything for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
               		     cout << " quotient: " << quotient << " valid_reject:" << valid_reject[quotient_reject];
               		     cout << " ghr->pf_issued: " << ghr->pf_issued << " ghr->pf_useful: " << ghr->pf_useful << endl; 
			   		 	 cout << " Calling Perceptron Update (DEC) as a useful L2C_DEMAND was rejected and reseting valid_reject" << endl;
               		 );
					// Not prefetched but could have been a good idea to prefetch
					perc->perc_update(address_reject[quotient_reject], pc_reject[quotient_reject], pc_1_reject[quotient_reject], pc_2_reject[quotient_reject], pc_3_reject[quotient_reject], delta_reject[quotient_reject], last_signature_reject[quotient_reject], cur_signature_reject[quotient_reject], confidence_reject[quotient_reject], la_depth_reject[quotient_reject], 0, perc_sum_reject[quotient_reject]);
					valid_reject[quotient_reject] = 0;
					remainder_tag_reject[quotient_reject] = 0;
				}
			}
            break;

        case L2C_EVICT:
            // Decrease global pf_useful counter when there is a useless prefetch (prefetched but not used)
            if (valid[quotient] && !useful[quotient]) {
				if (ghr->pf_useful) 
					ghr->pf_useful--;
				
				SPP_DP (
               		cout << "[FILTER] " << __func__ << " eviction for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
               		cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
					cout << " Calling Perceptron Update (DEC) as L2C_DEMAND was not useful" << endl;
					cout << " Reseting valid_reject" << endl;
            	);

				// Prefetch leads to eviction
				perc->perc_update(address[quotient], pc[quotient], pc_1[quotient], pc_2[quotient], pc_3[quotient], delta[quotient], last_signature[quotient], cur_signature[quotient], confidence[quotient], la_depth[quotient], 0, perc_sum[quotient]);
			}
            // Reset filter entry
            valid[quotient] = 0;
            useful[quotient] = 0;
            remainder_tag[quotient] = 0;

			// Reset reject filter too
			valid_reject[quotient_reject] = 0;
			remainder_tag_reject[quotient_reject] = 0;

            break;

        default:
            // Assertion
            cout << "[FILTER] Invalid filter request type: " << filter_request << endl;
            assert(0);
    }

    return true;
}

void GLOBAL_REGISTER::update_entry(uint32_t pf_sig, uint32_t pf_confidence, uint32_t pf_offset, int pf_delta) 
{
    // NOTE: GHR implementation is slightly different from the original paper
    // Instead of matching (last_offset + delta), GHR simply stores and matches the pf_offset
    uint32_t min_conf = 100,
             victim_way = MAX_GHR_ENTRY;

    SPP_DP (
        cout << "[GHR] Crossing the page boundary pf_sig: " << hex << pf_sig << dec;
        cout << " confidence: " << pf_confidence << " pf_offset: " << pf_offset << " pf_delta: " << pf_delta << endl;
    );

    for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
        //if (sig[i] == pf_sig) { // TODO: Which one is better and consistent?
            // If GHR already holds the same pf_sig, update the GHR entry with the latest info
        if (valid[i] && (offset[i] == pf_offset)) {
            // If GHR already holds the same pf_offset, update the GHR entry with the latest info
            sig[i] = pf_sig;
            confidence[i] = pf_confidence;
            //offset[i] = pf_offset;
            delta[i] = pf_delta;

            SPP_DP (cout << "[GHR] Found a matching index: " << i << endl;);

            return;
        }

        // GHR replacement policy is based on the stored confidence value
        // An entry with the lowest confidence is selected as a victim
        if (confidence[i] < min_conf) {
            min_conf = confidence[i];
            victim_way = i;
        }
    }

    // Assertion
    if (victim_way >= MAX_GHR_ENTRY) {
        cout << "[GHR] Cannot find a replacement victim!" << endl;
        assert(0);
    }

    SPP_DP (
        cout << "[GHR] Replace index: " << victim_way << " pf_sig: " << hex << sig[victim_way] << dec;
        cout << " confidence: " << confidence[victim_way] << " pf_offset: " << offset[victim_way] << " pf_delta: " << delta[victim_way] << endl;
    );

    valid[victim_way] = 1;
    sig[victim_way] = pf_sig;
    confidence[victim_way] = pf_confidence;
    offset[victim_way] = pf_offset;
    delta[victim_way] = pf_delta;
}

uint32_t GLOBAL_REGISTER::check_entry(uint32_t page_offset)
{
    uint32_t max_conf = 0,
             max_conf_way = MAX_GHR_ENTRY;

    for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
        if ((offset[i] == page_offset) && (max_conf < confidence[i])) {
            max_conf = confidence[i];
            max_conf_way = i;
        }
    }

    return max_conf_way;
}

void PERCEPTRON::get_perc_index(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, uint64_t perc_set[PERC_FEATURES])
{
	// Returns the imdexes for the perceptron tables
    uint64_t cache_line = base_addr >> LOG2_BLOCK_SIZE,
			 page_addr  = base_addr >> LOG2_PAGE_SIZE;

	int sig_delta = (cur_delta < 0) ? (((-1) * cur_delta) + (1 << (SIG_DELTA_BIT - 1))) : cur_delta;
	uint64_t  pre_hash[PERC_FEATURES];

	pre_hash[0] = base_addr;
	pre_hash[1] = cache_line;
	pre_hash[2] = page_addr;
	pre_hash[3] = confidence ^ page_addr;
	pre_hash[4] = curr_sig ^ sig_delta;
	pre_hash[5] = ip_1 ^ (ip_2>>1) ^ (ip_3>>2);
	pre_hash[6] = ip ^ depth;
	pre_hash[7] = ip ^ sig_delta;
	pre_hash[8] = confidence;

	for (int i = 0; i < PERC_FEATURES; i++) {
		perc_set[i] = (pre_hash[i]) % PERC_DEPTH[i]; // Variable depths
		SPP_DP (
			cout << "  Perceptron Set Index#: " << i << " = " <<  perc_set[i];
		);
	}
	SPP_DP (
		cout << endl;
	);		
}

int32_t	PERCEPTRON::perc_predict(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth)
{
	SPP_DP (
		int sig_delta = (cur_delta < 0) ? (((-1) * cur_delta) + (1 << (SIG_DELTA_BIT - 1))) : cur_delta;
		cout << "[PERC_PRED] Current IP: " << ip << "  and  Memory Adress: " << hex << base_addr << endl;
		cout << " Last Sig: " << last_sig << " Curr Sig: " << curr_sig << dec << endl;
		cout << " Cur Delta: " << cur_delta << " Sign Delta: " << sig_delta << " Confidence: " << confidence<< endl;
		cout << " ";
	);

	uint64_t perc_set[PERC_FEATURES];
	// Get the indexes in perc_set[]
	get_perc_index(base_addr, ip, ip_1, ip_2, ip_3, cur_delta, last_sig, curr_sig, confidence, depth, perc_set);
	
	int32_t sum = 0;
	for (int i = 0; i < PERC_FEATURES; i++) {
		sum += perc_weights[perc_set[i]][i];	
		// Calculate Sum
	}
	SPP_DP (
		cout << " Sum of perceptrons: " << sum << " Prediction made: " << ((sum >= knob::ppf_perc_threshold_lo) ?  ((sum >= knob::ppf_perc_threshold_hi) ? FILL_L2 : FILL_LLC) : 0)  << endl;
	);
	// Return the sum
	return sum;
}

void 	PERCEPTRON::perc_update(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, bool direction, int32_t perc_sum)
{
	SPP_DP (
		int sig_delta = (cur_delta < 0) ? (((-1) * cur_delta) + (1 << (SIG_DELTA_BIT - 1))) : cur_delta;
		cout << "[PERC_UPD] (Recorded) IP: " << ip << "  and  Memory Adress: " << hex << base_addr << endl;
		cout << " Last Sig: " << last_sig << " Curr Sig: " << curr_sig << dec << endl;
		cout << " Cur Delta: " << cur_delta << " Sign Delta: " << sig_delta << " Confidence: "<< confidence << " Update Direction: " << direction << endl;
		cout << " ";
	);

	uint64_t perc_set[PERC_FEATURES];
	// Get the perceptron indexes
	get_perc_index(base_addr, ip, ip_1, ip_2, ip_3, cur_delta, last_sig, curr_sig, confidence, depth, perc_set);
	
	int32_t sum = 0;
	// Restore the sum that led to the prediction
	sum = perc_sum;
	
	if (!direction) { // direction = 1 means the sum was in the correct direction, 0 means it was in the wrong direction
		// Prediction wrong
		for (int i = 0; i < PERC_FEATURES; i++) {
			if (sum >= knob::ppf_perc_threshold_hi) {
				// Prediction was to prefectch -- so decrement counters
				if (perc_weights[perc_set[i]][i] > -1*(PERC_COUNTER_MAX+1) )
					perc_weights[perc_set[i]][i]--;
			}
			if (sum < knob::ppf_perc_threshold_hi) {
				// Prediction was to not prefetch -- so increment counters
				if (perc_weights[perc_set[i]][i] < PERC_COUNTER_MAX)
					perc_weights[perc_set[i]][i]++;
			}
		}
		SPP_DP (
			int differential = (sum >= knob::ppf_perc_threshold_hi) ? -1 : 1;
			cout << " Direction is: " << direction << " and sum is:" << sum;
			cout << " Overall Differential: " << differential << endl;
		);
	}
	if (direction && sum > NEG_UPDT_THRESHOLD && sum < POS_UPDT_THRESHOLD) {
		// Prediction correct but sum not 'saturated' enough
		for (int i = 0; i < PERC_FEATURES; i++) {
			if (sum >= knob::ppf_perc_threshold_hi) {
				// Prediction was to prefetch -- so increment counters
				if (perc_weights[perc_set[i]][i] < PERC_COUNTER_MAX)
					perc_weights[perc_set[i]][i]++;
			}
			if (sum < knob::ppf_perc_threshold_hi) {
				// Prediction was to not prefetch -- so decrement counters
				if (perc_weights[perc_set[i]][i] > -1*(PERC_COUNTER_MAX+1) )
					perc_weights[perc_set[i]][i]--;
			}
		}
		SPP_DP (
			int differential = 0;
			if (sum >= knob::ppf_perc_threshold_hi) differential =  1;
			if (sum  < knob::ppf_perc_threshold_hi) differential = -1;
			cout << " Direction is: " << direction << " and sum is:" << sum;
			cout << " Overall Differential: " << differential << endl;
		);
	}
}