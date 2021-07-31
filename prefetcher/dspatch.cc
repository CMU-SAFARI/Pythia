#include <assert.h>
#include <algorithm>
#include <iomanip>
#include "champsim.h"
#include "util.h"
#include "dspatch.h"
#include "memory_class.h"

#if 0
#	define LOCKED(...) {fflush(stdout); __VA_ARGS__; fflush(stdout);}
#	define LOGID() fprintf(stdout, "[%25s@%3u] ", \
							__FUNCTION__, __LINE__ \
							);
#	define MYLOG(...) LOCKED(LOGID(); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n");)
#else
#	define MYLOG(...) {}
#endif

const char* DSPatch_pref_candidate_string[] = {"NONE", "CovP", "AccP"};
const char* Map_DSPatch_pref_candidate(DSPatch_pref_candidate candidate)
{
	assert((uint32_t)candidate < DSPatch_pref_candidate::Num_DSPatch_pref_candidates);
	return DSPatch_pref_candidate_string[(uint32_t)candidate];
}

namespace knob
{	
	extern uint32_t dspatch_log2_region_size;
	extern uint32_t dspatch_num_cachelines_in_region;
	extern uint32_t dspatch_pb_size;
	extern uint32_t dspatch_num_spt_entries;
	extern uint32_t dspatch_compression_granularity;
	extern uint32_t dspatch_pred_throttle_bw_thr;
	extern uint32_t dspatch_bitmap_selection_policy;
	extern uint32_t dspatch_sig_type;
	extern uint32_t dspatch_sig_hash_type;
	extern uint32_t dspatch_or_count_max;
	extern uint32_t dspatch_measure_covP_max;
	extern uint32_t dspatch_measure_accP_max;
	extern uint32_t dspatch_acc_thr;
	extern uint32_t dspatch_cov_thr;
	extern bool     dspatch_enable_pref_buffer;
	extern uint32_t dspatch_pref_buffer_size;
	extern uint32_t dspatch_pref_degree;
}

void DSPatch::init_knobs()
{
	assert(knob::dspatch_log2_region_size <= 12);
	assert(knob::dspatch_num_cachelines_in_region * knob::dspatch_compression_granularity <= 64);
}

void DSPatch::init_stats()
{
	bzero(&stats, sizeof(stats));
}

DSPatch::DSPatch(string type) : Prefetcher(type)
{
	init_knobs();
	init_stats();

	/* init bw to lowest value */
	bw_bucket = 0;

	/* init SPT */
	spt = (DSPatch_SPTEntry**)calloc(knob::dspatch_num_spt_entries, sizeof(DSPatch_SPTEntry**));
	assert(spt);
	for(uint32_t index = 0; index < knob::dspatch_num_spt_entries; ++index)
	{
		spt[index] = new DSPatch_SPTEntry();
	}
}

DSPatch::~DSPatch()
{

}

void DSPatch::print_config()
{
	cout << "dspatch_log2_region_size " << knob::dspatch_log2_region_size << endl
		<< "dspatch_num_cachelines_in_region " << knob::dspatch_num_cachelines_in_region << endl
		<< "dspatch_pb_size " << knob::dspatch_pb_size << endl
		<< "dspatch_num_spt_entries " << knob::dspatch_num_spt_entries << endl
		<< "dspatch_compression_granularity " << knob::dspatch_compression_granularity << endl
		<< "dspatch_pred_throttle_bw_thr " << knob::dspatch_pred_throttle_bw_thr << endl
		<< "dspatch_bitmap_selection_policy " << knob::dspatch_bitmap_selection_policy << endl
		<< "dspatch_sig_type " << knob::dspatch_sig_type << endl
		<< "dspatch_sig_hash_type " << knob::dspatch_sig_hash_type << endl
		<< "dspatch_or_count_max " << knob::dspatch_or_count_max << endl
		<< "dspatch_measure_covP_max " << knob::dspatch_measure_covP_max << endl
		<< "dspatch_measure_accP_max " << knob::dspatch_measure_accP_max << endl
		<< "dspatch_acc_thr " << knob::dspatch_acc_thr << endl
		<< "dspatch_cov_thr " << knob::dspatch_cov_thr << endl
		<< "dspatch_enable_pref_buffer " << knob::dspatch_enable_pref_buffer << endl
		<< "dspatch_pref_buffer_size " << knob::dspatch_pref_buffer_size << endl
		<< "dspatch_pref_degree " << knob::dspatch_pref_degree << endl
		<< endl;
}

