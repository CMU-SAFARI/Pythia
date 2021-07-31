#ifndef PREFETCHER_H
#define PREFETCHER_H

#include <string>
#include <vector>

class Prefetcher
{
protected:
	std::string type;

public:
	Prefetcher(std::string _type) {type = _type;}
	~Prefetcher(){}
	std::string get_type() {return type;}
	virtual void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr) = 0;
	virtual void dump_stats() = 0;
	virtual void print_config() = 0;
};

#endif /* PREFETCHER_H */
