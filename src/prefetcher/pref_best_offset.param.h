#ifndef __PREF_BO_PARAM_H__
#define __PREF_BO_PARAM_H__

#include "globals/global_types.h"

#define DEF_PARAM(name, variable, type, func, def, const) \
  extern const type variable;
#include "pref_best_offset.param.def"
#undef DEF_PARAM

#endif
