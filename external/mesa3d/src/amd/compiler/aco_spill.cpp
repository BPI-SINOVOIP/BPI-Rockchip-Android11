/*
 * Copyright © 2018 Valve Corporation
 * Copyright © 2018 Google
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "aco_ir.h"
#include "aco_builder.h"
#include "sid.h"

#include <map>
#include <set>
#include <stack>

/*
 * Implements the spilling algorithm on SSA-form from
 * "Register Spilling and Live-Range Splitting for SSA-Form Programs"
 * by Matthias Braun and Sebastian Hack.
 */

namespace aco {

namespace {

struct remat_info {
   Instruction *instr;
};

struct spill_ctx {
   RegisterDemand target_pressure;
   Program* program;
   std::vector<std::vector<RegisterDemand>> register_demand;
   std::vector<std::map<Temp, Temp>> renames;
   std::vector<std::map<Temp, uint32_t>> spills_entry;
   std::vector<std::map<Temp, uint32_t>> spills_exit;
   std::vector<bool> processed;
   std::stack<Block*> loop_header;
   std::vector<std::map<Temp, std::pair<uint32_t, uint32_t>>> next_use_distances_start;
   std::vector<std::map<Temp, std::pair<uint32_t, uint32_t>>> next_use_distances_end;
   std::vector<std::pair<RegClass, std::unordered_set<uint32_t>>> interferences;
   std::vector<std::vector<uint32_t>> affinities;
   std::vector<bool> is_reloaded;
   std::map<Temp, remat_info> remat;
   std::map<Instruction *, bool> remat_used;
   unsigned wave_size;

   spill_ctx(const RegisterDemand target_pressure, Program* program,
             std::vector<std::vector<RegisterDemand>> register_demand)
      : target_pressure(target_pressure), program(program),
        register_demand(std::move(register_demand)), renames(program->blocks.size()),
        spills_entry(program->blocks.size()), spills_exit(program->blocks.size()),
        processed(program->blocks.size(), false), wave_size(program->wave_size) {}

   void add_affinity(uint32_t first, uint32_t second)
   {
      unsigned found_first = affinities.size();
      unsigned found_second = affinities.size();
      for (unsigned i = 0; i < affinities.size(); i++) {
         std::vector<uint32_t>& vec = affinities[i];
         for (uint32_t entry : vec) {
            if (entry == first)
               found_first = i;
            else if (entry == second)
               found_second = i;
         }
      }
      if (found_first == affinities.size() && found_second == affinities.size()) {
         affinities.emplace_back(std::vector<uint32_t>({first, second}));
      } else if (found_first < affinities.size() && found_second == affinities.size()) {
         affinities[found_first].push_back(second);
      } else if (found_second < affinities.size() && found_first == affinities.size()) {
         affinities[found_second].push_back(first);
      } else if (found_first != found_second) {
         /* merge second into first */
         affinities[found_first].insert(affinities[found_first].end(), affinities[found_second].begin(), affinities[found_second].end());
         affinities.erase(std::next(affinities.begin(), found_second));
      } else {
         assert(found_first == found_second);
      }
   }

   void add_interference(uint32_t first, uint32_t second)
   {
      if (interferences[first].first.type() != interferences[second].first.type())
         return;

      bool inserted = interferences[first].second.insert(second).second;
      if (inserted)
         interferences[second].second.insert(first);
   }

   uint32_t allocate_spill_id(RegClass rc)
   {
      interferences.emplace_back(rc, std::unordered_set<uint32_t>());
      is_reloaded.push_back(false);
      return next_spill_id++;
   }

   uint32_t next_spill_id = 0;
};

int32_t get_dominator(int idx_a, int idx_b, Program* program, bool is_linear)
{

   if (idx_a == -1)
      return idx_b;
   if (idx_b == -1)
      return idx_a;
   if (is_linear) {
      while (idx_a != idx_b) {
         if (idx_a > idx_b)
            idx_a = program->blocks[idx_a].linear_idom;
         else
            idx_b = program->blocks[idx_b].linear_idom;
      }
   } else {
      while (idx_a != idx_b) {
         if (idx_a > idx_b)
            idx_a = program->blocks[idx_a].logical_idom;
         else
            idx_b = program->blocks[idx_b].logical_idom;
      }
   }
   assert(idx_a != -1);
   return idx_a;
}

void next_uses_per_block(spill_ctx& ctx, unsigned block_idx, std::set<uint32_t>& worklist)
{
   Block* block = &ctx.program->blocks[block_idx];
   std::map<Temp, std::pair<uint32_t, uint32_t>> next_uses = ctx.next_use_distances_end[block_idx];

   /* to compute the next use distance at the beginning of the block, we have to add the block's size */
   for (std::map<Temp, std::pair<uint32_t, uint32_t>>::iterator it = next_uses.begin(); it != next_uses.end(); ++it)
      it->second.second = it->second.second + block->instructions.size();

   int idx = block->instructions.size() - 1;
   while (idx >= 0) {
      aco_ptr<Instruction>& instr = block->instructions[idx];

      if (instr->opcode == aco_opcode::p_linear_phi ||
          instr->opcode == aco_opcode::p_phi)
         break;

      for (const Definition& def : instr->definitions) {
         if (def.isTemp())
            next_uses.erase(def.getTemp());
      }

      for (const Operand& op : instr->operands) {
         /* omit exec mask */
         if (op.isFixed() && op.physReg() == exec)
            continue;
         if (op.regClass().type() == RegType::vgpr && op.regClass().is_linear())
            continue;
         if (op.isTemp())
            next_uses[op.getTemp()] = {block_idx, idx};
      }
      idx--;
   }

   assert(block_idx != 0 || next_uses.empty());
   ctx.next_use_distances_start[block_idx] = next_uses;
   while (idx >= 0) {
      aco_ptr<Instruction>& instr = block->instructions[idx];
      assert(instr->opcode == aco_opcode::p_linear_phi || instr->opcode == aco_opcode::p_phi);

      for (unsigned i = 0; i < instr->operands.size(); i++) {
         unsigned pred_idx = instr->opcode == aco_opcode::p_phi ?
                             block->logical_preds[i] :
                             block->linear_preds[i];
         if (instr->operands[i].isTemp()) {
            if (instr->operands[i].getTemp() == ctx.program->blocks[pred_idx].live_out_exec)
               continue;
            if (ctx.next_use_distances_end[pred_idx].find(instr->operands[i].getTemp()) == ctx.next_use_distances_end[pred_idx].end() ||
                ctx.next_use_distances_end[pred_idx][instr->operands[i].getTemp()] != std::pair<uint32_t, uint32_t>{block_idx, 0})
               worklist.insert(pred_idx);
            ctx.next_use_distances_end[pred_idx][instr->operands[i].getTemp()] = {block_idx, 0};
         }
      }
      next_uses.erase(instr->definitions[0].getTemp());
      idx--;
   }

   /* all remaining live vars must be live-out at the predecessors */
   for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : next_uses) {
      Temp temp = pair.first;
      uint32_t distance = pair.second.second;
      uint32_t dom = pair.second.first;
      std::vector<unsigned>& preds = temp.is_linear() ? block->linear_preds : block->logical_preds;
      for (unsigned pred_idx : preds) {
         if (temp == ctx.program->blocks[pred_idx].live_out_exec)
            continue;
         if (ctx.program->blocks[pred_idx].loop_nest_depth > block->loop_nest_depth)
            distance += 0xFFFF;
         if (ctx.next_use_distances_end[pred_idx].find(temp) != ctx.next_use_distances_end[pred_idx].end()) {
            dom = get_dominator(dom, ctx.next_use_distances_end[pred_idx][temp].first, ctx.program, temp.is_linear());
            distance = std::min(ctx.next_use_distances_end[pred_idx][temp].second, distance);
         }
         if (ctx.next_use_distances_end[pred_idx][temp] != std::pair<uint32_t, uint32_t>{dom, distance})
            worklist.insert(pred_idx);
         ctx.next_use_distances_end[pred_idx][temp] = {dom, distance};
      }
   }

}

void compute_global_next_uses(spill_ctx& ctx)
{
   ctx.next_use_distances_start.resize(ctx.program->blocks.size());
   ctx.next_use_distances_end.resize(ctx.program->blocks.size());
   std::set<uint32_t> worklist;
   for (Block& block : ctx.program->blocks)
      worklist.insert(block.index);

   while (!worklist.empty()) {
      std::set<unsigned>::reverse_iterator b_it = worklist.rbegin();
      unsigned block_idx = *b_it;
      worklist.erase(block_idx);
      next_uses_per_block(ctx, block_idx, worklist);
   }
}

bool should_rematerialize(aco_ptr<Instruction>& instr)
{
   /* TODO: rematerialization is only supported for VOP1, SOP1 and PSEUDO */
   if (instr->format != Format::VOP1 && instr->format != Format::SOP1 && instr->format != Format::PSEUDO && instr->format != Format::SOPK)
      return false;
   /* TODO: pseudo-instruction rematerialization is only supported for p_create_vector/p_parallelcopy */
   if (instr->format == Format::PSEUDO && instr->opcode != aco_opcode::p_create_vector &&
       instr->opcode != aco_opcode::p_parallelcopy)
      return false;
   if (instr->format == Format::SOPK && instr->opcode != aco_opcode::s_movk_i32)
      return false;

   for (const Operand& op : instr->operands) {
      /* TODO: rematerialization using temporaries isn't yet supported */
      if (op.isTemp())
         return false;
   }

   /* TODO: rematerialization with multiple definitions isn't yet supported */
   if (instr->definitions.size() > 1)
      return false;

   return true;
}

aco_ptr<Instruction> do_reload(spill_ctx& ctx, Temp tmp, Temp new_name, uint32_t spill_id)
{
   std::map<Temp, remat_info>::iterator remat = ctx.remat.find(tmp);
   if (remat != ctx.remat.end()) {
      Instruction *instr = remat->second.instr;
      assert((instr->format == Format::VOP1 || instr->format == Format::SOP1 || instr->format == Format::PSEUDO || instr->format == Format::SOPK) && "unsupported");
      assert((instr->format != Format::PSEUDO || instr->opcode == aco_opcode::p_create_vector || instr->opcode == aco_opcode::p_parallelcopy) && "unsupported");
      assert(instr->definitions.size() == 1 && "unsupported");

      aco_ptr<Instruction> res;
      if (instr->format == Format::VOP1) {
         res.reset(create_instruction<VOP1_instruction>(instr->opcode, instr->format, instr->operands.size(), instr->definitions.size()));
      } else if (instr->format == Format::SOP1) {
         res.reset(create_instruction<SOP1_instruction>(instr->opcode, instr->format, instr->operands.size(), instr->definitions.size()));
      } else if (instr->format == Format::PSEUDO) {
         res.reset(create_instruction<Pseudo_instruction>(instr->opcode, instr->format, instr->operands.size(), instr->definitions.size()));
      } else if (instr->format == Format::SOPK) {
         res.reset(create_instruction<SOPK_instruction>(instr->opcode, instr->format, instr->operands.size(), instr->definitions.size()));
         static_cast<SOPK_instruction*>(res.get())->imm = static_cast<SOPK_instruction*>(instr)->imm;
      }
      for (unsigned i = 0; i < instr->operands.size(); i++) {
         res->operands[i] = instr->operands[i];
         if (instr->operands[i].isTemp()) {
            assert(false && "unsupported");
            if (ctx.remat.count(instr->operands[i].getTemp()))
               ctx.remat_used[ctx.remat[instr->operands[i].getTemp()].instr] = true;
         }
      }
      res->definitions[0] = Definition(new_name);
      return res;
   } else {
      aco_ptr<Pseudo_instruction> reload{create_instruction<Pseudo_instruction>(aco_opcode::p_reload, Format::PSEUDO, 1, 1)};
      reload->operands[0] = Operand(spill_id);
      reload->definitions[0] = Definition(new_name);
      ctx.is_reloaded[spill_id] = true;
      return reload;
   }
}

void get_rematerialize_info(spill_ctx& ctx)
{
   for (Block& block : ctx.program->blocks) {
      bool logical = false;
      for (aco_ptr<Instruction>& instr : block.instructions) {
         if (instr->opcode == aco_opcode::p_logical_start)
            logical = true;
         else if (instr->opcode == aco_opcode::p_logical_end)
            logical = false;
         if (logical && should_rematerialize(instr)) {
            for (const Definition& def : instr->definitions) {
               if (def.isTemp()) {
                  ctx.remat[def.getTemp()] = (remat_info){instr.get()};
                  ctx.remat_used[instr.get()] = false;
               }
            }
         }
      }
   }
}

std::vector<std::map<Temp, uint32_t>> local_next_uses(spill_ctx& ctx, Block* block)
{
   std::vector<std::map<Temp, uint32_t>> local_next_uses(block->instructions.size());

   std::map<Temp, uint32_t> next_uses;
   for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : ctx.next_use_distances_end[block->index])
      next_uses[pair.first] = pair.second.second + block->instructions.size();

