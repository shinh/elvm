#include <stdio.h>
#include <stdlib.h>

#include <ir/ir.h>
#include <target/elf.h>
#include <target/util.h>

static int REGNO[] = {
  0, 3, 1, 2, 5, 4, 6, 7
};

#define ESI ((Reg)6)
#define EDI ((Reg)7)

static void emit_header(uint32_t filesz) {
  Elf32_Ehdr ehdr = {
    .e_ident = {
      0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01
    },
    .e_type = 2,
    .e_machine = 3,
    .e_version = 1,
    .e_entry = 0x100000 + 84,
    .e_phoff = 52,
    .e_shoff = 0,
    .e_flags = 0,
    .e_ehsize = 52,
    .e_phentsize = 32,
    .e_phnum = 1,
    .e_shentsize = 40,
    .e_shnum = 0,
    .e_shstrndx = 0
  };
  Elf32_Phdr phdr = {
    .p_type = 1,
    .p_offset = 0,
    .p_vaddr = 0x100000,
    .p_paddr = 0x100000,
    .p_filesz = filesz + 84,
    .p_memsz = filesz + 84,
    .p_flags = 5,
    .p_align = 0x1000
  };
  fwrite(&ehdr, 52, 1, stdout);
  fwrite(&phdr, 32, 1, stdout);
}

static void emit_int80() {
  emit_2(0xcd, 0x80);
}

static void emit_reg2(Reg dst, Reg src) {
  emit_1(0xc0 | REGNO[dst] | (REGNO[src] << 3));
}

static void emit_mov_reg(Reg dst, Reg src) {
  emit_1(0x89);
  emit_reg2(dst, src);
}

static void emit_mov_imm(Reg dst, int src) {
  emit_1(0xb8 | REGNO[dst]);
  emit_le(src);
}

static void emit_mov(Reg dst, Value* src) {
  if (src->type == REG) {
    emit_mov_reg(dst, src->reg);
  } else {
    emit_mov_imm(dst, src->imm);
  }
}

static void emit_cmp(Inst* inst) {
  if (inst->src.type == REG) {
    //emit_1(0x01);
    //emit_reg2(inst->dst.reg, inst->src.reg);
    error("oops cmp");
  } else {
    emit_2(0x81, 0xf8 | REGNO[inst->dst.reg]);
    emit_le(inst->src.imm);
  }
}

static void init_state(Data* data) {
  emit_mov_imm(B, 0);
  emit_mov_imm(C, 1 << 24);
  emit_mov_imm(D, 3);  // PROT_READ | PROT_WRITE
  emit_mov_imm(ESI, 0x22);  // MAP_PRIVATE | MAP_ANONYMOUS
  emit_mov_imm(EDI, -1);
  emit_mov_imm(BP, 0);
  emit_mov_imm(A, 192);
  emit_int80();

  emit_mov_reg(ESI, A);

  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      // mov dword [EAX+mp], data->v
      emit_2(0xc7, 0x80);
      emit_le(mp);
      emit_le(data->v);
    }
  }

  // lea ESP, [EAX + (1<<24)]
  emit_6(0x8d, 0xa0, 0, 0, 0, 1);
  // xor EAX, EAX
  emit_2(0x31, 0xc0);
  // xor EBX, EBX
  emit_2(0x31, 0xdb);
  // xor ECX, ECX
  emit_2(0x31, 0xc9);
  // xor EDX, EDX
  emit_2(0x31, 0xd2);
  // xor EBP, EBP
  emit_2(0x31, 0xed);
}

