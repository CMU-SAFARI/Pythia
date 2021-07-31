#ifndef STREAMER_H
#define STREAMER_H

#include <deque>
#include "prefetcher.h"
using namespace std;

class Stream_Tracker
{
public:
    uint64_t page;
    uint32_t last_offset;
    int32_t last_dir; /* +1 means +ve stream, -1 means -ve stream, 0: init */
    uint8_t conf;
    
public:
    Stream_Tracker(uint64_t _page, uint32_t _last_offset)
    {
        page = _page;
        last_offset = _last_offset;
        last_dir = 0;
        conf = 0;
    }
    ~Stream_Tracker(){}
};

class Streamer : public Prefetcher
{
private:
    deque<Stream_Tracker*> trackers;

    struct 
    {
        uint64_t called;
        struct
        {
            uint64_t missed;
            uint64_t evict;
            uint64_t insert;
            uint64_t hit;
            uint64_t same_offset;
            uint64_t dir_match;
            uint64_t dir_mismatch;
        } tracker;
        struct
        {
            uint64_t dir_match;
            uint64_t total;
        } pred;
    } stats;
    
private:
    void init_knobs();
    void init_stats();

public:
    Streamer(string type);
    ~Streamer();
    void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
    void dump_stats();
    void print_config();
};

#endif /* STREAMER_H */