   for (int idx = block->instructions.size() - 1; idx >= 0; idx--) {
      aco_ptr<Instruction>& instr = block->instructions[idx];
      if (!instr)
         break;
      if (instr->opcode == aco_opcode::p_phi || instr->opcode == aco_opcode::p_linear_phi)
         break;

      for (const Operand& op : instr->operands) {
         if (op.isFixed() && op.physReg() == exec)
            continue;
         if (op.regClass().type() == RegType::vgpr && op.regClass().is_linear())
            continue;
         if (op.isTemp())
            next_uses[op.getTemp()] = idx;
      }
      for (const Definition& def : instr->definitions) {
         if (def.isTemp())
            next_uses.erase(def.getTemp());
      }
      local_next_uses[idx] = next_uses;
   }
   return local_next_uses;
}


RegisterDemand init_live_in_vars(spill_ctx& ctx, Block* block, unsigned block_idx)
{
   RegisterDemand spilled_registers;

   /* first block, nothing was spilled before */
   if (block_idx == 0)
      return {0, 0};

   /* loop header block */
   if (block->loop_nest_depth > ctx.program->blocks[block_idx - 1].loop_nest_depth) {
      assert(block->linear_preds[0] == block_idx - 1);
      assert(block->logical_preds[0] == block_idx - 1);

      /* create new loop_info */
      ctx.loop_header.emplace(block);

      /* check how many live-through variables should be spilled */
      RegisterDemand new_demand;
      unsigned i = block_idx;
      while (ctx.program->blocks[i].loop_nest_depth >= block->loop_nest_depth) {
         assert(ctx.program->blocks.size() > i);
         new_demand.update(ctx.program->blocks[i].register_demand);
         i++;
      }
      unsigned loop_end = i;

      /* keep live-through spilled */
      for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : ctx.next_use_distances_end[block_idx - 1]) {
         if (pair.second.first < loop_end)
            continue;

         Temp to_spill = pair.first;
         auto it = ctx.spills_exit[block_idx - 1].find(to_spill);
         if (it == ctx.spills_exit[block_idx - 1].end())
            continue;

         ctx.spills_entry[block_idx][to_spill] = it->second;
         spilled_registers += to_spill;
      }

      /* select live-through vgpr variables */
      while (new_demand.vgpr - spilled_registers.vgpr > ctx.target_pressure.vgpr) {
         unsigned distance = 0;
         Temp to_spill;
         for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : ctx.next_use_distances_end[block_idx - 1]) {
            if (pair.first.type() == RegType::vgpr &&
                pair.second.first >= loop_end &&
                pair.second.second > distance &&
                ctx.spills_entry[block_idx].find(pair.first) == ctx.spills_entry[block_idx].end()) {
               to_spill = pair.first;
               distance = pair.second.second;
            }
         }
         if (distance == 0)
            break;

         uint32_t spill_id;
         if (ctx.spills_exit[block_idx - 1].find(to_spill) == ctx.spills_exit[block_idx - 1].end()) {
            spill_id = ctx.allocate_spill_id(to_spill.regClass());
         } else {
            spill_id = ctx.spills_exit[block_idx - 1][to_spill];
         }

         ctx.spills_entry[block_idx][to_spill] = spill_id;
         spilled_registers.vgpr += to_spill.size();
      }

      /* select live-through sgpr variables */
      while (new_demand.sgpr - spilled_registers.sgpr > ctx.target_pressure.sgpr) {
         unsigned distance = 0;
         Temp to_spill;
         for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : ctx.next_use_distances_end[block_idx - 1]) {
            if (pair.first.type() == RegType::sgpr &&
                pair.second.first >= loop_end &&
                pair.second.second > distance &&
                ctx.spills_entry[block_idx].find(pair.first) == ctx.spills_entry[block_idx].end()) {
               to_spill = pair.first;
               distance = pair.second.second;
            }
         }
         if (distance == 0)
            break;

         uint32_t spill_id;
         if (ctx.spills_exit[block_idx - 1].find(to_spill) == ctx.spills_exit[block_idx - 1].end()) {
            spill_id = ctx.allocate_spill_id(to_spill.regClass());
         } else {
            spill_id = ctx.spills_exit[block_idx - 1][to_spill];
         }

         ctx.spills_entry[block_idx][to_spill] = spill_id;
         spilled_registers.sgpr += to_spill.size();
      }



      /* shortcut */
      if (!RegisterDemand(new_demand - spilled_registers).exceeds(ctx.target_pressure))
         return spilled_registers;

      /* if reg pressure is too high at beginning of loop, add variables with furthest use */
      unsigned idx = 0;
      while (block->instructions[idx]->opcode == aco_opcode::p_phi || block->instructions[idx]->opcode == aco_opcode::p_linear_phi)
         idx++;

      assert(idx != 0 && "loop without phis: TODO");
      idx--;
      RegisterDemand reg_pressure = ctx.register_demand[block_idx][idx] - spilled_registers;
      /* Consider register pressure from linear predecessors. This can affect
       * reg_pressure if the branch instructions define sgprs. */
      for (unsigned pred : block->linear_preds) {
         reg_pressure.sgpr = std::max<int16_t>(
            reg_pressure.sgpr, ctx.register_demand[pred].back().sgpr - spilled_registers.sgpr);
      }

      while (reg_pressure.sgpr > ctx.target_pressure.sgpr) {
         unsigned distance = 0;
         Temp to_spill;
         for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : ctx.next_use_distances_start[block_idx]) {
            if (pair.first.type() == RegType::sgpr &&
                pair.second.second > distance &&
                ctx.spills_entry[block_idx].find(pair.first) == ctx.spills_entry[block_idx].end()) {
               to_spill = pair.first;
               distance = pair.second.second;
            }
         }
         assert(distance != 0);

         ctx.spills_entry[block_idx][to_spill] = ctx.allocate_spill_id(to_spill.regClass());
         spilled_registers.sgpr += to_spill.size();
         reg_pressure.sgpr -= to_spill.size();
      }
      while (reg_pressure.vgpr > ctx.target_pressure.vgpr) {
         unsigned distance = 0;
         Temp to_spill;
         for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : ctx.next_use_distances_start[block_idx]) {
            if (pair.first.type() == RegType::vgpr &&
                pair.second.second > distance &&
                ctx.spills_entry[block_idx].find(pair.first) == ctx.spills_entry[block_idx].end()) {
               to_spill = pair.first;
               distance = pair.second.second;
            }
         }
         assert(distance != 0);
         ctx.spills_entry[block_idx][to_spill] = ctx.allocate_spill_id(to_spill.regClass());
         spilled_registers.vgpr += to_spill.size();
         reg_pressure.vgpr -= to_spill.size();
      }

      return spilled_registers;
   }

   /* branch block */
   if (block->linear_preds.size() == 1 && !(block->kind & block_kind_loop_exit)) {
      /* keep variables spilled if they are alive and not used in the current block */
      unsigned pred_idx = block->linear_preds[0];
      for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[pred_idx]) {
         if (pair.first.type() == RegType::sgpr &&
             ctx.next_use_distances_start[block_idx].find(pair.first) != ctx.next_use_distances_start[block_idx].end() &&
             ctx.next_use_distances_start[block_idx][pair.first].first != block_idx) {
            ctx.spills_entry[block_idx].insert(pair);
            spilled_registers.sgpr += pair.first.size();
         }
      }
      if (block->logical_preds.size() == 1) {
         pred_idx = block->logical_preds[0];
         for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[pred_idx]) {
            if (pair.first.type() == RegType::vgpr &&
                ctx.next_use_distances_start[block_idx].find(pair.first) != ctx.next_use_distances_start[block_idx].end() &&
                ctx.next_use_distances_start[block_idx][pair.first].first != block_idx) {
               ctx.spills_entry[block_idx].insert(pair);
               spilled_registers.vgpr += pair.first.size();
            }
         }
      }

      /* if register demand is still too high, we just keep all spilled live vars and process the block */
      if (block->register_demand.sgpr - spilled_registers.sgpr > ctx.target_pressure.sgpr) {
         pred_idx = block->linear_preds[0];
         for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[pred_idx]) {
            if (pair.first.type() == RegType::sgpr &&
                ctx.next_use_distances_start[block_idx].find(pair.first) != ctx.next_use_distances_start[block_idx].end() &&
                ctx.spills_entry[block_idx].insert(pair).second) {
               spilled_registers.sgpr += pair.first.size();
            }
         }
      }
      if (block->register_demand.vgpr - spilled_registers.vgpr > ctx.target_pressure.vgpr && block->logical_preds.size() == 1) {
         pred_idx = block->logical_preds[0];
         for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[pred_idx]) {
            if (pair.first.type() == RegType::vgpr &&
                ctx.next_use_distances_start[block_idx].find(pair.first) != ctx.next_use_distances_start[block_idx].end() &&
                ctx.spills_entry[block_idx].insert(pair).second) {
               spilled_registers.vgpr += pair.first.size();
            }
         }
      }

      return spilled_registers;
   }

   /* else: merge block */
   std::set<Temp> partial_spills;

   /* keep variables spilled on all incoming paths */
   for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : ctx.next_use_distances_start[block_idx]) {
      std::vector<unsigned>& preds = pair.first.is_linear() ? block->linear_preds : block->logical_preds;
      /* If it can be rematerialized, keep the variable spilled if all predecessors do not reload it.
       * Otherwise, if any predecessor reloads it, ensure it's reloaded on all other predecessors.
       * The idea is that it's better in practice to rematerialize redundantly than to create lots of phis. */
      /* TODO: test this idea with more than Dawn of War III shaders (the current pipeline-db doesn't seem to exercise this path much) */
      bool remat = ctx.remat.count(pair.first);
      bool spill = !remat;
      uint32_t spill_id = 0;
      for (unsigned pred_idx : preds) {
         /* variable is not even live at the predecessor: probably from a phi */
         if (ctx.next_use_distances_end[pred_idx].find(pair.first) == ctx.next_use_distances_end[pred_idx].end()) {
            spill = false;
            break;
         }
         if (ctx.spills_exit[pred_idx].find(pair.first) == ctx.spills_exit[pred_idx].end()) {
            if (!remat)
               spill = false;
         } else {
            partial_spills.insert(pair.first);
            /* it might be that on one incoming path, the variable has a different spill_id, but add_couple_code() will take care of that. */
            spill_id = ctx.spills_exit[pred_idx][pair.first];
            if (remat)
               spill = true;
         }
      }
      if (spill) {
         ctx.spills_entry[block_idx][pair.first] = spill_id;
         partial_spills.erase(pair.first);
         spilled_registers += pair.first;
      }
   }

   /* same for phis */
   unsigned idx = 0;
   while (block->instructions[idx]->opcode == aco_opcode::p_linear_phi ||
          block->instructions[idx]->opcode == aco_opcode::p_phi) {
      aco_ptr<Instruction>& phi = block->instructions[idx];
      std::vector<unsigned>& preds = phi->opcode == aco_opcode::p_phi ? block->logical_preds : block->linear_preds;
      bool spill = true;

      for (unsigned i = 0; i < phi->operands.size(); i++) {
         if (phi->operands[i].isUndefined())
            continue;
         assert(phi->operands[i].isTemp());
         if (ctx.spills_exit[preds[i]].find(phi->operands[i].getTemp()) == ctx.spills_exit[preds[i]].end())
            spill = false;
         else
            partial_spills.insert(phi->definitions[0].getTemp());
      }
      if (spill) {
         ctx.spills_entry[block_idx][phi->definitions[0].getTemp()] = ctx.allocate_spill_id(phi->definitions[0].regClass());
         partial_spills.erase(phi->definitions[0].getTemp());
         spilled_registers += phi->definitions[0].getTemp();
      }

      idx++;
   }

   /* if reg pressure at first instruction is still too high, add partially spilled variables */
   RegisterDemand reg_pressure;
   if (idx == 0) {
      for (const Definition& def : block->instructions[idx]->definitions) {
         if (def.isTemp()) {
            reg_pressure -= def.getTemp();
         }
      }
      for (const Operand& op : block->instructions[idx]->operands) {
         if (op.isTemp() && op.isFirstKill()) {
            reg_pressure += op.getTemp();
         }
      }
   } else {
      for (unsigned i = 0; i < idx; i++) {
         aco_ptr<Instruction>& instr = block->instructions[i];
         assert(is_phi(instr));
         /* Killed phi definitions increase pressure in the predecessor but not
          * the block they're in. Since the loops below are both to control
          * pressure of the start of this block and the ends of it's
          * predecessors, we need to count killed unspilled phi definitions here. */
         if (instr->definitions[0].isKill() &&
             !ctx.spills_entry[block_idx].count(instr->definitions[0].getTemp()))
            reg_pressure += instr->definitions[0].getTemp();
      }
      idx--;
   }
   reg_pressure += ctx.register_demand[block_idx][idx] - spilled_registers;

   /* Consider register pressure from linear predecessors. This can affect
    * reg_pressure if the branch instructions define sgprs. */
   for (unsigned pred : block->linear_preds) {
      reg_pressure.sgpr = std::max<int16_t>(
         reg_pressure.sgpr, ctx.register_demand[pred].back().sgpr - spilled_registers.sgpr);
   }

   while (reg_pressure.sgpr > ctx.target_pressure.sgpr) {
      assert(!partial_spills.empty());

      std::set<Temp>::iterator it = partial_spills.begin();
      Temp to_spill = Temp();
      unsigned distance = 0;
      while (it != partial_spills.end()) {
         assert(ctx.spills_entry[block_idx].find(*it) == ctx.spills_entry[block_idx].end());

         if (it->type() == RegType::sgpr && ctx.next_use_distances_start[block_idx][*it].second > distance) {
            distance = ctx.next_use_distances_start[block_idx][*it].second;
            to_spill = *it;
         }
         ++it;
      }
      assert(distance != 0);

      ctx.spills_entry[block_idx][to_spill] = ctx.allocate_spill_id(to_spill.regClass());
      partial_spills.erase(to_spill);
      spilled_registers.sgpr += to_spill.size();
      reg_pressure.sgpr -= to_spill.size();
   }

   while (reg_pressure.vgpr > ctx.target_pressure.vgpr) {
      assert(!partial_spills.empty());

      std::set<Temp>::iterator it = partial_spills.begin();
      Temp to_spill = Temp();
      unsigned distance = 0;
      while (it != partial_spills.end()) {
         assert(ctx.spills_entry[block_idx].find(*it) == ctx.spills_entry[block_idx].end());

         if (it->type() == RegType::vgpr && ctx.next_use_distances_start[block_idx][*it].second > distance) {
            distance = ctx.next_use_distances_start[block_idx][*it].second;
            to_spill = *it;
         }
         ++it;
      }
      assert(distance != 0);

      ctx.spills_entry[block_idx][to_spill] = ctx.allocate_spill_id(to_spill.regClass());
      partial_spills.erase(to_spill);
      spilled_registers.vgpr += to_spill.size();
      reg_pressure.vgpr -= to_spill.size();
   }

   return spilled_registers;
}


