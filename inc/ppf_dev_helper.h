#ifndef PPF_DEV_HELPER_H
#define PPF_DEV_HELPER_H

#include <iostream>
#include <fstream>
#include <iomanip>      // std::setw
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <cmath>
#include "cache.h"

using namespace std;

namespace spp_ppf {

// SPP functional knobs
#define LOOKAHEAD_ON
#define FILTER_ON
#define GHR_ON
#define SPP_SANITY_CHECK

//#define SPP_DEBUG_PRINT
#ifdef SPP_DEBUG_PRINT
#define SPP_DP(x) x
#else
#define SPP_DP(x)
#endif

//#define SPP_PERC_WGHT
#ifdef SPP_PERC_WGHT
#define SPP_PW(x) x
#else 
#define SPP_PW(x)
#endif

// Signature table parameters
#define ST_SET 1
#define ST_WAY 256
#define ST_TAG_BIT 16
#define ST_TAG_MASK ((1 << ST_TAG_BIT) - 1)
#define SIG_SHIFT 3
#define SIG_BIT 12
#define SIG_MASK ((1 << SIG_BIT) - 1)
#define SIG_DELTA_BIT 7

// Pattern table parameters
#define PT_SET 2048
#define PT_WAY 4
#define C_SIG_BIT 4
#define C_DELTA_BIT 4
#define C_SIG_MAX ((1 << C_SIG_BIT) - 1)
#define C_DELTA_MAX ((1 << C_DELTA_BIT) - 1)

// Prefetch filter parameters
#define QUOTIENT_BIT  10
#define REMAINDER_BIT 6
#define HASH_BIT (QUOTIENT_BIT + REMAINDER_BIT + 1)
#define FILTER_SET (1 << QUOTIENT_BIT)

#define QUOTIENT_BIT_REJ  10
#define REMAINDER_BIT_REJ 8
#define HASH_BIT_REJ (QUOTIENT_BIT_REJ + REMAINDER_BIT_REJ + 1)
#define FILTER_SET_REJ (1 << QUOTIENT_BIT_REJ)

// Global register parameters
#define GLOBAL_COUNTER_BIT 10
#define GLOBAL_COUNTER_MAX ((1 << GLOBAL_COUNTER_BIT) - 1) 
#define MAX_GHR_ENTRY 8
#define PAGES_TRACKED 6

// Perceptron paramaters
#define PERC_ENTRIES 4096 //Upto 12-bit addressing in hashed perceptron
#define PERC_FEATURES 9 //Keep increasing based on new features
#define PERC_COUNTER_MAX 15 //-16 to +15: 5 bits counter 
// #define PERC_THRESHOLD_HI  -5
// #define PERC_THRESHOLD_LO  -15
#define POS_UPDT_THRESHOLD  90
#define NEG_UPDT_THRESHOLD -80

enum FILTER_REQUEST {SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT, SPP_PERC_REJECT}; // Request type for prefetch filter

uint64_t get_hash(uint64_t key);

class GLOBAL_REGISTER
{
public:
    // Global counters to calculate global prefetching accuracy
    uint64_t pf_useful,
        pf_issued,
        global_accuracy; // Alpha value in Section III. Equation 3

    // Global History Register (GHR) entries
    uint8_t valid[MAX_GHR_ENTRY];
    uint32_t sig[MAX_GHR_ENTRY],
        confidence[MAX_GHR_ENTRY],
        offset[MAX_GHR_ENTRY];
    int delta[MAX_GHR_ENTRY];

    uint64_t ip_0,
        ip_1,
        ip_2,
        ip_3;

    uint64_t page_tracker[PAGES_TRACKED];

    GLOBAL_REGISTER()
    {
        pf_useful = 0;
        pf_issued = 0;
        global_accuracy = 0;
        ip_0 = 0;
        ip_1 = 0;
        ip_2 = 0;
        ip_3 = 0;

        for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++)
        {
            valid[i] = 0;
            sig[i] = 0;
            confidence[i] = 0;
            offset[i] = 0;
            delta[i] = 0;
        }
    }

    void update_entry(uint32_t pf_sig, uint32_t pf_confidence, uint32_t pf_offset, int pf_delta);
    uint32_t check_entry(uint32_t page_offset);
};

class PERCEPTRON
{
public:
    // Perc Weights
    int32_t perc_weights[PERC_ENTRIES][PERC_FEATURES];

    // CONST depths for different features
    int32_t PERC_DEPTH[PERC_FEATURES];

    PERCEPTRON()
    {
        cout << "\nInitialize PERCEPTRON" << endl;
        cout << "PERC_ENTRIES: " << PERC_ENTRIES << endl;
        cout << "PERC_FEATURES: " << PERC_FEATURES << endl;

        PERC_DEPTH[0] = 2048; //base_addr;
        PERC_DEPTH[1] = 4096; //cache_line;
        PERC_DEPTH[2] = 4096; //page_addr;
        PERC_DEPTH[3] = 4096; //confidence ^ page_addr;
        PERC_DEPTH[4] = 1024; //curr_sig ^ sig_delta;
        PERC_DEPTH[5] = 4096; //ip_1 ^ ip_2 ^ ip_3;
        PERC_DEPTH[6] = 1024; //ip ^ depth;
        PERC_DEPTH[7] = 2048; //ip ^ sig_delta;
        PERC_DEPTH[8] = 128;  //confidence;

        for (int i = 0; i < PERC_ENTRIES; i++)
        {
            for (int j = 0; j < PERC_FEATURES; j++)
            {
                perc_weights[i][j] = 0;
            }
        }
    }

