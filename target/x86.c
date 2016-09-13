#include <stdio.h>
#include <stdlib.h>

#include <ir/ir.h>
#include <target/elf.h>
#include <target/util.h>

static int REGNO[] = {
  0,  // A
  3,  // B
  1,  // C
  2,  // D
  5,  // BP
  7,  // SP - EDI
  6,  // ESI
  4   // ESP
};

#define EDI SP
#define ESI ((Reg)6)
#define ESP ((Reg)7)

static const int TEXT_START = 0x100000;
static const int HEADER_SIZE = 84;

static void emit_header(uint32_t filesz) {
  Elf32_Ehdr ehdr = {
    .e_ident = {
      0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01
    },
    .e_type = 2,
    .e_machine = 3,
    .e_version = 1,
    .e_entry = TEXT_START + HEADER_SIZE,
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
    .p_vaddr = TEXT_START,
    .p_paddr = TEXT_START,
    .p_filesz = filesz + HEADER_SIZE,
    .p_memsz = filesz + HEADER_SIZE,
    .p_flags = 5,
    .p_align = 0x1000
  };
  fwrite(&ehdr, 52, 1, stdout);
  fwrite(&phdr, 32, 1, stdout);
}

static void emit_int80() {
  emit_2(0xcd, 0x80);
}

static int modr(Reg dst, Reg src) {
  return 0xc0 | REGNO[dst] | (REGNO[src] << 3);
}

static void emit_reg2(Reg dst, Reg src) {
  emit_1(modr(dst, src));
}

static void emit_zero_reg(Reg r) {
  emit_2(0x31, modr(r, r));
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
    emit_2(0x39, modr(inst->dst.reg, inst->src.reg));
  } else {
    emit_2(0x81, 0xf8 | REGNO[inst->dst.reg]);
    emit_le(inst->src.imm);
  }
}

static void emit_setcc(Inst* inst, int op) {
  emit_cmp(inst);
  emit_mov_imm(inst->dst.reg, 0);
  emit_3(0x0f, op, 0xc0 | REGNO[inst->dst.reg]);
}

static void emit_jcc(Inst* inst, int op, int* pc2addr, int rodata_addr) {
  if (op) {
    emit_cmp(inst);
    emit_2(op, inst->jmp.type == REG ? 7 : 5);
  }

  if (inst->jmp.type == REG) {
    emit_3(0xff, 0x24, 0x85 | (REGNO[inst->jmp.reg] << 3));
    emit_le(rodata_addr);
  } else {
    emit_1(0xe9);
    emit_le(pc2addr[inst->jmp.imm] - emit_cnt() - 4);
  }
}

static void init_state(Data* data) {
  emit_mov_imm(B, 0);
  emit_mov_imm(C, (1 << 24) * 4);
  emit_mov_imm(D, 3);  // PROT_READ | PROT_WRITE
  emit_mov_imm(ESI, 0x22);  // MAP_PRIVATE | MAP_ANONYMOUS
  emit_mov_imm(EDI, -1);
  emit_mov_imm(BP, 0);
  emit_mov_imm(A, 192);  // mmap2
  emit_int80();

  emit_mov_reg(ESI, A);

  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      // mov dword [EAX+mp*4], data->v
      emit_2(0xc7, 0x80);
      emit_le(mp * 4);
      emit_le(data->v);
    }
  }

  emit_mov_imm(SP, 1 << 24);
  emit_zero_reg(A);
  emit_zero_reg(B);
  emit_zero_reg(C);
  emit_zero_reg(D);
  emit_zero_reg(BP);
}

