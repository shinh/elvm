
#include <ir/ir.h>
#include <target/util.h>
#include <stdlib.h>
#include <string.h>

#define MCF_SOA "scoreboard objectives add "
#define MCF_SPG "scoreboard players get "
#define MCF_SPS "scoreboard players set "
#define MCF_SPA "scoreboard players add "
#define MCF_SPR "scoreboard players remove "
#define MCF_SPO "scoreboard players operation "
#define MCF_DMS "data modify storage "
#define MCF_DGS "data get storage "
#define MCF_DRS "data remove storage "
#define MCF_DMES "data merge storage "
#define MCF_EIS "execute if score "
#define MCF_EUS "execute unless score "
#define MCF_E "execute "
#define MCF_SRST "store result storage "
#define MCF_SRSC "store result score "
#define MCF_SFS "set from storage "
#define MCF_F "function "

static int MCF_CACHE_DEPTH = 5;
static int MCF_CACHE_SIZE;
static int MCF_CACHE_COUNT = 2;

static const char* MCF_NAMESPACE = "elvm";

static int MCF_FLUSH_CHAR = 10; // newline
static bool MCF_STDOUT_CHARS = 1;
static const char* MCF_STDOUT_CALLBACK = NULL;

static bool MCF_STDIN_WAIT = 1;
static const char* MCF_STDIN_CALLBACK = NULL;

/*
 * NBT STORAGE SLOTS:
 * All data stored in NBT storage is stored in the namespace "MCF_NAMESPACE:elvm".
 * - mem: the main memory
 * - cache0: the first cache slot
 * - cache1: the second cache slot
 * - cachen: the nth cache slot (< MCF_CACHE_COUNT)
 * - mem_tmp: temporary storage used by memory accesses
 * - stdout: standard output, modified by putc, and flushed to chat whenever a newline is printed
 * - stdin: standard input, read by getc. Must be modified externally if you want to get any use
 *          out of it.
 * - chr: the character output of a conversion from an integer ascii value to a single-character
 *        string.
 *
 * SCOREBOARD OBJECTIVES:
 * All data stored on the scoreboard are stored inside a fake player with the same name as MCF_NAMESPACE.
 * - elvm_reg_name: The register called reg_name (there's 7 of these)
 * - elvm_tmp: A temporary value used by various operations. If an operation modifies
 *             elvm_tmp, it should state so.
 * - elvm_success: A value signifying whether previous operations were successful, so
 *                 as to skip subsequent operations.
 * - elvm_mem_addr: The memory address to access in a load/store operation
 * - elvm_param: The parameter to certain operations (such as store)
 * - elvm_mem_res: The result of a load operation
 * - elvm_cachen_lo: The bottom of the address range that the nth cache slot is mapped to
 * - elvm_cachen_hi: 1 more than the top of the address range that the nth cache slot is mapped to
 * - elvm_uint_max: The constant 2^24
 * - elvm_two: The constant 2
 * - elvm_cache_size: The constant MCF_CACHE_SIZE
 * - elvm_2cache_size: The constant 2 * MCF_CACHE_SIZE
 * - elvm_getc_pc: If getc is waiting for input, stores the pc of the getc instruction to jump back to.
 *
 *
 * MEMORY MODEL:
 *
 * Main memory is stored in NBT storage with the namespace "MCF_NAMESPACE:elvm" and the NBT compound key "mem".
 * It is constructed as a binary tree, where each node is an NBT compound with optional "l" and "r"
 * keys pointing to the left and right child. The leaves are NBT integers containing the value at
 * that memory address. Each layer of the tree corresponds to a bit of the memory address, and since
 * we are using a 24 bit address space, that's a 25-deep tree (including the leaves). The most
 * significant bit is at the top of the tree; a 0 corresponds to the left child and a 1 corresponds
 * to the right child.
 *
 * On its own this would be a bit slow, as each load/store would require a search down 24 edges.
 * Instead of accessing memory directly, we copy smaller subtrees of memory to a cache slot. A cache
 * slot is mapped to an address range, and any memory access within that address range works on the
 * cache slot instead, significantly reducing the number of commands needed. If a memory access occurs
 * outside a cache slot, one of the cache slots is copied back into main memory, and the subtree
 * corresponding to the new address will be copied into a cache slot, before the actual memory access
 * occurs on this cache slot. While an address range is being cached, the values in main memory for
 * that range may be outdated. The cache slots are stored in NBT storage with the namespace
 * "MCF_NAMESPACE:elvm" and the NBT compound key "cache%d" where "%d" is the cache slot ID.
 *
 * To load a value from a cached slot, we recursively copy either the left or right subtree into
 * temporary memory, which is initialized to a set of {} so that in case the memory is asbent, it
 * will still work properly. This process is called "unwinding" the tree.
 * To store a value to a cached slot, we first unwind the tree, then store the value into the 1-deep
 * subtree, then recursively copy the subtrees back into their parents. This process is called
 * "rewinding" the tree, and emulates the behaviour of the "data merge" command but for a dynamic
 * value.
 *
 * COMMAND LINE OPTIONS:
 * - cache_count: the number of cache slots
 * - cache_depth: the depth of a cache subtree. The address range of a cache is 2^cache_depth
 * - namespace: the namespace everything is done in. A program with a different namespace can run at the
 *              same time and independently of other programs.
 * - flush_char: the character which flushes stdout, defaults to 10 ('\n'). Set to -1 for no auto-flushing.
 * - stdout_chars: whether stdout is a stream made up of characters (true, default), or integers (false).
 * - stdout_callback: a function to be called directly after something is added to stdout. The integer value
 *                    will be stored in elvm_param, and (if applicable) the character value will be stored in
 *                    NBT storage chr.
 * - stdin_wait: whether to wait for more input when stdin is empty and a getc instruction is reached
 *               (default = true). If false, the behaviour of getc when stdin is empty is undefined.
 * - stdin_callback: a function to be called directly at the start of a getc instruction. May be used to put
 *                   values in stdin before they are read.
 *
 */

static const char* MCF_REG_NAMES[7] = { "elvm_a", "elvm_b", "elvm_c", "elvm_d", "elvm_bp", "elvm_sp", "elvm_pc" };

typedef struct {
  /* keep track of whether we use certain functions, to omit them in the output to make the output smaller */
  bool used_dyn_int2chr;
  bool used_flush_function;
  bool used_dyn_load;
  bool used_dyn_store;
  bool used_getc_function;

  /*
   * Whether the last instruction was a jump instruction, used to determine whether the default jump
   * to the adj pc should be inserted
   */
  bool was_jump;
  /*
   * An array of the "next" pc after the current block. Usually the current pc + 1, but may be different
   * once we rearrange the control flow graph
   */
  int* adj_pcs;
} MCFGen;

static MCFGen mcf;

static void mcf_char_to_string(signed char c, char* out) {
  if ((c >= 0 && c < 32) || c == 127)
    c = ' ';

  if (c == '"' || c == '\\') {
    out[0] = '\\';
    out[1] = c;
    out[2] = '\0';
  } else if (!(c & 0x80)) {
    out[0] = c;
    out[1] = '\0';
  } else {
    out[0] = (char)0xc0 | (char)(((unsigned)c & 0xff) >> 6);
    out[1] = (char)0x80 | (c & (char)0x3f);
    out[2] = '\0';
  }
}

/* Gets a chain of mem_name.l.l.l...r.r.l corresponding to address loc with mem_size bits and stores it in res */
static void mcf_static_read_mem_loc(int loc, const char* mem_name, char* res, int mem_size) {
  strcpy(res, mem_name);
  for (int i = 0; i < mem_size; i++) {
    if (loc & (1 << (mem_size - 1 - i)))
      strcat(res, ".r");
    else
      strcat(res, ".l");
  }
}

/*
 * Gets a chain of {"mem_name":{"l":{"l":{"l":{...:{"r":{"r":{"r":value}}}...}}}}} corresponding to address
 * loc with mem_size bits and stores it in res
 */
static void mcf_static_write_mem_loc(int loc, const char* mem_name, const char* value, char* res, int mem_size) {
  strcpy(res, "{\"");
  strcat(res, mem_name);
  strcat(res, "\":");
  for (int i = 0; i < mem_size; i++) {
    if (loc & (1 << (mem_size - 1 - i)))
      strcat(res, "{\"r\":");
    else
      strcat(res, "{\"l\":");
  }
  strcat(res, value);
  for (int i = 0; i <= mem_size; i++)
    strcat(res, "}");
}

