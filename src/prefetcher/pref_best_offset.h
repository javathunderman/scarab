#ifndef __PREF_BO_H__
#define __PREF_BO_H__
#include "pref_common.h"
#define RECENT_REQUESTS_SIZE 256
#define OFFSET_LIST_SIZE 52
#define SCOREMAX 31 // 5 bit scores
#define ROUNDMAX 100 // arbitrary
typedef struct Pref_BO_Struct {
  HWP_Info* hwp_info;
  CacheLevel type;
  uns current_prefetch_offset;
  uns offset_training_index;
  uns *recent_requests;
  uns *score_table;
  uns current_round;
} Pref_BO;

static uns offsets[OFFSET_LIST_SIZE] = {1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16,
 18, 20, 24, 25, 27, 30, 32, 36, 40, 45, 48, 50, 54, 60, 64, 72, 75, 80, 81, 90, 
 96, 100, 108, 120, 125, 128, 135, 144, 150, 160, 162, 180, 192, 200, 216, 225, 
 240, 243, 250, 256};

typedef struct{
  Pref_BO* bo_hwp_core_ul1;
  Pref_BO* bo_hwp_core_umlc;
} bo_prefetchers;
/*************************************************************/
/* HWP Interface */
void pref_bo_init(HWP* hwp);

// Taken from GHB prefetcher implementation
void pref_bo_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist);
void pref_bo_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist);
void pref_bo_umlc_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist);
void pref_bo_umlc_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist);

/*************************************************************/

void init_bo_core(HWP* hwp, Pref_BO* bo_hwp_core);

#endif
