#include "ir3_nir.h"

#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 2
 * transforms:
 *    ('fsin', 'x@32') => ('fsin', ('fsub', ('fmul', 6.2831853, ('ffract', ('fadd', ('fmul', 0.15915494, 'x'), 0.5))), 3.14159265))
 *    ('fcos', 'x@32') => ('fcos', ('fsub', ('fmul', 6.2831853, ('ffract', ('fadd', ('fmul', 0.15915494, 'x'), 0.5))), 3.14159265))
 */


   static const nir_search_variable search0_0 = {
   { nir_search_value_variable, 32 },
   0, /* x */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_fsin,
   { &search0_0.value },
   NULL,
};

   static const nir_search_constant replace0_0_0_0 = {
   { nir_search_value_constant, 32 },
   nir_type_float, { 0x401921fb53c8d4f1 /* 6.2831853 */ },
};

static const nir_search_constant replace0_0_0_1_0_0_0 = {
   { nir_search_value_constant, 32 },
   nir_type_float, { 0x3fc45f306725feed /* 0.15915494 */ },
};

/* replace0_0_0_1_0_0_1 -> search0_0 in the cache */
static const nir_search_expression replace0_0_0_1_0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   2, 1,
   nir_op_fmul,
   { &replace0_0_0_1_0_0_0.value, &search0_0.value },
   NULL,
};

static const nir_search_constant replace0_0_0_1_0_1 = {
   { nir_search_value_constant, 32 },
   nir_type_float, { 0x3fe0000000000000 /* 0.5 */ },
};
static const nir_search_expression replace0_0_0_1_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   1, 2,
   nir_op_fadd,
   { &replace0_0_0_1_0_0.value, &replace0_0_0_1_0_1.value },
   NULL,
};
static const nir_search_expression replace0_0_0_1 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 2,
   nir_op_ffract,
   { &replace0_0_0_1_0.value },
   NULL,
};
static const nir_search_expression replace0_0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 3,
   nir_op_fmul,
   { &replace0_0_0_0.value, &replace0_0_0_1.value },
   NULL,
};

static const nir_search_constant replace0_0_1 = {
   { nir_search_value_constant, 32 },
   nir_type_float, { 0x400921fb53c8d4f1 /* 3.14159265 */ },
};
static const nir_search_expression replace0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 3,
   nir_op_fsub,
   { &replace0_0_0.value, &replace0_0_1.value },
   NULL,
};
static const nir_search_expression replace0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 3,
   nir_op_fsin,
   { &replace0_0.value },
   NULL,
};

   /* search1_0 -> search0_0 in the cache */
static const nir_search_expression search1 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_fcos,
   { &search0_0.value },
   NULL,
};

   /* replace1_0_0_0 -> replace0_0_0_0 in the cache */

/* replace1_0_0_1_0_0_0 -> replace0_0_0_1_0_0_0 in the cache */

/* replace1_0_0_1_0_0_1 -> search0_0 in the cache */
/* replace1_0_0_1_0_0 -> replace0_0_0_1_0_0 in the cache */

/* replace1_0_0_1_0_1 -> replace0_0_0_1_0_1 in the cache */
/* replace1_0_0_1_0 -> replace0_0_0_1_0 in the cache */
/* replace1_0_0_1 -> replace0_0_0_1 in the cache */
/* replace1_0_0 -> replace0_0_0 in the cache */

/* replace1_0_1 -> replace0_0_1 in the cache */
/* replace1_0 -> replace0_0 in the cache */
static const nir_search_expression replace1 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 3,
   nir_op_fcos,
   { &replace0_0.value },
   NULL,
};


static const struct transform ir3_nir_apply_trig_workarounds_state2_xforms[] = {
  { &search0, &replace0.value, 0 },
};
static const struct transform ir3_nir_apply_trig_workarounds_state3_xforms[] = {
  { &search1, &replace1.value, 0 },
};

static const struct per_op_table ir3_nir_apply_trig_workarounds_table[nir_num_search_ops] = {
   [nir_op_fsin] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         2,
      },
   },
   [nir_op_fcos] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         3,
      },
   },
};

const struct transform *ir3_nir_apply_trig_workarounds_transforms[] = {
   NULL,
   NULL,
   ir3_nir_apply_trig_workarounds_state2_xforms,
   ir3_nir_apply_trig_workarounds_state3_xforms,
};

const uint16_t ir3_nir_apply_trig_workarounds_transform_counts[] = {
   0,
   0,
   (uint16_t)ARRAY_SIZE(ir3_nir_apply_trig_workarounds_state2_xforms),
   (uint16_t)ARRAY_SIZE(ir3_nir_apply_trig_workarounds_state3_xforms),
};

bool
ir3_nir_apply_trig_workarounds(nir_shader *shader)
{
   bool progress = false;
   bool condition_flags[1];
   const nir_shader_compiler_options *options = shader->options;
   const shader_info *info = &shader->info;
   (void) options;
   (void) info;

   condition_flags[0] = true;

   nir_foreach_function(function, shader) {
      if (function->impl) {
         progress |= nir_algebraic_impl(function->impl, condition_flags,
                                        ir3_nir_apply_trig_workarounds_transforms,
                                        ir3_nir_apply_trig_workarounds_transform_counts,
                                        ir3_nir_apply_trig_workarounds_table);
      }
   }

   return progress;
}