static void mcf_emit_function_header(const char* name) {
  emit_line("========= %s.mcfunction =========", name);
  emit_line("# %s.mcfunction: Generated by ELVM", name);
}

/* Sets the value of the register reg to value, if we don't know whether value is a constant or another register */
static void mcf_emit_set_reg(const char* reg, Value* value) {
  if (value->type == REG)
    emit_line(MCF_SPO "%s %s = %s %s", MCF_NAMESPACE, reg, MCF_NAMESPACE, reg_names[value->reg]);
  else if (value->type == IMM)
    emit_line(MCF_SPS "%s %s %d", MCF_NAMESPACE, reg, value->imm);
  else
    error("invalid value");
}

static void mcf_emit_mem_table_store(Value* addr, Value* value) {
  if (addr->type == IMM) {
    /* static address */
    char write_mem_loc[164];
    if (value->type == IMM) {
      /* static value */
      /* put value in main memory */
      mcf_static_write_mem_loc(addr->imm, "mem", format("%d", value->imm), write_mem_loc, 24);
      emit_line(MCF_DMES "%s:elvm %s", MCF_NAMESPACE, write_mem_loc);
      /* put value in caches if need be */
      for (int i = 0; i < MCF_CACHE_COUNT; i++) {
        mcf_static_write_mem_loc(addr->imm, format("cache%d", i), format("%d", value->imm), write_mem_loc,
                                 MCF_CACHE_DEPTH);
        emit_line(MCF_EIS "%s elvm_cache%d_lo matches %d if score %s elvm_cache%d_hi matches %d run " MCF_DMES "%s:elvm %s",
                  MCF_NAMESPACE, i, addr->imm & ~(MCF_CACHE_SIZE - 1), MCF_NAMESPACE, i,
                  (addr->imm & ~(MCF_CACHE_SIZE - 1)) + MCF_CACHE_SIZE, MCF_NAMESPACE, write_mem_loc);
      }
    } else {
      /* dynamic value */
      /* put value in main memory. Merge in 0 first to ensure the path exists in the tree */
      mcf_static_write_mem_loc(addr->imm, "mem", "0", write_mem_loc, 24);
      emit_line(MCF_DMES "%s:elvm %s", MCF_NAMESPACE, write_mem_loc);
      mcf_static_read_mem_loc(addr->imm, "mem", write_mem_loc, 24);
      emit_line(MCF_E MCF_SRST "%s:elvm %s int 1 run " MCF_SPG "%s %s",
                MCF_NAMESPACE, write_mem_loc, MCF_NAMESPACE, reg_names[value->reg]);
      /* put value in caches if need be, in the same way */
      for (int i = 0; i < MCF_CACHE_COUNT; i++) {
        mcf_static_write_mem_loc(addr->imm, format("cache%d", i), "0", write_mem_loc, MCF_CACHE_DEPTH);
        emit_line(MCF_EIS "%s elvm_cache%d_lo matches %d if score %s elvm_cache%d_hi matches %d run " MCF_DMES "%s:elvm %s",
                  MCF_NAMESPACE, i, addr->imm & ~(MCF_CACHE_SIZE - 1), MCF_NAMESPACE, i,
                  (addr->imm & ~(MCF_CACHE_SIZE - 1)) + MCF_CACHE_SIZE, MCF_NAMESPACE, write_mem_loc);
        mcf_static_read_mem_loc(addr->imm, format("cache%d", i), write_mem_loc, MCF_CACHE_DEPTH);
        emit_line(
            MCF_EIS "%s elvm_cache%d_lo matches %d if score %s elvm_cache%d_hi matches %d " MCF_SRST "%s:elvm %s int 1 run " MCF_SPG "%s %s",
            MCF_NAMESPACE, i, addr->imm & ~(MCF_CACHE_SIZE - 1), MCF_NAMESPACE, i,
            (addr->imm & ~(MCF_CACHE_SIZE - 1)) + MCF_CACHE_SIZE, MCF_NAMESPACE, write_mem_loc, MCF_NAMESPACE,
            reg_names[value->reg]);
      }
    }
  } else {
    /* dynamic address */
    mcf.used_dyn_store = 1;
    mcf_emit_set_reg("elvm_mem_addr", addr);
    mcf_emit_set_reg("elvm_param", value);
    /* try storing in each cache in turn. The elvm:storedcachedn function sets elvm_success to 1 */
    emit_line(MCF_SPS "%s elvm_success 0", MCF_NAMESPACE);
    emit_line(MCF_EIS "%s elvm_mem_addr >= %s elvm_cache0_lo if score %s elvm_mem_addr < %s elvm_cache0_hi run " MCF_F "%s:storecached0",
              MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE);
    for (int i = 1; i < MCF_CACHE_COUNT; i++)
      emit_line(MCF_EIS "%s elvm_success matches 0 if score %s elvm_mem_addr >= %s elvm_cache%d_lo if score %s elvm_mem_addr < %s elvm_cache%d_hi run " MCF_F "%s:storecached%d",
          MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE, i, MCF_NAMESPACE, MCF_NAMESPACE, i, MCF_NAMESPACE, i);
    /* if no caches cover this memory address, do a recache, which frees up cache slot 0, then store in cache 0 */
    emit_line(MCF_EIS "%s elvm_success matches 0 run " MCF_F "%s:recache", MCF_NAMESPACE, MCF_NAMESPACE);
    emit_line(MCF_EIS "%s elvm_success matches 0 run " MCF_F "%s:storecached0", MCF_NAMESPACE, MCF_NAMESPACE);
  }
}

static void mcf_emit_mem_table_load(const char* dst_reg, Value* addr) {
  if (addr->type == IMM) {
    /* static address */
    /* try loading from main memory. Minecraft conveniently returns 0 if the memory path doesn't exist */
    char read_mem_loc[52];
    mcf_static_read_mem_loc(addr->imm, "mem", read_mem_loc, 24);
    emit_line(MCF_E MCF_SRSC "%s %s run " MCF_DGS "%s:elvm %s", MCF_NAMESPACE, dst_reg, MCF_NAMESPACE, read_mem_loc);
    /* try loading from each of the caches, overwriting the value from main memory if necesary */
    for (int i = 0; i < MCF_CACHE_COUNT; i++) {
      mcf_static_read_mem_loc(addr->imm, format("cache%d", i), read_mem_loc, MCF_CACHE_DEPTH);
      emit_line(
          MCF_EIS "%s elvm_cache%d_lo matches %d if score %s elvm_cache%d_hi matches %d " MCF_SRSC "%s %s run " MCF_DGS "%s:elvm %s",
          MCF_NAMESPACE, i, addr->imm & ~(MCF_CACHE_SIZE - 1), MCF_NAMESPACE, i,
          (addr->imm & ~(MCF_CACHE_SIZE - 1)) + MCF_CACHE_SIZE, MCF_NAMESPACE, dst_reg, MCF_NAMESPACE, read_mem_loc);
    }
  } else {
    /* dynamic address */
    mcf.used_dyn_load = 1;
    mcf_emit_set_reg("elvm_mem_addr", addr);
    /* try loading from each cache in turn. The elvm:loadcachedn function sets elvm_success to 1 */
    emit_line(MCF_SPS "%s elvm_success 0", MCF_NAMESPACE);
    emit_line(MCF_EIS "%s elvm_mem_addr >= %s elvm_cache0_lo if score %s elvm_mem_addr < %s elvm_cache0_hi run " MCF_F "%s:loadcached0",
        MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE);
    for (int i = 1; i < MCF_CACHE_COUNT; i++)
      emit_line(MCF_EIS "%s elvm_success matches 0 if score %s elvm_mem_addr >= %s elvm_cache%d_lo if score %s elvm_mem_addr < %s elvm_cache%d_hi run " MCF_F "%s:loadcached%d",
          MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE, i, MCF_NAMESPACE, MCF_NAMESPACE, i, MCF_NAMESPACE, i);
    /* if no caches cover this memory address, do a recache, which frees up cache slot 0, then load from cache 0 */
    emit_line(MCF_EIS "%s elvm_success matches 0 run " MCF_F "%s:recache", MCF_NAMESPACE, MCF_NAMESPACE);
    emit_line(MCF_EIS "%s elvm_success matches 0 run " MCF_F "%s:loadcached0", MCF_NAMESPACE, MCF_NAMESPACE);
    emit_line(MCF_SPO "%s %s = %s elvm_mem_res", MCF_NAMESPACE, dst_reg, MCF_NAMESPACE);
  }
}

/*
 * Emit a function which stores the value in elvm_param to address elvm_mem_addr, in cache slot cache_id.
 * Modifies elvm_tmp in the process
 */
