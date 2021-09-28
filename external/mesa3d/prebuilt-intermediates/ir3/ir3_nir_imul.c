#include "ir3_nir.h"

#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 2
 * transforms:
 *    ('imul', 'a@32', 'b@32') => ('imadsh_mix16', 'b', 'a', ('imadsh_mix16', 'a', 'b', ('umul_low', 'a', 'b')))
 *    ('iadd', ('imul24', 'a', 'b'), 'c') => ('imad24_ir3', 'a', 'b', 'c')
 */


   static const nir_search_variable search0_0 = {
   { nir_search_value_variable, 32 },
   0, /* a */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};

static const nir_search_variable search0_1 = {
   { nir_search_value_variable, 32 },
   1, /* b */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search0 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_imul,
   { &search0_0.value, &search0_1.value },
   NULL,
};

   /* replace0_0 -> search0_1 in the cache */

/* replace0_1 -> search0_0 in the cache */

/* replace0_2_0 -> search0_0 in the cache */

/* replace0_2_1 -> search0_1 in the cache */

/* replace0_2_2_0 -> search0_0 in the cache */

/* replace0_2_2_1 -> search0_1 in the cache */
static const nir_search_expression replace0_2_2 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_umul_low,
   { &search0_0.value, &search0_1.value },
   NULL,
};
static const nir_search_expression replace0_2 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 1,
   nir_op_imadsh_mix16,
   { &search0_0.value, &search0_1.value, &replace0_2_2.value },
   NULL,
};
static const nir_search_expression replace0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 1,
   nir_op_imadsh_mix16,
   { &search0_1.value, &search0_0.value, &replace0_2.value },
   NULL,
};

   /* search1_0_0 -> search0_0 in the cache */

/* search1_0_1 -> search0_1 in the cache */
static const nir_search_expression search1_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   1, 1,
   nir_op_imul24,
   { &search0_0.value, &search0_1.value },
   NULL,
};

static const nir_search_variable search1_1 = {
   { nir_search_value_variable, 32 },
   2, /* c */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search1 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 2,
   nir_op_iadd,
   { &search1_0.value, &search1_1.value },
   NULL,
};

   /* replace1_0 -> search0_0 in the cache */

/* replace1_1 -> search0_1 in the cache */

/* replace1_2 -> search1_1 in the cache */
static const nir_search_expression replace1 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_imad24_ir3,
   { &search0_0.value, &search0_1.value, &search1_1.value },
   NULL,
};


static const struct transform ir3_nir_lower_imul_state2_xforms[] = {
  { &search0, &replace0.value, 0 },
};
static const struct transform ir3_nir_lower_imul_state4_xforms[] = {
  { &search1, &replace1.value, 0 },
};

static const struct per_op_table ir3_nir_lower_imul_table[nir_num_search_ops] = {
   [nir_op_imul] = {
      .filter = (uint16_t []) {
         0,
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
   [nir_op_iadd] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         1,
         0,
      },
      
      .num_filtered_states = 2,
      .table = (uint16_t []) {
      
         0,
         4,
         4,
         4,
      },
   },
   [nir_op_imul24] = {
      .filter = (uint16_t []) {
         0,
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

const struct transform *ir3_nir_lower_imul_transforms[] = {
   NULL,
   NULL,
   ir3_nir_lower_imul_state2_xforms,
   NULL,
   ir3_nir_lower_imul_state4_xforms,
};

const uint16_t ir3_nir_lower_imul_transform_counts[] = {
   0,
   0,
   (uint16_t)ARRAY_SIZE(ir3_nir_lower_imul_state2_xforms),
   0,
   (uint16_t)ARRAY_SIZE(ir3_nir_lower_imul_state4_xforms),
};

bool
ir3_nir_lower_imul(nir_shader *shader)
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
                                        ir3_nir_lower_imul_transforms,
                                        ir3_nir_lower_imul_transform_counts,
                                        ir3_nir_lower_imul_table);
      }
   }

   return progress;
}