RegisterDemand get_demand_before(spill_ctx& ctx, unsigned block_idx, unsigned idx)
{
   if (idx == 0) {
      RegisterDemand demand = ctx.register_demand[block_idx][idx];
      aco_ptr<Instruction>& instr = ctx.program->blocks[block_idx].instructions[idx];
      aco_ptr<Instruction> instr_before(nullptr);
      return get_demand_before(demand, instr, instr_before);
   } else {
      return ctx.register_demand[block_idx][idx - 1];
   }
}

void add_coupling_code(spill_ctx& ctx, Block* block, unsigned block_idx)
{
   /* no coupling code necessary */
   if (block->linear_preds.size() == 0)
      return;

   std::vector<aco_ptr<Instruction>> instructions;
   /* branch block: TODO take other branch into consideration */
   if (block->linear_preds.size() == 1 && !(block->kind & (block_kind_loop_exit | block_kind_loop_header))) {
      assert(ctx.processed[block->linear_preds[0]]);
      assert(ctx.register_demand[block_idx].size() == block->instructions.size());
      std::vector<RegisterDemand> reg_demand;
      unsigned insert_idx = 0;
      unsigned pred_idx = block->linear_preds[0];
      RegisterDemand demand_before = get_demand_before(ctx, block_idx, 0);

      for (std::pair<Temp, std::pair<uint32_t, uint32_t>> live : ctx.next_use_distances_start[block_idx]) {
         if (!live.first.is_linear())
            continue;
         /* still spilled */
         if (ctx.spills_entry[block_idx].find(live.first) != ctx.spills_entry[block_idx].end())
            continue;

         /* in register at end of predecessor */
         if (ctx.spills_exit[pred_idx].find(live.first) == ctx.spills_exit[pred_idx].end()) {
            std::map<Temp, Temp>::iterator it = ctx.renames[pred_idx].find(live.first);
            if (it != ctx.renames[pred_idx].end())
               ctx.renames[block_idx].insert(*it);
            continue;
         }

         /* variable is spilled at predecessor and live at current block: create reload instruction */
         Temp new_name = ctx.program->allocateTmp(live.first.regClass());
         aco_ptr<Instruction> reload = do_reload(ctx, live.first, new_name, ctx.spills_exit[pred_idx][live.first]);
         instructions.emplace_back(std::move(reload));
         reg_demand.push_back(demand_before);
         ctx.renames[block_idx][live.first] = new_name;
      }

      if (block->logical_preds.size() == 1) {
         do {
            assert(insert_idx < block->instructions.size());
            instructions.emplace_back(std::move(block->instructions[insert_idx]));
            reg_demand.push_back(ctx.register_demand[block_idx][insert_idx]);
            insert_idx++;
         } while (instructions.back()->opcode != aco_opcode::p_logical_start);

         unsigned pred_idx = block->logical_preds[0];
         for (std::pair<Temp, std::pair<uint32_t, uint32_t>> live : ctx.next_use_distances_start[block_idx]) {
            if (live.first.is_linear())
               continue;
            /* still spilled */
            if (ctx.spills_entry[block_idx].find(live.first) != ctx.spills_entry[block_idx].end())
               continue;

            /* in register at end of predecessor */
            if (ctx.spills_exit[pred_idx].find(live.first) == ctx.spills_exit[pred_idx].end()) {
               std::map<Temp, Temp>::iterator it = ctx.renames[pred_idx].find(live.first);
               if (it != ctx.renames[pred_idx].end())
                  ctx.renames[block_idx].insert(*it);
               continue;
            }

            /* variable is spilled at predecessor and live at current block: create reload instruction */
            Temp new_name = ctx.program->allocateTmp(live.first.regClass());
            aco_ptr<Instruction> reload = do_reload(ctx, live.first, new_name, ctx.spills_exit[pred_idx][live.first]);
            instructions.emplace_back(std::move(reload));
            reg_demand.emplace_back(reg_demand.back());
            ctx.renames[block_idx][live.first] = new_name;
         }
      }

      /* combine new reload instructions with original block */
      if (!instructions.empty()) {
         reg_demand.insert(reg_demand.end(), std::next(ctx.register_demand[block->index].begin(), insert_idx),
                           ctx.register_demand[block->index].end());
         ctx.register_demand[block_idx] = std::move(reg_demand);
         instructions.insert(instructions.end(),
                             std::move_iterator<std::vector<aco_ptr<Instruction>>::iterator>(std::next(block->instructions.begin(), insert_idx)),
                             std::move_iterator<std::vector<aco_ptr<Instruction>>::iterator>(block->instructions.end()));
         block->instructions = std::move(instructions);
      }
      return;
   }

   /* loop header and merge blocks: check if all (linear) predecessors have been processed */
   for (ASSERTED unsigned pred : block->linear_preds)
      assert(ctx.processed[pred]);

   /* iterate the phi nodes for which operands to spill at the predecessor */
   for (aco_ptr<Instruction>& phi : block->instructions) {
      if (phi->opcode != aco_opcode::p_phi &&
          phi->opcode != aco_opcode::p_linear_phi)
         break;

      /* if the phi is not spilled, add to instructions */
      if (ctx.spills_entry[block_idx].find(phi->definitions[0].getTemp()) == ctx.spills_entry[block_idx].end()) {
         instructions.emplace_back(std::move(phi));
         continue;
      }

      std::vector<unsigned>& preds = phi->opcode == aco_opcode::p_phi ? block->logical_preds : block->linear_preds;
      uint32_t def_spill_id = ctx.spills_entry[block_idx][phi->definitions[0].getTemp()];

      for (unsigned i = 0; i < phi->operands.size(); i++) {
         if (phi->operands[i].isUndefined())
            continue;

         unsigned pred_idx = preds[i];
         assert(phi->operands[i].isTemp() && phi->operands[i].isKill());
         Temp var = phi->operands[i].getTemp();

         std::map<Temp, Temp>::iterator rename_it = ctx.renames[pred_idx].find(var);
         /* prevent the definining instruction from being DCE'd if it could be rematerialized */
         if (rename_it == ctx.renames[preds[i]].end() && ctx.remat.count(var))
            ctx.remat_used[ctx.remat[var].instr] = true;

         /* build interferences between the phi def and all spilled variables at the predecessor blocks */
         for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[pred_idx]) {
            if (var == pair.first)
               continue;
            ctx.add_interference(def_spill_id, pair.second);
         }

         /* check if variable is already spilled at predecessor */
         std::map<Temp, uint32_t>::iterator spilled = ctx.spills_exit[pred_idx].find(var);
         if (spilled != ctx.spills_exit[pred_idx].end()) {
            if (spilled->second != def_spill_id)
               ctx.add_affinity(def_spill_id, spilled->second);
            continue;
         }

         /* rename if necessary */
         if (rename_it != ctx.renames[pred_idx].end()) {
            var = rename_it->second;
            ctx.renames[pred_idx].erase(rename_it);
         }

         uint32_t spill_id = ctx.allocate_spill_id(phi->definitions[0].regClass());
         ctx.add_affinity(def_spill_id, spill_id);
         aco_ptr<Pseudo_instruction> spill{create_instruction<Pseudo_instruction>(aco_opcode::p_spill, Format::PSEUDO, 2, 0)};
         spill->operands[0] = Operand(var);
         spill->operands[1] = Operand(spill_id);
         Block& pred = ctx.program->blocks[pred_idx];
         unsigned idx = pred.instructions.size();
         do {
            assert(idx != 0);
            idx--;
         } while (phi->opcode == aco_opcode::p_phi && pred.instructions[idx]->opcode != aco_opcode::p_logical_end);
         std::vector<aco_ptr<Instruction>>::iterator it = std::next(pred.instructions.begin(), idx);
         pred.instructions.insert(it, std::move(spill));
         ctx.spills_exit[pred_idx][phi->operands[i].getTemp()] = spill_id;
      }

      /* remove phi from instructions */
      phi.reset();
   }

   /* iterate all (other) spilled variables for which to spill at the predecessor */
   // TODO: would be better to have them sorted: first vgprs and first with longest distance
   for (std::pair<Temp, uint32_t> pair : ctx.spills_entry[block_idx]) {
      std::vector<unsigned> preds = pair.first.is_linear() ? block->linear_preds : block->logical_preds;

      for (unsigned pred_idx : preds) {
         /* variable is already spilled at predecessor */
         std::map<Temp, uint32_t>::iterator spilled = ctx.spills_exit[pred_idx].find(pair.first);
         if (spilled != ctx.spills_exit[pred_idx].end()) {
            if (spilled->second != pair.second)
               ctx.add_affinity(pair.second, spilled->second);
            continue;
         }

         /* variable is dead at predecessor, it must be from a phi: this works because of CSSA form */
         if (ctx.next_use_distances_end[pred_idx].find(pair.first) == ctx.next_use_distances_end[pred_idx].end())
            continue;

         /* add interferences between spilled variable and predecessors exit spills */
         for (std::pair<Temp, uint32_t> exit_spill : ctx.spills_exit[pred_idx]) {
            if (exit_spill.first == pair.first)
               continue;
            ctx.add_interference(exit_spill.second, pair.second);
         }

         /* variable is in register at predecessor and has to be spilled */
         /* rename if necessary */
         Temp var = pair.first;
         std::map<Temp, Temp>::iterator rename_it = ctx.renames[pred_idx].find(var);
         if (rename_it != ctx.renames[pred_idx].end()) {
            var = rename_it->second;
            ctx.renames[pred_idx].erase(rename_it);
         }

         aco_ptr<Pseudo_instruction> spill{create_instruction<Pseudo_instruction>(aco_opcode::p_spill, Format::PSEUDO, 2, 0)};
         spill->operands[0] = Operand(var);
         spill->operands[1] = Operand(pair.second);
         Block& pred = ctx.program->blocks[pred_idx];
         unsigned idx = pred.instructions.size();
         do {
            assert(idx != 0);
            idx--;
         } while (pair.first.type() == RegType::vgpr && pred.instructions[idx]->opcode != aco_opcode::p_logical_end);
         std::vector<aco_ptr<Instruction>>::iterator it = std::next(pred.instructions.begin(), idx);
         pred.instructions.insert(it, std::move(spill));
         ctx.spills_exit[pred.index][pair.first] = pair.second;
      }
   }

   /* iterate phis for which operands to reload */
   for (aco_ptr<Instruction>& phi : instructions) {
      assert(phi->opcode == aco_opcode::p_phi || phi->opcode == aco_opcode::p_linear_phi);
      assert(ctx.spills_entry[block_idx].find(phi->definitions[0].getTemp()) == ctx.spills_entry[block_idx].end());

      std::vector<unsigned>& preds = phi->opcode == aco_opcode::p_phi ? block->logical_preds : block->linear_preds;
      for (unsigned i = 0; i < phi->operands.size(); i++) {
         if (!phi->operands[i].isTemp())
            continue;
         unsigned pred_idx = preds[i];

         /* rename operand */
         if (ctx.spills_exit[pred_idx].find(phi->operands[i].getTemp()) == ctx.spills_exit[pred_idx].end()) {
            std::map<Temp, Temp>::iterator it = ctx.renames[pred_idx].find(phi->operands[i].getTemp());
            if (it != ctx.renames[pred_idx].end())
               phi->operands[i].setTemp(it->second);
            /* prevent the definining instruction from being DCE'd if it could be rematerialized */
            else if (ctx.remat.count(phi->operands[i].getTemp()))
               ctx.remat_used[ctx.remat[phi->operands[i].getTemp()].instr] = true;
            continue;
         }

         Temp tmp = phi->operands[i].getTemp();

         /* reload phi operand at end of predecessor block */
         Temp new_name = ctx.program->allocateTmp(tmp.regClass());
         Block& pred = ctx.program->blocks[pred_idx];
         unsigned idx = pred.instructions.size();
         do {
            assert(idx != 0);
            idx--;
         } while (phi->opcode == aco_opcode::p_phi && pred.instructions[idx]->opcode != aco_opcode::p_logical_end);
         std::vector<aco_ptr<Instruction>>::iterator it = std::next(pred.instructions.begin(), idx);

         aco_ptr<Instruction> reload = do_reload(ctx, tmp, new_name, ctx.spills_exit[pred_idx][tmp]);
         pred.instructions.insert(it, std::move(reload));

         ctx.spills_exit[pred_idx].erase(tmp);
         ctx.renames[pred_idx][tmp] = new_name;
         phi->operands[i].setTemp(new_name);
      }
   }

   /* iterate live variables for which to reload */
   // TODO: reload at current block if variable is spilled on all predecessors
   for (std::pair<Temp, std::pair<uint32_t, uint32_t>> pair : ctx.next_use_distances_start[block_idx]) {
      /* skip spilled variables */
      if (ctx.spills_entry[block_idx].find(pair.first) != ctx.spills_entry[block_idx].end())
         continue;
      std::vector<unsigned> preds = pair.first.is_linear() ? block->linear_preds : block->logical_preds;

      /* variable is dead at predecessor, it must be from a phi */
      bool is_dead = false;
      for (unsigned pred_idx : preds) {
         if (ctx.next_use_distances_end[pred_idx].find(pair.first) == ctx.next_use_distances_end[pred_idx].end())
            is_dead = true;
      }
      if (is_dead)
         continue;
      for (unsigned pred_idx : preds) {
         /* the variable is not spilled at the predecessor */
         if (ctx.spills_exit[pred_idx].find(pair.first) == ctx.spills_exit[pred_idx].end())
            continue;

         /* variable is spilled at predecessor and has to be reloaded */
         Temp new_name = ctx.program->allocateTmp(pair.first.regClass());
         Block& pred = ctx.program->blocks[pred_idx];
         unsigned idx = pred.instructions.size();
         do {
            assert(idx != 0);
            idx--;
         } while (pair.first.type() == RegType::vgpr && pred.instructions[idx]->opcode != aco_opcode::p_logical_end);
         std::vector<aco_ptr<Instruction>>::iterator it = std::next(pred.instructions.begin(), idx);

         aco_ptr<Instruction> reload = do_reload(ctx, pair.first, new_name, ctx.spills_exit[pred.index][pair.first]);
         pred.instructions.insert(it, std::move(reload));

         ctx.spills_exit[pred.index].erase(pair.first);
         ctx.renames[pred.index][pair.first] = new_name;
      }

      /* check if we have to create a new phi for this variable */
      Temp rename = Temp();
      bool is_same = true;
      for (unsigned pred_idx : preds) {
         if (ctx.renames[pred_idx].find(pair.first) == ctx.renames[pred_idx].end()) {
            if (rename == Temp())
               rename = pair.first;
            else
               is_same = rename == pair.first;
         } else {
            if (rename == Temp())
               rename = ctx.renames[pred_idx][pair.first];
            else
               is_same = rename == ctx.renames[pred_idx][pair.first];
         }

         if (!is_same)
            break;
      }

      if (!is_same) {
         /* the variable was renamed differently in the predecessors: we have to create a phi */
         aco_opcode opcode = pair.first.is_linear() ? aco_opcode::p_linear_phi : aco_opcode::p_phi;
         aco_ptr<Pseudo_instruction> phi{create_instruction<Pseudo_instruction>(opcode, Format::PSEUDO, preds.size(), 1)};
         rename = ctx.program->allocateTmp(pair.first.regClass());
         for (unsigned i = 0; i < phi->operands.size(); i++) {
            Temp tmp;
            if (ctx.renames[preds[i]].find(pair.first) != ctx.renames[preds[i]].end()) {
               tmp = ctx.renames[preds[i]][pair.first];
            } else if (preds[i] >= block_idx) {
               tmp = rename;
            } else {
               tmp = pair.first;
               /* prevent the definining instruction from being DCE'd if it could be rematerialized */
               if (ctx.remat.count(tmp))
                  ctx.remat_used[ctx.remat[tmp].instr] = true;
            }
            phi->operands[i] = Operand(tmp);
         }
         phi->definitions[0] = Definition(rename);
         instructions.emplace_back(std::move(phi));
      }

      /* the variable was renamed: add new name to renames */
      if (!(rename == Temp() || rename == pair.first))
         ctx.renames[block_idx][pair.first] = rename;
   }

   /* combine phis with instructions */
   unsigned idx = 0;
   while (!block->instructions[idx]) {
      idx++;
   }

   if (!ctx.processed[block_idx]) {
      assert(!(block->kind & block_kind_loop_header));
      RegisterDemand demand_before = get_demand_before(ctx, block_idx, idx);
      ctx.register_demand[block->index].erase(ctx.register_demand[block->index].begin(), ctx.register_demand[block->index].begin() + idx);
      ctx.register_demand[block->index].insert(ctx.register_demand[block->index].begin(), instructions.size(), demand_before);
   }

   std::vector<aco_ptr<Instruction>>::iterator start = std::next(block->instructions.begin(), idx);
   instructions.insert(instructions.end(), std::move_iterator<std::vector<aco_ptr<Instruction>>::iterator>(start),
               std::move_iterator<std::vector<aco_ptr<Instruction>>::iterator>(block->instructions.end()));
   block->instructions = std::move(instructions);
}

