#ifndef AMPM_H
#define AMPM_H

#include <deque>
#include <vector>
#include "prefetcher.h"
#include "bitmap.h"
using namespace std;

#define MAX_OFFSETS 64

class AMPM_PB_Entry
{
public:
    uint64_t page_id;
    Bitmap bitmap;

    AMPM_PB_Entry()
    {
        page_id = 0xdeadbeef;
        bitmap.reset();
    }
    ~AMPM_PB_Entry(){}
};


class AMPM : public Prefetcher 
{
private:
    deque<AMPM_PB_Entry*> page_buffer;
    deque<uint64_t> pref_buffer; 

    struct 
    {
        uint64_t invoke_called;
        struct
        {
            uint64_t hit;
            uint64_t evict;
            uint64_t insert;
        } pb;

        struct
        {
            uint64_t pos_histogram[MAX_OFFSETS];
            uint64_t neg_histogram[MAX_OFFSETS];
            uint64_t total;
            uint64_t degree_reached_pos;
            uint64_t degree_reached_neg;
        } pred;

        struct
        {
            uint64_t hit;
            uint64_t dropped;
            uint64_t insert;
            uint64_t issued;
        } pref_buffer;

        struct
        {
            uint64_t total;
        } pref;

    } stats;

private:
    void init_knobs();
    void init_stats();
    void buffer_prefetch(vector<uint64_t> predicted_addrs);
    void issue_prefetch(vector<uint64_t> &pref_addr);

public:
    AMPM(string type);
    ~AMPM();
    void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
    void dump_stats();
    void print_config();
};

#endif /* AMPM_H */

