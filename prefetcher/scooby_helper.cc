#include <assert.h>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include "champsim.h"
#include "scooby_helper.h"
#include "util.h"
#include "feature_knowledge.h"

#define DELTA_SIG_MAX_BITS 12
#define DELTA_SIG_SHIFT 3
#define PC_SIG_MAX_BITS 32
#define PC_SIG_SHIFT 4
#define OFFSET_SIG_MAX_BITS 24
#define OFFSET_SIG_SHIFT 4

#define SIG_SHIFT 3
#define SIG_BIT 12
#define SIG_MASK ((1 << SIG_BIT) - 1)
#define SIG_DELTA_BIT 7

namespace knob
{
	extern uint32_t scooby_max_pcs;
	extern uint32_t scooby_max_offsets;
	extern uint32_t scooby_max_deltas;
	extern uint32_t scooby_state_type;
	extern uint32_t scooby_max_states;
	extern uint32_t scooby_state_hash_type;
	extern bool 	scooby_enable_pt_address_compression;
	extern uint32_t scooby_pt_address_hash_type;
	extern uint32_t scooby_pt_address_hashed_bits;
	extern bool     scooby_access_debug;
	extern uint64_t scooby_print_access_debug_pc;
	extern uint32_t scooby_print_access_debug_pc_count;
	extern uint32_t scooby_seed;
	extern uint32_t scooby_bloom_filter_size;
	extern bool     scooby_enable_dyn_degree_detector;
	extern bool     scooby_print_trace;
	extern uint32_t scooby_action_tracker_size;
}

uint32_t debug_print_count = 0;

const char* MapFeatureString[] = {"PC", "Offset", "Delta", "PC_path", "Offset_path", "Delta_path", "Address", "Page"};
const char* getFeatureString(Feature feature)
{
	assert(feature < Feature::NumFeatures);
	return MapFeatureString[(uint32_t)feature];
}

const char* MapRewardTypeString[] = {"none", "incorrect", "correct_untimely", "correct_timely", "out_of_bounds", "tracker_hit"};
const char* getRewardTypeString(RewardType type)
{
	assert(type < RewardType::num_rewards);
	return MapRewardTypeString[(uint32_t)type];
}

uint32_t State::value()
{
	uint64_t value = 0;
	switch(knob::scooby_state_type)
	{
		case 1: /* Only PC */
			// return (uint32_t)(pc % knob::scooby_max_states);
			value = pc;
			break;

		case 2: /* PC+ offset */
			value = pc;
			value = value << 6;
			value = value + offset;
			// value = value % knob::scooby_max_states;
			// return value;
			break;

		case 3: /* Only offset */
			value = offset;
			// value = value % knob::scooby_max_states;
			// return value;
			break;

		case 4: /* SPP like delta-path signature */
			value = local_delta_sig;
			// value = value % knob::scooby_max_states;
			// return value;
			break;

		case 5: /* SPP like path signature, but made with shifted PC */
			value = local_pc_sig;
			// value = value % knob::scooby_max_states;
			// return value;
			break;

		case 6: /* SPP's delta-path signature */
			value = local_delta_sig2;
			// value = value % knob::scooby_max_states;
			// return value;
			break;

		default:
			assert(false);
	}

	uint32_t hashed_value = get_hash(value);
	assert(hashed_value < knob::scooby_max_states);

	return hashed_value;
}

