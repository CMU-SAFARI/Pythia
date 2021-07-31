#ifndef STRIDE_H
#define STRIDE_H

#include <deque>
#include <vector>
#include "prefetcher.h"

using namespace std;

class Tracker
{
  public:
    uint64_t pc;
    uint64_t last_cl_addr;
    int64_t last_stride;

    Tracker()
    {
        pc = 0;
        last_cl_addr = 0;
        last_stride = 0;
    };
};

class StridePrefetcher : public Prefetcher
{
private:
   deque<Tracker*> trackers;

   /* stats */
   struct
   {
      struct
      {
         uint64_t lookup;
         uint64_t evict;
         uint64_t insert;
         uint64_t hit;
      } tracker;

      struct
      {
         uint64_t pos;
         uint64_t neg;
         uint64_t zero;
      } stride;

      struct
      {
         uint64_t stride_match;
         uint64_t generated;
      } pref;

   } stats;

private:
   void init_knobs();
   void init_stats();
   uint32_t generate_prefetch(uint64_t address, int32_t stride, vector<uint64_t> &pref_addr);

public:
   StridePrefetcher(string type);
   ~StridePrefetcher();
   void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);
   void dump_stats();
   void print_config();
};


#endif /* STRIDE_H */
