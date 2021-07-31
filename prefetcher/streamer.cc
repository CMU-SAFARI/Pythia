#include <iostream>
#include <algorithm>
#include "streamer.h"
#include "champsim.h"

namespace knob
{
    extern uint32_t streamer_num_trackers;
    extern uint32_t streamer_pref_degree;
}

void Streamer::init_knobs()
{

}

void Streamer::init_stats()
{
    bzero(&stats, sizeof(stats));
}

void Streamer::print_config()
{
    cout << "streamer_num_trackers " << knob::streamer_num_trackers << endl
        << "streamer_pref_degree " << knob::streamer_pref_degree << endl
        << endl;
}

Streamer::Streamer(string type) : Prefetcher(type)
{
    init_knobs();
    init_stats();
}

Streamer::~Streamer()
{

}

void Streamer::invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr)
{
    uint64_t page = address >> LOG2_PAGE_SIZE;
    uint32_t offset = (address >> LOG2_BLOCK_SIZE) & ((1ull << (LOG2_PAGE_SIZE - LOG2_BLOCK_SIZE)) - 1);

    stats.called++;

    auto it = find_if(trackers.begin(), trackers.end(), [page](Stream_Tracker *tracker){return tracker->page == page;});
    Stream_Tracker *tracker = (it != trackers.end()) ? (*it) : NULL;

    if(!tracker)
    {
        stats.tracker.missed++;
        if(trackers.size() >= knob::streamer_num_trackers)
        {
            tracker = trackers.front();
            trackers.pop_front();
            delete(tracker);
            stats.tracker.evict++;
        }

        tracker = new Stream_Tracker(page, offset);
        trackers.push_back(tracker);
        stats.tracker.insert++;
        return;
    }

    assert(tracker->page == page);
    stats.tracker.hit++;

    if(offset == tracker->last_offset)
    {
        stats.tracker.same_offset++;
        return;
    }

    bool dir_match = false;
    int32_t dir = offset > tracker->last_offset ? +1 : -1;
    if(dir == tracker->last_dir)
    {
        tracker->conf = 1;
        dir_match = true;
        stats.tracker.dir_match++;
    }
    else
    {
        tracker->conf = 0;
        stats.tracker.dir_mismatch++;
    }
    tracker->page = page;
    tracker->last_offset = offset;
    tracker->last_dir = dir;
    /* update recency */
    trackers.erase(it);
    trackers.push_back(tracker);

    /* generate prefetch */
    if(dir_match)
    {
        stats.pred.dir_match++;
        int32_t pref_offset = offset;
        for(uint32_t index = 0; index < knob::streamer_pref_degree; ++index)
        {
            pref_offset = (dir == +1) ? (pref_offset + 1) : (pref_offset - 1);
            if(pref_offset >= 0 && pref_offset < 64)
            {
                uint64_t pf_addr = (page << LOG2_PAGE_SIZE) + (pref_offset << LOG2_BLOCK_SIZE);
                pref_addr.push_back(pf_addr);
            }
            else
            {
                break;
            }
        }
    }

    stats.pred.total += pref_addr.size();
    return;
}

void Streamer::dump_stats()
{
    cout << "streamer.called " << stats.called << endl
         << "streamer.tracker.missed " << stats.tracker.missed << endl
         << "streamer.tracker.evict " << stats.tracker.evict << endl
         << "streamer.tracker.insert " << stats.tracker.insert << endl
         << "streamer.tracker.hit " << stats.tracker.hit << endl
         << "streamer.tracker.same_offset " << stats.tracker.same_offset << endl
         << "streamer.tracker.dir_match " << stats.tracker.dir_match << endl
         << "streamer.tracker.dir_mismatch " << stats.tracker.dir_mismatch << endl
         << "streamer.pred.dir_match " << stats.pred.dir_match << endl
         << "streamer.pred.total " << stats.pred.total << endl
         << endl;
}