#include <iostream>
#include "pref_power7.h"
#include "champsim.h"

namespace knob
{
    extern uint32_t streamer_num_trackers;
    extern uint32_t streamer_pref_degree;
    extern uint32_t stride_num_trackers;
    extern uint32_t stride_pref_degree;
    extern uint32_t power7_explore_epoch;
    extern uint32_t power7_exploit_epoch;
    extern uint32_t power7_default_streamer_degree;
}

string POWER7_Pref::get_mode_string(Mode mode)
{
    switch(mode)
    {
        case Mode::Explore: return string("Explore");
        case Mode::Exploit: return string("Exploit");
        default:            return string("UNK");
    }
}

string POWER7_Pref::get_config_string(Config cfg)
{
    switch (cfg)
    {
        case Config::Default:       return string("Default");
        case Config::Off:           return string("Off");
        case Config::Shallowest:    return string("Shallowest");
        case Config::S_Shallowest:  return string("S_Shallowest");
        case Config::Shallow:       return string("Shallow");
        case Config::S_Shallow:     return string("S_Shallow");
        case Config::Medium:        return string("Medium");
        case Config::S_Medium:      return string("S_Medium");
        case Config::Deep:          return string("Deep");
        case Config::S_Deep:        return string("S_Deep");
        case Config::Deeper:        return string("Deeper");
        case Config::S_Deeper:      return string("S_Deeper");
        case Config::Deepest:       return string("Deepest");
        case Config::S_Deepest:     return string("S_Deepest");
        default:                    return ("UNK");
    }
}

void POWER7_Pref::init_knobs()
{
    // assert(knob::power7_exploit_epoch >= knob::power7_explore_epoch*Config::NumConfigs);
}

void POWER7_Pref::init_stats()
{
    bzero(&stats, sizeof(stats));
}

void POWER7_Pref::print_config()
{
    cout << "streamer_num_trackers " << knob::streamer_num_trackers << endl
        << "streamer_pref_degree " << knob::streamer_pref_degree << endl
        << "stride_num_trackers " << knob::stride_num_trackers << endl
        << "stride_pref_degree " << knob::stride_pref_degree << endl
        << "power7_explore_epoch " << knob::power7_explore_epoch << endl
        << "power7_exploit_epoch " << knob::power7_exploit_epoch << endl
        << "power7_default_streamer_degree " << knob::power7_default_streamer_degree << endl
        << endl;
}

POWER7_Pref::POWER7_Pref(string type, CACHE *cache) : Prefetcher(type), m_parent_cache(cache)
{
    init_knobs();
    init_stats();

    access_counter = 0;
    cycle_stamp = 0;
    clear_cycle_stats();
    mode = Mode::Exploit;
    config = Config::Default;
    set_params();

    /* create component prefetchers */
    streamer = new Streamer("streamer");
    assert(streamer);
    stride = new StridePrefetcher("stride");
    assert(stride);
}

POWER7_Pref::~POWER7_Pref()
{

}

void POWER7_Pref::set_params()
{
    knob::streamer_pref_degree = get_streamer_degree(config);
    knob::stride_pref_degree = get_stride_degree(config);
}

uint32_t POWER7_Pref::get_streamer_degree(Config config)
{
    switch(config)
    {
        case Config::Default:           return knob::power7_default_streamer_degree;
        case Config::Off:               return 0;
        case Config::Shallowest:
        case Config::S_Shallowest:      return 2;
        case Config::Shallow:
        case Config::S_Shallow:         return 3;
        case Config::Medium:
        case Config::S_Medium:          return 4;
        case Config::Deep:
        case Config::S_Deep:            return 5;
        case Config::Deeper:
        case Config::S_Deeper:          return 6;
        case Config::Deepest:
        case Config::S_Deepest:         return 7;
        default:                        return knob::power7_default_streamer_degree;
    }
}

uint32_t POWER7_Pref::get_stride_degree(Config config)
{
    switch(config)
    {
        case Config::Default:           
        case Config::Off:               
        case Config::Shallowest:
        case Config::Shallow:
        case Config::Medium:
        case Config::Deep:
        case Config::Deeper:
        case Config::Deepest:                   return 0;
        case Config::S_Shallowest:
        case Config::S_Shallow:
        case Config::S_Medium:
        case Config::S_Deep:
        case Config::S_Deeper:
        case Config::S_Deepest:                 return 4;
        default:                                return 0;
    }
}

