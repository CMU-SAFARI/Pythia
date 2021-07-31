#ifndef LEARNING_ENGINE_FEATUREWISE_H
#define LEARNING_ENGINE_FEATUREWISE_H

#include <random>
#include "learning_engine_base.h"
#include "feature_knowledge.h"

class LearningEngineFeaturewise : public LearningEngineBase
{
private:
	FeatureKnowledge* m_feature_knowledges[NumFeatureTypes];
	float m_max_q_value;

	std::default_random_engine m_generator;
	std::bernoulli_distribution *m_explore;
	std::uniform_int_distribution<int> *m_actiongen;

	vector<float> m_q_value_buckets;
	vector<uint64_t> m_q_value_histogram;

	/* tracing related knobs */
	uint32_t trace_interval;
	uint64_t trace_timestamp;
	FILE *trace;

	/* stats */
	struct
	{
		struct
		{
			uint64_t called;
			uint64_t explore;
			uint64_t exploit;
			uint64_t dist[MAX_ACTIONS][2]; /* 0:explored, 1:exploited */
			uint64_t fallback;
			uint64_t dyn_fallback_saved_bw;
			uint64_t dyn_fallback_saved_bw_acc;
		} action;

		struct
		{
			uint64_t called;
			uint64_t su_skip[NumFeatureTypes];
		} learn;

		struct
		{
			uint64_t total;
			uint64_t feature_align_dist[NumFeatureTypes];
			uint64_t feature_align_all;
		} consensus;

	} stats;

private:
	void init_knobs();
	void init_stats();
	uint32_t getMaxAction(State *state, float &max_q, float &max_to_avg_q_ratio, vector<bool> &consensus_vec);
	float consultQ(State *state, uint32_t action);
	void gather_stats(float max_q, float max_to_avg_q_ratio);
	void action_selection_consensus(State *state, uint32_t selected_action, vector<bool> &consensus_vec);
	void adjust_feature_weights(vector<bool> consensus_vec, RewardType reward_type);
	bool do_fallback(State *state);
	void plot_scores();

public:
	LearningEngineFeaturewise(Prefetcher *p, float alpha, float gamma, float epsilon, uint32_t actions, uint64_t seed, std::string policy, std::string type, bool zero_init);
	~LearningEngineFeaturewise();
	uint32_t chooseAction(State *state, float &max_to_avg_q_ratio, vector<bool> &consensus_vec);
	void learn(State *state1, uint32_t action1, int32_t reward, State *state2, uint32_t action2, vector<bool> consensus_vec, RewardType reward_type);
	void dump_stats();
};

#endif /* LEARNING_ENGINE_FEATUREWISE_H */
