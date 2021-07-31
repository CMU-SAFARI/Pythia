#ifndef PREF_POWER7_H
#define PREF_POWER7_H

#include "prefetcher.h"
#include "streamer.h"
#include "stride.h"
#include "cache.h"
using namespace std;

typedef enum
{
    Default = 0, /* streamer degree 5, stride disabled */
    Off,
    Shallowest, S_Shallowest,
    Shallow, S_Shallow,
    Medium, S_Medium,
    Deep, S_Deep,
    Deeper, S_Deeper,
    Deepest, S_Deepest,

    NumConfigs
} Config;

typedef enum
{
    Explore = 0,
    Exploit,

    NumModes
} Mode;

class POWER7_Pref : public Prefetcher
{
public:
    CACHE *m_parent_cache;
    Config config;
    Mode mode;
    uint64_t access_counter;
    uint64_t cycle_stats[NumConfigs];
    uint64_t cycle_stamp;
    StridePrefetcher *stride;
    Streamer *streamer;

    /* stats */
    struct 
    {
        uint64_t called;
        struct
        {
            uint64_t explore;
            uint64_t exploit;
            uint64_t explore_to_exploit;
            uint64_t exploit_to_explore;
        } mode;

        struct
        {
            uint64_t histogram[Config::NumConfigs][Mode::NumModes];
        } config;

        struct
        {
            uint64_t total;
            uint64_t streamer;
            uint64_t stride;
        } pred;
    } stats;
    

private:
    void init_knobs();
    void init_stats();
    void set_params();
    uint32_t get_streamer_degree(Config config);
    uint32_t get_stride_degree(Config config);
    bool empty_cycle_stats();
    void clear_cycle_stats();
    Config get_winner_config();
    string get_config_string(Config cfg);
    string get_mode_string(Mode mode);

public : 
    POWER7_Pref(string type, CACHE *cache);
    ~POWER7_Pref();
    void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
    void dump_stats();
    void print_config();
};

#endif /* PREF_POWER7 */

