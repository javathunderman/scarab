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

// cannot do offset initialization in header file?
uns offsets[] = {1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 25, 27, 30,
 32, 36, 40, 45, 48, 50, 54, 60, 64, 72, 75, 80, 81, 90, 96, 100, 108, 120, 125, 
 128, 135, 144, 150, 160, 162, 180, 192, 200, 216, 225, 240, 243, 250, 256};

void pref_bo_init(HWP* hwp) {
  DEBUG(0, "Called pref_bo_init public function\n");
  if(!PREF_BO_ON)
    return;
  DEBUG(0, "PREF_BO_ON is enabled\n");
  // PREF_UMLC/PREF_UL1 determines the cache line that uses this prefetcher?
  if(PREF_UMLC_ON){
    DEBUG(0, "PREF_UMLC_ON is enabled\n");
    bo_prefetchers_array.bo_hwp_core_umlc = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bo_prefetchers_array.bo_hwp_core_umlc-> type = UMLC;
    init_bo_core(hwp, bo_prefetchers_array.bo_hwp_core_umlc);
  }
  if(PREF_UL1_ON){
    DEBUG(0, "PREF_UL1_ON is enabled\n");
    bo_prefetchers_array.bo_hwp_core_ul1  = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bo_prefetchers_array.bo_hwp_core_ul1-> type = UL1;
    init_bo_core(hwp, bo_prefetchers_array.bo_hwp_core_ul1);
  }
}

void pref_bo_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  DEBUG(0, "PREF_BO_UL1 hit!\n");
  STAT_EVENT(proc_id, PF_BO_UL1_HIT);
  pref_bo_train(&bo_prefetchers_array.bo_hwp_core_ul1[proc_id], proc_id, lineAddr, loadPC, TRUE);
}

void pref_bo_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  DEBUG(0, "PREF_BO_UL1 miss!\n");
  STAT_EVENT(proc_id, PF_BO_UL1_MISS);
  pref_bo_train(&bo_prefetchers_array.bo_hwp_core_ul1[proc_id], proc_id, lineAddr, loadPC, FALSE);
}

void init_bo_core(HWP* hwp, Pref_BO* bo_hwp_core) {
    DEBUG(0, "Call internal BO prefetcher initialization!\n");
    uns8 proc_id;
    for (proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      // Malloc the recent requests table
      bo_hwp_core->recent_requests = (Addr*)malloc(sizeof(Addr) * RECENT_REQUESTS_SIZE);
      // Malloc the score table, and set all entries to 0 on init
      bo_hwp_core->score_table = (uns*)malloc(sizeof(uns) * OFFSET_LIST_SIZE);
      bo_hwp_core->current_prefetch_offset = 1;
      bo_hwp_core->offset_training_index = 0;
      bo_hwp_core->current_round = 0;
      memset(bo_hwp_core->score_table, 0, OFFSET_LIST_SIZE * sizeof(uns));
      bo_hwp_core->hwp_info = hwp->hwp_info;
      bo_hwp_core->hwp_info->enabled = TRUE;
    }
}

uns hash_addr(Addr lineAddr) {
  // XOR the last 16 bits with each other in 8 bit increments
  uns lsb_8 = lineAddr & 0xFF;
  uns next_8 = (lineAddr & 0xFF00) >> 8;
  return (lsb_8 ^ next_8);
}

// Add the new base address to the recent_requests table, using the hash of the line addr
void pref_update_rr(Pref_BO* bo_hwp_core, Addr lineAddr, uns8 proc_id) {
  DEBUG(0, "Adding lineAddr %lld with base address %lld to recent requests table\n", lineAddr, (lineAddr - bo_hwp_core->current_prefetch_offset));
  STAT_EVENT(proc_id, PF_BO_NEW_RECENT_REQUEST);
  bo_hwp_core->recent_requests[hash_addr(lineAddr)] = lineAddr - bo_hwp_core->current_prefetch_offset;
}