void process_block(spill_ctx& ctx, unsigned block_idx, Block* block,
                   std::map<Temp, uint32_t> &current_spills, RegisterDemand spilled_registers)
{
   assert(!ctx.processed[block_idx]);

   std::vector<std::map<Temp, uint32_t>> local_next_use_distance;
   std::vector<aco_ptr<Instruction>> instructions;
   unsigned idx = 0;

   /* phis are handled separetely */
   while (block->instructions[idx]->opcode == aco_opcode::p_phi ||
          block->instructions[idx]->opcode == aco_opcode::p_linear_phi) {
      instructions.emplace_back(std::move(block->instructions[idx++]));
   }

   if (block->register_demand.exceeds(ctx.target_pressure))
      local_next_use_distance = local_next_uses(ctx, block);

   while (idx < block->instructions.size()) {
      aco_ptr<Instruction>& instr = block->instructions[idx];

      std::map<Temp, std::pair<Temp, uint32_t>> reloads;
      std::map<Temp, uint32_t> spills;
      /* rename and reload operands */
      for (Operand& op : instr->operands) {
         if (!op.isTemp())
            continue;
         if (current_spills.find(op.getTemp()) == current_spills.end()) {
            /* the Operand is in register: check if it was renamed */
            if (ctx.renames[block_idx].find(op.getTemp()) != ctx.renames[block_idx].end())
               op.setTemp(ctx.renames[block_idx][op.getTemp()]);
            /* prevent it's definining instruction from being DCE'd if it could be rematerialized */
            else if (ctx.remat.count(op.getTemp()))
               ctx.remat_used[ctx.remat[op.getTemp()].instr] = true;
            continue;
         }
         /* the Operand is spilled: add it to reloads */
         Temp new_tmp = ctx.program->allocateTmp(op.regClass());
         ctx.renames[block_idx][op.getTemp()] = new_tmp;
         reloads[new_tmp] = std::make_pair(op.getTemp(), current_spills[op.getTemp()]);
         current_spills.erase(op.getTemp());
         op.setTemp(new_tmp);
         spilled_registers -= new_tmp;
      }

      /* check if register demand is low enough before and after the current instruction */
      if (block->register_demand.exceeds(ctx.target_pressure)) {

         RegisterDemand new_demand = ctx.register_demand[block_idx][idx];
         new_demand.update(get_demand_before(ctx, block_idx, idx));

         assert(!local_next_use_distance.empty());

         /* if reg pressure is too high, spill variable with furthest next use */
         while (RegisterDemand(new_demand - spilled_registers).exceeds(ctx.target_pressure)) {
            unsigned distance = 0;
            Temp to_spill;
            bool do_rematerialize = false;
            if (new_demand.vgpr - spilled_registers.vgpr > ctx.target_pressure.vgpr) {
               for (std::pair<Temp, uint32_t> pair : local_next_use_distance[idx]) {
                  bool can_rematerialize = ctx.remat.count(pair.first);
                  if (pair.first.type() == RegType::vgpr &&
                      ((pair.second > distance && can_rematerialize == do_rematerialize) ||
                       (can_rematerialize && !do_rematerialize && pair.second > idx)) &&
                      current_spills.find(pair.first) == current_spills.end() &&
                      ctx.spills_exit[block_idx].find(pair.first) == ctx.spills_exit[block_idx].end()) {
                     to_spill = pair.first;
                     distance = pair.second;
                     do_rematerialize = can_rematerialize;
                  }
               }
            } else {
               for (std::pair<Temp, uint32_t> pair : local_next_use_distance[idx]) {
                  bool can_rematerialize = ctx.remat.count(pair.first);
                  if (pair.first.type() == RegType::sgpr &&
                      ((pair.second > distance && can_rematerialize == do_rematerialize) ||
                       (can_rematerialize && !do_rematerialize && pair.second > idx)) &&
                      current_spills.find(pair.first) == current_spills.end() &&
                      ctx.spills_exit[block_idx].find(pair.first) == ctx.spills_exit[block_idx].end()) {
                     to_spill = pair.first;
                     distance = pair.second;
                     do_rematerialize = can_rematerialize;
                  }
               }
            }

            assert(distance != 0 && distance > idx);
            uint32_t spill_id = ctx.allocate_spill_id(to_spill.regClass());

            /* add interferences with currently spilled variables */
            for (std::pair<Temp, uint32_t> pair : current_spills)
               ctx.add_interference(spill_id, pair.second);
            for (std::pair<Temp, std::pair<Temp, uint32_t>> pair : reloads)
               ctx.add_interference(spill_id, pair.second.second);

            current_spills[to_spill] = spill_id;
            spilled_registers += to_spill;

            /* rename if necessary */
            if (ctx.renames[block_idx].find(to_spill) != ctx.renames[block_idx].end()) {
               to_spill = ctx.renames[block_idx][to_spill];
            }

            /* add spill to new instructions */
            aco_ptr<Pseudo_instruction> spill{create_instruction<Pseudo_instruction>(aco_opcode::p_spill, Format::PSEUDO, 2, 0)};
            spill->operands[0] = Operand(to_spill);
            spill->operands[1] = Operand(spill_id);
            instructions.emplace_back(std::move(spill));
         }
      }

      /* add reloads and instruction to new instructions */
      for (std::pair<Temp, std::pair<Temp, uint32_t>> pair : reloads) {
         aco_ptr<Instruction> reload = do_reload(ctx, pair.second.first, pair.first, pair.second.second);
         instructions.emplace_back(std::move(reload));
      }
      instructions.emplace_back(std::move(instr));
      idx++;
   }

   block->instructions = std::move(instructions);
   ctx.spills_exit[block_idx].insert(current_spills.begin(), current_spills.end());
}