static void mcf_emit_storecached_function(int cache_id) {
  mcf_emit_function_header(format("%s:storecached%d", MCF_NAMESPACE, cache_id));

  emit_line(MCF_SPS "%s elvm_success 1", MCF_NAMESPACE);

  /* make the address relative to the start of the mapped memory */
  emit_line(MCF_SPO "%s elvm_mem_addr -= %s elvm_cache%d_lo", MCF_NAMESPACE, MCF_NAMESPACE, cache_id);

  /* initialize temporary array */
  char* mem_val = (char*) malloc(3 * (MCF_CACHE_DEPTH - 1) * sizeof(char));
  strcpy(mem_val, "{}");
  for (int i = 1; i < MCF_CACHE_DEPTH - 1; i++)
    strcat(mem_val, ",{}");
  emit_line(MCF_DMS "%s:elvm mem_tmp set value [%s]", MCF_NAMESPACE, mem_val);
  free(mem_val);

  /* unwind the tree */
  emit_line(MCF_SPO "%s elvm_tmp = %s elvm_mem_addr", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EIS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[0] " MCF_SFS "%s:elvm cache%d.l",
            MCF_NAMESPACE, MCF_CACHE_SIZE / 2 - 1, MCF_NAMESPACE, MCF_NAMESPACE, cache_id);
  emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[0] " MCF_SFS "%s:elvm cache%d.r",
            MCF_NAMESPACE, MCF_CACHE_SIZE / 2 - 1, MCF_NAMESPACE, MCF_NAMESPACE, cache_id);
  emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_SPR "%s elvm_tmp %d",
            MCF_NAMESPACE, MCF_CACHE_SIZE / 2 - 1, MCF_NAMESPACE, MCF_CACHE_SIZE / 2);
  for (int i = 1; i < MCF_CACHE_DEPTH - 1; i++) {
    emit_line(MCF_EIS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d] " MCF_SFS "%s:elvm mem_tmp[%d].l",
              MCF_NAMESPACE, (1 << (MCF_CACHE_DEPTH - 1 - i)) - 1, MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d] " MCF_SFS "%s:elvm mem_tmp[%d].r",
              MCF_NAMESPACE, (1 << (MCF_CACHE_DEPTH - 1 - i)) - 1, MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_SPR "%s elvm_tmp %d",
              MCF_NAMESPACE, (1 << (MCF_CACHE_DEPTH - 1 - i)) - 1, MCF_NAMESPACE, 1 << (MCF_CACHE_DEPTH - 1 - i));
  }

  /* store the value */
  emit_line(MCF_EIS "%s elvm_tmp matches 0 " MCF_SRST "%s:elvm mem_tmp[%d].l int 1 run " MCF_SPG "%s elvm_param",
            MCF_NAMESPACE, MCF_NAMESPACE, MCF_CACHE_DEPTH - 2, MCF_NAMESPACE);
  emit_line(MCF_EUS "%s elvm_tmp matches 0 " MCF_SRST "%s:elvm mem_tmp[%d].r int 1 run " MCF_SPG "%s elvm_param",
            MCF_NAMESPACE, MCF_NAMESPACE, MCF_CACHE_DEPTH - 2, MCF_NAMESPACE);

  /* rewind the tree */
  for (int i = MCF_CACHE_DEPTH - 2; i >= 1; i--) {
    emit_line(MCF_SPO "%s elvm_mem_addr /= %s elvm_two", MCF_NAMESPACE, MCF_NAMESPACE);
    emit_line(MCF_SPO "%s elvm_tmp = %s elvm_mem_addr", MCF_NAMESPACE, MCF_NAMESPACE);
    emit_line(MCF_SPO "%s elvm_tmp %%= %s elvm_two", MCF_NAMESPACE, MCF_NAMESPACE);
    emit_line(MCF_EIS "%s elvm_tmp matches 0 run " MCF_DMS "%s:elvm mem_tmp[%d].l " MCF_SFS "%s:elvm mem_tmp[%d]",
              MCF_NAMESPACE, MCF_NAMESPACE, i - 1, MCF_NAMESPACE, i);
    emit_line(MCF_EUS "%s elvm_tmp matches 0 run " MCF_DMS "%s:elvm mem_tmp[%d].r " MCF_SFS "%s:elvm mem_tmp[%d]",
              MCF_NAMESPACE, MCF_NAMESPACE, i - 1, MCF_NAMESPACE, i);
  }
  emit_line(MCF_SPO "%s elvm_mem_addr /= %s elvm_two", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_SPO "%s elvm_mem_addr %%= %s elvm_two", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EIS "%s elvm_mem_addr matches 0 run " MCF_DMS "%s:elvm cache%d.l " MCF_SFS "%s:elvm mem_tmp[0]",
            MCF_NAMESPACE, MCF_NAMESPACE, cache_id, MCF_NAMESPACE);
  emit_line(MCF_EUS "%s elvm_mem_addr matches 0 run " MCF_DMS "%s:elvm cache%d.r " MCF_SFS "%s:elvm mem_tmp[0]",
            MCF_NAMESPACE, MCF_NAMESPACE, cache_id, MCF_NAMESPACE);
}

/*
 * Emit a function that loads the value at address elvm_mem_addr, in cache slot cache_id, and puts it in elvm_mem_res.
 * Modifies elvm_tmp in the process.
 */
static void mcf_emit_loadcached_function(int cache_id) {
  mcf_emit_function_header(format("%s:loadcached%d", MCF_NAMESPACE, cache_id));

  emit_line(MCF_SPS "%s elvm_success 1", MCF_NAMESPACE);

  /* make the address relative to the start of the mapped memory */
  emit_line(MCF_SPO "%s elvm_mem_addr -= %s elvm_cache%d_lo", MCF_NAMESPACE, MCF_NAMESPACE, cache_id);

  /* initialize temporary array */
  char* mem_val = (char*) malloc(3 * (MCF_CACHE_DEPTH - 1) * sizeof(char));
  strcpy(mem_val, "{}");
  for (int i = 1; i < MCF_CACHE_DEPTH - 1; i++)
    strcat(mem_val, ",{}");
  emit_line(MCF_DMS "%s:elvm mem_tmp set value [%s]", MCF_NAMESPACE, mem_val);
  free(mem_val);

  /* unwind the tree */
  emit_line(MCF_EIS "%s elvm_mem_addr matches ..%d run " MCF_DMS "%s:elvm mem_tmp[0] " MCF_SFS "%s:elvm cache%d.l",
            MCF_NAMESPACE, MCF_CACHE_SIZE / 2 - 1, MCF_NAMESPACE, MCF_NAMESPACE, cache_id);
  emit_line(MCF_EUS "%s elvm_mem_addr matches ..%d run " MCF_DMS "%s:elvm mem_tmp[0] " MCF_SFS "%s:elvm cache%d.r",
            MCF_NAMESPACE, MCF_CACHE_SIZE / 2 - 1, MCF_NAMESPACE, MCF_NAMESPACE, cache_id);
  emit_line(MCF_EUS "%s elvm_mem_addr matches ..%d run " MCF_SPR "%s elvm_mem_addr %d",
            MCF_NAMESPACE, MCF_CACHE_SIZE / 2 - 1, MCF_NAMESPACE, MCF_CACHE_SIZE / 2);
  for (int i = 1; i < MCF_CACHE_DEPTH - 1; i++) {
    emit_line(MCF_EIS "%s elvm_mem_addr matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d] " MCF_SFS "%s:elvm mem_tmp[%d].l",
              MCF_NAMESPACE, (1 << (MCF_CACHE_DEPTH - 1 - i)) - 1, MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_EUS "%s elvm_mem_addr matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d] " MCF_SFS "%s:elvm mem_tmp[%d].r",
              MCF_NAMESPACE, (1 << (MCF_CACHE_DEPTH - 1 - i)) - 1, MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_EUS "%s elvm_mem_addr matches ..%d run " MCF_SPR "%s elvm_mem_addr %d",
              MCF_NAMESPACE, (1 << (MCF_CACHE_DEPTH - 1 - i)) - 1, MCF_NAMESPACE, 1 << (MCF_CACHE_DEPTH - 1 - i));
  }

  /* get the value and put it in elvm_mem_res */
  emit_line(MCF_EIS "%s elvm_mem_addr matches 0 " MCF_SRSC "%s elvm_mem_res run " MCF_DGS "%s:elvm mem_tmp[%d].l",
            MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE, MCF_CACHE_DEPTH - 2);
  emit_line(MCF_EUS "%s elvm_mem_addr matches 0 " MCF_SRSC "%s elvm_mem_res run " MCF_DGS "%s:elvm mem_tmp[%d].r",
            MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE, MCF_CACHE_DEPTH - 2);
}

