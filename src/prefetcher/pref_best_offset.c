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

  if(PREF_UMLC_ON && PREF_UMLC_BO_ON){
    DEBUG(0, "PREF_UMLC_ON is enabled\n");
    bo_prefetchers_array.bo_hwp_core_umlc = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bo_prefetchers_array.bo_hwp_core_umlc-> type = UMLC;
    init_bo_core(hwp, bo_prefetchers_array.bo_hwp_core_umlc);
  }
  if(PREF_UL1_ON && PREF_UL1_BO_ON){
    DEBUG(0, "PREF_UL1_ON is enabled\n");
    bo_prefetchers_array.bo_hwp_core_ul1  = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bo_prefetchers_array.bo_hwp_core_ul1-> type = UL1;
    init_bo_core(hwp, bo_prefetchers_array.bo_hwp_core_ul1);
  }
  if (PREF_DCACHE_BO_ON) {
    DEBUG(0, "Enabling data cache prefetch with BO\n");
    bo_prefetchers_array.bo_hwp_core_dcache  = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    init_bo_core(hwp, bo_prefetchers_array.bo_hwp_core_dcache);
  }
}

void pref_bo_umlc_hit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  STAT_EVENT(proc_id, PF_BO_MLC_NONPF_HIT);
}

void pref_bo_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  STAT_EVENT(proc_id, PF_BO_L1_NONPF_HIT);
}

void pref_bo_dcache_hit(Addr lineAddr, Addr loadPC) {
  uns proc_id = get_proc_id_from_cmp_addr(lineAddr);
  STAT_EVENT(proc_id, PF_BO_DCACHE_NONPF_HIT);
}

void pref_bo_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  if (!PREF_UL1_BO_ON || !PREF_UL1_ON || !PREF_BO_ON)
    return;
  DEBUG(proc_id, "PREF_BO_UL1 hit!\n");
  STAT_EVENT(proc_id, PF_BO_UL1_HIT);
  pref_bo_get_offset_ul1(&bo_prefetchers_array.bo_hwp_core_ul1[proc_id], proc_id, lineAddr, loadPC);
  pref_bo_train(&bo_prefetchers_array.bo_hwp_core_ul1[proc_id], proc_id, lineAddr, loadPC, TRUE);
}

void pref_bo_umlc_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  if (!PREF_UMLC_BO_ON || !PREF_UMLC_ON || !PREF_BO_ON)
    return;
  DEBUG(proc_id, "PREF_BO_UMLC hit!\n");
  STAT_EVENT(proc_id, PF_BO_UMLC_HIT);
  pref_bo_get_offset_umlc(&bo_prefetchers_array.bo_hwp_core_umlc[proc_id], proc_id, lineAddr, loadPC);
  pref_bo_train(&bo_prefetchers_array.bo_hwp_core_umlc[proc_id], proc_id, lineAddr, loadPC, TRUE);
}

void pref_bo_dcache_prefhit(Addr lineAddr, Addr loadPC) {
  if (!PREF_DCACHE_BO_ON || !PREF_BO_ON)
    return;
  DEBUG(0, "PREF_BO_DCACHE hit!\n");
  uns proc_id = get_proc_id_from_cmp_addr(lineAddr);
  STAT_EVENT(0, PF_BO_DCACHE_HIT);
  pref_bo_get_offset_dcache(&bo_prefetchers_array.bo_hwp_core_dcache[proc_id], proc_id, lineAddr, loadPC);
  pref_bo_train(&bo_prefetchers_array.bo_hwp_core_dcache[proc_id], proc_id, lineAddr, loadPC, TRUE);
}

void pref_bo_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  if (!PREF_UL1_BO_ON || !PREF_UL1_ON || !PREF_BO_ON)
    return;
  DEBUG(proc_id, "PREF_BO_UL1 miss!\n");
  STAT_EVENT(proc_id, PF_BO_UL1_MISS);
  pref_bo_get_offset_ul1(&bo_prefetchers_array.bo_hwp_core_ul1[proc_id], proc_id, lineAddr, loadPC);
  pref_bo_train(&bo_prefetchers_array.bo_hwp_core_ul1[proc_id], proc_id, lineAddr, loadPC, FALSE);
}

void pref_bo_umlc_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  if (!PREF_UMLC_BO_ON || !PREF_UMLC_ON || !PREF_BO_ON)
    return;
  DEBUG(proc_id, "PREF_BO_UMLC miss!\n");
  STAT_EVENT(proc_id, PF_BO_UMLC_MISS);
  pref_bo_get_offset_umlc(&bo_prefetchers_array.bo_hwp_core_umlc[proc_id], proc_id, lineAddr, loadPC);
  pref_bo_train(&bo_prefetchers_array.bo_hwp_core_umlc[proc_id], proc_id, lineAddr, loadPC, FALSE);
}