void DSPatch::invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr)
{
	uint64_t page = address >> knob::dspatch_log2_region_size;
	uint32_t offset = (address >> LOG2_BLOCK_SIZE) & ((1ull << (knob::dspatch_log2_region_size - LOG2_BLOCK_SIZE)) - 1);

	MYLOG("---------------------------------------------------------------------");
	MYLOG("%s %lx pc %lx page %lx off %u", GetAccessType(type), address, pc, page, offset);

	DSPatch_PBEntry *pbentry = NULL;
	pbentry = search_pb(page);
	stats.pb.lookup++;
	if(pbentry)
	{
		/* record the access */
		pbentry->bmp_real[offset] = true;
		stats.pb.hit++;
	}
	else /* page buffer miss, prefetch trigger opportunity */
	{
		/* insert the new page buffer entry */
		if(page_buffer.size() >= knob::dspatch_pb_size)
		{
			pbentry = page_buffer.front();
			page_buffer.pop_front();
			add_to_spt(pbentry);
			// if(knob::dspatch_enable_debug)
			// {
			// 	debug_pbentry(pbentry);
			// }
			delete pbentry;
			stats.pb.evict++;
		}
		pbentry = new DSPatch_PBEntry();
		pbentry->page = page;
		pbentry->trigger_pc = pc;
		pbentry->trigger_offset = offset;
		pbentry->bmp_real[offset] = true;
		page_buffer.push_back(pbentry);
		stats.pb.insert++;

		/* trigger prefetch */
		generate_prefetch(pc, page, offset, address, pref_addr);
		if(knob::dspatch_enable_pref_buffer)
		{
			buffer_prefetch(pref_addr);
			pref_addr.clear();
		}
	}

	/* slowly inject prefetches at every demand access, if buffer is turned on */
	if(knob::dspatch_enable_pref_buffer)
	{
		issue_prefetch(pref_addr);
	}
}

void DSPatch::generate_prefetch(uint64_t pc, uint64_t page, uint32_t offset, uint64_t address, vector<uint64_t> &pref_addr)
{
	Bitmap bmp_cov, bmp_acc, bmp_pred;
	uint64_t signature = 0xdeadbeef;
	DSPatch_pref_candidate candidate = DSPatch_pref_candidate::NONE;
	DSPatch_SPTEntry *sptentry = NULL;

	stats.gen_pref.called++;
	signature = create_signature(pc, page, offset);
	uint32_t spt_index = get_spt_index(signature);
	assert(spt_index < knob::dspatch_num_spt_entries);
	
	sptentry = spt[spt_index];
	candidate = select_bitmap(sptentry, bmp_pred);
	stats.gen_pref.selection_dist[candidate]++;
	MYLOG("pc %lx sig %lx spt_index %u candidate %s", pc, signature, spt_index, Map_DSPatch_pref_candidate(candidate));

	/* decompress and rotate back the bitmap */
	MYLOG("orig bitmap %s", BitmapHelper::to_string(bmp_pred, knob::dspatch_num_cachelines_in_region).c_str());
	bmp_pred = BitmapHelper::decompress(bmp_pred, knob::dspatch_compression_granularity, knob::dspatch_num_cachelines_in_region);
	bmp_pred = BitmapHelper::rotate_left(bmp_pred, offset, knob::dspatch_num_cachelines_in_region);
	MYLOG("rotated bitmap %s", BitmapHelper::to_string(bmp_pred, knob::dspatch_num_cachelines_in_region).c_str());

	/* Throttling predictions incase of predicting with bmp_acc and b/w is high */
	if(bw_bucket >= knob::dspatch_pred_throttle_bw_thr && candidate == DSPatch_pref_candidate::ACCP)
	{
		bmp_pred.reset();
		stats.gen_pref.reset++;
	}
	
	/* generate prefetch requests */
	MYLOG("pred bitmap %s", BitmapHelper::to_string(bmp_pred, knob::dspatch_num_cachelines_in_region).c_str());	
	for(uint32_t index = 0; index < knob::dspatch_num_cachelines_in_region; ++index)
	{
		if(bmp_pred[index] && index != offset)
		{
			uint64_t addr = (page << knob::dspatch_log2_region_size) + (index << LOG2_BLOCK_SIZE);
			pref_addr.push_back(addr);
		}
	}
	stats.gen_pref.total += pref_addr.size();
}

void DSPatch::update_bw(uint8_t bw)
{
	assert(bw < DSPATCH_MAX_BW_LEVEL);
	bw_bucket = bw;
	stats.bw.called++;
	stats.bw.bw_histogram[bw]++;
}

