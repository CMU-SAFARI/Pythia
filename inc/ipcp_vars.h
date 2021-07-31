#ifndef IPCP_VARS_H
#define IPCP_VARS_H

/***************************************************************************
Code taken from
Samuel Pakalapati - samuelpakalapati@gmail.com
Biswabandan Panda - biswap@cse.iitk.ac.in
***************************************************************************/

#define NUM_IP_TABLE_L1_ENTRIES 1024                        // IP table entries
#define NUM_GHB_ENTRIES 16                                  // Entries in the GHB
#define NUM_IP_INDEX_BITS 10                                // Bits to index into the IP table
#define NUM_IP_TAG_BITS 6                                   // Tag bits per IP table entry
#define S_TYPE 1                                            // stream
#define CS_TYPE 2                                           // constant stride
#define CPLX_TYPE 3                                         // complex stride
#define NL_TYPE 4                                           // next line
#define NUM_IP_TABLE_L2_ENTRIES 1024

// #define SIG_DEBUG_PRINT
#ifdef SIG_DEBUG_PRINT
#define SIG_DP(x) x
#else
#define SIG_DP(x)
#endif

// #define SIG_DEBUG_PRINT_L2
#ifdef SIG_DEBUG_PRINT_L2
#define SIG_DP(x) x
#else
#define SIG_DP(x)
#endif

#endif /* IPCP_VARS_H */