void pref_bo_dcache_miss(Addr lineAddr, Addr loadPC) {
  if (!PREF_DCACHE_BO_ON || !PREF_BO_ON)
    return;
  uns proc_id = get_proc_id_from_cmp_addr(lineAddr);
  DEBUG(proc_id, "PREF_BO_DCACHE miss!\n");
  STAT_EVENT(proc_id, PF_BO_DCACHE_MISS);
  pref_bo_get_offset_dcache(&bo_prefetchers_array.bo_hwp_core_dcache[proc_id], proc_id, lineAddr, loadPC);
  pref_bo_train(&bo_prefetchers_array.bo_hwp_core_dcache[proc_id], proc_id, lineAddr, loadPC, TRUE);
}

void init_bo_core(HWP* hwp, Pref_BO* bo_hwp_core) {
    DEBUG(0, "Call internal BO prefetcher initialization!\n");
    uns8 proc_id;
    for (proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      // Malloc the recent requests table
      bo_hwp_core[proc_id].recent_requests = (Addr*)malloc(sizeof(Addr) * RECENT_REQUESTS_SIZE);
      // Malloc the score table, and set all entries to 0 on init
      bo_hwp_core[proc_id].score_table = (uns*)malloc(sizeof(uns) * OFFSET_LIST_SIZE);
      bo_hwp_core[proc_id].current_prefetch_offset = offsets[0];
      bo_hwp_core[proc_id].offset_training_index = 0;
      bo_hwp_core[proc_id].current_round = 0;
      memset(bo_hwp_core[proc_id].score_table, 0, OFFSET_LIST_SIZE * sizeof(uns));
      bo_hwp_core[proc_id].hwp_info = hwp->hwp_info;
      bo_hwp_core[proc_id].hwp_info->enabled = TRUE;
      bo_hwp_core[proc_id].best_score = 0;
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
  DEBUG(proc_id, "Adding lineAddr %lld to recent requests table\n", lineAddr);
  STAT_EVENT(proc_id, PF_BO_NEW_RECENT_REQUEST);
  uns hash_index = hash_addr(lineAddr);
  bo_hwp_core->recent_requests[hash_index] = lineAddr;
}

void pref_bo_train(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC, Flag is_hit) {
  int retFlag;
  train_termination_check(proc_id, bo_hwp, &retFlag);
  if (retFlag == 1)
    return;

  // Calculate base_addr using the current test offset
  // Addr base_addr = lineAddr - offsets[bo_hwp->offset_training_index];
  Addr base_addr = 0;
  if (bo_hwp->type == UMLC) {
    base_addr = lineAddr - ((offsets[bo_hwp->offset_training_index]) << LOG2(MLC_LINE_SIZE));
  } else if (bo_hwp->type == UL1) {
    base_addr = lineAddr - ((offsets[bo_hwp->offset_training_index]) << LOG2(L1_LINE_SIZE));
  } else if (bo_hwp->type == DCACHE) {
    base_addr = lineAddr - ((offsets[bo_hwp->offset_training_index]) << LOG2(DCACHE_LINE_SIZE));
  }

  uns hash_index = hash_addr(base_addr);
  DEBUG(proc_id, "Currently testing offset %d in round %d\n", offsets[bo_hwp->offset_training_index], bo_hwp->current_round);
  DEBUG(proc_id, "Incoming lineAddr is %lld\n", lineAddr);
  if (bo_hwp->recent_requests[hash_index] == base_addr) {
    DEBUG(proc_id, "Found base_addr %lld in recent_requests table at index %d\n", base_addr, hash_index);
    bo_hwp->score_table[bo_hwp->offset_training_index]++;
  } 

  // Update indices
  bo_hwp->offset_training_index = (bo_hwp->offset_training_index + 1) % OFFSET_LIST_SIZE;
  if (bo_hwp->offset_training_index == 0) {
    bo_hwp->current_round++;
  }
}


void train_termination_check(uns8 proc_id, Pref_BO* bo_hwp, int *retFlag) {
  *retFlag = 1;
  int scoreMaxInd = -1;
  // dump_score_table(bo_hwp->score_table, OFFSET_LIST_SIZE);
  // Check for SCOREMAX
  uns scoreMax = 0;
  for(uns i = 0; i < OFFSET_LIST_SIZE; i++) {
    if (bo_hwp->score_table[i] == SCOREMAX) {
      STAT_EVENT(proc_id, PF_BO_SCOREMAX_REACHED);
      scoreMaxInd = i;
      DEBUG(proc_id, "SCOREMAX detected with offset being set to %d\n", offsets[i]);
      memset(bo_hwp->score_table, 0, sizeof(uns) * OFFSET_LIST_SIZE);
      bo_hwp->current_prefetch_offset = offsets[scoreMaxInd];
      bo_hwp->current_round           = 0;
      bo_hwp->offset_training_index   = 0;
      return;
    }
    if(bo_hwp->score_table[i] > scoreMax) {
      scoreMax   = bo_hwp->score_table[i];
    }
  }
  bo_hwp->best_score = scoreMax;

  // Check for ROUNDMAX
  if(bo_hwp->current_round == ROUNDMAX) {
    STAT_EVENT(proc_id, PF_BO_ROUNDMAX_REACHED);
    uns scoreMax = 0;
    for(uns i = 0; i < OFFSET_LIST_SIZE; i++) {
      if(bo_hwp->score_table[i] > scoreMax) {
        scoreMax    = bo_hwp->score_table[i];
        scoreMaxInd = i;
      }
    }
    if(scoreMaxInd == -1) {
      DEBUG(proc_id,
            "ROUNDMAX detected with offset being set to %d - all scores were "
            "equal\n",
            offsets[0]);
      bo_hwp->current_prefetch_offset = offsets[0];
    } else {
      DEBUG(proc_id, "ROUNDMAX detected with offset being set to %d\n",
            offsets[scoreMaxInd]);
      bo_hwp->current_prefetch_offset = offsets[scoreMaxInd];
    }
    bo_hwp->current_round = 0;
    memset(bo_hwp->score_table, 0, sizeof(uns) * OFFSET_LIST_SIZE);
    bo_hwp->offset_training_index = 0;
    return;
  }
  *retFlag = 0;
}

void pref_bo_get_offset_ul1(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr,
                            Addr loadPC) {
  if (bo_hwp->best_score > BADSCORE) {
    Addr prefetch_addr = lineAddr + ((bo_hwp->current_prefetch_offset) << LOG2(L1_LINE_SIZE));
    DEBUG(proc_id, "UL1 Prefetching with line addr %lld", prefetch_addr);
    // Queue access to the index of the line we want
    pref_addto_ul1req_queue(proc_id, (prefetch_addr >> LOG2(L1_LINE_SIZE)), bo_hwp->hwp_info->id);
  }
  if (bo_hwp->best_score > BADSCORE) {
    pref_update_rr(bo_hwp, lineAddr - (bo_hwp->current_prefetch_offset << LOG2(L1_LINE_SIZE)), proc_id);
  } else {
    pref_update_rr(bo_hwp, lineAddr, proc_id);
  }
}

void pref_bo_get_offset_umlc(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC) {
    if (bo_hwp->best_score > BADSCORE) {
      // shift increment left by the number of offset bits
      Addr prefetch_addr = lineAddr + ((bo_hwp->current_prefetch_offset) << LOG2(MLC_LINE_SIZE));
      // Queue access to the index of the line we want
      pref_addto_umlc_req_queue(proc_id, (prefetch_addr >> LOG2(MLC_LINE_SIZE)), bo_hwp->hwp_info->id);
    }
    pref_update_rr(bo_hwp, lineAddr, proc_id);
    DEBUG(proc_id, "Current UMLC offset: %d\n", bo_hwp->current_prefetch_offset);
    DEBUG(proc_id, "delta for address %d\n", ((bo_hwp->current_prefetch_offset) << LOG2(MLC_LINE_SIZE)));
    
    
}

void pref_bo_get_offset_dcache(Pref_BO* bo_hwp, uns8 proc_id, Addr lineAddr, Addr loadPC) {
    if (bo_hwp->best_score > BADSCORE) {
      // shift increment left by the number of offset bits
      Addr prefetch_addr = lineAddr + ((bo_hwp->current_prefetch_offset) << LOG2(L1_LINE_SIZE));
      DEBUG(proc_id, "DL0 Prefetching with line addr %lld", prefetch_addr);
      // Queue access to the index of the line we want
      new_mem_req(MRT_DPRF, 0,
                        (prefetch_addr >> LOG2(DCACHE_LINE_SIZE)),
                        L1_LINE_SIZE, 1, NULL,
                        (L2L1_FILL_PREF_CACHE ? dc_pref_cache_fill_line :
                                                dcache_fill_line),
                        unique_count,
                        0);
    }
    if (bo_hwp->best_score > BADSCORE) {
      pref_update_rr(bo_hwp, lineAddr - (bo_hwp->current_prefetch_offset << LOG2(DCACHE_LINE_SIZE)), proc_id);
    } else {
      pref_update_rr(bo_hwp, lineAddr, proc_id);
    }
}

void dump_recent_requests(Addr *recent_requests) {
  for (int i = 0; i < RECENT_REQUESTS_SIZE; i++) {
    printf("%lld ", recent_requests[i]);
    if ((i % 8) == 7) {
      printf("\n");
    }
  }
}

void dump_score_table(uns *score_table, uns size) {
  for (int i = 0; i < size; i++) {
    printf("%d ", score_table[i]);
    if ((i % 8) == 7) {
      printf("\n");
    }
  }
}