/*
 * Stores the final cache back into main memory, shifts the cache slots up by 1, and frees up cache slot 0.
 * Modifies elvm_tmp in the process.
 */
static void mcf_emit_recache_function() {
  mcf_emit_function_header(format("%s:recache", MCF_NAMESPACE));

  /* Store the last cache back into main memory */

  /* initialize temporary array */
  char* mem_val = (char*) malloc(3 * (23 - MCF_CACHE_DEPTH) * sizeof(char));
  strcpy(mem_val, "{}");
  for (int i = 1; i < 23 - MCF_CACHE_DEPTH; i++)
    strcat(mem_val, ",{}");
  emit_line(MCF_DMS "%s:elvm mem_tmp set value [%s]", MCF_NAMESPACE, mem_val);
  /* unwind main memory down to the cached layer */
  emit_line(MCF_SPO "%s elvm_tmp = %s elvm_cache%d_lo", MCF_NAMESPACE, MCF_NAMESPACE, MCF_CACHE_COUNT - 1);
  emit_line(MCF_EIS "%s elvm_tmp matches ..8388607 run " MCF_DMS "%s:elvm mem_tmp[0] " MCF_SFS "%s:elvm mem.l",
            MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EUS "%s elvm_tmp matches ..8388607 run " MCF_DMS "%s:elvm mem_tmp[0] " MCF_SFS "%s:elvm mem.r",
            MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EUS "%s elvm_tmp matches ..8388607 run " MCF_SPR "%s elvm_tmp 8388608", MCF_NAMESPACE, MCF_NAMESPACE);
  for (int i = 1; i < 23 - MCF_CACHE_DEPTH; i++) {
    emit_line(MCF_EIS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d] " MCF_SFS "%s:elvm mem_tmp[%d].l",
              MCF_NAMESPACE, (1 << (23 - i)) - 1, MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d] " MCF_SFS "%s:elvm mem_tmp[%d].r",
              MCF_NAMESPACE, (1 << (23 - i)) - 1, MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_SPR "%s elvm_tmp %d",
              MCF_NAMESPACE, (1 << (23 - i)) - 1, MCF_NAMESPACE, 1 << (23 - i));
  }
  /* put the cache there */
  emit_line(MCF_EIS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d].l " MCF_SFS "%s:elvm cache%d",
            MCF_NAMESPACE, MCF_CACHE_SIZE - 1, MCF_NAMESPACE, 22 - MCF_CACHE_DEPTH, MCF_NAMESPACE, MCF_CACHE_COUNT - 1);
  emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d].r " MCF_SFS "%s:elvm cache%d",
            MCF_NAMESPACE, MCF_CACHE_SIZE - 1, MCF_NAMESPACE, 22 - MCF_CACHE_DEPTH, MCF_NAMESPACE, MCF_CACHE_COUNT - 1);
  /* rewind main memory */
  for (int i = 22 - MCF_CACHE_DEPTH; i >= 1; i--) {
    emit_line(MCF_SPO "%s elvm_cache%d_lo /= %s %s",
              MCF_NAMESPACE, MCF_CACHE_COUNT - 1, MCF_NAMESPACE,
              i == 22 - MCF_CACHE_DEPTH ? "elvm_2cache_size" : "elvm_two");
    emit_line(MCF_SPO "%s elvm_tmp = %s elvm_cache%d_lo", MCF_NAMESPACE, MCF_NAMESPACE, MCF_CACHE_COUNT - 1);
    emit_line(MCF_SPO "%s elvm_tmp %%= %s elvm_two", MCF_NAMESPACE, MCF_NAMESPACE);
    emit_line(MCF_EIS "%s elvm_tmp matches 0 run " MCF_DMS "%s:elvm mem_tmp[%d].l " MCF_SFS "%s:elvm mem_tmp[%d]",
              MCF_NAMESPACE, MCF_NAMESPACE, i - 1, MCF_NAMESPACE, i);
    emit_line(MCF_EUS "%s elvm_tmp matches 0 run " MCF_DMS "%s:elvm mem_tmp[%d].r " MCF_SFS "%s:elvm mem_tmp[%d]",
              MCF_NAMESPACE, MCF_NAMESPACE, i - 1, MCF_NAMESPACE, i);
  }
  emit_line(MCF_SPO "%s elvm_cache%d_lo /= %s elvm_two", MCF_NAMESPACE, MCF_CACHE_COUNT - 1, MCF_NAMESPACE);
  emit_line(MCF_SPO "%s elvm_cache%d_lo %%= %s elvm_two", MCF_NAMESPACE, MCF_CACHE_COUNT - 1, MCF_NAMESPACE);
  emit_line(MCF_EIS "%s elvm_cache%d_lo matches 0 run " MCF_DMS "%s:elvm mem.l " MCF_SFS "%s:elvm mem_tmp[0]",
            MCF_NAMESPACE, MCF_CACHE_COUNT - 1, MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EUS "%s elvm_cache%d_lo matches 0 run " MCF_DMS "%s:elvm mem.r " MCF_SFS "%s:elvm mem_tmp[0]",
            MCF_NAMESPACE, MCF_CACHE_COUNT - 1, MCF_NAMESPACE, MCF_NAMESPACE);

  /* Shift the cache slots up by 1 */

  for (int i = MCF_CACHE_COUNT - 1; i > 0; i--) {
    emit_line(MCF_DMS "%s:elvm cache%d " MCF_SFS "%s:elvm cache%d", MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_SPO "%s elvm_cache%d_lo = %s elvm_cache%d_lo", MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_SPO "%s elvm_cache%d_hi = %s elvm_cache%d_hi", MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
  }

  /* Establish cache slot 0's address range */

  emit_line(MCF_SPO "%s elvm_cache0_lo = %s elvm_mem_addr", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_SPO "%s elvm_cache0_lo /= %s elvm_cache_size", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_SPO "%s elvm_cache0_lo *= %s elvm_cache_size", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_SPO "%s elvm_cache0_hi = %s elvm_cache0_lo", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_SPA "%s elvm_cache0_hi %d", MCF_NAMESPACE, MCF_CACHE_SIZE);
  emit_line(MCF_DMS "%s:elvm cache0 set value {}", MCF_NAMESPACE);

  /* Load cache slot 0 from main memory */

  /* initialize temporary array */
  emit_line(MCF_DMS "%s:elvm mem_tmp set value [%s]", MCF_NAMESPACE, mem_val);
  free(mem_val);
  /* unwind main memory down to cache layer */
  emit_line(MCF_SPO "%s elvm_tmp = %s elvm_mem_addr", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EIS "%s elvm_tmp matches ..8388607 run " MCF_DMS "%s:elvm mem_tmp[0] " MCF_SFS "%s:elvm mem.l",
            MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EUS "%s elvm_tmp matches ..8388607 run " MCF_DMS "%s:elvm mem_tmp[0] " MCF_SFS "%s:elvm mem.r",
            MCF_NAMESPACE, MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EUS "%s elvm_tmp matches ..8388607 run " MCF_SPR "%s elvm_tmp 8388608", MCF_NAMESPACE, MCF_NAMESPACE);
  for (int i = 1; i < 23 - MCF_CACHE_DEPTH; i++) {
    emit_line(MCF_EIS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d] " MCF_SFS "%s:elvm mem_tmp[%d].l",
              MCF_NAMESPACE, (1 << (23 - i)) - 1, MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm mem_tmp[%d] " MCF_SFS "%s:elvm mem_tmp[%d].r",
              MCF_NAMESPACE, (1 << (23 - i)) - 1, MCF_NAMESPACE, i, MCF_NAMESPACE, i - 1);
    emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_SPR "%s elvm_tmp %d",
              MCF_NAMESPACE, (1 << (23 - i)) - 1, MCF_NAMESPACE, 1 << (23 - i));
  }
  /* put it in cache0 */
  emit_line(MCF_EIS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm cache0 " MCF_SFS "%s:elvm mem_tmp[%d].l",
            MCF_NAMESPACE, MCF_CACHE_SIZE - 1, MCF_NAMESPACE, MCF_NAMESPACE, 22 - MCF_CACHE_DEPTH);
  emit_line(MCF_EUS "%s elvm_tmp matches ..%d run " MCF_DMS "%s:elvm cache0 " MCF_SFS "%s:elvm mem_tmp[%d].r",
            MCF_NAMESPACE, MCF_CACHE_SIZE - 1, MCF_NAMESPACE, MCF_NAMESPACE, 22 - MCF_CACHE_DEPTH);
}

