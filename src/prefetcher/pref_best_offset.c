#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "globals/assert.h"
#include "globals/utils.h"
#include "op.h"

#include "core.param.h"
#include "dcache_stage.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "prefetcher/pref_best_offset.h"
#include "prefetcher/pref_best_offset.param.h"
#include "statistics.h"

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_PREF_BO, ##args)
bo_prefetchers bo_prefetchers_array;

void pref_ghb_init(HWP* hwp) {
  if(!PREF_BO_ON)
    return;
  // PREF_UMLC/PREF_UL1 determines the cache line that uses this prefetcher?
  if(PREF_UMLC_ON){
    bo_prefetchers_array.bo_hwp_core_umlc = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bo_prefetchers_array.bo_hwp_core_umlc-> type = UMLC;
    init_bo_core(hwp, bo_prefetchers_array.bo_hwp_core_umlc);
  }
  if(PREF_UL1_ON){
    bo_prefetchers_array.bo_hwp_core_ul1  = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bo_prefetchers_array.bo_hwp_core_ul1-> type = UL1;
    init_bo_core(hwp, bo_prefetchers_array.bo_hwp_core_ul1);
  }

}

void init_bo_core(HWP* hwp, Pref_BO* bo_hwp_core) {
    // Malloc the recent requests table
    bo_hwp_core->bo_tables->recent_requests = (uns*)malloc(sizeof(uns) * RECENT_REQUESTS_SIZE);
    // Malloc the score table, and set all entries to 0 on init
    bo_hwp_core->score_table = (uns*)malloc(sizeof(uns) * OFFSET_LIST_SIZE);
    bo_hwp_core->current_prefetch_offset = 1;
    bo_hwp_core->offset_training_index = 0;
    bo_hwp_core->current_round = 0;
    memset(bo_hwp_core->score_table, 0, OFFSET_LIST_SIZE * sizeof(uns));
}

uns hash_addr(Addr lineAddr) {
  uns lsb_8 = lineAddr & 0xFF;
  uns next_8 = (lineAddr & 0xFF00) >> 8;
  return (lsb_8 ^ next_8);
}
void pref_update_rr(Pref_BO* bo_hwp_core, Addr lineAddr) {
  DEBUG(0, "Adding lineAddr %lld with base address %lld to recent requests table\n", lineAddr, (lineAddr - bo_hwp_core->current_prefetch_offset));
  bo_hwp_core->recent_requests[hash_addr(lineAddr)] = lineAddr - bo_hwp_core->current_prefetch_offset;
}
uns pref_bo_get_offset(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC, Flag is_hit) {
  int scoreMaxInd = -1;
  for (uns i = 0; i < OFFSET_LIST_SIZE; i++) {
    if (bo_hwp->score_table[i] == SCOREMAX) {
      scoreMaxInd = i;
      break;
    }
  }
  if (bo_hwp->current_round < ROUNDMAX) {
    // we still have more training to do here for subsequent rounds
    return bo_hwp->current_prefetch_offset;
  }
  return -1;
}