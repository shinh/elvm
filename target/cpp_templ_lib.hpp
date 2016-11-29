#ifndef LIB_HPP
#define LIB_HPP
const int MEM_SIZE = 1 << 16;

constexpr static const char* in_buf =
#   include "input.txt"
    ;

/*
  Fundamental Data Structure
 */
struct Nil { typedef Nil head; typedef Nil tail; };

template <typename Head, typename Tail/*=Nil*/>
struct Pair { typedef Head head; typedef Tail tail; };

template <int n>  struct Int {static const int  val = n;};
template <bool b> struct Bool{static const bool val = b;};

/*
  List
*/

template<typename l>
struct is_nil : Bool<false> {};
template<>
struct is_nil<Nil> : Bool<true> {};

template <typename head, typename tail>
struct cons : Pair<head, tail> {};

template <typename list, int n>
struct get_at : get_at<typename list::tail, n-1> {};
template <typename list>
struct get_at<list, 0> : list::head {};

template <typename list, int n, int v>
struct set_at : cons<typename list::head, set_at<typename list::tail, n - 1, v>> {};
template <typename list, int v>
struct set_at<list, 0, v> : cons<Int<v>, typename list::tail> {};

template <int n>
struct init_list_aux {
  typedef typename init_list_aux<n-1>::result tail;
  typedef cons<Int<0>, tail> result;
};
template <>
struct init_list_aux<0> {
  typedef Nil result;
};
// init_list : 'a list -> int -> 'a -> 'a list
template <int n>
struct init_list : init_list_aux<n>::result {};

/*
  Binary Tree (Used for Memory)
*/
template <typename node_v, typename lnode, typename rnode>
struct Node {
  typedef node_v value;
  typedef lnode left;
  typedef rnode right;
  static const bool is_leaf = is_nil<lnode>::val && is_nil<rnode>::val;
};
template <typename v>
struct Leaf : Node<v, Nil, Nil> {};

template <int depth>
struct mk_tree {
  typedef Int<depth> value;
  typedef mk_tree<depth-1> left;
  typedef mk_tree<depth-1> right;
};
template <>
struct mk_tree<0> : Leaf<Int<0>> {};

template <typename tree, int flg>
struct get_child : tree::left {};
template <typename tree>
struct get_child<tree, 1> : tree::right {};
template <typename tree, typename child, int flg>
struct update_child : tree { typedef child left; };
template <typename tree, typename child>
struct update_child<tree, child, 1> : tree {typedef child right;};

/*
  Memory
*/
constexpr int const_log2(int x) {
  return x < 2 ? x : 1 + const_log2(x >> 1);
}
template <int mem_size>
struct init_memory {
  static const int depth = const_log2(mem_size); // memory size == 2^d
  typedef mk_tree<depth> tree;
};

/*
  Memory Access
*/
template <typename tree, int depth, int idx>
struct load_value_aux {
  static const int flg = (idx >> (depth-1)) & 1;
  typedef get_child<tree, flg> child;
  typedef typename load_value_aux<child, depth-1, idx>::result result;
};

template <typename tree, int idx>
struct load_value_aux<tree, 0, idx> {
  typedef typename tree::value result;
};

template <typename memory, int idx>
struct load_value : load_value_aux<typename memory::tree, memory::depth, idx & (MEM_SIZE - 1)>::result {};

template <typename tree, int depth, int idx, int new_v>
struct store_value_aux {
  static const int flg = (idx >> (depth-1)) & 1;
  typedef get_child<tree, flg> child;
  typedef typename store_value_aux<child, depth-1, idx, new_v>::result new_child;
  typedef update_child<tree, new_child, flg> result;
};
template <typename tree, int idx, int new_v>
struct store_value_aux<tree, 0, idx, new_v> {
  typedef Leaf<Int<new_v>> result;
};

template <typename memory, int idx, int new_v>
struct store_value : memory {
  typedef typename store_value_aux<typename memory::tree, memory::depth, idx & (MEM_SIZE - 1), new_v>::result tree;
};

