/*
 * Copyright © 2018 Valve Corporation
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

#include <map>
#include <unordered_map>
#include "aco_ir.h"

/*
 * Implements the algorithm for dominator-tree value numbering
 * from "Value Numbering" by Briggs, Cooper, and Simpson.
 */

namespace aco {
namespace {

struct InstrHash {
   std::size_t operator()(Instruction* instr) const
   {
      uint64_t hash = (uint64_t) instr->opcode + (uint64_t) instr->format;
      for (unsigned i = 0; i < instr->operands.size(); i++) {
         Operand op = instr->operands[i];
         uint64_t val = op.isTemp() ? op.tempId() : op.isFixed() ? op.physReg() : op.constantValue();
         hash |= val << (i+1) * 8;
      }
      if (instr->isVOP3()) {
         VOP3A_instruction* vop3 = static_cast<VOP3A_instruction*>(instr);
         for (unsigned i = 0; i < 3; i++) {
            hash ^= vop3->abs[i] << (i*3 + 0);
            hash ^= vop3->opsel[i] << (i*3 + 1);
            hash ^= vop3->neg[i] << (i*3 + 2);
         }
         hash ^= (vop3->clamp << 28) * 13;
         hash += vop3->omod << 19;
      }
      switch (instr->format) {
      case Format::SMEM:
         break;
      case Format::VINTRP: {
         Interp_instruction* interp = static_cast<Interp_instruction*>(instr);
         hash ^= interp->attribute << 13;
         hash ^= interp->component << 27;
         break;
      }
      case Format::DS:
         break;
      default:
         break;
      }

      return hash;
   }
};

struct InstrPred {
   bool operator()(Instruction* a, Instruction* b) const
   {
      if (a->format != b->format)
         return false;
      if (a->opcode != b->opcode)
         return false;
      if (a->operands.size() != b->operands.size() || a->definitions.size() != b->definitions.size())
         return false; /* possible with pseudo-instructions */
      /* We can't value number v_readlane_b32 across control flow or discards
       * because of the possibility of live-range splits.
       * We can't value number permutes for the same reason as
       * v_readlane_b32 and because discards affect the result */
      if (a->opcode == aco_opcode::v_readfirstlane_b32 || a->opcode == aco_opcode::v_readlane_b32 ||
          a->opcode == aco_opcode::ds_bpermute_b32 || a->opcode == aco_opcode::ds_permute_b32 ||
          a->opcode == aco_opcode::ds_swizzle_b32 || a->format == Format::PSEUDO_REDUCTION ||
          a->opcode == aco_opcode::p_phi || a->opcode == aco_opcode::p_linear_phi) {
         if (a->pass_flags != b->pass_flags)
            return false;
      }
      for (unsigned i = 0; i < a->operands.size(); i++) {
         if (a->operands[i].isConstant()) {
            if (!b->operands[i].isConstant())
               return false;
            if (a->operands[i].constantValue() != b->operands[i].constantValue())
               return false;
         }
         else if (a->operands[i].isTemp()) {
            if (!b->operands[i].isTemp())
               return false;
            if (a->operands[i].tempId() != b->operands[i].tempId())
               return false;
         }
         else if (a->operands[i].isUndefined() ^ b->operands[i].isUndefined())
            return false;
         if (a->operands[i].isFixed()) {
            if (a->operands[i].physReg() == exec)
               return false;
            if (!b->operands[i].isFixed())
               return false;
            if (!(a->operands[i].physReg() == b->operands[i].physReg()))
               return false;
         }
      }
      for (unsigned i = 0; i < a->definitions.size(); i++) {
         if (a->definitions[i].isTemp()) {
            if (!b->definitions[i].isTemp())
               return false;
            if (a->definitions[i].regClass() != b->definitions[i].regClass())
               return false;
         }
         if (a->definitions[i].isFixed()) {
            if (!b->definitions[i].isFixed())
               return false;
            if (!(a->definitions[i].physReg() == b->definitions[i].physReg()))
               return false;
         }
      }
      if (a->format == Format::PSEUDO_BRANCH)
         return false;
      if (a->isVOP3()) {
         VOP3A_instruction* a3 = static_cast<VOP3A_instruction*>(a);
         VOP3A_instruction* b3 = static_cast<VOP3A_instruction*>(b);
         for (unsigned i = 0; i < 3; i++) {
            if (a3->abs[i] != b3->abs[i] ||
                a3->opsel[i] != b3->opsel[i] ||
                a3->neg[i] != b3->neg[i])
               return false;
         }
         return a3->clamp == b3->clamp &&
                a3->omod == b3->omod;
      }
      if (a->isDPP()) {
         DPP_instruction* aDPP = static_cast<DPP_instruction*>(a);
         DPP_instruction* bDPP = static_cast<DPP_instruction*>(b);
         return aDPP->dpp_ctrl == bDPP->dpp_ctrl &&
                aDPP->bank_mask == bDPP->bank_mask &&
                aDPP->row_mask == bDPP->row_mask &&
                aDPP->bound_ctrl == bDPP->bound_ctrl &&
                aDPP->abs[0] == bDPP->abs[0] &&
                aDPP->abs[1] == bDPP->abs[1] &&
                aDPP->neg[0] == bDPP->neg[0] &&
                aDPP->neg[1] == bDPP->neg[1];
      }
      switch (a->format) {
         case Format::VOPC: {
            /* Since the results depend on the exec mask, these shouldn't
             * be value numbered (this is especially useful for subgroupBallot()). */
            return false;
         }
         case Format::SOPK: {
            SOPK_instruction* aK = static_cast<SOPK_instruction*>(a);
            SOPK_instruction* bK = static_cast<SOPK_instruction*>(b);
            return aK->imm == bK->imm;
         }
         case Format::SMEM: {
            SMEM_instruction* aS = static_cast<SMEM_instruction*>(a);
            SMEM_instruction* bS = static_cast<SMEM_instruction*>(b);
            return aS->can_reorder && bS->can_reorder &&
                   aS->glc == bS->glc && aS->nv == bS->nv;
         }
         case Format::VINTRP: {
            Interp_instruction* aI = static_cast<Interp_instruction*>(a);
            Interp_instruction* bI = static_cast<Interp_instruction*>(b);
            if (aI->attribute != bI->attribute)
               return false;
            if (aI->component != bI->component)
               return false;
            return true;
         }
         case Format::PSEUDO_REDUCTION: {
            Pseudo_reduction_instruction *aR = static_cast<Pseudo_reduction_instruction*>(a);
            Pseudo_reduction_instruction *bR = static_cast<Pseudo_reduction_instruction*>(b);
            return aR->reduce_op == bR->reduce_op && aR->cluster_size == bR->cluster_size;
         }
         case Format::MTBUF: {
            /* this is fine since they are only used for vertex input fetches */
            MTBUF_instruction* aM = static_cast<MTBUF_instruction *>(a);
            MTBUF_instruction* bM = static_cast<MTBUF_instruction *>(b);
            return aM->can_reorder == bM->can_reorder &&
                   aM->barrier == bM->barrier &&
                   aM->dfmt == bM->dfmt &&
                   aM->nfmt == bM->nfmt &&
                   aM->offset == bM->offset &&
                   aM->offen == bM->offen &&
                   aM->idxen == bM->idxen &&
                   aM->glc == bM->glc &&
                   aM->slc == bM->slc &&
                   aM->tfe == bM->tfe &&
                   aM->disable_wqm == bM->disable_wqm;
         }
         /* we want to optimize these in NIR and don't hassle with load-store dependencies */
         case Format::MUBUF:
         case Format::FLAT:
         case Format::GLOBAL:
         case Format::SCRATCH:
            return false;
         case Format::DS: {
            /* we already handle potential issue with permute/swizzle above */
            DS_instruction* aD = static_cast<DS_instruction *>(a);
            DS_instruction* bD = static_cast<DS_instruction *>(b);
            if (a->opcode != aco_opcode::ds_bpermute_b32 &&
                a->opcode != aco_opcode::ds_permute_b32 &&
                a->opcode != aco_opcode::ds_swizzle_b32)
               return false;
            return aD->gds == bD->gds && aD->offset0 == bD->offset0 && aD->offset1 == bD->offset1;
         }
         case Format::MIMG: {
            MIMG_instruction* aM = static_cast<MIMG_instruction*>(a);
            MIMG_instruction* bM = static_cast<MIMG_instruction*>(b);
            return aM->can_reorder && bM->can_reorder &&
                   aM->barrier == bM->barrier &&
                   aM->dmask == bM->dmask &&
                   aM->unrm == bM->unrm &&
                   aM->glc == bM->glc &&
                   aM->slc == bM->slc &&
                   aM->tfe == bM->tfe &&
                   aM->da == bM->da &&
                   aM->lwe == bM->lwe &&
                   aM->r128 == bM->r128 &&
                   aM->a16 == bM->a16 &&
                   aM->d16 == bM->d16 &&
                   aM->disable_wqm == bM->disable_wqm;
         }
         default:
            return true;
      }
   }
};

using expr_set = std::unordered_map<Instruction*, uint32_t, InstrHash, InstrPred>;

struct vn_ctx {
   Program* program;
   expr_set expr_values;
   std::map<uint32_t, Temp> renames;
   uint32_t exec_id = 0;