static void emit_inst(Inst* inst, int* pc2addr, int rodata_addr) {
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
      emit_2(0x81, 0xe0 | REGNO[inst->dst.reg]);
      emit_le(0xffffff);
      break;

    case SUB:
      if (inst->src.type == REG) {
        emit_1(0x29);
        emit_reg2(inst->dst.reg, inst->src.reg);
      } else {
        emit_2(0x81, 0xe8 | REGNO[inst->dst.reg]);
        emit_le(inst->src.imm);
      }
      emit_2(0x81, 0xe0 | REGNO[inst->dst.reg]);
      emit_le(0xffffff);
      break;

    case LOAD:
      emit_1(0x8b);
      goto store_load_common;

    case STORE:
      emit_1(0x89);
    store_load_common:
      if (inst->src.type == REG) {
        emit_2(4 | (REGNO[inst->dst.reg] << 3),
               0x86 | (REGNO[inst->src.reg] << 3));
      } else {
        emit_1(0x86 | (REGNO[inst->dst.reg] << 3));
        emit_le(inst->src.imm * 4);
      }
      break;

    case PUTC:
      // push EDI
      emit_1(0x57);
      emit_mov(EDI, &inst->src);
      // push EAX, ECX, EDX, EBX, EDI
      emit_5(0x50, 0x51, 0x52, 0x53, 0x57);
      emit_mov_imm(B, 1);  // stdout
      emit_mov_reg(C, ESP);
      emit_mov_imm(D, 1);
      emit_mov_imm(A, 4);  // write
      emit_int80();
      // pop EDI, EBX, EDX, ECX, EAX
      emit_5(0x5f, 0x5b, 0x5a, 0x59, 0x58);
      // pop EDI
      emit_1(0x5f);
      break;

    case GETC:
      // push EDI
      emit_1(0x57);
      // push EAX, ECX, EDX, EBX
      emit_4(0x50, 0x51, 0x52, 0x53);
      // push 0
      emit_2(0x6a, 0x00);
      emit_mov_imm(B, 0);  // stdin
      emit_mov_reg(C, ESP);
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
      // pop EDI
      emit_1(0x5f);
      break;

    case EXIT:
      emit_mov_imm(B, 0);
      emit_mov_imm(A, 1);  // exit
      emit_int80();
      break;

    case DUMP:
      break;

    case EQ:
      emit_setcc(inst, 0x94);
      break;

    case NE:
      emit_setcc(inst, 0x95);
      break;

    case LT:
      emit_setcc(inst, 0x9c);
      break;

    case GT:
      emit_setcc(inst, 0x9f);
      break;

    case LE:
      emit_setcc(inst, 0x9e);
      break;

    case GE:
      emit_setcc(inst, 0x9d);
      break;

    case JEQ:
      emit_jcc(inst, 0x75, pc2addr, rodata_addr);
      break;

    case JNE:
      emit_jcc(inst, 0x74, pc2addr, rodata_addr);
      break;

    case JLT:
      emit_jcc(inst, 0x7d, pc2addr, rodata_addr);
      break;

    case JGT:
      emit_jcc(inst, 0x7e, pc2addr, rodata_addr);
      break;

    case JLE:
      emit_jcc(inst, 0x7f, pc2addr, rodata_addr);
      break;

    case JGE:
      emit_jcc(inst, 0x7c, pc2addr, rodata_addr);
      break;

    case JMP:
      emit_jcc(inst, 0, pc2addr, rodata_addr);
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

  int* pc2addr = malloc(pc_cnt * sizeof(int));
  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      pc2addr[inst->pc] = emit_cnt();
    }
    prev_pc = inst->pc;
    emit_inst(inst, pc2addr, 0);
  }

  int rodata_addr = TEXT_START + emit_cnt() + HEADER_SIZE;

  emit_header(emit_cnt() + pc_cnt * sizeof(int));

  emit_reset();
  emit_start();
  init_state(module->data);

  for (Inst* inst = module->text; inst; inst = inst->next) {
    emit_inst(inst, pc2addr, rodata_addr);
  }

  for (int i = 0; i < pc_cnt; i++) {
    emit_le(TEXT_START + pc2addr[i] + HEADER_SIZE);
  }
}