/*
  Registers
 */
template <int PC>
struct init_regs {
  static const int a = 0;
  static const int b = 0;
  static const int c = 0;
  static const int d = 0;
  static const int sp = 0;
  static const int bp = 0;
  static const int pc = PC;
  static const bool exit_flag = false;
  static const int input_cur = 0;
};

template <typename regs, int src_reg>
struct reg_val {};
template <typename regs>
struct reg_val<regs, 0> { static const int val = regs::a;};
template <typename regs>
struct reg_val<regs, 1> { static const int val = regs::b;};
template <typename regs>
struct reg_val<regs, 2> { static const int val = regs::c;};
template <typename regs>
struct reg_val<regs, 3> { static const int val = regs::d;};
template <typename regs>
struct reg_val<regs, 4> { static const int val = regs::sp;};
template <typename regs>
struct reg_val<regs, 5> { static const int val = regs::bp;};

/*
  Environment
*/
template <typename Regs, typename Mem, typename Buf>
struct make_env {
  typedef Regs regs;
  typedef Mem mem;
  typedef Buf buf;
};

template <typename regs, int PC>
struct update_pc : regs {static const int pc = PC;};
template <typename regs>
struct inc_input_cur : regs {static const int input_cur = regs::input_cur + (in_buf[regs::input_cur] ? 1 : 0);};

// PC change
template <typename regs, typename mem, typename buf>
struct inc_pc : make_env<update_pc<regs, 1 + regs::pc>, mem, buf> {};

/*
  MOV
 */
template <typename r, int dst_reg, int imm>
struct mov_imm_aux : r {};
template <typename r, int imm>
struct mov_imm_aux<r, 0, imm> : r {static const int a = imm;};
template <typename r, int imm>
struct mov_imm_aux<r, 1, imm> : r {static const int b = imm;};
template <typename r, int imm>
struct mov_imm_aux<r, 2, imm> : r {static const int c = imm;};
template <typename r, int imm>
struct mov_imm_aux<r, 3, imm> : r {static const int d = imm;};
template <typename r, int imm>
struct mov_imm_aux<r, 4, imm> : r {static const int sp = imm;};
template <typename r, int imm>
struct mov_imm_aux<r, 5, imm> : r {static const int bp = imm;};

template <typename regs, int dst_reg, int imm>
struct mov_imm : mov_imm_aux<regs, dst_reg, imm> {};
template <typename regs, int dst_reg, int src_reg>
struct mov_reg : mov_imm_aux<regs, dst_reg, reg_val<regs, src_reg>::val> {};

/*
  ADD, SUB
 */
template <typename regs, int dst_reg, int imm>
struct add_aux {
  static const int dst_val = reg_val<regs, dst_reg>::val;
  typedef mov_imm<regs, dst_reg, (dst_val + imm) & (MEM_SIZE - 1)> result;
};
template <typename regs, int dst_reg, int imm>
struct add_imm : add_aux<regs, dst_reg, imm>::result {};
template <typename regs, int dst_reg, int src_reg>
struct add_reg : add_aux<regs, dst_reg, reg_val<regs, src_reg>::val>::result {};
template <typename regs, int dst_reg, int imm>
struct sub_imm : add_aux<regs, dst_reg, -imm>::result {};
template <typename regs, int dst_reg, int src_reg>
struct sub_reg : add_aux<regs, dst_reg, -reg_val<regs, src_reg>::val>::result {};

/*
  LOAD
 */
template <typename mem, int addr>
struct load_aux : load_value<mem, addr> {};

template <typename regs, typename mem, int dst_reg, int addr>
struct load_imm : mov_imm<regs, dst_reg, load_aux<mem, addr>::val> {};
template <typename regs, typename mem, int dst_reg, int src_reg>
struct load_reg : load_imm<regs, mem, dst_reg, reg_val<regs, src_reg>::val> {};

