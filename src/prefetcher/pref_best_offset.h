#ifndef __PREF_BO_H__
#define __PREF_BO_H__
#include "pref_common.h"
#define RECENT_REQUESTS_SIZE 256
#define OFFSET_LIST_SIZE 52
#define SCOREMAX 31 // 5 bit scores
#define ROUNDMAX 100 // arbitrary
#define BADSCORE 1
typedef struct Pref_BO_Struct {
  HWP_Info* hwp_info;
  CacheLevel type;
  uns current_prefetch_offset;
  uns offset_training_index;
  Addr *recent_requests;
  uns *score_table;
  uns current_round;
  uns best_score;
} Pref_BO;

extern uns offsets[OFFSET_LIST_SIZE];

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
void pref_bo_train(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC,
                   Flag is_hit);
void train_termination_check(uns8 proc_id, Pref_BO* bo_hwp, int* retFlag);
void pref_bo_get_offset_ul1(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC);
void pref_bo_get_offset_umlc(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC);
void pref_update_rr(Pref_BO* bo_hwp_core, Addr lineAddr, uns8 proc_id);
uns hash_addr(Addr lineAddr);
void dump_recent_requests(Addr *recent_requests);
void dump_score_table(uns *score_table, uns size);
#endif