/*
 * Recursively emits functions which take the value in elvm_param and stores the character it
 * represents in NBT storage slot chr. Works if min <= elvm_param < max
 */
static void mcf_emit_chr_function(int min, int max) {
  int range = max - min;
  int mid = min + range / 2;
  if (range == 256)
    mcf_emit_function_header(format("%s:chr", MCF_NAMESPACE));
  else
    mcf_emit_function_header(format("%s:chr_%d_%d", MCF_NAMESPACE, min, max));

  if (range == 2) {
    /* emit leaf nodes */
    char ch[3];
    mcf_char_to_string((signed char) min, ch);
    emit_line(MCF_EIS "%s elvm_param matches %d run " MCF_DMS "%s:elvm chr set value \"%s\"",
              MCF_NAMESPACE, min, MCF_NAMESPACE, ch);
    mcf_char_to_string((signed char) mid, ch);
    emit_line(MCF_EIS "%s elvm_param matches %d run " MCF_DMS "%s:elvm chr set value \"%s\"",
              MCF_NAMESPACE, mid, MCF_NAMESPACE, ch);
  } else {
    /* emit sub-function calls, and recursively emit the corresponding sub-functions */

    /* everything <= 32 will be a space anyway, so there's no need to search the subtree */
    if (mid <= 32)
      emit_line(MCF_EIS "%s elvm_param matches %d..%d run " MCF_DMS "%s:elvm chr set value \" \"",
                MCF_NAMESPACE, min, mid - 1, MCF_NAMESPACE);
    else
      emit_line(MCF_EIS "%s elvm_param matches %d..%d run " MCF_F "%s:chr_%d_%d",
                MCF_NAMESPACE, min, mid - 1, MCF_NAMESPACE, min, mid);
    emit_line(MCF_EIS "%s elvm_param matches %d..%d run " MCF_F "%s:chr_%d_%d",
              MCF_NAMESPACE, mid, max - 1, MCF_NAMESPACE, mid, max);
    if (mid > 32)
      mcf_emit_chr_function(min, mid);
    mcf_emit_chr_function(mid, max);
  }
}

/* Gets the json text component that can print len characters from stdout. Stores the result in out */
static char* mcf_get_json_string(int len) {
  const char* out = "[";
  for (int j = 0; j < len; j++) {
    out = format("%s%s{\"storage\":\"%s:elvm\",\"nbt\":\"stdout[%d]\"}", out, j == 0 ? "" : ",", MCF_NAMESPACE, j);
  }
  return format("%s]", out);
}

/*
 * Recursively emits a function which will print between min (inclusive) and max (exclusive) from stdout
 * to chat. Expects the length of stdout to be in elvm_tmp
 */
static void mcf_emit_flush_function_recursive(int min, int max) {
  int range = max - min;
  int mid = min + range / 2;

  mcf_emit_function_header(format("%s:flush_%d_%d", MCF_NAMESPACE, min, max));

  if (min == 0 && range == 4) {
    /* 1-3 characters (no need to do anything about 0 characters) */
    for (int i = 1; i < 4; i++) {
      const char* json_string = mcf_get_json_string(i);
      emit_line(MCF_EIS "%s elvm_tmp matches %d run tellraw @a %s", MCF_NAMESPACE, i, json_string);
    }
  } else if (range == 2) {
    /* leaf nodes */
    const char* json_string = mcf_get_json_string(min);
    emit_line(MCF_EIS "%s elvm_tmp matches %d run tellraw @a %s", MCF_NAMESPACE, min, json_string);
    json_string = mcf_get_json_string(mid);
    emit_line(MCF_EIS "%s elvm_tmp matches %d run tellraw @a %s", MCF_NAMESPACE, mid, json_string);
  } else {
    /* call sub-functions and recursively emit them */
    emit_line(MCF_EIS "%s elvm_tmp matches %d..%d run " MCF_F "%s:flush_%d_%d",
              MCF_NAMESPACE, min, mid - 1, MCF_NAMESPACE, min, mid);
    emit_line(MCF_EUS "%s elvm_tmp matches %d..%d run " MCF_F "%s:flush_%d_%d",
              MCF_NAMESPACE, min, mid - 1, MCF_NAMESPACE, mid, max);
    mcf_emit_flush_function_recursive(min, mid);
    mcf_emit_flush_function_recursive(mid, max);
  }
}

/*
 * Emit a function that removes the first 255 characters from stdout
 * (used if there were more than 255 characters that need flushing)
 */
static void mcf_emit_flush256() {
  mcf_emit_function_header(format("%s:flush256", MCF_NAMESPACE));
  emit_line(MCF_DRS "%s:elvm stdout[0]", MCF_NAMESPACE);
  emit_line(MCF_SPA "%s elvm_tmp 1", MCF_NAMESPACE);
  emit_line(MCF_EIS "%s elvm_tmp matches ..509 run " MCF_F "%s:flush256", MCF_NAMESPACE, MCF_NAMESPACE);
}

/* Takes the list of characters in the stdout buffer and prints them to chat. Modifies elvm_tmp */
static void mcf_emit_flush_function() {
  mcf_emit_function_header(format("%s:flush", MCF_NAMESPACE));

  /* get the length of stdout */
  emit_line(MCF_E MCF_SRSC "%s elvm_tmp run " MCF_DGS "%s:elvm stdout", MCF_NAMESPACE, MCF_NAMESPACE);
  /* flush up to 255 characters from stdout */
  emit_line(MCF_EIS "%s elvm_tmp matches 255.. run " MCF_SPS "%s elvm_tmp 255", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_F "%s:flush_0_256", MCF_NAMESPACE);
  /* if there were 255 or more characters in stdout, remove them and keep flushing again until stdout is empty */
  emit_line(MCF_EIS "%s elvm_tmp matches 255 run " MCF_F "%s:flush256", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_EIS "%s elvm_tmp matches 255.. run " MCF_F "%s:flush", MCF_NAMESPACE, MCF_NAMESPACE);
  /* empty stdout */
  emit_line(MCF_DMS "%s:elvm stdout set value []", MCF_NAMESPACE);

  mcf_emit_flush256();
  mcf_emit_flush_function_recursive(0, 256);
}

static void mcf_emit_getc_reenter_function() {
  mcf_emit_function_header(format("%s:getc_reenter", MCF_NAMESPACE));
  emit_line(MCF_SPO "%s elvm_pc = %s elvm_getc_pc", MCF_NAMESPACE, MCF_NAMESPACE);
  emit_line(MCF_F "%s:loop", MCF_NAMESPACE);
}


/*
 * Emits commands emulating the test performed by inst, and runs the commands in cmds_when_false if
 * the test evaluated to false, and the commands in cmds_when_true otherwise.
 */
static void mcf_emit_test(Inst* inst,
                          int num_commands_when_false, const char** cmds_when_false,
                          int num_commands_when_true, const char** cmds_when_true,
                          bool true_first) {
  /*
   * normalize EQ, NE, GT etc to JEQ, JNE, JGT. Also, JNE, JLT and JGT are the inverse of JEQ, JGE and JLE,
   * so no need to write twice the amount of code
   */
  bool inverted = 0;
  Op op = normalize_cond(inst->op, 0);
  if (op == JNE || op == JLT || op == JGT) {
    inverted = 1;
    op = normalize_cond(op, 1);
  }

  /* simple way of applying the inversion */
  const char* if_ = inverted ? "unless" : "if";
  const char* unless = inverted ? "if" : "unless";


  const char* false_test;
  const char* true_test;
  if (inst->src.type == IMM) {
    /* we're comparing against a static value */
    const char* left_dots = op == JLE ? ".." : "";
    const char* right_dots = op == JGE ? ".." : "";
    false_test = format(MCF_E "%s score %s %s matches %s%d%s run ",
                        unless, MCF_NAMESPACE, reg_names[inst->dst.reg], left_dots, inst->src.imm, right_dots);
    true_test = format(MCF_E "%s score %s %s matches %s%d%s run ",
                       if_, MCF_NAMESPACE, reg_names[inst->dst.reg], left_dots, inst->src.imm, right_dots);
  } else {
    /* we're comparing against a dynamic value */
    const char* op_str = op == JEQ ? "=" : op == JLE ? "<=" : ">=";
    false_test = format(MCF_E "%s score %s %s %s %s %s run ",
                        unless, MCF_NAMESPACE, reg_names[inst->dst.reg], op_str, MCF_NAMESPACE, reg_names[inst->src.reg]);
    true_test = format(MCF_E "%s score %s %s %s %s %s run ",
                       if_, MCF_NAMESPACE, reg_names[inst->dst.reg], op_str, MCF_NAMESPACE, reg_names[inst->src.reg]);
  }


  if (true_first) {
    for (int i = 0; i < num_commands_when_true; i++)
      emit_line("%s%s", true_test, cmds_when_true[i]);
  }
  for (int i = 0; i < num_commands_when_false; i++)
    emit_line("%s%s", false_test, cmds_when_false[i]);
  if (!true_first) {
    for (int i = 0; i < num_commands_when_true; i++)
      emit_line("%s%s", true_test, cmds_when_true[i]);
  }
}