    void perc_update(uint64_t check_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, bool direction, int32_t perc_sum);
    int32_t perc_predict(uint64_t check_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth);
    void get_perc_index(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, uint64_t perc_set[PERC_FEATURES]);
};

class PREFETCH_FILTER
{
public:
    /* cross-reference pointers */
    GLOBAL_REGISTER *ghr;
    PERCEPTRON *perc;

    uint64_t remainder_tag[FILTER_SET],
        pc[FILTER_SET],
        pc_1[FILTER_SET],
        pc_2[FILTER_SET],
        pc_3[FILTER_SET],
        address[FILTER_SET];
    bool valid[FILTER_SET], // Consider this as "prefetched"
        useful[FILTER_SET]; // Consider this as "used"
    int32_t delta[FILTER_SET],
        perc_sum[FILTER_SET];
    uint32_t last_signature[FILTER_SET],
        confidence[FILTER_SET],
        cur_signature[FILTER_SET],
        la_depth[FILTER_SET];

    uint64_t remainder_tag_reject[FILTER_SET_REJ],
        pc_reject[FILTER_SET_REJ],
        pc_1_reject[FILTER_SET_REJ],
        pc_2_reject[FILTER_SET_REJ],
        pc_3_reject[FILTER_SET_REJ],
        address_reject[FILTER_SET_REJ];
    bool valid_reject[FILTER_SET_REJ]; // Entries which the perceptron rejected
    int32_t delta_reject[FILTER_SET_REJ],
        perc_sum_reject[FILTER_SET_REJ];
    uint32_t last_signature_reject[FILTER_SET_REJ],
        confidence_reject[FILTER_SET_REJ],
        cur_signature_reject[FILTER_SET_REJ],
        la_depth_reject[FILTER_SET_REJ];

    PREFETCH_FILTER()
    {
        cout << endl
             << "Initialize PREFETCH FILTER" << endl;
        cout << "FILTER_SET: " << FILTER_SET << endl;

        for (uint32_t set = 0; set < FILTER_SET; set++)
        {
            remainder_tag[set] = 0;
            valid[set] = 0;
            useful[set] = 0;
        }
        for (uint32_t set = 0; set < FILTER_SET_REJ; set++)
        {
            valid_reject[set] = 0;
            remainder_tag_reject[set] = 0;
        }
    }

    bool check(uint64_t pf_addr, uint64_t base_addr, uint64_t ip, FILTER_REQUEST filter_request, int32_t cur_delta, uint32_t last_sign, uint32_t cur_sign, uint32_t confidence, int32_t sum, uint32_t depth);
};

class SIGNATURE_TABLE
{
  public:
    /* cross-reference pointers */
    GLOBAL_REGISTER *ghr;

    bool     valid[ST_SET][ST_WAY];
    uint32_t tag[ST_SET][ST_WAY],
             last_offset[ST_SET][ST_WAY],
             sig[ST_SET][ST_WAY],
             lru[ST_SET][ST_WAY];

    SIGNATURE_TABLE() {
        cout << "Initialize SIGNATURE TABLE" << endl;
        cout << "ST_SET: " << ST_SET << endl;
        cout << "ST_WAY: " << ST_WAY << endl;
        cout << "ST_TAG_BIT: " << ST_TAG_BIT << endl;
        cout << "ST_TAG_MASK: " << hex << ST_TAG_MASK << dec << endl;

        for (uint32_t set = 0; set < ST_SET; set++)
            for (uint32_t way = 0; way < ST_WAY; way++) {
                valid[set][way] = 0;
                tag[set][way] = 0;
                last_offset[set][way] = 0;
                sig[set][way] = 0;
                lru[set][way] = way;
            }
    };

    void read_and_update_sig(uint64_t page, uint32_t page_offset, uint32_t &last_sig, uint32_t &curr_sig, int32_t &delta);
};

class PATTERN_TABLE
{
public:
    /* cross-reference pointers */
    GLOBAL_REGISTER *ghr;
    PERCEPTRON *perc;
    PREFETCH_FILTER *filter;

    int      delta[PT_SET][PT_WAY];
    uint32_t c_delta[PT_SET][PT_WAY],
             c_sig[PT_SET];

    PATTERN_TABLE() {
        cout << endl << "Initialize PATTERN TABLE" << endl;
        cout << "PT_SET: " << PT_SET << endl;
        cout << "PT_WAY: " << PT_WAY << endl;
        cout << "SIG_DELTA_BIT: " << SIG_DELTA_BIT << endl;
        cout << "C_SIG_BIT: " << C_SIG_BIT << endl;
        cout << "C_DELTA_BIT: " << C_DELTA_BIT << endl;

        for (uint32_t set = 0; set < PT_SET; set++) {
            for (uint32_t way = 0; way < PT_WAY; way++) {
                delta[set][way] = 0;
                c_delta[set][way] = 0;
            }
            c_sig[set] = 0;
        }
    }

    void update_pattern(uint32_t last_sig, int curr_delta),
         read_pattern(uint32_t curr_sig, int *prefetch_delta, uint32_t *confidence_q, int32_t *perc_sum_q, uint32_t &lookahead_way, uint32_t &lookahead_conf, uint32_t &pf_q_tail, uint32_t &depth, uint64_t addr, uint64_t base_addr, uint64_t train_addr, uint64_t curr_ip, int32_t train_delta, uint32_t last_sig, uint32_t pq_occupancy, uint32_t pq_SIZE, uint32_t mshr_occupancy, uint32_t mshr_SIZE);
};
}

#endif /* PPF_DEV_HELPER_H */