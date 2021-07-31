#ifndef KNOBS_H
#define KNOBS_H

#include <vector>
#include <cstdint>
#define MAX_LEN 256

void parse_args(int argc, char* argv[]);
void parse_config(char *config_file_name);
int parse_knobs(void* user, const char* section, const char* name, const char* value);
int handler(void* user, const char* section, const char* name, const char* value);

/* auxiliary functions */
std::vector<int32_t> get_array_int(const char *str);
std::vector<float> get_array_float(const char *str);

#endif /* KNOBS_H */