uint32_t State::get_hash(uint64_t key)
{
	uint32_t value = 0;
	switch(knob::scooby_state_hash_type)
	{
		case 1: /* simple modulo */
			value = (uint32_t)(key % knob::scooby_max_states);
			break;

		case 2:
			value = HashZoo::jenkins((uint32_t)key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 3:
			value = HashZoo::knuth((uint32_t)key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 4:
			value = HashZoo::murmur3(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 5:
			value = HashZoo::jenkins32(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 6:
			value = HashZoo::hash32shift(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 7:
			value = HashZoo::hash32shiftmult(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 8:
			value = HashZoo::hash64shift(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 9:
			value = HashZoo::hash5shift(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 10:
			value = HashZoo::hash7shift(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 11:
			value = HashZoo::Wang6shift(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 12:
			value = HashZoo::Wang5shift(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 13:
			value = HashZoo::Wang4shift(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		case 14:
			value = HashZoo::Wang3shift(key);
			value = (uint32_t)(value % knob::scooby_max_states);
			break;

		default:
			assert(false);
	}

	return value;
}

std::string State::to_string()
{
	std::stringstream ss;

	ss << std::hex << pc << std::dec << "|"
		<< offset << "|"
		<< delta;

	return ss.str();
}

void Scooby_STEntry::update(uint64_t page, uint64_t pc, uint32_t offset, uint64_t address)
{
	assert(this->page == page);

	/* insert PC */
	if(this->pcs.size() >= knob::scooby_max_pcs)
	{
		this->pcs.pop_front();
	}
	this->pcs.push_back(pc);
	this->unique_pcs.insert(pc);

	/* insert deltas */
	if(!this->offsets.empty())
	{
		int32_t delta = (offset > this->offsets.back()) ? (offset - this->offsets.back()) : (-1)*(this->offsets.back() - offset);
		if(this->deltas.size() >= knob::scooby_max_deltas)
		{
			this->deltas.pop_front();
		}
		this->deltas.push_back(delta);
		this->unique_deltas.insert(delta);
	}

	/* insert offset */
	if(this->offsets.size() >= knob::scooby_max_offsets)
	{
		this->offsets.pop_front();
	}
	this->offsets.push_back(offset);

	/* update demanded pattern */
	this->bmp_real[offset] = 1;

	if(knob::scooby_print_trace)
	{
		fprintf(stdout, "[TRACE] page %16lx| pc %16lx| offset %2u\n", page, pc, offset);
	}
}

uint32_t Scooby_STEntry::get_delta_sig()
{
	uint32_t signature = 0;
	uint32_t delta = 0;

	/* compute signature only using last 4 deltas */
	uint32_t n = deltas.size();
	uint32_t ptr = (n >= 4) ? (n - 4) : 0;

	for(uint32_t index = ptr; index < deltas.size(); ++index)
	{
		signature = (signature << DELTA_SIG_SHIFT);
		signature = signature & ((1ull << DELTA_SIG_MAX_BITS) - 1);
		delta = (uint32_t)(deltas[index] & ((1ull << 7) - 1));
		signature = (signature ^ delta);
		signature = signature & ((1ull << DELTA_SIG_MAX_BITS) - 1);
	}
	return signature;
}

/* This is directly inspired by SPP's signature */
uint32_t Scooby_STEntry::get_delta_sig2()
{
	uint32_t curr_sig = 0;

	/* compute signature only using last 4 deltas */
	uint32_t n = deltas.size();
	uint32_t ptr = (n >= 4) ? (n - 4) : 0;

	for(uint32_t index = ptr; index < deltas.size(); ++index)
	{
		int sig_delta = (deltas[index] < 0) ? (((-1) * deltas[index]) + (1 << (SIG_DELTA_BIT - 1))) : deltas[index];
		curr_sig = ((curr_sig << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
	}

	return curr_sig;
}

uint32_t Scooby_STEntry::get_pc_sig()
{
	uint32_t signature = 0;

	/* compute signature only using last 4 PCs */
	uint32_t n = pcs.size();
	uint32_t ptr = (n >= 4) ? (n - 4) : 0;

	for(uint32_t index = ptr; index < pcs.size(); ++index)
	{
		signature = (signature << PC_SIG_SHIFT);
		signature = (signature ^ pcs[index]);
	}
	signature = signature & ((1ull << PC_SIG_MAX_BITS) - 1);
	return signature;
}

uint32_t Scooby_STEntry::get_offset_sig()
{
	uint32_t signature = 0;

	/* compute signature only using last 4 offsets */
	uint32_t n = offsets.size();
	uint32_t ptr = (n >= 4) ? (n - 4) : 0;

	for(uint32_t index = ptr; index < offsets.size(); ++index)
	{
		signature = (signature << OFFSET_SIG_SHIFT);
		signature = (signature ^ offsets[index]);
	}
	signature = signature & ((1ull << OFFSET_SIG_MAX_BITS) - 1);
	return signature;
}

void Scooby_STEntry::track_prefetch(uint32_t pred_offset, int32_t pref_offset)
{
	if(!bmp_pred[pred_offset])
	{
		bmp_pred[pred_offset] = 1;
		total_prefetches++;

		insert_action_tracker(pref_offset);
	}
}

void Scooby_STEntry::insert_action_tracker(int32_t pref_offset)
{
	// bool found = false;
	auto it = find_if(action_tracker.begin(), action_tracker.end(), [pref_offset](ActionTracker *at){return at->action == pref_offset;});
	if(it != action_tracker.end())
	{
		(*it)->conf++;
		/* maintain the recency order */
		action_tracker.erase(it);
		action_tracker.push_back((*it));
	}
	else
	{
		if(action_tracker.size() >= knob::scooby_action_tracker_size)
		{
			ActionTracker *victim = action_tracker.front();
			action_tracker.pop_front();
			delete victim;
		}
		action_tracker.push_back(new ActionTracker(pref_offset, 0));
	}
}

bool Scooby_STEntry::search_action_tracker(int32_t action, int32_t &conf)
{
	conf = 0;
	auto it = find_if(action_tracker.begin(), action_tracker.end(), [action](ActionTracker *at){return at->action == action;});
	if(it != action_tracker.end())
	{
		conf = (*it)->conf;
		return true;
	}
	else
	{
		return false;
	}
}

void ScoobyRecorder::record_access(uint64_t pc, uint64_t address, uint64_t page, uint32_t offset, uint8_t bw_level)
{
	unique_pcs.insert(pc);
	unique_pages.insert(page);

	/* pc bw distribution */
	// assert(bw_level < DRAM_BW_LEVELS);
	// auto it = pc_bw_dist.find(pc);
	// if(it != pc_bw_dist.end())
	// {
	// 	assert(it->second.size() == DRAM_BW_LEVELS);
	// 	it->second[bw_level]++;
	// }
	// else
	// {
	// 	vector<uint64_t> v;
	// 	v.resize(DRAM_BW_LEVELS, 0);
	// 	v[bw_level]++;
	// 	pc_bw_dist.insert(pair<uint64_t, vector<uint64_t> >(pc, v));
	// }
}

void ScoobyRecorder::record_trigger_access(uint64_t page, uint64_t pc, uint32_t offset)
{
	unique_trigger_pcs.insert(pc);
}

void ScoobyRecorder::record_access_knowledge(Scooby_STEntry *stentry)
{
	Bitmap bmp_real = stentry->bmp_real;
	uint32_t trigger_offset = stentry->offsets.front();
	uint64_t bmp_real_rot_value = BitmapHelper::value(BitmapHelper::rotate_left(bmp_real, trigger_offset));
	auto it = access_bitmap_dist.find(bmp_real_rot_value);
	if(it != access_bitmap_dist.end())
	{
		it->second++;
	}
	else
	{
		access_bitmap_dist.insert(std::pair<uint64_t, uint64_t>(bmp_real_rot_value, 1));
		unique_bitmaps_seen++;
	}
	total_bitmaps_seen++;

	/* delta statistics */
	for(uint32_t hop = 1; hop <= MAX_HOP_COUNT; ++hop)
	{
		for(uint32_t index = 0; index < stentry->offsets.size(); ++index)
		{
			int32_t delta = 0;
			if(index+hop < stentry->offsets.size())
			{
				delta = (stentry->offsets[index+hop] - stentry->offsets[index]);
				if(delta < 0) delta = (-1)*delta + 63;
				hop_delta_dist[hop][delta]++;
			}
		}
	}
}

void ScoobyRecorder::dump_stats()
{
	cout << "unique_pcs " << unique_pcs.size() << endl
		<< "unique_pages " << unique_pages.size() << endl
		<< "unique_trigger_pcs " << unique_trigger_pcs.size() << endl
		<< "total_bitmaps_seen " << total_bitmaps_seen << endl
		<< "unique_bitmaps_seen " << unique_bitmaps_seen << endl
		<< endl;

	/* PC BW distribution */
	// for(auto it = pc_bw_dist.begin(); it != pc_bw_dist.end(); ++it)
	// {
	// 	uint64_t sum = 0;
	// 	cout << "PC_bw_dist_" << hex << it->first << dec << " ";
	// 	for(uint32_t index = 0; index < DRAM_BW_LEVELS; ++index)
	// 	{
	// 		cout << it->second[index] << ",";
	// 		sum += it->second[index];
	// 	}
	// 	cout << sum << endl;
	// }
	// cout << endl;

	if(knob::scooby_access_debug)
	{
		std::vector<std::pair<uint64_t, uint64_t>> pairs;
		for (auto itr = access_bitmap_dist.begin(); itr != access_bitmap_dist.end(); ++itr)
		    pairs.push_back(*itr);

		sort(pairs.begin(), pairs.end(), [](std::pair<uint64_t, uint64_t>& a, std::pair<uint64_t, uint64_t>& b){return a.second > b.second;});

		cout << "Bitmap, #count" <<endl;
		uint32_t total_occ = 0, top_20_occ = 0;;
		for(uint32_t index = 0; index < pairs.size(); ++index)
		{
			total_occ += pairs[index].second;
			if(index < 20)
			{
				top_20_occ += pairs[index].second;
				cout << hex << pairs[index].first << dec << "," << pairs[index].second << endl;
			}
		}
		cout << "top_20_perc " << (float)top_20_occ/total_occ*100 << "%" << endl;
		cout << std::endl;

		/* delta statistics */
		for(uint32_t hop = 1; hop <= MAX_HOP_COUNT; ++hop)
		{
			cout << "hop_" << hop << "_delta_dist ";
			for(uint32_t index = 0; index < 127; ++index)
			{
				cout << hop_delta_dist[hop][index] << ",";
			}
			cout << endl;
		}
		cout << std::endl;
	}
}

void print_access_debug(Scooby_STEntry *stentry)
{
	bool to_print = true;
	if(knob::scooby_print_access_debug_pc)
	{
		if(stentry->unique_pcs.find(knob::scooby_print_access_debug_pc) == stentry->unique_pcs.end())
		{
			to_print = false;
		}
		else
		{
			debug_print_count++;
			if(debug_print_count > knob::scooby_print_access_debug_pc_count)
			{
				to_print = false;
			}
		}
	}
	if(!to_print)
	{
		return;
	}

	uint64_t trigger_pc = stentry->pcs.front();
	uint32_t trigger_offset = stentry->offsets.front();
	uint32_t unique_pc_count = stentry->unique_pcs.size();
	fprintf(stdout, "[ACCESS] %16lx|%16lx|%2u|%2u|%64s|%64s|%2u|%2u|%2u|%2u|%2u|",
		stentry->page,
		trigger_pc,
		trigger_offset,
		unique_pc_count,
		BitmapHelper::to_string(stentry->bmp_real).c_str(),
		BitmapHelper::to_string(stentry->bmp_pred).c_str(),
		BitmapHelper::count_bits_set(stentry->bmp_real),
		BitmapHelper::count_bits_set(stentry->bmp_pred),
		BitmapHelper::count_bits_same(stentry->bmp_real, stentry->bmp_pred), /* covered */
		BitmapHelper::count_bits_diff(stentry->bmp_real, stentry->bmp_pred), /* uncovered */
		BitmapHelper::count_bits_diff(stentry->bmp_pred, stentry->bmp_real)  /* over prediction */
	);
	for (const uint64_t& pc: stentry->unique_pcs)
	{
		fprintf(stdout, "%lx,", pc);
	}
	fprintf(stdout, "|");
	for (const int32_t& delta: stentry->unique_deltas)
	{
		fprintf(stdout, "%d,", delta);
	}
	fprintf(stdout, "\n");
}

string print_active_features(vector<int32_t> features)
{
	std::stringstream ss;
	for(uint32_t index = 0; index < features.size(); ++index)
	{
		if(index) ss << "+";
		ss << getFeatureString((Feature)features[index]);
	}
	return ss.str();
}

string print_active_features2(vector<int32_t> features)
{
	std::stringstream ss;
	for(uint32_t index = 0; index < features.size(); ++index)
	{
		if(index) ss << ", ";
		ss << FeatureKnowledge::getFeatureString((FeatureType)features[index]);
	}
	return ss.str();
}
