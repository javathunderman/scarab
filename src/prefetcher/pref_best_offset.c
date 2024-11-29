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
    bo_hwp_core->recent_requests = (uns*)malloc(sizeof(uns) * RECENT_REQUESTS_SIZE);
    // Malloc the score table, and set all entries to 0 on init
    bo_hwp_core->score_table = (uns*)malloc(sizeof(uns) * OFFSET_LIST_SIZE);
    bo_hwp_core->current_prefetch_offset = 1;
    bo_hwp_core->offset_training_index = 0;
    bo_hwp_core->current_round = 0;
    memset(bo_hwp_core->score_table, 0, OFFSET_LIST_SIZE * sizeof(uns));
}

uns hash_addr(Addr lineAddr) {
  // XOR the last 16 bits with each other in 8 bit increments
  uns lsb_8 = lineAddr & 0xFF;
  uns next_8 = (lineAddr & 0xFF00) >> 8;
  return (lsb_8 ^ next_8);
}

// Add the new base address to the recent_requests table, using the hash of the line addr
void pref_update_rr(Pref_BO* bo_hwp_core, Addr lineAddr) {
  DEBUG(0, "Adding lineAddr %lld with base address %lld to recent requests table\n", lineAddr, (lineAddr - bo_hwp_core->current_prefetch_offset));
  bo_hwp_core->recent_requests[hash_addr(lineAddr)] = lineAddr - bo_hwp_core->current_prefetch_offset;
}

void pref_bo_train(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC, Flag is_hit) {
  int scoreMaxInd = -1;
  // Look through score table for an existing SCOREMAX entry
  for (uns i = 0; i < OFFSET_LIST_SIZE; i++) {
    if (bo_hwp->score_table[i] == SCOREMAX) {
      scoreMaxInd = i;
      memset(bo_hwp->score_table, 0, sizeof(uns) * OFFSET_LIST_SIZE);
      bo_hwp->current_prefetch_offset = offsets[scoreMaxInd];
      bo_hwp->current_round = 0;
      return;
    }
  }
  
  if (bo_hwp->current_round == ROUNDMAX) {
    uns scoreMax = 0;
    for (uns i = 0; i < OFFSET_LIST_SIZE; i++) {
      if (bo_hwp->score_table[i] > scoreMax) {
        scoreMax = bo_hwp->score_table[i];
        scoreMaxInd = i;
      }
    }
    memset(bo_hwp->score_table, 0, sizeof(uns) * OFFSET_LIST_SIZE);
    bo_hwp->current_prefetch_offset = offsets[scoreMaxInd];
    bo_hwp->current_round = 0;
    return;
  }

  // If we have neither a SCOREMAX or ROUNDMAX case, continue updating the score table

  // Calculate base_addr using the current test offset
  Addr base_addr = lineAddr - offsets[bo_hwp->offset_training_index];
  uns hash_index = hash_addr(base_addr);

  // If the recent requests table contains the base address of the incoming PF request
  if (bo_hwp->recent_requests[hash_index] == base_addr) {
    // our current test offset is good - increment its score
    bo_hwp->score_table[bo_hwp->offset_training_index]++;
  } else {
      // If it doesn't match, then check if the score is greater than 0 (underflow prevention check)
      if (bo_hwp->score_table[bo_hwp->offset_training_index] > 0) {
        bo_hwp->score_table[bo_hwp->offset_training_index]--;
      } else {
        // Set to 0 otherwise
        bo_hwp->score_table[bo_hwp->offset_training_index] = 0;
      }
  }

  // Increment the test offset by 1 to move to the d_(i + 1)th offset
  uns new_pf_test_offset = bo_hwp->current_prefetch_offset + 1;
  // If we run out of offsets, increment the round counter and reset the test offset index to 0
  if (new_pf_test_offset >= OFFSET_LIST_SIZE) {
    bo_hwp->current_round++;
    bo_hwp->current_prefetch_offset = 0;
  } else {
    // otherwise just increment
    bo_hwp->current_prefetch_offset = new_pf_test_offset;
  }

  // TODO: update RR before or after we check the score table?
  pref_update_rr(bo_hwp, lineAddr);
}

void pref_bo_get_offset(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC, Flag is_hit) {
    return;
}