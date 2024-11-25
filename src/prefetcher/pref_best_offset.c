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
    init_ghb_core(hwp, bo_prefetchers_array.bo_hwp_core_umlc);
  }
  if(PREF_UL1_ON){
    bo_prefetchers_array.bo_hwp_core_ul1  = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bo_prefetchers_array.bo_hwp_core_ul1-> type = UL1;
    init_ghb_core(hwp, bo_prefetchers_array.bo_hwp_core_ul1);
  }

}

void init_ghb_core(HWP* hwp, Pref_BO* bo_hwp_core) {
    bo_hwp_core->bo_tables->recent_requests = (Hash_Table*)malloc(sizeof(Hash_Table));
    init_hash_table(bo_hwp_core->bo_tables->recent_requests, "recent requests", OFFSET_LIST_SIZE, sizeof(uns));
}