void pref_bo_train(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC, Flag is_hit) {
  int scoreMaxInd = -1;
  // Look through score table for an existing SCOREMAX entry
  DEBUG(proc_id, "Currently testing offset %d in round %d\n", offsets[bo_hwp->current_prefetch_offset], bo_hwp->current_round);
  for (uns i = 0; i < OFFSET_LIST_SIZE; i++) {
    if (bo_hwp->score_table[i] == SCOREMAX) {
      STAT_EVENT(proc_id, PF_BO_SCOREMAX_REACHED);
      scoreMaxInd = i;
      DEBUG(proc_id, "SCOREMAX detected with offset being set to %d\n", offsets[i]);
      memset(bo_hwp->score_table, 0, sizeof(uns) * OFFSET_LIST_SIZE);
      bo_hwp->current_prefetch_offset = offsets[scoreMaxInd];
      bo_hwp->current_round = 0;
      return;
    }
  }
  
  if (bo_hwp->current_round == ROUNDMAX) {
    STAT_EVENT(proc_id, PF_BO_ROUNDMAX_REACHED);
    uns scoreMax = 0;
    for (uns i = 0; i < OFFSET_LIST_SIZE; i++) {
      if (bo_hwp->score_table[i] > scoreMax) {
        scoreMax = bo_hwp->score_table[i];
        scoreMaxInd = i;
      }
    }
    if (scoreMaxInd == -1) {
      DEBUG(proc_id, "ROUNDMAX detected with offset being set to 1 - all scores were equal\n");
      bo_hwp->current_prefetch_offset = offsets[0];
    } else {
      DEBUG(proc_id, "ROUNDMAX detected with offset being set to %d\n", offsets[scoreMaxInd]);
      bo_hwp->current_prefetch_offset = offsets[scoreMaxInd];
    }
    bo_hwp->current_round = 0;
    memset(bo_hwp->score_table, 0, sizeof(uns) * OFFSET_LIST_SIZE);
    return;
  }

  // If we have neither a SCOREMAX or ROUNDMAX case, continue updating the score table

  // Calculate base_addr using the current test offset
  Addr base_addr = lineAddr - offsets[bo_hwp->offset_training_index];
  uns hash_index = hash_addr(base_addr);
  DEBUG(proc_id, "Incoming lineAddr is %lld\n", lineAddr);
  // If the recent requests table contains the base address of the incoming PF request
  if (bo_hwp->recent_requests[hash_index] == base_addr) {
    // our current test offset is good - increment its score
    DEBUG(proc_id, "Found base_addr %lld in recent_requests table, with hash_index %d!\nDumping all recent requests:\n", base_addr, hash_index);
    dump_recent_requests(bo_hwp->recent_requests);
    bo_hwp->score_table[bo_hwp->offset_training_index]++;
  } else {
      // If it doesn't match, then check if the score is greater than 0 (underflow prevention check)
      if (bo_hwp->score_table[bo_hwp->offset_training_index] > 0) {
        bo_hwp->score_table[bo_hwp->offset_training_index]--;
      } else {
        // Set to 0 otherwise
        bo_hwp->score_table[bo_hwp->offset_training_index] = 0;
      }
      DEBUG(proc_id, "Did not find base_addr %lld in recent_requests table, with hash_index %d!\nDumping all recent requests:\n", base_addr, hash_index);
      dump_recent_requests(bo_hwp->recent_requests);
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
}

void pref_bo_get_offset(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC) {
    pref_addto_ul1req_queue_set(proc_id, lineAddr, bo_hwp->hwp_info->id, 0, loadPC, 0, FALSE);
    pref_update_rr(bo_hwp, lineAddr, proc_id);
}

void dump_recent_requests(Addr *recent_requests) {
  for (int i = 0; i < RECENT_REQUESTS_SIZE; i++) {
    printf("%lld ", recent_requests[i]);
    if ((i % 8) == 0) {
      printf("\n");
    }
  }
}