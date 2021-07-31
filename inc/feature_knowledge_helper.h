#ifndef FEATURE_KNOWLEDGE_HELPER_H
#define FEATURE_KNOWLEDGE_HELPER_H

#include "util.h"

#define DELTA_BITS 7

const uint32_t tiling_offset[] = {	0xaca081b9,0x666a1c67,0xc11d6a53,0x8e5d97c1,0x0d1cad54,0x874f71cb,0x20d2fa13,0x73f7c4a7,
									0x0b701f6c,0x8388d86d,0xf72ac9f2,0xbab16d82,0x524ac258,0xb5900302,0xb48ccc72,0x632f05bf,
									0xe7111073,0xeb602af4,0xf3f29ebb,0x2a6184f2,0x461da5da,0x6693471d,0x62fd0138,0xc484efb3,
									0x81c9eeeb,0x860f3766,0x334faf86,0x5e81e881,0x14bc2195,0xf47671a8,0x75414279,0x357bc5e0
								};

uint32_t FeatureKnowledge::process_PC(uint32_t tiling, uint64_t pc)
{
	uint32_t raw_index = folded_xor(pc, 2); /* 32-b folded XOR */
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_offset(uint32_t tiling, uint32_t offset)
{
	if(m_enable_tiling_offset) offset = offset ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, offset);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_delta(uint32_t tiling, int32_t delta)
{
	uint32_t unsigned_delta = (delta < 0) ? (((-1) * delta) + (1 << (DELTA_BITS - 1))) : delta;
	if(m_enable_tiling_offset) unsigned_delta = unsigned_delta ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, unsigned_delta);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_address(uint32_t tiling, uint64_t address)
{
	uint32_t raw_index = folded_xor(address, 2); /* 32-b folded XOR */
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_offset(uint32_t tiling, uint64_t pc, uint32_t offset)
{
	uint64_t tmp = pc;
	tmp = tmp << 6;
	tmp += offset;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_address(uint32_t tiling, uint64_t pc, uint64_t address)
{
	uint64_t tmp = pc;
	tmp = tmp << 16;
	tmp ^= address;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_page(uint32_t tiling, uint64_t pc, uint64_t page)
{
	uint64_t tmp = pc;
	tmp = tmp << 16;
	tmp ^= page;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_path(uint32_t tiling, uint32_t pc_path)
{
	if(m_enable_tiling_offset) pc_path = pc_path ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, pc_path);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_delta_path(uint32_t tiling, uint32_t delta_path)
{
	if(m_enable_tiling_offset) delta_path = delta_path ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, delta_path);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_offset_path(uint32_t tiling, uint32_t offset_path)
{
	if(m_enable_tiling_offset) offset_path = offset_path ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, offset_path);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_delta(uint32_t tiling, uint64_t pc, int32_t delta)
{
	uint32_t unsigned_delta = (delta < 0) ? (((-1) * delta) + (1 << (DELTA_BITS - 1))) : delta;
	uint64_t tmp = pc;
	tmp = tmp << 7;
	tmp += unsigned_delta;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_offset_delta(uint32_t tiling, uint64_t pc, uint32_t offset, int32_t delta)
{
	uint32_t unsigned_delta = (delta < 0) ? (((-1) * delta) + (1 << (DELTA_BITS - 1))) : delta;
	uint64_t tmp = pc;
	tmp = tmp << 6; tmp += offset;
	tmp = tmp << 7; tmp += unsigned_delta;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_Page(uint32_t tiling, uint64_t page)
{
	uint32_t raw_index = folded_xor(page, 2); /* 32-b folded XOR */
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);

}

uint32_t FeatureKnowledge::process_PC_Path_Offset(uint32_t tiling, uint32_t pc_path, uint32_t offset)
{
	uint64_t tmp = pc_path;
	tmp = tmp << 6;
	tmp += offset;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_Path_Offset_Path(uint32_t tiling, uint32_t pc_path, uint32_t offset_path)
{
	uint64_t tmp = pc_path;
	tmp = tmp << 16;
	tmp += offset_path;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_Path_Delta(uint32_t tiling, uint32_t pc_path, int32_t delta)
{
	uint32_t unsigned_delta = (delta < 0) ? (((-1) * delta) + (1 << (DELTA_BITS - 1))) : delta;
	uint64_t tmp = pc_path;
	tmp = tmp << 7;
	tmp += unsigned_delta;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_Path_Delta_Path(uint32_t tiling, uint32_t pc_path, uint32_t delta_path)
{
	uint64_t tmp = pc_path;
	tmp = tmp << 16;
	tmp += delta_path;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_PC_Path_Offset_Path_Delta_Path(uint32_t tiling, uint32_t pc_path, uint32_t offset_path, uint32_t delta_path)
{
	uint64_t tmp = pc_path;
	tmp = tmp << 10; tmp ^= offset_path;
	tmp = tmp << 10; tmp ^= delta_path;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_Offset_Path_PC(uint32_t tiling, uint32_t offset_path, uint64_t pc)
{
	uint64_t tmp = offset_path;
	tmp = tmp << 32;
	tmp += pc;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}

uint32_t FeatureKnowledge::process_Delta_Path_PC(uint32_t tiling, uint32_t delta_path, uint64_t pc)
{
	uint64_t tmp = delta_path;
	tmp = tmp << 32;
	tmp += pc;
	uint32_t raw_index = folded_xor(tmp, 2);
	if(m_enable_tiling_offset) raw_index = raw_index ^ tiling_offset[tiling];
	uint32_t hashed_index = HashZoo::getHash(m_hash_type, raw_index);
	return (hashed_index % m_num_tiles);
}


#endif /* FEATURE_KNOWLEDGE_HELPER_H */

