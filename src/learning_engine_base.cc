#include <assert.h>
#include "learning_engine_base.h"

const char* PolicyString[] = {"EGreddy"};
const char* MapPolicyString(Policy policy)
{
	assert((uint32_t)policy < Policy::NumPolicies);
	return PolicyString[(uint32_t)policy];
}

const char* LearningTypeString[] = {"QLearning", "SARSA"};
const char* MapLearningTypeString(LearningType type)
{
	assert((uint32_t)type < LearningType::NumLearningTypes);
	return LearningTypeString[(uint32_t)type];
}

LearningEngineBase::LearningEngineBase(Prefetcher *parent, float alpha, float gamma, float epsilon, uint32_t actions, uint32_t states, uint64_t seed, std::string policy, std::string type)
	: m_parent(parent)
	, m_alpha(alpha)
	, m_gamma(gamma)
	, m_epsilon(epsilon) // make it small, as true value indicates exploration
	, m_actions(actions)
	, m_states(states)
	, m_seed(seed)
	, m_policy(parsePolicy(policy))
	, m_type(parseLearningType(type))
{

}

LearningType LearningEngineBase::parseLearningType(std::string str)
{
	if(!str.compare("QLearning"))	return LearningType::QLearning;
	if(!str.compare("SARSA"))		return LearningType::SARSA;

	printf("unsupported learning_type %s\n", str.c_str());
	assert(false);
	return LearningType::InvalidLearningType;
}

Policy LearningEngineBase::parsePolicy(std::string str)
{
	if(!str.compare("EGreedy"))		return Policy::EGreedy;

	printf("unsupported policy %s\n", str.c_str());
	assert(false);
	return Policy::InvalidPolicy;
}