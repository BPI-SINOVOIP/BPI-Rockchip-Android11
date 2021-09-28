/* -*- mesa-c++  -*-
 *
 * Copyright (c) 2019 Collabora LTD
 *
 * Author: Gert Wollny <gert.wollny@collabora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef sfn_fragment_shader_from_nir_h
#define sfn_fragment_shader_from_nir_h

#include "sfn_shader_base.h"
#include "sfn_shaderio.h"
#include <bitset>

namespace r600 {

class FragmentShaderFromNir : public ShaderFromNirProcessor {
public:
   FragmentShaderFromNir(const nir_shader& nir, r600_shader& sh_info,
                         r600_pipe_shader_selector &sel, const r600_shader_key &key,
                         enum chip_class chip_class);
   bool scan_sysvalue_access(nir_instr *instr) override;
private:

   struct Interpolator {
      bool enabled;
      unsigned ij_index;
      PValue i;
      PValue j;
   };

   void emit_shader_start() override;
   bool do_process_inputs(nir_variable *input) override;
   bool do_allocate_reserved_registers() override;
   bool do_process_outputs(nir_variable *output) override;
   bool do_emit_load_deref(const nir_variable *in_var, nir_intrinsic_instr* instr) override;
   bool do_emit_store_deref(const nir_variable *out_var, nir_intrinsic_instr* instr) override;
   bool emit_export_pixel(const nir_variable *, nir_intrinsic_instr* instr, int outputs);
   bool load_interpolated(GPRVector &dest, ShaderInput &io, const Interpolator& ip,
                          int num_components, int start_comp);
   bool load_interpolated_one_comp(GPRVector &dest, ShaderInput& io, const Interpolator& ip, EAluOp op);
   bool load_interpolated_two_comp(GPRVector &dest, ShaderInput& io, const Interpolator& ip,EAluOp op, int writemask);
   bool load_interpolated_two_comp_for_one(GPRVector &dest,
                                           ShaderInput& io, const Interpolator& ip, EAluOp op, int start, int comp);
   bool emit_interp_deref_at_centroid(nir_intrinsic_instr* instr);

   bool emit_intrinsic_instruction_override(nir_intrinsic_instr* instr) override;
   void do_finalize() override;

   void load_front_face();

   bool emit_load_front_face(nir_intrinsic_instr* instr);
   bool emit_load_sample_mask_in(nir_intrinsic_instr* instr);
   bool emit_load_sample_pos(nir_intrinsic_instr* instr);
   bool emit_load_sample_id(nir_intrinsic_instr* instr);
   bool emit_interp_deref_at_sample(nir_intrinsic_instr* instr);
   bool emit_interp_deref_at_offset(nir_intrinsic_instr* instr);

   unsigned m_max_color_exports;
   unsigned m_max_counted_color_exports;
   bool m_two_sided_color;
   ExportInstruction *m_last_pixel_export;
   const nir_shader& m_nir;


   std::array<Interpolator, 6> m_interpolator;
   unsigned m_reserved_registers;
   unsigned m_frag_pos_index;
   PGPRValue m_front_face_reg;
   PGPRValue m_sample_mask_reg;
   PGPRValue m_sample_id_reg;
   PGPRValue m_helper_invocation;
   GPRVector m_frag_pos;
   bool m_need_back_color;
   bool m_front_face_loaded;
   ShaderIO m_shaderio;
   unsigned m_depth_exports;

   std::map<unsigned, PValue> m_input_cache;
   bool m_enable_centroid_interpolators;
   bool m_enable_sample_interpolators;
   unsigned m_apply_sample_mask;
   bool m_dual_source_blend;
};
	
}

#endif