static void mcf_emit_inst(Inst* inst) {
  fputs("# ", stdout);
  dump_inst_fp(inst, stdout);

  mcf.was_jump = 0;
  switch (inst->op) {
    case MOV: {
      mcf_emit_set_reg(reg_names[inst->dst.reg], &inst->src);
      break;
    }

    case ADD: {
      const char* dst = reg_names[inst->dst.reg];
      if (inst->src.type == IMM) {
        /* static value */
        if (inst->src.imm & UINT_MAX) { /* is there actually any addition to do? Also guard against -2^31 */
          if (inst->src.imm < 0)
            emit_line(MCF_SPR "%s %s %d", MCF_NAMESPACE, dst, -inst->src.imm);
          else
            emit_line(MCF_SPA "%s %s %d", MCF_NAMESPACE, dst, inst->src.imm);
        }
      } else {
        /* dynamic value */
        emit_line(MCF_SPO "%s %s += %s %s", MCF_NAMESPACE, dst, MCF_NAMESPACE, reg_names[inst->src.reg]);
      }
      /* stay within the word size */
      emit_line(MCF_SPO "%s %s %%= %s elvm_uint_max", MCF_NAMESPACE, dst, MCF_NAMESPACE);
      break;
    }

    case SUB: {
      const char* dst = reg_names[inst->dst.reg];
      if (inst->src.type == IMM) {
        /* static value */
        if (inst->src.imm & UINT_MAX) { /* is there actually any subtraction to do? Also guard against -2^31 */
          if (inst->src.imm < 0)
            emit_line(MCF_SPA "%s %s %d", MCF_NAMESPACE, dst, -inst->src.imm);
          else
            emit_line(MCF_SPR "%s %s %d", MCF_NAMESPACE, dst, inst->src.imm);
        }
      } else {
        /* dynamic value */
        emit_line(MCF_SPO "%s %s -= %s %s", MCF_NAMESPACE, dst, MCF_NAMESPACE, reg_names[inst->src.reg]);
      }
      /* stay within the word size */
      emit_line(MCF_SPO "%s %s %%= %s elvm_uint_max", MCF_NAMESPACE, dst, MCF_NAMESPACE);
      break;
    }

    case LOAD: {
      mcf_emit_mem_table_load(reg_names[inst->dst.reg], &inst->src);
      break;
    }

    case STORE: {
      mcf_emit_mem_table_store(&inst->src, &inst->dst);
      break;
    }

    case EXIT: {
      mcf.was_jump = 1;
      emit_line(MCF_SPS "%s elvm_pc -1", MCF_NAMESPACE);
      break;
    }

    case PUTC: {
      if (inst->src.type == IMM) {
        /* static value */
        int val = inst->src.imm;
        if (val == MCF_FLUSH_CHAR) {
          mcf.used_flush_function = 1;
          emit_line(MCF_F "%s:flush", MCF_NAMESPACE);
        } else {
          if (MCF_STDOUT_CHARS) {
            char ch[3];
            mcf_char_to_string((signed char) val, ch);
            emit_line(MCF_DMS "%s:elvm stdout append value \"%s\"", MCF_NAMESPACE, ch);
            if (MCF_STDOUT_CALLBACK) {
              emit_line(MCF_DMS "%s:elvm chr set value \"%s\"", MCF_NAMESPACE, ch);
            }
          } else {
            emit_line(MCF_DMS "%s:elvm stdout append value %d", MCF_NAMESPACE, val);
          }
        }
        if (MCF_STDOUT_CALLBACK) {
          emit_line(MCF_SPS "%s elvm_param %d", MCF_NAMESPACE, val);
          emit_line(MCF_F "%s", MCF_STDOUT_CALLBACK);
        }
      } else {
        /* dynamic value */
        if (MCF_STDOUT_CHARS || MCF_STDOUT_CALLBACK) {
          mcf_emit_set_reg("elvm_param", &inst->src);
        }
        if (MCF_FLUSH_CHAR >= 0) {
          mcf.used_flush_function = 1;
          if (MCF_STDOUT_CHARS) {
            mcf.used_dyn_int2chr = 1;
            emit_line(MCF_EUS "%s %s matches %d run " MCF_F "%s:chr",
                      MCF_NAMESPACE, reg_names[inst->src.reg], MCF_FLUSH_CHAR, MCF_NAMESPACE);
            emit_line(MCF_EUS "%s %s matches %d run " MCF_DMS "%s:elvm stdout append from storage %s:elvm chr",
                      MCF_NAMESPACE, reg_names[inst->src.reg], MCF_FLUSH_CHAR, MCF_NAMESPACE, MCF_NAMESPACE);
          } else {
            emit_line(MCF_EUS "%s %s matches %d run " MCF_DMS "%s:elvm stdout append value 0",
                      MCF_NAMESPACE, reg_names[inst->src.reg], MCF_FLUSH_CHAR, MCF_NAMESPACE);
            emit_line(MCF_EUS "%s %s matches %d " MCF_SRST "%s:elvm stdout[-1] int 1 run " MCF_SPG "%s %s",
                      MCF_NAMESPACE, reg_names[inst->src.reg], MCF_FLUSH_CHAR, MCF_NAMESPACE, MCF_NAMESPACE,
                      reg_names[inst->src.reg]);
          }
          emit_line(MCF_EIS "%s %s matches %d run " MCF_F "%s:flush",
                    MCF_NAMESPACE, reg_names[inst->src.reg], MCF_FLUSH_CHAR, MCF_NAMESPACE);
        } else {
          if (MCF_STDOUT_CHARS) {
            mcf.used_dyn_int2chr = 1;
            emit_line(MCF_F "%s:chr", MCF_NAMESPACE);
            emit_line(MCF_DMS "%s:elvm stdout append from storage %s:elvm chr", MCF_NAMESPACE, MCF_NAMESPACE);
          } else {
            emit_line(MCF_DMS "%s:elvm stdout append value 0", MCF_NAMESPACE);
            emit_line(MCF_E MCF_SRST "%s:elvm stdout[-1] int 1 run " MCF_SPG "%s %s",
                      MCF_NAMESPACE, MCF_NAMESPACE, reg_names[inst->src.reg]);
          }
        }
        if (MCF_STDOUT_CALLBACK) {
          emit_line(MCF_F "%s", MCF_STDOUT_CALLBACK);
        }
      }
      break;
    }

    case GETC: {
      if (MCF_STDIN_CALLBACK) {
        emit_line(MCF_F "%s", MCF_STDIN_CALLBACK);
      }
      if (MCF_STDIN_WAIT) {
        mcf.was_jump = 1;
        mcf.used_getc_function = 1;
        /* get the length of stdin */
        emit_line(MCF_E MCF_SRSC "%s elvm_tmp run " MCF_DGS "%s:elvm stdin", MCF_NAMESPACE, MCF_NAMESPACE);

        /*
         * If stdin is empty, schedule code to be run next tick when it might not be empty, and tell it to return
         * to this instruction.
         */
        emit_line(MCF_EIS "%s elvm_tmp matches 0 run schedule function %s:getc_reenter 1", MCF_NAMESPACE, MCF_NAMESPACE);
        emit_line(MCF_EIS "%s elvm_tmp matches 0 run " MCF_SPS "%s elvm_getc_pc %d",
                  MCF_NAMESPACE, MCF_NAMESPACE, inst->pc);
        emit_line(MCF_EIS "%s elvm_tmp matches 0 run " MCF_SPS "%s elvm_pc -1", MCF_NAMESPACE, MCF_NAMESPACE);

        /*
         * Otherwise, remove elvm_getc_pc to ensure we aren't waiting for a character, and move it into the
         * destination register.
         */
        emit_line(MCF_EUS "%s elvm_tmp matches 0 " MCF_SRSC "%s %s run " MCF_DGS "%s:elvm stdin[0]",
                  MCF_NAMESPACE, MCF_NAMESPACE, reg_names[inst->dst.reg], MCF_NAMESPACE);
        emit_line(MCF_EUS "%s elvm_tmp matches 0 run " MCF_DRS "%s:elvm stdin[0]", MCF_NAMESPACE, MCF_NAMESPACE);
        emit_line(MCF_EUS "%s elvm_tmp matches 0 run " MCF_SPS "%s elvm_pc %d",
                  MCF_NAMESPACE, MCF_NAMESPACE, inst->jmp.imm);
        emit_line(MCF_EUS "%s elvm_tmp matches 0 run " MCF_F "%s:func_%d", MCF_NAMESPACE, MCF_NAMESPACE, inst->jmp.imm);
      } else {
        emit_line(MCF_E MCF_SRSC "%s %s run " MCF_DGS "%s:elvm stdin[0]",
                  MCF_NAMESPACE, reg_names[inst->dst.reg], MCF_NAMESPACE);
        emit_line(MCF_DRS "%s:elvm stdin[0]", MCF_NAMESPACE);
      }
      break;
    }

    case DUMP: {
      break;
    }

    case EQ:
    case NE:
    case LT:
    case LE:
    case GT:
    case GE: {
      const char* cmd_when_false = format(MCF_SPS "%s elvm_tmp 0", MCF_NAMESPACE);
      const char* cmd_when_true = format(MCF_SPS "%s elvm_tmp 1", MCF_NAMESPACE);
      mcf_emit_test(inst, 1, &cmd_when_false, 1, &cmd_when_true, 0);
      emit_line(MCF_SPO "%s %s = %s elvm_tmp", MCF_NAMESPACE, reg_names[inst->dst.reg], MCF_NAMESPACE);
      break;
    }

    case JMP: {
      mcf.was_jump = 1;
      mcf_emit_set_reg("elvm_pc", &inst->jmp);
      if (inst->jmp.type == IMM)
        emit_line(MCF_F "%s:func_%d", MCF_NAMESPACE, inst->jmp.imm);
      break;
    }

    case JEQ:
    case JNE:
    case JLT:
    case JLE:
    case JGT:
    case JGE: {
      mcf.was_jump = 1;
      /*
       * Here we set the pc based on the condition first, then run the functions based on the pc.
       * Doing this in a different order can mess up the outcome of the test or which functions are run.
       */
      const char* cmd_when_false = format(MCF_SPS "%s elvm_pc %d", MCF_NAMESPACE, mcf.adj_pcs[inst->pc]);
      const char* cmd_when_true;
      if (inst->jmp.type == IMM)
        cmd_when_true = format(MCF_SPS "%s elvm_pc %d", MCF_NAMESPACE, inst->jmp.imm);
      else
        cmd_when_true = format(MCF_SPO "%s elvm_pc = %s %s", MCF_NAMESPACE, MCF_NAMESPACE, reg_names[inst->jmp.reg]);

      mcf_emit_test(inst, 1, &cmd_when_false, 1, &cmd_when_true, 0);

      if (mcf.adj_pcs[inst->pc] != -1)
        emit_line(MCF_EIS "%s elvm_pc matches %d run " MCF_F "%s:func_%d",
                  MCF_NAMESPACE, mcf.adj_pcs[inst->pc], MCF_NAMESPACE, mcf.adj_pcs[inst->pc]);
      if (inst->jmp.type == IMM)
        emit_line(MCF_EIS "%s elvm_pc matches %d run " MCF_F "%s:func_%d",
                  MCF_NAMESPACE, inst->jmp.imm, MCF_NAMESPACE, inst->jmp.imm);
      break;
    }

    default:
      error("oops");
  }
}