DSPatch_pref_candidate DSPatch::select_bitmap(DSPatch_SPTEntry *sptentry, Bitmap &bmp_selected)
{
	DSPatch_pref_candidate candidate = DSPatch_pref_candidate::NONE;
	switch(knob::dspatch_bitmap_selection_policy)
	{
		case 1:
			/* always select coverage bitmap */
			bmp_selected = sptentry->bmp_cov;
			candidate = DSPatch_pref_candidate::COVP;
			break;
		case 2:
			/* always select accuracy bitmap */
			bmp_selected = sptentry->bmp_acc;
			candidate = DSPatch_pref_candidate::ACCP;
			break;
		case 3:
			/* hybrid selection */
			candidate = dyn_selection(sptentry, bmp_selected);
			break;
		default:
			cout << "invalid dspatch_bitmap_selection_policy " << knob::dspatch_bitmap_selection_policy << endl;
			assert(false);
	}
	return candidate;
}

DSPatch_PBEntry* DSPatch::search_pb(uint64_t page)
{
	auto it = find_if(page_buffer.begin(), page_buffer.end(), [page](DSPatch_PBEntry *pbentry){return pbentry->page == page;});
	return it != page_buffer.end() ? (*it) : NULL;
}

void DSPatch::buffer_prefetch(vector<uint64_t> pref_addr)
{
	uint32_t count = 0;
	for(uint32_t index = 0; index < pref_addr.size(); ++index)
	{
		if(pref_buffer.size() >= knob::dspatch_pref_buffer_size)
		{
			break;
		}
		pref_buffer.push_back(pref_addr[index]);
		count++;
	}
	stats.pref_buffer.buffered += count;
	stats.pref_buffer.spilled += (pref_addr.size() - count);
}

void DSPatch::issue_prefetch(vector<uint64_t> &pref_addr)
{
	uint32_t count = 0;
	while(!pref_buffer.empty() && count < knob::dspatch_pref_degree)
	{
		pref_addr.push_back(pref_buffer.front());
		pref_buffer.pop_front();
		count++;
	}
	stats.pref_buffer.issued += pref_addr.size();
}

uint64_t DSPatch::create_signature(uint64_t pc, uint64_t page, uint32_t offset)
{
	uint64_t signature = 0;
	switch(knob::dspatch_sig_type)
	{
		case 1:
			signature = pc;
			break;
		default:
			cout << "invalid dspatch_sig_type " << knob::dspatch_sig_type << endl;
			assert(false);
	}
	return signature;
}

uint32_t DSPatch::get_spt_index(uint64_t signature)
{
	uint32_t folded_sig = folded_xor(signature, 2);
	/* apply hash */
	uint32_t hashed_index = get_hash(folded_sig);
	return hashed_index % knob::dspatch_num_spt_entries;
}

uint32_t DSPatch::get_hash(uint32_t key)
{
	switch(knob::dspatch_sig_hash_type)
	{
		case 1: 	return key;
		case 2: 	return HashZoo::jenkins(key);
		case 3: 	return HashZoo::knuth(key);
		case 4: 	return HashZoo::murmur3(key);
		case 5: 	return HashZoo::jenkins32(key);
		case 6: 	return HashZoo::hash32shift(key);
		case 7: 	return HashZoo::hash32shiftmult(key);
		case 8: 	return HashZoo::hash64shift(key);
		case 9: 	return HashZoo::hash5shift(key);
		case 10: 	return HashZoo::hash7shift(key);
		case 11: 	return HashZoo::Wang6shift(key);
		case 12: 	return HashZoo::Wang5shift(key);
		case 13: 	return HashZoo::Wang4shift(key);
		case 14: 	return HashZoo::Wang3shift(key);
		default: 	assert(false);
	}
}