void spill_block(spill_ctx& ctx, unsigned block_idx)
{
   Block* block = &ctx.program->blocks[block_idx];

   /* determine set of variables which are spilled at the beginning of the block */
   RegisterDemand spilled_registers = init_live_in_vars(ctx, block, block_idx);

   /* add interferences for spilled variables */
   for (auto it = ctx.spills_entry[block_idx].begin(); it != ctx.spills_entry[block_idx].end(); ++it) {
      for (auto it2 = std::next(it); it2 != ctx.spills_entry[block_idx].end(); ++it2)
         ctx.add_interference(it->second, it2->second);
   }

   bool is_loop_header = block->loop_nest_depth && ctx.loop_header.top()->index == block_idx;
   if (!is_loop_header) {
      /* add spill/reload code on incoming control flow edges */
      add_coupling_code(ctx, block, block_idx);
   }

   std::map<Temp, uint32_t> current_spills = ctx.spills_entry[block_idx];

   /* check conditions to process this block */
   bool process = RegisterDemand(block->register_demand - spilled_registers).exceeds(ctx.target_pressure) ||
                  !ctx.renames[block_idx].empty() ||
                  ctx.remat_used.size();

   std::map<Temp, uint32_t>::iterator it = current_spills.begin();
   while (!process && it != current_spills.end()) {
      if (ctx.next_use_distances_start[block_idx][it->first].first == block_idx)
         process = true;
      ++it;
   }

   if (process)
      process_block(ctx, block_idx, block, current_spills, spilled_registers);
   else
      ctx.spills_exit[block_idx].insert(current_spills.begin(), current_spills.end());

   ctx.processed[block_idx] = true;

   /* check if the next block leaves the current loop */
   if (block->loop_nest_depth == 0 || ctx.program->blocks[block_idx + 1].loop_nest_depth >= block->loop_nest_depth)
      return;

   Block* loop_header = ctx.loop_header.top();

   /* preserve original renames at end of loop header block */
   std::map<Temp, Temp> renames = std::move(ctx.renames[loop_header->index]);

   /* add coupling code to all loop header predecessors */
   add_coupling_code(ctx, loop_header, loop_header->index);

   /* propagate new renames through loop: i.e. repair the SSA */
   renames.swap(ctx.renames[loop_header->index]);
   for (std::pair<Temp, Temp> rename : renames) {
      for (unsigned idx = loop_header->index; idx <= block_idx; idx++) {
         Block& current = ctx.program->blocks[idx];
         std::vector<aco_ptr<Instruction>>::iterator instr_it = current.instructions.begin();

         /* first rename phis */
         while (instr_it != current.instructions.end()) {
            aco_ptr<Instruction>& phi = *instr_it;
            if (phi->opcode != aco_opcode::p_phi && phi->opcode != aco_opcode::p_linear_phi)
               break;
            /* no need to rename the loop header phis once again. this happened in add_coupling_code() */
            if (idx == loop_header->index) {
               instr_it++;
               continue;
            }

            for (Operand& op : phi->operands) {
               if (!op.isTemp())
                  continue;
               if (op.getTemp() == rename.first)
                  op.setTemp(rename.second);
            }
            instr_it++;
         }

         std::map<Temp, std::pair<uint32_t, uint32_t>>::iterator it = ctx.next_use_distances_start[idx].find(rename.first);

         /* variable is not live at beginning of this block */
         if (it == ctx.next_use_distances_start[idx].end())
            continue;

         /* if the variable is live at the block's exit, add rename */
         if (ctx.next_use_distances_end[idx].find(rename.first) != ctx.next_use_distances_end[idx].end())
            ctx.renames[idx].insert(rename);

         /* rename all uses in this block */
         bool renamed = false;
         while (!renamed && instr_it != current.instructions.end()) {
            aco_ptr<Instruction>& instr = *instr_it;
            for (Operand& op : instr->operands) {
               if (!op.isTemp())
                  continue;
               if (op.getTemp() == rename.first) {
                  op.setTemp(rename.second);
                  /* we can stop with this block as soon as the variable is spilled */
                  if (instr->opcode == aco_opcode::p_spill)
                    renamed = true;
               }
            }
            instr_it++;
         }
      }
   }

   /* remove loop header info from stack */
   ctx.loop_header.pop();
}