void POWER7_Pref::invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr)
{
    access_counter++;
    uint64_t cpu_cycle = get_cpu_cycle(m_parent_cache->cpu);
    stats.called++;

    /* state machine design */
    if(mode == Mode::Exploit)
    {
        /* exploit->explore transition */
        if(access_counter >= knob::power7_exploit_epoch)
        {
            mode = Mode::Explore;
            config = Config::Default;
            set_params();
            access_counter = 0;
            cycle_stamp = cpu_cycle;
            clear_cycle_stats();
            stats.mode.exploit_to_explore++;
        }
        else
        {
            stats.mode.exploit++;
        }
        
    }
    else
    {
        /* exploration config change */
        if(access_counter >= knob::power7_explore_epoch)
        {
            /* if last config, then change to exploitation */
            if(config == Config::S_Deepest)
            {
                cycle_stats[config] = (cpu_cycle - cycle_stamp);
                mode = Mode::Exploit;
                config = get_winner_config();
                clear_cycle_stats();
                set_params();
                stats.mode.explore_to_exploit++;
            }
            else
            {
                cycle_stats[config] = (cpu_cycle - cycle_stamp);
                mode = Mode::Explore;
                config = (Config)((uint32_t)config + 1); /* try out next config */
                set_params();
            }
            access_counter = 0;
            cycle_stamp = cpu_cycle;
        }
        else
        {
            stats.mode.explore++;
        }
        
    }

    /* validate */
    assert(mode == Mode::Exploit || mode == Mode::Explore);
    assert(config < Config::NumConfigs);
    if(mode == Mode::Explore)
    {
        for(uint32_t cfg = (uint32_t)Config::Default; config < (uint32_t)config; ++cfg)
        {
            assert(cycle_stats[cfg] != 0);
        }
    }

    /* Config stats */
    stats.config.histogram[config][mode]++;

    /* invoke each individual prefetcher */
    uint32_t pred_streamer = 0, pred_stride = 0;
    streamer->invoke_prefetcher(pc, address, cache_hit, type, pref_addr);
    pred_streamer = pref_addr.size();
    stride->invoke_prefetcher(pc, address, cache_hit, type, pref_addr);
    pred_stride = pref_addr.size() - pred_streamer;

    stats.pred.streamer += pred_streamer;
    stats.pred.stride += pred_stride;
    stats.pred.total += pref_addr.size();

    return;
}

bool POWER7_Pref::empty_cycle_stats()
{
    for (uint32_t index = (uint32_t)Config::Default; index < (uint32_t)Config::NumConfigs; ++index)
    {
        if(cycle_stats[index]) return false;
    }
    return true;
}

void POWER7_Pref::clear_cycle_stats()
{
    for(uint32_t index = (uint32_t)Config::Default; index < (uint32_t)Config::NumConfigs; ++index)
    {
        cycle_stats[index] = 0;
    }
}

Config POWER7_Pref::get_winner_config()
{
    uint64_t min_cycles = UINT64_MAX;
    Config min_cfg = Config::Default;

    for (uint32_t index = (uint32_t)Config::Default; index < (uint32_t)Config::NumConfigs; ++index)
    {
        if(cycle_stats[index] <= min_cycles)
        {
            min_cycles = cycle_stats[index];
            min_cfg = (Config)index;
        }
    }

    return min_cfg;
}

void POWER7_Pref::dump_stats()
{
    cout << "power7.called " << stats.called << endl 
         << "power7.mode.explore " << stats.mode.explore << endl
         << "power7.mode.exploit " << stats.mode.exploit << endl
         << "power7.mode.explore_to_exploit " << stats.mode.explore_to_exploit << endl
         << "power7.mode.exploit_to_explore " << stats.mode.exploit_to_explore << endl
         << endl;

    for (uint32_t cfg = (uint32_t)Config::Default; cfg < (uint32_t)Config::NumConfigs; ++cfg)
    {
        for (uint32_t mode = (uint32_t)Mode::Explore; mode < (uint32_t)Mode::NumModes; ++mode)
        {
            cout << "power7.config." << get_config_string((Config)cfg) << "." << get_mode_string((Mode)mode) << " " << stats.config.histogram[cfg][mode] << endl;
        }
    }
    cout << endl;

    cout << "power7.pred.total " << stats.pred.total << endl
        << "power7.pred.streamer " << stats.pred.streamer << endl
        << "power7.pred.stride " << stats.pred.stride << endl
        << endl;
}