void DSPatch::add_to_spt(DSPatch_PBEntry *pbentry)
{
	stats.spt.called++;
	Bitmap bmp_real, bmp_cov, bmp_acc;
	bmp_real = pbentry->bmp_real;
	uint64_t trigger_pc = pbentry->trigger_pc;
	uint32_t trigger_offset = pbentry->trigger_offset;

	uint64_t signature = create_signature(trigger_pc, 0xdeadbeef, trigger_offset);
	uint32_t spt_index = get_spt_index(signature);
	assert(spt_index < knob::dspatch_num_spt_entries);
	DSPatch_SPTEntry *sptentry = spt[spt_index];
	MYLOG("page %lx trigger_pc %lx trigger_offset %u sig %lx spt_index %u", pbentry->page, trigger_pc, trigger_offset, signature, spt_index);

	bmp_real = BitmapHelper::rotate_right(bmp_real, trigger_offset, knob::dspatch_num_cachelines_in_region);
	bmp_cov  = BitmapHelper::decompress(sptentry->bmp_cov, knob::dspatch_compression_granularity, knob::dspatch_num_cachelines_in_region);
	bmp_acc  = BitmapHelper::decompress(sptentry->bmp_acc, knob::dspatch_compression_granularity, knob::dspatch_num_cachelines_in_region);

	uint32_t pop_count_bmp_real = BitmapHelper::count_bits_set(bmp_real);
	uint32_t pop_count_bmp_cov  = BitmapHelper::count_bits_set(bmp_cov);
	uint32_t pop_count_bmp_acc  = BitmapHelper::count_bits_set(bmp_acc);
	uint32_t same_count_bmp_cov = BitmapHelper::count_bits_same(bmp_cov, bmp_real);
	uint32_t same_count_bmp_acc = BitmapHelper::count_bits_same(bmp_acc, bmp_real);

	uint32_t cov_bmp_cov = 100 * (float)same_count_bmp_cov / pop_count_bmp_real;
	uint32_t acc_bmp_cov = 100 * (float)same_count_bmp_cov / pop_count_bmp_cov;
	uint32_t cov_bmp_acc = 100 * (float)same_count_bmp_acc / pop_count_bmp_real;
	uint32_t acc_bmp_acc = 100 * (float)same_count_bmp_acc / pop_count_bmp_acc;

	MYLOG("bmp_real %s", BitmapHelper::to_string(bmp_real, knob::dspatch_num_cachelines_in_region).c_str());
	MYLOG("bmp_cov  %s", BitmapHelper::to_string(bmp_cov,  knob::dspatch_num_cachelines_in_region).c_str());
	MYLOG("bmp_acc  %s", BitmapHelper::to_string(bmp_acc,  knob::dspatch_num_cachelines_in_region).c_str());
	MYLOG("cov_bmp_cov %u acc_bmp_acc %u cov_bmp_acc %u acc_bmp_acc %u", cov_bmp_cov, acc_bmp_cov, cov_bmp_acc, acc_bmp_acc);

	/* Update CovP counters */
	if(BitmapHelper::count_bits_diff(bmp_real, bmp_cov) != 0)
	{
		sptentry->or_count.incr(knob::dspatch_or_count_max);
		stats.spt.or_count_incr++;
	}
	if(acc_bmp_cov < knob::dspatch_acc_thr || cov_bmp_cov < knob::dspatch_cov_thr)
	{
		sptentry->measure_covP.incr(knob::dspatch_measure_covP_max);
		stats.spt.measure_covP_incr++;
	}

	/* Update CovP */
	MYLOG("sptentry->bmp_cov before %s", BitmapHelper::to_string(sptentry->bmp_cov, knob::dspatch_num_cachelines_in_region).c_str());
	if(sptentry->measure_covP.value() == knob::dspatch_measure_covP_max)
	{
		MYLOG("measure_covP saturated %u", sptentry->measure_covP.value());
		if(bw_bucket == 3 || cov_bmp_cov < 50) /* WARNING: hardcoded values */
		{
			MYLOG("reseting CovP");
			sptentry->bmp_cov = BitmapHelper::compress(bmp_real, knob::dspatch_compression_granularity);
			sptentry->or_count.reset();
			stats.spt.bmp_cov_reset++;
		}
	}
	else
	{
		sptentry->bmp_cov = BitmapHelper::compress(BitmapHelper::bitwise_or(bmp_cov, bmp_real), knob::dspatch_compression_granularity);
		stats.spt.bmp_cov_update++;
	}
	MYLOG("sptentry->bmp_cov after  %s", BitmapHelper::to_string(sptentry->bmp_cov, knob::dspatch_num_cachelines_in_region).c_str());

	/* Update AccP counter(s) */
	if(acc_bmp_acc < 50) /* WARNING: hardcoded value */
	{
		sptentry->measure_accP.incr();
		stats.spt.measure_accP_incr++;
	}
	else
	{
		sptentry->measure_accP.decr();
		stats.spt.measure_accP_decr++;
	}

	/* Update AccP */
	MYLOG("sptentry->bmp_acc before %s", BitmapHelper::to_string(sptentry->bmp_acc, knob::dspatch_num_cachelines_in_region).c_str());
	sptentry->bmp_acc = BitmapHelper::bitwise_and(bmp_real, BitmapHelper::decompress(sptentry->bmp_cov, knob::dspatch_compression_granularity, knob::dspatch_num_cachelines_in_region));
	sptentry->bmp_acc = BitmapHelper::compress(sptentry->bmp_acc, knob::dspatch_compression_granularity);
	MYLOG("sptentry->bmp_acc after  %s", BitmapHelper::to_string(sptentry->bmp_acc, knob::dspatch_num_cachelines_in_region).c_str());
	stats.spt.bmp_acc_update++;
}

