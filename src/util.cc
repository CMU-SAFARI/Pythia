#include <cassert>
#include <iostream>
#include <sstream>
#include "util.h"

/* helper function */
void gen_random(char *s, const int len) 
{
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) 
    {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    
    s[len] = 0;
}

uint32_t folded_xor(uint64_t value, uint32_t num_folds)
{
	assert(num_folds > 1);
	assert((num_folds & (num_folds-1)) == 0); /* has to be power of 2 */
	uint32_t mask = 0;
	uint32_t bits_in_fold = 64/num_folds;
	if(num_folds == 2)
	{
		mask = 0xffffffff;
	}
	else
	{
		mask = (1ul << bits_in_fold) - 1;
	}
	uint32_t folded_value = 0;
	for(uint32_t fold = 0; fold < num_folds; ++fold)
	{
		folded_value = folded_value ^ ((value >> (fold * bits_in_fold)) & mask);
	}
	return folded_value;
}

uint32_t HashZoo::jenkins(uint32_t key)
{
    // Robert Jenkins' 32 bit mix function
    key += (key << 12);
    key ^= (key >> 22);
    key += (key << 4);
    key ^= (key >> 9);
    key += (key << 10);
    key ^= (key >> 2);
    key += (key << 7);
    key ^= (key >> 12);
    return key;
}

uint32_t HashZoo::knuth(uint32_t key)
{
    // Knuth's multiplicative method
    key = (key >> 3) * 2654435761;
    return key;
}

uint32_t HashZoo::murmur3(uint32_t key)
{
	/* TODO: define it using murmur3's finilization steps */
	assert(false);
}

/* originally ment for 32b key */
uint32_t HashZoo::jenkins32(uint32_t key)
{
   key = (key+0x7ed55d16) + (key<<12);
   key = (key^0xc761c23c) ^ (key>>19);
   key = (key+0x165667b1) + (key<<5);
   key = (key+0xd3a2646c) ^ (key<<9);
   key = (key+0xfd7046c5) + (key<<3);
   key = (key^0xb55a4f09) ^ (key>>16);
   return key;
}

/* originally ment for 32b key */
uint32_t HashZoo::hash32shift(uint32_t key)
{
	key = ~key + (key << 15); // key = (key << 15) - key - 1;
	key = key ^ (key >> 12);
	key = key + (key << 2);
	key = key ^ (key >> 4);
	key = key * 2057; // key = (key + (key << 3)) + (key << 11);
	key = key ^ (key >> 16);
	return key;
}

/* originally ment for 32b key */
uint32_t HashZoo::hash32shiftmult(uint32_t key)
{
	int c2=0x27d4eb2d; // a prime or an odd constant
	key = (key ^ 61) ^ (key >> 16);
	key = key + (key << 3);
	key = key ^ (key >> 4);
	key = key * c2;
	key = key ^ (key >> 15);
	return key;
}

uint32_t HashZoo::hash64shift(uint32_t key)
{
	key = (~key) + (key << 21); // key = (key << 21) - key - 1;
	key = key ^ (key >> 24);
	key = (key + (key << 3)) + (key << 8); // key * 265
	key = key ^ (key >> 14);
	key = (key + (key << 2)) + (key << 4); // key * 21
	key = key ^ (key >> 28);
	key = key + (key << 31);
	return key;
}

uint32_t HashZoo::hash5shift(uint32_t key)
{
	key = (key ^ 61) ^ (key >> 16);
    key = key + (key << 3);
    key = key ^ (key >> 4);
    key = key * 0x27d4eb2d;
    key = key ^ (key >> 15);
    return key;
}

/* hash6shift is jenkin32 */

uint32_t HashZoo::hash7shift(uint32_t key)
{
    key -= (key << 6);
    key ^= (key >> 17);
    key -= (key << 9);
    key ^= (key << 4);
    key -= (key << 3);
    key ^= (key << 10);
    key ^= (key >> 15);
    return key ;
}

/* use low bit values */
uint32_t HashZoo::Wang6shift(uint32_t key)
{
    key += ~(key << 15);
    key ^=  (key >> 10);
    key +=  (key << 3);
    key ^=  (key >> 6);
    key += ~(key << 11);
    key ^=  (key >> 16);
    return key;
}

uint32_t HashZoo::Wang5shift(uint32_t key)
{
    key = (key + 0x479ab41d) + (key << 8);
    key = (key ^ 0xe4aa10ce) ^ (key >> 5);
    key = (key + 0x9942f0a6) - (key << 14);
    key = (key ^ 0x5aedd67d) ^ (key >> 3);
    key = (key + 0x17bea992) + (key << 7);
    return key;
}

uint32_t HashZoo::Wang4shift( uint32_t key)
{
    key = (key ^ 0xdeadbeef) + (key << 4);
    key = key ^ (key >> 10);
    key = key + (key << 7);
    key = key ^ (key >> 13);
    return key;
}

uint32_t HashZoo::Wang3shift( uint32_t key)
{
    key = key ^ (key >> 4);
    key = (key ^ 0xdeadbeef) + (key << 5);
    key = key ^ (key >> 11);
    return key;
}

