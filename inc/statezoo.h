#ifndef STATEZOO_H
#define STATEZOO_H

#define DELTA_BITS 7

namespace knob
{
	extern vector<int32_t> le_cmac2_active_features;
	extern uint32_t le_cmac2_feature_shift_amount; 	
}

/*********************************************************************
 ******************* Feature processing functions ********************
 *** If you're adding a new feature, add processing function here ****
 *********************************************************************/

uint32_t LearningEngineCMAC2::process_PC(uint64_t pc, uint32_t plane)
{
	// cout << "Processing PC" << endl;
	uint32_t folded_pc = folded_xor(pc, 2); /* 32b folded XOR */
	folded_pc += m_plane_offsets[plane]; /* add CMAC plane offset */
	folded_pc >>= m_feature_granularities[Feature::PC];
	return folded_pc;
}

uint32_t LearningEngineCMAC2::process_offset(uint32_t offset, uint32_t plane)
{
	// cout << "Processing Offset" << endl;
	offset += m_plane_offsets[plane];
	offset >>= m_feature_granularities[Feature::Offset];
	return offset;
}

uint32_t LearningEngineCMAC2::process_delta(int32_t delta, uint32_t plane)
{
	// cout << "Processing Delta" << endl;
	uint32_t unsigned_delta = (delta < 0) ? (((-1) * delta) + (1 << (DELTA_BITS - 1))) : delta; /* converts into 7 bit signed representation */ 
	unsigned_delta += m_plane_offsets[plane];
	unsigned_delta >>= m_feature_granularities[Feature::Delta];
	return unsigned_delta;
}

uint32_t LearningEngineCMAC2::process_PC_path(uint32_t pc_path, uint32_t plane)
{
	// cout << "Processing PC_path" << endl;
	pc_path += m_plane_offsets[plane];
	pc_path >>= m_feature_granularities[Feature::PC_path];
	return pc_path;
}

uint32_t LearningEngineCMAC2::process_offset_path(uint32_t offset_path, uint32_t plane)
{
	// cout << "Processing Offset_path" << endl;
	offset_path += m_plane_offsets[plane];
	offset_path >>= m_feature_granularities[Feature::Offset_path];
	return offset_path;
}

uint32_t LearningEngineCMAC2::process_delta_path(uint32_t delta_path, uint32_t plane)
{
	// cout << "Processing Delta_path" << endl;
	delta_path += m_plane_offsets[plane];
	delta_path >>= m_feature_granularities[Feature::Delta_path];
	return delta_path;
}

uint32_t LearningEngineCMAC2::process_address(uint64_t address, uint32_t plane)
{
	// cout << "Processing Address" << endl;
	uint32_t folded_address = folded_xor(address, 2); /* 32b folded XOR */
	folded_address += m_plane_offsets[plane];
	folded_address >>= m_feature_granularities[Feature::Address];
	return folded_address;
}

uint32_t LearningEngineCMAC2::process_page(uint64_t page, uint32_t plane)
{
	// cout << "Processing Page" << endl;
	uint32_t folded_page = folded_xor(page, 2); /* 32b folded XOR */
	folded_page += m_plane_offsets[plane];
	folded_page >>= m_feature_granularities[Feature::Address];
	return folded_page;
}

uint32_t LearningEngineCMAC2::get_processed_feature(Feature feature, State *state, uint32_t plane)
{
	uint64_t pc = state->pc;
	uint64_t page = state->page;
	uint64_t address = state->address;
	uint32_t offset = state->offset;
	int32_t  delta = state->delta;
	uint32_t delta_path = state->local_delta_sig2;
	uint32_t pc_path = state->local_pc_sig;
	uint32_t offset_path = state->local_offset_sig;

	switch(feature)
	{
		case PC:			return process_PC(pc, plane);
		case Offset:		return process_offset(offset, plane);	
		case Delta:			return process_delta(delta, plane);	
		case PC_path:		return process_PC_path(pc_path, plane);	
		case Offset_path:	return process_offset_path(offset_path, plane);	
		case Delta_path:	return process_delta_path(delta_path, plane);
		case Address:		return process_address(address, plane);
		case Page:			return process_page(page, plane);
		default:
			cout << "invalid feature type " << feature << endl;
			assert(false);
	}
}


/************************************************
 ******* Generic state generation function ******
 ************************************************/
uint32_t LearningEngineCMAC2::gen_state_generic(uint32_t plane, State *state)
{
	uint32_t initial_index = 0;
	for(uint32_t index = 0; index < knob::le_cmac2_active_features.size(); ++index)
	{
		initial_index <<= knob::le_cmac2_feature_shift_amount;
		initial_index += get_processed_feature((Feature)knob::le_cmac2_active_features[index], state, plane);
	}
	return initial_index;
}



#endif /* STATEZOO_H */

