#include <algorithm>
#include "stride.h"
#include "champsim.h"

namespace knob
{
   extern uint32_t stride_num_trackers;
   extern uint32_t stride_pref_degree;
}

void StridePrefetcher::init_knobs()
{

}

void StridePrefetcher::init_stats()
{
   bzero(&stats, sizeof(stats));
}

StridePrefetcher::StridePrefetcher(string type) : Prefetcher(type)
{

}

StridePrefetcher::~StridePrefetcher()
{

}

void StridePrefetcher::print_config()
{
   cout << "stride_num_trackers " << knob::stride_num_trackers << endl
      << "stride_pref_degree " << knob::stride_pref_degree << endl
      ;
}

void StridePrefetcher::invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr)
{
   // uint64_t page = address >> LOG2_PAGE_SIZE;
   uint64_t cl_addr = address >> LOG2_BLOCK_SIZE;
	// uint32_t offset = (address >> LOG2_BLOCK_SIZE) & ((1ull << (LOG2_PAGE_SIZE - LOG2_BLOCK_SIZE)) - 1);

   stats.tracker.lookup++;

   Tracker *tracker = NULL;
   auto it = find_if(trackers.begin(), trackers.end(), [pc](Tracker *t){return t->pc == pc;});
   if(it == trackers.end())
   {
      if(trackers.size() >= knob::stride_num_trackers)
      {
         /* evict */
         Tracker *victim = trackers.back();
         trackers.pop_back();
         delete victim;
         stats.tracker.evict++;
      }

      tracker = new Tracker();
      tracker->pc = pc;
      tracker->last_cl_addr = cl_addr;
      tracker->last_stride = 0;
      trackers.push_front(tracker);
      stats.tracker.insert++;
      return;
   }

   stats.tracker.hit++;
   int32_t stride = 0;
   tracker = (*it);
   if(cl_addr > tracker->last_cl_addr)
   {
      stride = cl_addr - tracker->last_cl_addr;
      stats.stride.pos++;
   }
   else if(cl_addr < tracker->last_cl_addr)
   {
      stride = tracker->last_cl_addr - cl_addr;
      stride *= (-1);
      stats.stride.neg++;
   }

   if(stride == 0)
   {
      stats.stride.zero++;
      return;
   }

   if(stride == tracker->last_stride)
   {
      stats.pref.stride_match++;
      uint32_t count = generate_prefetch(address, stride, pref_addr);
      stats.pref.generated += count;
   }

   /* update tracker */
   tracker->last_stride = stride;
   tracker->last_cl_addr = cl_addr;
   trackers.erase(it);
   trackers.push_front(tracker);
}

uint32_t StridePrefetcher::generate_prefetch(uint64_t address, int32_t stride, vector<uint64_t> &pref_addr)
{
   uint64_t page = address >> LOG2_PAGE_SIZE;
	uint32_t offset = (address >> LOG2_BLOCK_SIZE) & ((1ull << (LOG2_PAGE_SIZE - LOG2_BLOCK_SIZE)) - 1);
   uint32_t count = 0;

   for(uint32_t deg = 0; deg < knob::stride_pref_degree; ++deg)
   {
      int32_t pref_offset = offset + stride*deg;
      if(pref_offset >= 0 && pref_offset < 64) /* bounds check */
      {
         uint64_t addr = (page << LOG2_PAGE_SIZE) + (pref_offset << LOG2_BLOCK_SIZE);
         pref_addr.push_back(addr);
         count++;
      }
      else
      {
         break;
      }
   }

   assert(count <= knob::stride_pref_degree);
   return count;
}

void StridePrefetcher::dump_stats()
{
   cout << "stride_tracker_lookup " << stats.tracker.lookup << endl
      << "stride_tracker_evict " << stats.tracker.evict << endl
      << "stride_tracker_insert " << stats.tracker.insert << endl
      << "stride_tracker_hit " << stats.tracker.hit << endl
      << "stride_stride_pos " << stats.stride.pos << endl
      << "stride_stride_neg " << stats.stride.neg << endl
      << "stride_stride_zero " << stats.stride.zero << endl
      << "stride_pref_stride_match " << stats.pref.stride_match << endl
      << "stride_pref_generated " << stats.pref.generated << endl
      << endl;
}