   vn_ctx(Program* program) : program(program) {}
};

bool dominates(vn_ctx& ctx, uint32_t parent, uint32_t child)
{
   while (parent < child)
      child = ctx.program->blocks[child].logical_idom;

   return parent == child;
}

void process_block(vn_ctx& ctx, Block& block)
{
   std::vector<aco_ptr<Instruction>> new_instructions;
   new_instructions.reserve(block.instructions.size());

   for (aco_ptr<Instruction>& instr : block.instructions) {
      /* first, rename operands */
      for (Operand& op : instr->operands) {
         if (!op.isTemp())
            continue;
         auto it = ctx.renames.find(op.tempId());
         if (it != ctx.renames.end())
            op.setTemp(it->second);
      }

      if (instr->definitions.empty()) {
         new_instructions.emplace_back(std::move(instr));
         continue;
      }

      /* simple copy-propagation through renaming */
      if ((instr->opcode == aco_opcode::s_mov_b32 || instr->opcode == aco_opcode::s_mov_b64 || instr->opcode == aco_opcode::v_mov_b32) &&
          !instr->definitions[0].isFixed() && instr->operands[0].isTemp() && instr->operands[0].regClass() == instr->definitions[0].regClass() &&
          !instr->isDPP() && !((int)instr->format & (int)Format::SDWA)) {
         ctx.renames[instr->definitions[0].tempId()] = instr->operands[0].getTemp();
      }

      if (instr->opcode == aco_opcode::p_discard_if ||
          instr->opcode == aco_opcode::p_demote_to_helper)
         ctx.exec_id++;

      instr->pass_flags = ctx.exec_id;
      std::pair<expr_set::iterator, bool> res = ctx.expr_values.emplace(instr.get(), block.index);

      /* if there was already an expression with the same value number */
      if (!res.second) {
         Instruction* orig_instr = res.first->first;
         assert(instr->definitions.size() == orig_instr->definitions.size());
         /* check if the original instruction dominates the current one */
         if (dominates(ctx, res.first->second, block.index)) {
            for (unsigned i = 0; i < instr->definitions.size(); i++) {
               assert(instr->definitions[i].regClass() == orig_instr->definitions[i].regClass());
               ctx.renames[instr->definitions[i].tempId()] = orig_instr->definitions[i].getTemp();
            }
         } else {
            ctx.expr_values.erase(res.first);
            ctx.expr_values.emplace(instr.get(), block.index);
            new_instructions.emplace_back(std::move(instr));
         }
      } else {
         new_instructions.emplace_back(std::move(instr));
      }
   }

   block.instructions = std::move(new_instructions);
}

void rename_phi_operands(Block& block, std::map<uint32_t, Temp>& renames)
{
   for (aco_ptr<Instruction>& phi : block.instructions) {
      if (phi->opcode != aco_opcode::p_phi && phi->opcode != aco_opcode::p_linear_phi)
         break;

      for (Operand& op : phi->operands) {
         if (!op.isTemp())
            continue;
         auto it = renames.find(op.tempId());
         if (it != renames.end())
            op.setTemp(it->second);
      }
   }
}
} /* end namespace */


void value_numbering(Program* program)
{
   vn_ctx ctx(program);

   for (Block& block : program->blocks) {
      if (block.logical_idom != -1)
         process_block(ctx, block);
      else
         rename_phi_operands(block, ctx.renames);

      ctx.exec_id++;
   }

   /* rename loop header phi operands */
   for (Block& block : program->blocks) {
      if (block.kind & block_kind_loop_header)
         rename_phi_operands(block, ctx.renames);
   }
}

}