uint32_t HashZoo::three_hybrid1(uint32_t key) { return knuth(hash64shift(jenkins32(key))); }
uint32_t HashZoo::three_hybrid2(uint32_t key) { return jenkins32(Wang5shift(hash5shift(key))); }
uint32_t HashZoo::three_hybrid3(uint32_t key) { return jenkins(hash32shiftmult(Wang3shift(key))); }
uint32_t HashZoo::three_hybrid4(uint32_t key) { return Wang6shift(hash32shift(Wang5shift(key))); }
uint32_t HashZoo::three_hybrid5(uint32_t key) { return hash64shift(hash32shift(knuth(key))); }
uint32_t HashZoo::three_hybrid6(uint32_t key) { return hash5shift(jenkins(Wang6shift(key))); }
uint32_t HashZoo::three_hybrid7(uint32_t key) { return Wang4shift(jenkins32(hash7shift(key))); }
uint32_t HashZoo::three_hybrid8(uint32_t key) { return Wang3shift(Wang6shift(hash64shift(key))); }
uint32_t HashZoo::three_hybrid9(uint32_t key) { return hash32shift(Wang3shift(jenkins(key))); }
uint32_t HashZoo::three_hybrid10(uint32_t key) { return hash32shiftmult(Wang4shift(hash32shiftmult(key))); }
uint32_t HashZoo::three_hybrid11(uint32_t key) { return hash7shift(hash5shift(Wang4shift(key))); }
uint32_t HashZoo::three_hybrid12(uint32_t key) { return Wang5shift(jenkins32(hash32shift(key))); }

uint32_t HashZoo::four_hybrid1(uint32_t key) { return Wang6shift(Wang5shift(Wang3shift(Wang4shift(key)))); }
uint32_t HashZoo::four_hybrid2(uint32_t key) { return hash32shiftmult(jenkins(Wang5shift(Wang6shift(key)))); }
uint32_t HashZoo::four_hybrid3(uint32_t key) { return hash64shift(hash7shift(jenkins32(hash32shift(key)))); }
uint32_t HashZoo::four_hybrid4(uint32_t key) { return knuth(knuth(hash32shiftmult(hash5shift(key)))); }
uint32_t HashZoo::four_hybrid5(uint32_t key) { return jenkins32(Wang4shift(hash64shift(hash32shiftmult(key)))); }
uint32_t HashZoo::four_hybrid6(uint32_t key) { return jenkins(hash32shift(Wang4shift(Wang3shift(key)))); }
uint32_t HashZoo::four_hybrid7(uint32_t key) { return hash32shift(hash64shift(hash5shift(hash64shift(key)))); }
uint32_t HashZoo::four_hybrid8(uint32_t key) { return hash7shift(hash5shift(hash32shiftmult(Wang6shift(key)))); }
uint32_t HashZoo::four_hybrid9(uint32_t key) { return hash32shiftmult(Wang6shift(jenkins32(knuth(key)))); }
uint32_t HashZoo::four_hybrid10(uint32_t key) { return Wang3shift(jenkins32(knuth(jenkins(key)))); }
uint32_t HashZoo::four_hybrid11(uint32_t key) { return hash5shift(hash32shiftmult(hash32shift(jenkins32(key)))); }
uint32_t HashZoo::four_hybrid12(uint32_t key) { return Wang4shift(Wang3shift(jenkins(hash7shift(key)))); }

uint32_t HashZoo::getHash(uint32_t selector, uint32_t key)
{
    switch(selector)
    {
        case 1:     return key;
        case 2:     return jenkins(key);
        case 3:     return knuth(key);
        case 4:     return murmur3(key);
        case 5:     return jenkins32(key);
        case 6:     return hash32shift(key);
        case 7:     return hash32shiftmult(key);
        case 8:     return hash64shift(key);
        case 9:     return hash5shift(key);
        case 10:    return hash7shift(key);
        case 11:    return Wang6shift(key);
        case 12:    return Wang5shift(key);
        case 13:    return Wang4shift(key);
        case 14:    return Wang3shift(key);
        
        /* three hybrid */
        case 101:  return three_hybrid1(key);
        case 102:  return three_hybrid2(key);
        case 103:  return three_hybrid3(key);
        case 104:  return three_hybrid4(key);
        case 105:  return three_hybrid5(key);
        case 106:  return three_hybrid6(key);
        case 107:  return three_hybrid7(key);
        case 108:  return three_hybrid8(key);
        case 109:  return three_hybrid9(key);
        case 110:  return three_hybrid10(key);
        case 111:  return three_hybrid11(key);
        case 112:  return three_hybrid12(key);

        /* four hybrid */
        case 1001:  return four_hybrid1(key);
        case 1002:  return four_hybrid2(key);
        case 1003:  return four_hybrid3(key);
        case 1004:  return four_hybrid4(key);
        case 1005:  return four_hybrid5(key);
        case 1006:  return four_hybrid6(key);
        case 1007:  return four_hybrid7(key);
        case 1008:  return four_hybrid8(key);
        case 1009:  return four_hybrid9(key);
        case 1010:  return four_hybrid10(key);
        case 1011:  return four_hybrid11(key);
        case 1012:  return four_hybrid12(key);

        default:    assert(false);
    }
}