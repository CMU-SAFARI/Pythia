#ifndef LEARNING_ENGINE
#define LEARNING_ENGINE

#include <random>
#include <string.h>
#include "prefetcher.h"
#include "learning_engine_base.h"

#define MAX_ACTIONS 64

/*
 * table format
 *      |action 0| action 1| action 2|...| action n
state 0 |
state 1 |
		|	  ____         _        _     _      
		|	 / __ \       | |      | |   | |     
		|	| |  | |______| |_ __ _| |__ | | ___ 
		|	| |  | |______| __/ _` | '_ \| |/ _ \
		|	| |__| |      | || (_| | |_) | |  __/
		|	 \___\_\       \__\__,_|_.__/|_|\___|
		|                             
state m |
*/

class LearningEngineBasic : public LearningEngineBase
{
private:
	float init_value;

    std::default_random_engine generator;
    std::bernoulli_distribution *explore;
    std::uniform_int_distribution<int> *actiongen;

	float **qtable;

	/* tracing related knobs */
	uint32_t trace_interval;
	uint64_t trace_timestamp;
	FILE *trace;
	uint32_t action_trace_interval;
	uint64_t action_trace_timestamp;
	FILE *action_trace;
	uint64_t m_action_counter;
	uint64_t m_early_exploration_window;

	struct
	{
		struct
		{
			uint64_t called;
			uint64_t explore;
			uint64_t exploit;
			uint64_t dist[MAX_ACTIONS][2]; /* 0:explored, 1:exploited */
		} action;

		struct
		{
			uint64_t called;
		} learn;
	} stats;

	float consultQ(uint32_t state, uint32_t action);
	void updateQ(uint32_t state, uint32_t action, float value);
	std::string getStringQ(uint32_t state);
	uint32_t getMaxAction(uint32_t state);
	void print_aux_stats();
	void dump_state_trace(uint32_t state);
	void plot_scores();
	void dump_action_trace(uint32_t action);

public:
	LearningEngineBasic(Prefetcher *p, float alpha, float gamma, float epsilon, uint32_t actions, uint32_t states, uint64_t seed, std::string policy, std::string type, bool zero_init, uint64_t early_exploration_window);
	~LearningEngineBasic();

	uint32_t chooseAction(uint32_t state);
	void learn(uint32_t state1, uint32_t action1, int32_t reward, uint32_t state2, uint32_t action2);
	void dump_stats();
};

#endif /* LEARNING_ENGINE */