Temp load_scratch_resource(spill_ctx& ctx, Temp& scratch_offset,
                           std::vector<aco_ptr<Instruction>>& instructions,
                           unsigned offset, bool is_top_level)
{
   Builder bld(ctx.program);
   if (is_top_level) {
      bld.reset(&instructions);
   } else {
      /* find p_logical_end */
      unsigned idx = instructions.size() - 1;
      while (instructions[idx]->opcode != aco_opcode::p_logical_end)
         idx--;
      bld.reset(&instructions, std::next(instructions.begin(), idx));
   }

   Temp private_segment_buffer = ctx.program->private_segment_buffer;
   if (ctx.program->stage != compute_cs)
      private_segment_buffer = bld.smem(aco_opcode::s_load_dwordx2, bld.def(s2), private_segment_buffer, Operand(0u));

   if (offset)
      scratch_offset = bld.sop2(aco_opcode::s_add_u32, bld.def(s1), bld.def(s1, scc), scratch_offset, Operand(offset));

   uint32_t rsrc_conf = S_008F0C_ADD_TID_ENABLE(1) |
                        S_008F0C_INDEX_STRIDE(ctx.program->wave_size == 64 ? 3 : 2);

   if (ctx.program->chip_class >= GFX10) {
      rsrc_conf |= S_008F0C_FORMAT(V_008F0C_IMG_FORMAT_32_FLOAT) |
                   S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_RAW) |
                   S_008F0C_RESOURCE_LEVEL(1);
   } else if (ctx.program->chip_class <= GFX7) { /* dfmt modifies stride on GFX8/GFX9 when ADD_TID_EN=1 */
      rsrc_conf |= S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) |
                   S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);
   }
   /* older generations need element size = 4 bytes. element size removed in GFX9 */
   if (ctx.program->chip_class <= GFX8)
      rsrc_conf |= S_008F0C_ELEMENT_SIZE(1);

   return bld.pseudo(aco_opcode::p_create_vector, bld.def(s4),
                     private_segment_buffer, Operand(-1u),
                     Operand(rsrc_conf));
}

void add_interferences(spill_ctx& ctx, std::vector<bool>& is_assigned,
                       std::vector<uint32_t>& slots, std::vector<bool>& slots_used,
                       unsigned id)
{
   for (unsigned other : ctx.interferences[id].second) {
      if (!is_assigned[other])
         continue;

      RegClass other_rc = ctx.interferences[other].first;
      unsigned slot = slots[other];
      std::fill(slots_used.begin() + slot, slots_used.begin() + slot + other_rc.size(), true);
   }
}