/*
  STORE
 */
template <typename regs, typename mem, int src_reg, int addr>
struct store_aux {
  typedef reg_val<regs, src_reg> data;
  typedef store_value<mem, addr, data::val> result;
};

template <typename regs, typename mem, int src_reg, int addr>
struct store_imm : store_aux<regs, mem, src_reg, addr>::result {};
template <typename regs, typename mem, int src_reg, int dst_reg>
struct store_reg : store_imm<regs, mem, src_reg, reg_val<regs, dst_reg>::val> {};

/*
  PUTC
 */
template <typename regs, typename buffer, int imm>
struct putc_aux {
  typedef cons<Int<imm>, buffer> result;
};
template <typename regs, typename buffer, int imm>
struct putc_imm : putc_aux<regs, buffer, imm>::result {};
template <typename regs, typename buffer, int src_reg>
struct putc_reg : putc_imm<regs, buffer, reg_val<regs, src_reg>::val> {};

/*
  GETC
 */
template <typename regs, int dst_reg>
struct getc_aux {
  static const int read_val = in_buf[regs::input_cur];
  typedef inc_input_cur<regs> regs2;
  typedef mov_imm<regs2, dst_reg, read_val & (MEM_SIZE - 1)> result;
};
template <typename regs, int dst_reg>
struct getc_reg : getc_aux<regs, dst_reg>::result {};

/*
  EXIT
 */
template <typename regs>
struct exit_inst : regs {
  static const int pc = 1000000;
  static const bool exit_flag = true;
};

/*
  JMP
 */
template <typename regs, int label, int cmp_flg>
struct jmp_aux : update_pc<regs, label>{};
template <typename regs, int label>
struct jmp_aux<regs, label, 0> : regs {};

template <typename regs, int imm>
struct jmp_imm : jmp_aux<regs, imm - 1, 1> {};
template <typename regs, int reg>
struct jmp_reg : jmp_aux<regs, reg_val<regs, reg>::val - 1, 1> {};

/*
  CMP
 */
template <typename regs, int reg0, int imm>
struct eq_imm {
  static const int reg_v = reg_val<regs, reg0>::val;
  static const bool result = (reg_v == imm);
};

template <typename regs, int reg, int imm, int op>
struct cmp_op_imm {
  static const int v = reg_val<regs, reg>::val;
  static const int result =
    op == 0 ? (v == imm) :
    op == 1 ? (v != imm) :
    op == 2 ? (v <  imm) :
    op == 3 ? (v >  imm) :
    op == 4 ? (v <= imm) :
    op == 5 ? (v >= imm) :
    1;
};

template <typename regs, int reg1, int reg2, int op>
struct cmp_op_reg : cmp_op_imm<regs, reg1, reg_val<regs, reg2>::val, op> {};

template <typename regs, int dst_reg, int imm, int op>
struct cmp_imm : mov_imm<regs, dst_reg, cmp_op_imm<regs, dst_reg, imm, op>::result> {};
template <typename regs, int dst_reg, int src_reg, int op>
struct cmp_reg : mov_imm<regs, dst_reg, cmp_op_reg<regs, dst_reg, src_reg, op>::result> {};

template <typename regs, int label, int src_reg, int imm, int op>
struct jcmp_imm : jmp_aux<regs, label, cmp_op_imm<regs, src_reg, imm, op>::result> {};
template <typename regs, int label, int src_reg1, int src_reg2, int op>
struct jcmp_reg : jmp_aux<regs, label, cmp_op_reg<regs, src_reg1, src_reg2, op>::result> {};

template <typename list>
struct print_buffer_aux {
  static inline void run () {
    print_buffer_aux<typename list::tail>::run();
    putchar(list::head::val);
  }
};
template <>
struct print_buffer_aux<Nil> { static inline void run () {}};
template <typename list>
struct print_buffer : print_buffer_aux<list> {};

#endif
