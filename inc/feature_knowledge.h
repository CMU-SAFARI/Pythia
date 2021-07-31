#ifndef FEATURE_KNOWLEDGE
#define FEATURE_KNOWLEDGE

#include <string>
#include "scooby_helper.h"
#define FK_MAX_TILINGS 32

typedef enum
{
	F_PC = 0,								// 0
	F_Offset,								// 1
	F_Delta,								// 2
	F_Address,								// 3
	F_PC_Offset,							// 4
	F_PC_Address,							// 5
	F_PC_Page,								// 6
	F_PC_Path,								// 7
	F_Delta_Path,							// 8
	F_Offset_Path,							// 9
	F_PC_Delta,								// 10
	F_PC_Offset_Delta, 						// 11
	F_Page,									// 12
	F_PC_Path_Offset,						// 13
	F_PC_Path_Offset_Path,					// 14
	F_PC_Path_Delta,						// 15
	F_PC_Path_Delta_Path,					// 16
	F_PC_Path_Offset_Path_Delta_Path,		// 17
	F_Offset_Path_PC,						// 18
	F_Delta_Path_PC,						// 19

	NumFeatureTypes
} FeatureType;

class FeatureKnowledge
{
private:
	FeatureType m_feature_type;
	float m_alpha, m_gamma;
	uint32_t m_actions;
	float m_init_value;
	float m_weight;
	float m_weight_gradient;
	uint32_t m_hash_type;

	uint32_t m_num_tilings, m_num_tiles;
	float ***m_qtable;
	bool m_enable_tiling_offset;

	float min_weight, max_weight;

	/* q-value tracing related variables */
	uint32_t trace_interval;
	uint64_t trace_timestamp;
	uint32_t trace_record_count;
	FILE *trace;

private:
	float getQ(uint32_t tiling, uint32_t tile_index, uint32_t action);
	void setQ(uint32_t tiling, uint32_t tile_index, uint32_t action, float value);
	uint32_t get_tile_index(uint32_t tiling, State *state);
	string get_feature_string(State *state);

	/* feature index generators */
	uint32_t process_PC(uint32_t tiling, uint64_t pc);
	uint32_t process_offset(uint32_t tiling, uint32_t offset);
	uint32_t process_delta(uint32_t tiling, int32_t delta);
	uint32_t process_address(uint32_t tiling, uint64_t address);
	uint32_t process_PC_offset(uint32_t tiling, uint64_t pc, uint32_t offset);
	uint32_t process_PC_address(uint32_t tiling, uint64_t pc, uint64_t address);
	uint32_t process_PC_page(uint32_t tiling, uint64_t pc, uint64_t page);
	uint32_t process_PC_path(uint32_t tiling, uint32_t pc_path);
	uint32_t process_delta_path(uint32_t tiling, uint32_t delta_path);
	uint32_t process_offset_path(uint32_t tiling, uint32_t offset_path);
	uint32_t process_PC_delta(uint32_t tiling, uint64_t pc, int32_t delta);
	uint32_t process_PC_offset_delta(uint32_t tiling, uint64_t pc, uint32_t offset, int32_t delta);
	uint32_t process_Page(uint32_t tiling, uint64_t page);
	uint32_t process_PC_Path_Offset(uint32_t tiling, uint32_t pc_path, uint32_t offset);
	uint32_t process_PC_Path_Offset_Path(uint32_t tiling, uint32_t pc_path, uint32_t offset_path);
	uint32_t process_PC_Path_Delta(uint32_t tiling, uint32_t pc_path, int32_t delta);
	uint32_t process_PC_Path_Delta_Path(uint32_t tiling, uint32_t pc_path, uint32_t delta_path);
	uint32_t process_PC_Path_Offset_Path_Delta_Path(uint32_t tiling, uint32_t pc_path, uint32_t offset_path, uint32_t delta_path);
	uint32_t process_Offset_Path_PC(uint32_t tiling, uint32_t offset_path, uint64_t pc);
	uint32_t process_Delta_Path_PC(uint32_t tiling, uint32_t delta_path, uint64_t pc);

	/* for q-value tracing */
	void dump_feature_trace(State *state);
	void plot_scores();

public:
	FeatureKnowledge(FeatureType feature_type, float alpha, float gamma, uint32_t actions, float weight, float weight_gradient, uint32_t num_tilings, uint32_t num_tiles, bool zero_init, uint32_t hash_type, int32_t enable_tiling_offset);
	~FeatureKnowledge();
	float retrieveQ(State *state, uint32_t action_index);
	void updateQ(State *state1, uint32_t action1, int32_t reward, State *state2, uint32_t action2);
	static string getFeatureString(FeatureType type);
	uint32_t getMaxAction(State *state); /* Called by featurewise engine only to get a consensus from all the features */

	/* weight manipulation */
	inline void increase_weight() {m_weight = m_weight + m_weight_gradient * m_weight; if(m_weight < min_weight) min_weight = m_weight;}
	inline void decrease_weight() {m_weight = m_weight - m_weight_gradient * m_weight; if(m_weight > max_weight) max_weight = m_weight;}
	inline float get_weight() {return m_weight;}
	inline float get_min_weight() {return min_weight;}
	inline float get_max_weight() {return max_weight;}
};

#endif /* FEATURE_KNOWLEDGE */