unsigned find_available_slot(std::vector<bool>& used, unsigned wave_size,
                             unsigned size, bool is_sgpr, unsigned *num_slots)
{
   unsigned wave_size_minus_one = wave_size - 1;
   unsigned slot = 0;

   while (true) {
      bool available = true;
      for (unsigned i = 0; i < size; i++) {
         if (slot + i < used.size() && used[slot + i]) {
            available = false;
            break;
         }
      }
      if (!available) {
         slot++;
         continue;
      }

      if (is_sgpr && ((slot & wave_size_minus_one) > wave_size - size)) {
         slot = align(slot, wave_size);
         continue;
      }

      std::fill(used.begin(), used.end(), false);

      if (slot + size > used.size())
         used.resize(slot + size);

      return slot;
   }
}

void assign_spill_slots_helper(spill_ctx& ctx, RegType type,
                               std::vector<bool>& is_assigned,
                               std::vector<uint32_t>& slots,
                               unsigned *num_slots)
{
   std::vector<bool> slots_used(*num_slots);

   /* assign slots for ids with affinities first */
   for (std::vector<uint32_t>& vec : ctx.affinities) {
      if (ctx.interferences[vec[0]].first.type() != type)
         continue;

      for (unsigned id : vec) {
         if (!ctx.is_reloaded[id])
            continue;

         add_interferences(ctx, is_assigned, slots, slots_used, id);
      }

      unsigned slot = find_available_slot(slots_used, ctx.wave_size,
                                          ctx.interferences[vec[0]].first.size(),
                                          type == RegType::sgpr, num_slots);

      for (unsigned id : vec) {
         assert(!is_assigned[id]);

         if (ctx.is_reloaded[id]) {
            slots[id] = slot;
            is_assigned[id] = true;
         }
      }
   }

   /* assign slots for ids without affinities */
   for (unsigned id = 0; id < ctx.interferences.size(); id++) {
      if (is_assigned[id] || !ctx.is_reloaded[id] || ctx.interferences[id].first.type() != type)
         continue;

      add_interferences(ctx, is_assigned, slots, slots_used, id);

      unsigned slot = find_available_slot(slots_used, ctx.wave_size,
                                          ctx.interferences[id].first.size(),
                                          type == RegType::sgpr, num_slots);

      slots[id] = slot;
      is_assigned[id] = true;
   }

   *num_slots = slots_used.size();
}