static void emit_inst(Inst* inst, int* pc2addr) {
  switch (inst->op) {
    case MOV:
      emit_mov(inst->dst.reg, &inst->src);
      break;

    case ADD:
      if (inst->src.type == REG) {
        emit_1(0x01);
        emit_reg2(inst->dst.reg, inst->src.reg);
      } else {
        emit_2(0x81, 0xc0 | REGNO[inst->dst.reg]);
        emit_le(inst->src.imm);
      }
      break;

    case SUB:
      if (inst->src.type == REG) {
        emit_1(0x29);
        emit_reg2(inst->dst.reg, inst->src.reg);
      } else {
        emit_2(0x81, 0xe8 | REGNO[inst->dst.reg]);
        emit_le(inst->src.imm);
      }
      break;

    case LOAD:
      emit_1(0x8b);
      goto store_load_common;

    case STORE:
      emit_1(0x89);
    store_load_common:
      if (inst->src.type == REG) {
        emit_2(4 | (REGNO[inst->dst.reg] << 3), 0x30 | REGNO[inst->src.reg]);
      } else {
        emit_1(0x86 | (REGNO[inst->dst.reg] << 3));
        emit_le(inst->src.imm);
      }
      break;

    case PUTC:
      emit_mov(EDI, &inst->src);
      // push EAX, ECX, EDX, EBX, EDI
      emit_5(0x50, 0x51, 0x52, 0x53, 0x57);
      emit_mov_imm(B, 1);  // stdout
      emit_mov_reg(C, SP);
      emit_mov_imm(D, 1);
      emit_mov_imm(A, 4);  // write
      emit_int80();
      // pop EDI, EBX, EDX, ECX, EAX
      emit_5(0x5f, 0x5b, 0x5a, 0x59, 0x58);
      break;

    case GETC:
      // push EAX, ECX, EDX, EBX, EDI
      emit_5(0x50, 0x51, 0x52, 0x53, 0x57);
      emit_mov_imm(B, 0);  // stdin
      emit_mov_reg(C, SP);
      emit_mov_imm(D, 1);
      emit_mov_imm(A, 3);  // read
      emit_int80();

      // pop EDI
      emit_1(0x5f);
      emit_mov_imm(B, 0);
      // cmp EAX, 1
      emit_3(0x83, 0xf8, 0x01);
      // cmovnz EDI, EBX
      emit_3(0x0f, 0x45, 0xfb);

      // pop EBX, EDX, ECX, EAX
      emit_4(0x5b, 0x5a, 0x59, 0x58);
      emit_mov_reg(inst->src.reg, EDI);
      break;

    case EXIT:
      emit_mov_imm(B, 0);
      emit_mov_imm(A, 1);  // exit
      emit_int80();
      break;

    case DUMP:
      break;

    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
      // TODO
      error("cmp");
      break;

    case JEQ:
      emit_cmp(inst);
      if (inst->jmp.type == REG) {
        error("oops");
      } else {
        emit_2(0x0f, 0x84);
        emit_le(pc2addr[inst->jmp.imm] - emit_cnt() - 4);
      }
      break;

    case JNE:
    case JLT:
    case JGT:
      error("oops jne");

    case JLE:
      emit_cmp(inst);
      if (inst->jmp.type == REG) {
        error("oops");
      } else {
        emit_2(0x0f, 0x8e);
        emit_le(pc2addr[inst->jmp.imm] - emit_cnt() - 4);
      }
      break;

    case JGE:
      error("oops jge");

    case JMP:
      if (inst->jmp.type == REG) {
        error("oops");
      } else {
        emit_1(0xe9);
        emit_le(pc2addr[inst->jmp.imm] - emit_cnt() - 4);
      }
      break;

    default:
      error("oops");
  }
}

void target_x86(Module* module) {
  emit_reset();
  init_state(module->data);

  int pc_cnt = 0;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    pc_cnt++;
  }

  emit_reset();
  int* pc2addr = malloc(pc_cnt * sizeof(int));
  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      pc2addr[inst->pc] = emit_cnt();
    }
    prev_pc = inst->pc;
    emit_inst(inst, pc2addr);
  }

  emit_header(emit_cnt());

  emit_start();
  init_state(module->data);

  emit_reset();
  emit_start();
  for (Inst* inst = module->text; inst; inst = inst->next) {
    emit_inst(inst, pc2addr);
  }
}