DSPatch_pref_candidate DSPatch::dyn_selection(DSPatch_SPTEntry *sptentry, Bitmap &bmp_selected)
{
	stats.dyn_selection.called++;
	DSPatch_pref_candidate candidate = DSPatch_pref_candidate::NONE;

	if(bw_bucket == 3)
	{
		if(sptentry->measure_accP.value() == knob::dspatch_measure_accP_max)
		{
			/* no prefetch */
			bmp_selected.reset();
			candidate = DSPatch_pref_candidate::NONE;
			stats.dyn_selection.none++;
		}
		else
		{
			/* Prefetch with accP */
			bmp_selected = sptentry->bmp_acc;
			candidate = DSPatch_pref_candidate::ACCP;
			stats.dyn_selection.accp_reason1++;
		}
	}
	else if(bw_bucket == 2)
	{
		if(sptentry->measure_covP.value() == knob::dspatch_measure_covP_max)
		{
			/* Prefetch with accP */
			bmp_selected = sptentry->bmp_acc;
			candidate = DSPatch_pref_candidate::ACCP;			
			stats.dyn_selection.accp_reason2++;
		}
		else
		{
			/* Prefetch with covP */
			bmp_selected = sptentry->bmp_cov;
			candidate = DSPatch_pref_candidate::COVP;
			stats.dyn_selection.covp_reason1++;
		}
	}
	else
	{
		/* Prefetch with covP */
		bmp_selected = sptentry->bmp_cov;
		candidate = DSPatch_pref_candidate::COVP;		
		stats.dyn_selection.covp_reason2++;
	}

	return candidate;
}

void DSPatch::dump_stats()
{
	cout << "dspatch.pb.lookup " << stats.pb.lookup << endl
		<< "dspatch.pb.hit " << stats.pb.hit << endl
		<< "dspatch.pb.evict " << stats.pb.evict << endl
		<< "dspatch.pb.insert " << stats.pb.insert << endl
		<< endl
		<< "dspatch.gen_pref.called " << stats.gen_pref.called << endl
		<< "dspatch.gen_pref.reset " << stats.gen_pref.reset << endl
		<< "dspatch.gen_pref.total " << stats.gen_pref.total << endl
		<< endl
		<< "dspatch.dyn_selection.called " << stats.dyn_selection.called << endl
		<< "dspatch.dyn_selection.none " << stats.dyn_selection.none << endl
		<< "dspatch.dyn_selection.accp_reason1 " << stats.dyn_selection.accp_reason1 << endl
		<< "dspatch.dyn_selection.accp_reason2 " << stats.dyn_selection.accp_reason2 << endl
		<< "dspatch.dyn_selection.covp_reason1 " << stats.dyn_selection.covp_reason1 << endl
		<< "dspatch.dyn_selection.covp_reason2 " << stats.dyn_selection.covp_reason2 << endl
		<< endl
		<< "dspatch.spt.called " << stats.spt.called << endl
		<< "dspatch.spt.or_count_incr " << stats.spt.or_count_incr << endl
		<< "dspatch.spt.measure_covP_incr " << stats.spt.measure_covP_incr << endl
		<< "dspatch.spt.bmp_cov_reset " << stats.spt.bmp_cov_reset << endl
		<< "dspatch.spt.bmp_cov_update " << stats.spt.bmp_cov_update << endl
		<< "dspatch.spt.measure_accP_incr " << stats.spt.measure_accP_incr << endl
		<< "dspatch.spt.measure_accP_decr " << stats.spt.measure_accP_decr << endl
		<< "dspatch.spt.bmp_acc_update " << stats.spt.bmp_acc_update << endl
		<< endl
		<< "dspatch.pref_buffer.spilled " << stats.pref_buffer.spilled << endl
		<< "dspatch.pref_buffer.buffered " << stats.pref_buffer.buffered << endl
		<< "dspatch.pref_buffer.issued " << stats.pref_buffer.issued << endl
		<< endl;

	cout << "dspatch.bw.called " << stats.bw.called << endl
		<< "dspatch.bw.bw_histogram ";
	for(uint32_t index = 0; index < DSPATCH_MAX_BW_LEVEL; ++index)
	{
		cout << stats.bw.bw_histogram[index] << ",";
	}
	cout << endl << endl;
}