void assign_spill_slots(spill_ctx& ctx, unsigned spills_to_vgpr) {
   std::vector<uint32_t> slots(ctx.interferences.size());
   std::vector<bool> is_assigned(ctx.interferences.size());

   /* first, handle affinities: just merge all interferences into both spill ids */
   for (std::vector<uint32_t>& vec : ctx.affinities) {
      for (unsigned i = 0; i < vec.size(); i++) {
         for (unsigned j = i + 1; j < vec.size(); j++) {
            assert(vec[i] != vec[j]);
            bool reloaded = ctx.is_reloaded[vec[i]] || ctx.is_reloaded[vec[j]];
            ctx.is_reloaded[vec[i]] = reloaded;
            ctx.is_reloaded[vec[j]] = reloaded;
         }
      }
   }
   for (ASSERTED uint32_t i = 0; i < ctx.interferences.size(); i++)
      for (ASSERTED uint32_t id : ctx.interferences[i].second)
         assert(i != id);

   /* for each spill slot, assign as many spill ids as possible */
   unsigned sgpr_spill_slots = 0, vgpr_spill_slots = 0;
   assign_spill_slots_helper(ctx, RegType::sgpr, is_assigned, slots, &sgpr_spill_slots);
   assign_spill_slots_helper(ctx, RegType::vgpr, is_assigned, slots, &vgpr_spill_slots);

   for (unsigned id = 0; id < is_assigned.size(); id++)
      assert(is_assigned[id] || !ctx.is_reloaded[id]);

   for (std::vector<uint32_t>& vec : ctx.affinities) {
      for (unsigned i = 0; i < vec.size(); i++) {
         for (unsigned j = i + 1; j < vec.size(); j++) {
            assert(is_assigned[vec[i]] == is_assigned[vec[j]]);
            if (!is_assigned[vec[i]])
               continue;
            assert(ctx.is_reloaded[vec[i]] == ctx.is_reloaded[vec[j]]);
            assert(ctx.interferences[vec[i]].first.type() == ctx.interferences[vec[j]].first.type());
            assert(slots[vec[i]] == slots[vec[j]]);
         }
      }
   }

   /* hope, we didn't mess up */
   std::vector<Temp> vgpr_spill_temps((sgpr_spill_slots + ctx.wave_size - 1) / ctx.wave_size);
   assert(vgpr_spill_temps.size() <= spills_to_vgpr);

   /* replace pseudo instructions with actual hardware instructions */
   Temp scratch_offset = ctx.program->scratch_offset, scratch_rsrc = Temp();
   unsigned last_top_level_block_idx = 0;
   std::vector<bool> reload_in_loop(vgpr_spill_temps.size());
   for (Block& block : ctx.program->blocks) {

      /* after loops, we insert a user if there was a reload inside the loop */
      if (block.loop_nest_depth == 0) {
         int end_vgprs = 0;
         for (unsigned i = 0; i < vgpr_spill_temps.size(); i++) {
            if (reload_in_loop[i])
               end_vgprs++;
         }

         if (end_vgprs > 0) {
            aco_ptr<Instruction> destr{create_instruction<Pseudo_instruction>(aco_opcode::p_end_linear_vgpr, Format::PSEUDO, end_vgprs, 0)};
            int k = 0;
            for (unsigned i = 0; i < vgpr_spill_temps.size(); i++) {
               if (reload_in_loop[i])
                  destr->operands[k++] = Operand(vgpr_spill_temps[i]);
               reload_in_loop[i] = false;
            }
            /* find insertion point */
            std::vector<aco_ptr<Instruction>>::iterator it = block.instructions.begin();
            while ((*it)->opcode == aco_opcode::p_linear_phi || (*it)->opcode == aco_opcode::p_phi)
               ++it;
            block.instructions.insert(it, std::move(destr));
         }
      }

      if (block.kind & block_kind_top_level && !block.linear_preds.empty()) {
         last_top_level_block_idx = block.index;

         /* check if any spilled variables use a created linear vgpr, otherwise destroy them */
         for (unsigned i = 0; i < vgpr_spill_temps.size(); i++) {
            if (vgpr_spill_temps[i] == Temp())
               continue;

            bool can_destroy = true;
            for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[block.linear_preds[0]]) {

               if (ctx.interferences[pair.second].first.type() == RegType::sgpr &&
                   slots[pair.second] / ctx.wave_size == i) {
                  can_destroy = false;
                  break;
               }
            }
            if (can_destroy)
               vgpr_spill_temps[i] = Temp();
         }
      }

      std::vector<aco_ptr<Instruction>>::iterator it;
      std::vector<aco_ptr<Instruction>> instructions;
      instructions.reserve(block.instructions.size());
      Builder bld(ctx.program, &instructions);
      for (it = block.instructions.begin(); it != block.instructions.end(); ++it) {

         if ((*it)->opcode == aco_opcode::p_spill) {
            uint32_t spill_id = (*it)->operands[1].constantValue();

            if (!ctx.is_reloaded[spill_id]) {
               /* never reloaded, so don't spill */
            } else if (!is_assigned[spill_id]) {
               unreachable("No spill slot assigned for spill id");
            } else if (ctx.interferences[spill_id].first.type() == RegType::vgpr) {
               /* spill vgpr */
               ctx.program->config->spilled_vgprs += (*it)->operands[0].size();
               uint32_t spill_slot = slots[spill_id];
               bool add_offset_to_sgpr = ctx.program->config->scratch_bytes_per_wave / ctx.program->wave_size + vgpr_spill_slots * 4 > 4096;
               unsigned base_offset = add_offset_to_sgpr ? 0 : ctx.program->config->scratch_bytes_per_wave / ctx.program->wave_size;

               /* check if the scratch resource descriptor already exists */
               if (scratch_rsrc == Temp()) {
                  unsigned offset = add_offset_to_sgpr ? ctx.program->config->scratch_bytes_per_wave : 0;
                  scratch_rsrc = load_scratch_resource(ctx, scratch_offset,
                                                       last_top_level_block_idx == block.index ?
                                                       instructions : ctx.program->blocks[last_top_level_block_idx].instructions,
                                                       offset,
                                                       last_top_level_block_idx == block.index);
               }

               unsigned offset = base_offset + spill_slot * 4;
               aco_opcode opcode = aco_opcode::buffer_store_dword;
               assert((*it)->operands[0].isTemp());
               Temp temp = (*it)->operands[0].getTemp();
               assert(temp.type() == RegType::vgpr && !temp.is_linear());
               if (temp.size() > 1) {
                  Instruction* split{create_instruction<Pseudo_instruction>(aco_opcode::p_split_vector, Format::PSEUDO, 1, temp.size())};
                  split->operands[0] = Operand(temp);
                  for (unsigned i = 0; i < temp.size(); i++)
                     split->definitions[i] = bld.def(v1);
                  bld.insert(split);
                  for (unsigned i = 0; i < temp.size(); i++) {
                     Instruction *instr = bld.mubuf(opcode, scratch_rsrc, Operand(v1), scratch_offset, split->definitions[i].getTemp(), offset + i * 4, false, true);
                     static_cast<MUBUF_instruction *>(instr)->sync = memory_sync_info(storage_vgpr_spill, semantic_private);
                  }
               } else {
                  Instruction *instr = bld.mubuf(opcode, scratch_rsrc, Operand(v1), scratch_offset, temp, offset, false, true);
                  static_cast<MUBUF_instruction *>(instr)->sync = memory_sync_info(storage_vgpr_spill, semantic_private);
               }
            } else {
               ctx.program->config->spilled_sgprs += (*it)->operands[0].size();

               uint32_t spill_slot = slots[spill_id];

               /* check if the linear vgpr already exists */
               if (vgpr_spill_temps[spill_slot / ctx.wave_size] == Temp()) {
                  Temp linear_vgpr = ctx.program->allocateTmp(v1.as_linear());
                  vgpr_spill_temps[spill_slot / ctx.wave_size] = linear_vgpr;
                  aco_ptr<Pseudo_instruction> create{create_instruction<Pseudo_instruction>(aco_opcode::p_start_linear_vgpr, Format::PSEUDO, 0, 1)};
                  create->definitions[0] = Definition(linear_vgpr);
                  /* find the right place to insert this definition */
                  if (last_top_level_block_idx == block.index) {
                     /* insert right before the current instruction */
                     instructions.emplace_back(std::move(create));
                  } else {
                     assert(last_top_level_block_idx < block.index);
                     /* insert before the branch at last top level block */
                     std::vector<aco_ptr<Instruction>>& instructions = ctx.program->blocks[last_top_level_block_idx].instructions;
                     instructions.insert(std::next(instructions.begin(), instructions.size() - 1), std::move(create));
                  }
               }

               /* spill sgpr: just add the vgpr temp to operands */
               Pseudo_instruction* spill = create_instruction<Pseudo_instruction>(aco_opcode::p_spill, Format::PSEUDO, 3, 0);
               spill->operands[0] = Operand(vgpr_spill_temps[spill_slot / ctx.wave_size]);
               spill->operands[1] = Operand(spill_slot % ctx.wave_size);
               spill->operands[2] = (*it)->operands[0];
               instructions.emplace_back(aco_ptr<Instruction>(spill));
            }

         } else if ((*it)->opcode == aco_opcode::p_reload) {
            uint32_t spill_id = (*it)->operands[0].constantValue();
            assert(ctx.is_reloaded[spill_id]);

            if (!is_assigned[spill_id]) {
               unreachable("No spill slot assigned for spill id");
            } else if (ctx.interferences[spill_id].first.type() == RegType::vgpr) {
               /* reload vgpr */
               uint32_t spill_slot = slots[spill_id];
               bool add_offset_to_sgpr = ctx.program->config->scratch_bytes_per_wave / ctx.program->wave_size + vgpr_spill_slots * 4 > 4096;
               unsigned base_offset = add_offset_to_sgpr ? 0 : ctx.program->config->scratch_bytes_per_wave / ctx.program->wave_size;

               /* check if the scratch resource descriptor already exists */
               if (scratch_rsrc == Temp()) {
                  unsigned offset = add_offset_to_sgpr ? ctx.program->config->scratch_bytes_per_wave : 0;
                  scratch_rsrc = load_scratch_resource(ctx, scratch_offset,
                                                       last_top_level_block_idx == block.index ?
                                                       instructions : ctx.program->blocks[last_top_level_block_idx].instructions,
                                                       offset,
                                                       last_top_level_block_idx == block.index);
               }

               unsigned offset = base_offset + spill_slot * 4;
               aco_opcode opcode = aco_opcode::buffer_load_dword;
               Definition def = (*it)->definitions[0];
               if (def.size() > 1) {
                  Instruction* vec{create_instruction<Pseudo_instruction>(aco_opcode::p_create_vector, Format::PSEUDO, def.size(), 1)};
                  vec->definitions[0] = def;
                  for (unsigned i = 0; i < def.size(); i++) {
                     Temp tmp = bld.tmp(v1);
                     vec->operands[i] = Operand(tmp);
                     Instruction *instr = bld.mubuf(opcode, Definition(tmp), scratch_rsrc, Operand(v1), scratch_offset, offset + i * 4, false, true);
                     static_cast<MUBUF_instruction *>(instr)->sync = memory_sync_info(storage_vgpr_spill, semantic_private);
                  }
                  bld.insert(vec);
               } else {
                  Instruction *instr = bld.mubuf(opcode, def, scratch_rsrc, Operand(v1), scratch_offset, offset, false, true);
                  static_cast<MUBUF_instruction *>(instr)->sync = memory_sync_info(storage_vgpr_spill, semantic_private);
               }
            } else {
               uint32_t spill_slot = slots[spill_id];
               reload_in_loop[spill_slot / ctx.wave_size] = block.loop_nest_depth > 0;

               /* check if the linear vgpr already exists */
               if (vgpr_spill_temps[spill_slot / ctx.wave_size] == Temp()) {
                  Temp linear_vgpr = ctx.program->allocateTmp(v1.as_linear());
                  vgpr_spill_temps[spill_slot / ctx.wave_size] = linear_vgpr;
                  aco_ptr<Pseudo_instruction> create{create_instruction<Pseudo_instruction>(aco_opcode::p_start_linear_vgpr, Format::PSEUDO, 0, 1)};
                  create->definitions[0] = Definition(linear_vgpr);
                  /* find the right place to insert this definition */
                  if (last_top_level_block_idx == block.index) {
                     /* insert right before the current instruction */
                     instructions.emplace_back(std::move(create));
                  } else {
                     assert(last_top_level_block_idx < block.index);
                     /* insert before the branch at last top level block */
                     std::vector<aco_ptr<Instruction>>& instructions = ctx.program->blocks[last_top_level_block_idx].instructions;
                     instructions.insert(std::next(instructions.begin(), instructions.size() - 1), std::move(create));
                  }
               }

               /* reload sgpr: just add the vgpr temp to operands */
               Pseudo_instruction* reload = create_instruction<Pseudo_instruction>(aco_opcode::p_reload, Format::PSEUDO, 2, 1);
               reload->operands[0] = Operand(vgpr_spill_temps[spill_slot / ctx.wave_size]);
               reload->operands[1] = Operand(spill_slot % ctx.wave_size);
               reload->definitions[0] = (*it)->definitions[0];
               instructions.emplace_back(aco_ptr<Instruction>(reload));
            }
         } else if (!ctx.remat_used.count(it->get()) || ctx.remat_used[it->get()]) {
            instructions.emplace_back(std::move(*it));
         }

      }
      block.instructions = std::move(instructions);
   }

   /* update required scratch memory */
   ctx.program->config->scratch_bytes_per_wave += align(vgpr_spill_slots * 4 * ctx.program->wave_size, 1024);

   /* SSA elimination inserts copies for logical phis right before p_logical_end
    * So if a linear vgpr is used between that p_logical_end and the branch,
    * we need to ensure logical phis don't choose a definition which aliases
    * the linear vgpr.
    * TODO: Moving the spills and reloads to before p_logical_end might produce
    *       slightly better code. */
   for (Block& block : ctx.program->blocks) {
      /* loops exits are already handled */
      if (block.logical_preds.size() <= 1)
         continue;

      bool has_logical_phis = false;
      for (aco_ptr<Instruction>& instr : block.instructions) {
         if (instr->opcode == aco_opcode::p_phi) {
            has_logical_phis = true;
            break;
         } else if (instr->opcode != aco_opcode::p_linear_phi) {
            break;
         }
      }
      if (!has_logical_phis)
         continue;

      std::set<Temp> vgprs;
      for (unsigned pred_idx : block.logical_preds) {
         Block& pred = ctx.program->blocks[pred_idx];
         for (int i = pred.instructions.size() - 1; i >= 0; i--) {
            aco_ptr<Instruction>& pred_instr = pred.instructions[i];
            if (pred_instr->opcode == aco_opcode::p_logical_end) {
               break;
            } else if (pred_instr->opcode == aco_opcode::p_spill ||
                       pred_instr->opcode == aco_opcode::p_reload) {
               vgprs.insert(pred_instr->operands[0].getTemp());
            }
         }
      }
      if (!vgprs.size())
         continue;

      aco_ptr<Instruction> destr{create_instruction<Pseudo_instruction>(aco_opcode::p_end_linear_vgpr, Format::PSEUDO, vgprs.size(), 0)};
      int k = 0;
      for (Temp tmp : vgprs) {
         destr->operands[k++] = Operand(tmp);
      }
      /* find insertion point */
      std::vector<aco_ptr<Instruction>>::iterator it = block.instructions.begin();
      while ((*it)->opcode == aco_opcode::p_linear_phi || (*it)->opcode == aco_opcode::p_phi)
         ++it;
      block.instructions.insert(it, std::move(destr));
   }
}

} /* end namespace */


void spill(Program* program, live& live_vars)
{
   program->config->spilled_vgprs = 0;
   program->config->spilled_sgprs = 0;

   /* no spilling when register pressure is low enough */
   if (program->num_waves > 0)
      return;

   /* lower to CSSA before spilling to ensure correctness w.r.t. phis */
   lower_to_cssa(program, live_vars);

   /* calculate target register demand */
   RegisterDemand register_target = program->max_reg_demand;
   if (register_target.sgpr > program->sgpr_limit)
      register_target.vgpr += (register_target.sgpr - program->sgpr_limit + program->wave_size - 1 + 32) / program->wave_size;
   register_target.sgpr = program->sgpr_limit;

   if (register_target.vgpr > program->vgpr_limit)
      register_target.sgpr = program->sgpr_limit - 5;
   int spills_to_vgpr = (program->max_reg_demand.sgpr - register_target.sgpr + program->wave_size - 1 + 32) / program->wave_size;
   register_target.vgpr = program->vgpr_limit - spills_to_vgpr;

   /* initialize ctx */
   spill_ctx ctx(register_target, program, live_vars.register_demand);
   compute_global_next_uses(ctx);
   get_rematerialize_info(ctx);

   /* create spills and reloads */
   for (unsigned i = 0; i < program->blocks.size(); i++)
      spill_block(ctx, i);

   /* assign spill slots and DCE rematerialized code */
   assign_spill_slots(ctx, spills_to_vgpr);

   /* update live variable information */
   live_vars = live_var_analysis(program);

   assert(program->num_waves > 0);
}

}