typedef union Node_ {
  struct {
    union Node_* left;
    union Node_* right;
  };
  int val;
} Node;

static void mcf_set_in_tree(Node* tree, int depth, int addr, int value) {
  if (depth == 0) {
    tree->val = value;
  } else {
    if (addr & (1 << (depth - 1))) {
      if (!tree->right)
        tree->right = (Node*) calloc(1, sizeof(*tree->right));
      mcf_set_in_tree(tree->right, depth - 1, addr, value);
    } else {
      if (!tree->left)
        tree->left = (Node*) calloc(1, sizeof(*tree->left));
      mcf_set_in_tree(tree->left, depth - 1, addr, value);
    }
  }
}

static char* mcf_tree_to_nbt(Node* tree, int depth) {
  if (depth == 0) {
    return format("%d", tree->val);
  }
  if (tree->left) {
    if (tree->right) {
      return format("{\"l\":%s,\"r\":%s}",
                    mcf_tree_to_nbt(tree->left, depth - 1),
                    mcf_tree_to_nbt(tree->right, depth - 1));
    } else {
      return format("{\"l\":%s}", mcf_tree_to_nbt(tree->left, depth - 1));
    }
  } else {
    if (tree->right) {
      return format("{\"r\":%s}", mcf_tree_to_nbt(tree->right, depth - 1));
    } else {
      return strdup("{}");
    }
  }
}

static void mcf_free_tree(Node* tree, int depth) {
  if (depth > 0) {
    if (tree->left)
      mcf_free_tree(tree->left, depth - 1);
    if (tree->right)
      mcf_free_tree(tree->right, depth - 1);
  }
  free(tree);
}

static void mcf_emit_memory_initialization(Data* data) {
  Node* root = (Node*) calloc(1, sizeof(*root));

  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      mcf_set_in_tree(root, 24, mp, data->v);
    }
  }

  emit_line(MCF_DMS "%s:elvm mem set value %s", MCF_NAMESPACE, mcf_tree_to_nbt(root, 24));

  mcf_free_tree(root, 24);
}

static void mcf_emit_main_function(Data* data, int pc_count) {
  mcf_emit_function_header(format("%s:main", MCF_NAMESPACE));

  /* register the registers */
  for (int i = 0; i < 7; i++) {
    emit_line(MCF_SOA "%s dummy", reg_names[i]);
    emit_line(MCF_SPS "%s %s 0", MCF_NAMESPACE, reg_names[i]);
  }
  emit_line(MCF_SOA "elvm_getc_pc dummy");

  emit_line(MCF_SOA "elvm_tmp dummy");
  emit_line(MCF_SOA "elvm_success dummy");

  mcf_emit_memory_initialization(data);

  emit_line(MCF_SOA "elvm_mem_addr dummy");
  emit_line(MCF_SOA "elvm_param dummy");
  emit_line(MCF_SOA "elvm_mem_res dummy");

  emit_line(MCF_SOA "elvm_uint_max dummy");
  emit_line(MCF_SPS "%s elvm_uint_max %d", MCF_NAMESPACE, UINT_MAX + 1);
  emit_line(MCF_SOA "elvm_two dummy");
  emit_line(MCF_SPS "%s elvm_two 2", MCF_NAMESPACE);

  emit_line(MCF_DMS "%s:elvm stdout set value []", MCF_NAMESPACE);

  /* cache slots are initialized to the start of memory. The caching code breaks if the caches don't point somewhere. */
  Data* cache_data = data;
  for (int i = 0; i < MCF_CACHE_COUNT; i++) {
    emit_line(MCF_SOA "elvm_cache%d_lo dummy", i);
    emit_line(MCF_SPS "%s elvm_cache%d_lo %d", MCF_NAMESPACE, i, i * MCF_CACHE_SIZE);
    emit_line(MCF_SOA "elvm_cache%d_hi dummy", i);
    emit_line(MCF_SPS "%s elvm_cache%d_hi %d", MCF_NAMESPACE, i, (i + 1) * MCF_CACHE_SIZE);

    Node* root = (Node*) calloc(1, sizeof(*root));
    for (int p = 0; cache_data && p < MCF_CACHE_SIZE; cache_data = cache_data->next, p++) {
      if (cache_data->v) {
        mcf_set_in_tree(root, MCF_CACHE_DEPTH, p, cache_data->v);
      }
    }
    emit_line(MCF_DMS "%s:elvm cache%d set value %s", MCF_NAMESPACE, i, mcf_tree_to_nbt(root, MCF_CACHE_DEPTH));
    mcf_free_tree(root, MCF_CACHE_DEPTH);
  }
  emit_line(MCF_SOA "elvm_cache_size dummy");
  emit_line(MCF_SPS "%s elvm_cache_size %d", MCF_NAMESPACE, MCF_CACHE_SIZE);
  emit_line(MCF_SOA "elvm_2cache_size dummy");
  emit_line(MCF_SPS "%s elvm_2cache_size %d", MCF_NAMESPACE, MCF_CACHE_SIZE * 2);

  /* start the loop */
  emit_line(MCF_EIS "%s elvm_pc matches 0..%d run " MCF_F "%s:loop", MCF_NAMESPACE, pc_count - 1, MCF_NAMESPACE);
}

static void mcf_emit_loop_function(int pc_count) {
  mcf_emit_function_header(format("%s:loop", MCF_NAMESPACE));
  emit_line(MCF_F "%s:func_0_%d", MCF_NAMESPACE, pc_count);
  emit_line(MCF_EIS "%s elvm_pc matches 0..%d run " MCF_F "%s:loop", MCF_NAMESPACE, pc_count - 1, MCF_NAMESPACE);
}

/* Recursively emits functions which search for the right function to call based on the value of elvm_pc */
static void mcf_emit_pc_search_function(int min, int max) {
  int range = max - min;
  int mid = min + range / 2;
  mcf_emit_function_header(format("%s:func_%d_%d", MCF_NAMESPACE, min, max));

  if (range < 4) {
    /* leaf nodes */
    for (int i = 0; i < range; i++) {
      emit_line(MCF_EIS "%s elvm_pc matches %d run " MCF_F "%s:func_%d", MCF_NAMESPACE, min + i, MCF_NAMESPACE, min + i);
    }
  } else {
    /* call sub-functions and recursively emit them */
    emit_line(MCF_EIS "%s elvm_pc matches %d..%d run " MCF_F "%s:func_%d_%d",
              MCF_NAMESPACE, min, mid - 1, MCF_NAMESPACE, min, mid);
    emit_line(MCF_EIS "%s elvm_pc matches %d..%d run " MCF_F "%s:func_%d_%d",
              MCF_NAMESPACE, mid, max - 1, MCF_NAMESPACE, mid, max);
    mcf_emit_pc_search_function(min, mid);
    mcf_emit_pc_search_function(mid, max);
  }
}

static void mcf_emit_utility_functions() {
  if (mcf.used_dyn_int2chr)
    mcf_emit_chr_function(0, 256);
  if (mcf.used_flush_function)
    mcf_emit_flush_function();
  if (mcf.used_dyn_load) {
    for (int i = 0; i < MCF_CACHE_COUNT; i++) {
      mcf_emit_loadcached_function(i);
    }
  }
  if (mcf.used_dyn_store) {
    for (int i = 0; i < MCF_CACHE_COUNT; i++) {
      mcf_emit_storecached_function(i);
    }
  }
  if (mcf.used_dyn_load || mcf.used_dyn_store)
    mcf_emit_recache_function();

  if (mcf.used_getc_function)
    mcf_emit_getc_reenter_function();
}

/*
 * Preprocess the code, rearranging the control flow graph for our special requirements.
 * Our main special requirement is that getc has to have a pc of its own.
 * This should only add blocks at the end, to avoid messing up with existing jump targets
 * in the code.
 */
static void mcf_preprocess_code(Inst* code) {
  if (!code)
    return;

  typedef struct Block_ {
    int pc;
    Inst* code;
    int adj_pc;
    struct Block_* next;
  } Block;

  /* convert instruction list to list of blocks of instructions */
  int block_count = 1;
  Block* first_block = (Block*) malloc(sizeof(*first_block));
  first_block->pc = code->pc;
  first_block->code = code;
  first_block->adj_pc = -1;
  first_block->next = NULL;
  Block* last_block = first_block;

  Inst* prev_inst = NULL;
  for (Inst* inst = code; inst; inst = inst->next) {
    if (inst->pc != last_block->pc) {
      if (prev_inst)
        prev_inst->next = NULL;
      block_count++;
      Block* new_block = (Block*) malloc(sizeof(*new_block));
      new_block->pc = inst->pc;
      new_block->code = inst;
      new_block->adj_pc = -1;
      new_block->next = NULL;
      last_block->adj_pc = inst->pc;
      last_block->next = new_block;
      last_block = new_block;
    }
    prev_inst = inst;
  }

  /* Process the blocks */
  for (Block* block = first_block; block; block = block->next) {
    prev_inst = NULL;
    for (Inst* inst = block->code; inst; inst = inst->next) {
      /* there should be no instructions before getc (so it can schedule itself) */
      if (MCF_STDIN_WAIT && prev_inst && inst->op == GETC) {
        block_count++;
        Block* new_block = (Block*) malloc(sizeof(*new_block));
        new_block->pc = last_block->pc + 1;
        new_block->code = inst;
        new_block->adj_pc = block->adj_pc;
        new_block->next = NULL;
        last_block->next = new_block;
        last_block = new_block;
        block->adj_pc = new_block->pc;
        prev_inst->next = NULL;
        break;
      }

      /* there should be no instructions after getc or exit (they count as jump instructions for our purposes) */
      if (inst->next && ((MCF_STDIN_WAIT && inst->op == GETC) || inst->op == EXIT)) {
        block_count++;
        Block* new_block = (Block*) malloc(sizeof(*new_block));
        new_block->pc = last_block->pc + 1;
        new_block->code = inst->next;
        new_block->adj_pc = block->adj_pc;
        new_block->next = NULL;
        last_block->next = new_block;
        last_block = new_block;
        inst->next = NULL;
        inst->jmp.type = IMM;
        inst->jmp.imm = new_block->pc;
        break;
      }

      prev_inst = inst;
    }
  }

  /* Convert blocks back into instruction list */
  mcf.adj_pcs = (int*) malloc(block_count * sizeof(int));
  Inst* tail = NULL;
  for (Block* block = first_block; block; block = block->next) {
    for (Inst* inst = block->code; inst; inst = inst->next) {
      inst->pc = block->pc;
      if (tail)
        tail->next = inst;
      tail = inst;
    }
    mcf.adj_pcs[block->pc] = block->adj_pc;
  }

  /* free memory used by blocks */
  while (first_block) {
    Block* next = first_block->next;
    free(first_block);
    first_block = next;
  }
}

void target_mcfunction(Module* module) {
  reg_names = MCF_REG_NAMES;
  MCF_CACHE_SIZE = 1 << MCF_CACHE_DEPTH;

  mcf.used_dyn_int2chr = 0;
  mcf.used_flush_function = 0;
  mcf.used_dyn_load = 0;
  mcf.used_dyn_store = 0;
  mcf.used_getc_function = 0;
  mcf.was_jump = 1;

  mcf_preprocess_code(module->text);

  int pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (inst->pc != pc) {
      if (!mcf.was_jump) {
        emit_line(MCF_SPS "%s elvm_pc %d", MCF_NAMESPACE, mcf.adj_pcs[pc]);
        if (mcf.adj_pcs[pc] != -1)
          emit_line(MCF_F "%s:func_%d", MCF_NAMESPACE, mcf.adj_pcs[pc]);
      }
      pc = inst->pc;
      mcf_emit_function_header(format("%s:func_%d", MCF_NAMESPACE, pc));
    }
    mcf_emit_inst(inst);
  }

  mcf_emit_pc_search_function(0, pc + 1);
  mcf_emit_main_function(module->data, pc + 1);
  mcf_emit_loop_function(pc + 1);
  mcf_emit_utility_functions();

}

bool handle_mcfunction_args(const char* arg, const char* value) {
  if (!strcmp(arg, "cache_count")) {
    MCF_CACHE_COUNT = atoi(value);
    if (MCF_CACHE_COUNT < 1)
      MCF_CACHE_COUNT = 1;
    return true;
  }
  if (!strcmp(arg, "cache_depth")) {
    MCF_CACHE_DEPTH = atoi(value);
    if (MCF_CACHE_DEPTH < 2)
      MCF_CACHE_DEPTH = 2;
    else if (MCF_CACHE_DEPTH > 22)
      MCF_CACHE_DEPTH = 22;
    return true;
  }
  if (!strcmp(arg, "namespace")) {
    if (*value)
      MCF_NAMESPACE = value;
    return true;
  }
  if (!strcmp(arg, "flush_char")) {
    MCF_FLUSH_CHAR = atoi(value);
    return true;
  }
  if (!strcmp(arg, "stdout_chars")) {
    MCF_STDOUT_CHARS = parse_bool_value(value);
    return true;
  }
  if (!strcmp(arg, "stdout_callback")) {
    if (*value)
      MCF_STDOUT_CALLBACK = value;
    return true;
  }
  if (!strcmp(arg, "stdin_wait")) {
    MCF_STDIN_WAIT = parse_bool_value(value);
    return true;
  }
  if (!strcmp(arg, "stdin_callback")) {
    if (*value)
      MCF_STDIN_CALLBACK = value;
    return true;
  }
  return false;
}
