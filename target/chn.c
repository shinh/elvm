#include <stdlib.h>
#include <assert.h>
#include <ir/ir.h>
#include <target/util.h>

const int CHN_EXIT=0;
const int CHN_ADD=2;
const int CHN_SUB=3;
const int CHN_MUL=4;
const int CHN_CMP=5;
const int CHN_LOAD=6;
const int CHN_STORE=7;
const int CHN_JMP=8;
const int CHN_CHAR=9;
const int CHN_RI=12;
const int CHN_RM=13;
const int CHN_RT=14;
const int CHN_STDOUT=15;
const int CHN_MEMOFF=16;
const int CHN_RT2=17;
const int CHN_RT3=18;
struct _chn_num_node{
 int num; struct _chn_num_node*nxt;
};
struct _chn_code{ struct _chn_num_node*head,*tail;
};
static struct _chn_code *chn_nodes;
static struct _chn_code *chn_jump_table;
int chn_len_code;
static int chn_len_nojump;
void chn_init(){
 chn_nodes=(struct _chn_code*)malloc(sizeof(struct _chn_code));
 chn_nodes->head=chn_nodes->tail=NULL;
 chn_jump_table=(struct _chn_code*)malloc(sizeof(struct _chn_code));
 chn_jump_table->head=chn_jump_table->tail=NULL;
}
void chn_emit_num(int x){ struct _chn_num_node *newnode=(struct _chn_num_node*)malloc(sizeof(struct _chn_num_node));
 newnode->num=x;
 newnode->nxt=NULL;
 if(chn_nodes->head==NULL){
 chn_nodes->head=chn_nodes->tail=newnode;
 return;
 }
 chn_nodes->tail->nxt=newnode;
 chn_nodes->tail=newnode;
}
void chn_emit_num_jmp(int x){ struct _chn_num_node *newnode=(struct _chn_num_node*)malloc(sizeof(struct _chn_num_node));
 newnode->num=x;
 newnode->nxt=NULL;
 if(chn_jump_table->head==NULL){
 chn_jump_table->head=chn_jump_table->tail=newnode;
 return;
 }
 chn_jump_table->tail->nxt=newnode;
 chn_jump_table->tail=newnode;
}
void chn_emit_nilad(int x){
 assert(x!=CHN_LOAD);
 chn_emit_num(x);
 ++chn_len_code;
 ++chn_len_nojump;
}
void chn_emit_nilad_jmp(int x){
 assert(x!=CHN_LOAD);
 chn_emit_num(x);
 ++chn_len_code;
}
void chn_emit_monad(int x,int y){
 assert(x==CHN_LOAD);
 chn_emit_num(x);
 chn_emit_num(y);
 chn_len_code+=2;
 chn_len_nojump+=2;
}
void chn_emit_monad_jmp(int x,int y){
 assert(x==CHN_LOAD);
 chn_emit_num(x);
 chn_emit_num(y);
 chn_len_code+=2;
}
void chn_emit_static_length_num(int x){
 emit_str("%d ",((0)+10));
 emit_str("%d %d %d %d ",((16)+10),CHN_MUL,(((x>>20)&15)+10),CHN_ADD);
 emit_str("%d %d %d %d ",((16)+10),CHN_MUL,(((x>>16)&15)+10),CHN_ADD);
 emit_str("%d %d %d %d ",((16)+10),CHN_MUL,(((x>>12)&15)+10),CHN_ADD);
 emit_str("%d %d %d %d ",((16)+10),CHN_MUL,(((x>>8)&15)+10),CHN_ADD);
 emit_str("%d %d %d %d ",((16)+10),CHN_MUL,(((x>>4)&15)+10),CHN_ADD);
 emit_str("%d %d %d %d ",((16)+10),CHN_MUL,((x&15)+10),CHN_ADD);
}
void chn_emit_header(){
 chn_emit_static_length_num(chn_len_code);
 emit_str("%d %d ",((2)+10),CHN_STORE);
 chn_emit_static_length_num(chn_len_nojump);
 emit_str("%d %d ",((3)+10),CHN_STORE);
 emit_str("%d ",((1)+10));
 for(int i=0;i<6;i++)emit_str("%d %d ",((16)+10),CHN_MUL);
 emit_str("%d %d ",((4)+10),CHN_STORE);
 chn_emit_static_length_num(140);
 emit_str("%d %d ",((5)+10),CHN_STORE);
 for(int i=6;i<14;i++){
 emit_str("%d %d %d ",((0)+10),((i)+10),CHN_STORE);
 }
 emit_str("%d %d %d %d ",((10)+10),CHN_CHAR,((CHN_STDOUT)+10),CHN_STORE);
 emit_str("%d ",((1)+10));
 for(int i=0;i<3;i++)emit_str("%d %d ",((16)+10),CHN_MUL);
 emit_str("%d %d ",((CHN_MEMOFF)+10),CHN_STORE);
 emit_str("%d %d %d 0 %d ",((1)+10),((3)+10),CHN_LOAD,CHN_JMP);
}
void chn_emit_var_length_num(int x){
 chn_emit_nilad(((0)+10));
 if(x>=0x100000)chn_emit_nilad(((16)+10)),chn_emit_nilad(CHN_MUL),chn_emit_nilad((((x>>20)&15)+10)),chn_emit_nilad(CHN_ADD);
 if(x>=0x10000)chn_emit_nilad(((16)+10)),chn_emit_nilad(CHN_MUL),chn_emit_nilad((((x>>16)&15)+10)),chn_emit_nilad(CHN_ADD);
 if(x>=0x1000)chn_emit_nilad(((16)+10)),chn_emit_nilad(CHN_MUL),chn_emit_nilad((((x>>12)&15)+10)),chn_emit_nilad(CHN_ADD);
 if(x>=0x100)chn_emit_nilad(((16)+10)),chn_emit_nilad(CHN_MUL),chn_emit_nilad((((x>>8)&15)+10)),chn_emit_nilad(CHN_ADD);
 if(x>=0x10)chn_emit_nilad(((16)+10)),chn_emit_nilad(CHN_MUL),chn_emit_nilad((((x>>4)&15)+10)),chn_emit_nilad(CHN_ADD);
 if(x>=0x1)chn_emit_nilad(((16)+10)),chn_emit_nilad(CHN_MUL),chn_emit_nilad(((x&15)+10)),chn_emit_nilad(CHN_ADD);
}
void chn_emit_var_length_num_jmp(int x){
 chn_emit_nilad_jmp(((0)+10));
 if(x>=0x100000)chn_emit_nilad_jmp(((16)+10)),chn_emit_nilad_jmp(CHN_MUL),chn_emit_nilad_jmp((((x>>20)&15)+10)),chn_emit_nilad_jmp(CHN_ADD);
 if(x>=0x10000)chn_emit_nilad_jmp(((16)+10)),chn_emit_nilad_jmp(CHN_MUL),chn_emit_nilad_jmp((((x>>16)&15)+10)),chn_emit_nilad_jmp(CHN_ADD);
 if(x>=0x1000)chn_emit_nilad_jmp(((16)+10)),chn_emit_nilad_jmp(CHN_MUL),chn_emit_nilad_jmp((((x>>12)&15)+10)),chn_emit_nilad_jmp(CHN_ADD);
 if(x>=0x100)chn_emit_nilad_jmp(((16)+10)),chn_emit_nilad_jmp(CHN_MUL),chn_emit_nilad_jmp((((x>>8)&15)+10)),chn_emit_nilad_jmp(CHN_ADD);
 if(x>=0x10)chn_emit_nilad_jmp(((16)+10)),chn_emit_nilad_jmp(CHN_MUL),chn_emit_nilad_jmp((((x>>4)&15)+10)),chn_emit_nilad_jmp(CHN_ADD);
 if(x>=0x1)chn_emit_nilad_jmp(((16)+10)),chn_emit_nilad_jmp(CHN_MUL),chn_emit_nilad_jmp(((x&15)+10)),chn_emit_nilad_jmp(CHN_ADD);
}
void chn_emit_jump_table_initializer(){
 chn_emit_nilad_jmp(((3)+10));
 chn_emit_monad_jmp(CHN_LOAD,0);
 chn_emit_nilad_jmp(((5)+10));
 chn_emit_monad_jmp(CHN_LOAD,0);
 chn_emit_nilad_jmp(CHN_ADD);
 chn_emit_nilad_jmp(((6)+10));
 chn_emit_nilad_jmp(CHN_STORE);
 for(struct _chn_num_node*i=chn_jump_table->head;i!=NULL;i=i->nxt){
 chn_emit_var_length_num_jmp(i->num);
 chn_emit_nilad_jmp(((6)+10));
 chn_emit_monad_jmp(CHN_LOAD,0);
 chn_emit_nilad_jmp(CHN_STORE);
 chn_emit_nilad_jmp(((6)+10));
 chn_emit_monad_jmp(CHN_LOAD,0);
 chn_emit_nilad_jmp(((1)+10));
 chn_emit_nilad_jmp(CHN_ADD);
 chn_emit_nilad_jmp(((6)+10));
 chn_emit_nilad_jmp(CHN_STORE);
 }
 chn_emit_nilad_jmp(((0)+10));
 chn_emit_nilad_jmp(((6)+10));
 chn_emit_nilad_jmp(CHN_STORE);
 chn_emit_nilad_jmp(((1)+10));
 chn_emit_nilad_jmp(((0)+10));
 chn_emit_nilad_jmp(((2)+10));
 chn_emit_monad_jmp(CHN_LOAD,0);
 chn_emit_nilad_jmp(CHN_SUB);
 chn_emit_nilad_jmp(CHN_JMP);
}
void chn_emit_whole_program(){
 chn_emit_header();
 for(struct _chn_num_node*i=chn_nodes->head;i!=NULL;i=i->nxt) emit_str("%d ",i->num);
}
void chn_exit(){
 chn_emit_nilad(CHN_EXIT);
}
void chn_non_negative(){
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(CHN_CHAR);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((0)+10));
 chn_emit_monad(CHN_LOAD,CHN_RT);
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(CHN_SUB);
}
void chn_neg(){
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(((1)+10));
 chn_emit_nilad(CHN_SUB);
 chn_emit_nilad(CHN_MUL);
}
void chn_not(){
 chn_neg();
 chn_emit_nilad(((1)+10));
 chn_emit_nilad(CHN_ADD);
}
void chn_isntnan(){
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_CMP);
}
void chn_isnan(){
 chn_isntnan();
 chn_not();
}
void chn_eq(){
 chn_emit_nilad(CHN_CMP);
}
void chn_ne(){
 chn_eq();
 chn_not();
}
void chn_lt(){
 chn_emit_nilad(CHN_SUB);
 chn_non_negative();
 chn_isnan();
}
void chn_ge(){
 chn_emit_nilad(CHN_SUB);
 chn_non_negative();
 chn_isntnan();
}
void chn_gt(){
 chn_emit_nilad(CHN_SUB);
 chn_neg();
 chn_non_negative();
 chn_isnan();
}
void chn_le(){
 chn_emit_nilad(CHN_SUB);
 chn_neg();
 chn_non_negative();
 chn_isntnan();
}
void chn_getc(){
 chn_emit_nilad(((CHN_RI)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_monad(CHN_LOAD,1);
 chn_emit_nilad(((8)+10));
 chn_emit_nilad(CHN_MUL);
 chn_emit_nilad(((8)+10));
 chn_emit_nilad(CHN_MUL);
 chn_emit_nilad(((CHN_RI)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((1)+10));
 chn_emit_nilad(CHN_ADD);
 chn_emit_monad(CHN_LOAD,1);
 chn_emit_nilad(((8)+10));
 chn_emit_nilad(CHN_MUL);
 chn_emit_nilad(((CHN_RI)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((2)+10));
 chn_emit_nilad(CHN_ADD);
 chn_emit_monad(CHN_LOAD,1);
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(CHN_SUB);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(((CHN_RI)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((3)+10));
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(((CHN_RI)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_isnan();
 chn_emit_nilad(((4)+10));
 chn_emit_nilad(CHN_JMP);
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
}
void chn_put(){
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((CHN_STDOUT)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(((CHN_STDOUT)+10));
 chn_emit_nilad(CHN_STORE);
}
void chn_putc(){
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((CHN_STDOUT)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((CHN_RT)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_CHAR);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(((CHN_STDOUT)+10));
 chn_emit_nilad(CHN_STORE);
}
void chn_output(){
 chn_emit_nilad(((CHN_STDOUT)+10));
 chn_emit_monad(CHN_LOAD,0);
}
void chn_modulo_once(){
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((4)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_lt();
 chn_emit_nilad(((9)+10));
 chn_emit_nilad(CHN_JMP);
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((4)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_SUB);
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_monad(CHN_LOAD,0);
}
void chn_read_memory(){
 chn_emit_nilad(((CHN_MEMOFF)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_modulo_once();
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((CHN_RM)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_gt();
 chn_emit_nilad(((14)+10));
 chn_emit_nilad(CHN_JMP);
 chn_emit_nilad(((2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((5)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
}
void chn_write_memory(){
 chn_emit_nilad(((CHN_MEMOFF)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_modulo_once();
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((CHN_RT3)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((CHN_RM)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_le();
 chn_emit_nilad(((25)+10));
 chn_emit_nilad(CHN_JMP);
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(((CHN_RM)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((1)+10));
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(((CHN_RM)+10));
 chn_emit_nilad(CHN_STORE);
 chn_emit_nilad(((CHN_RM)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_ne();
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(((25)+10));
 chn_emit_nilad(CHN_SUB);
 chn_emit_nilad(CHN_JMP);
 chn_emit_nilad(((CHN_RT3)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((CHN_RT2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((2)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(((5)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(CHN_STORE);
}
void chn_jump(int src_pc){
 chn_emit_nilad(((3)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((5)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(CHN_ADD);
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_var_length_num(src_pc+1);
 chn_emit_nilad(((3)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(((5)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_ADD);
 chn_emit_nilad(CHN_ADD);
 chn_emit_monad(CHN_LOAD,0);
 chn_emit_nilad(CHN_SUB);
 chn_emit_nilad(CHN_JMP);
}
static int chn_cur_pc=0;
static void init_state_chn(Data* data) {
 chn_init();
 for (int mp = 0; data; data = data->next, mp++) {
 if (data->v) {
 chn_emit_var_length_num(data->v);
 chn_emit_var_length_num(mp);
 chn_write_memory();
 }
 }
}
static void chn_emit_func_prologue(int func_id) {
}
static void chn_emit_func_epilogue(void) {
}
static void chn_emit_pc_change(int pc) {
 chn_cur_pc=pc;
 chn_emit_num_jmp(chn_len_code);
}
static void chn_load_val(Value*val){
 if(val->type==IMM)
 {
 chn_emit_var_length_num(val->imm);
 }else{
 chn_emit_nilad((((int)val->reg+6)+10));
 chn_emit_monad(CHN_LOAD,0);
 }
}
static void chn_store_reg(Value*val){
 chn_emit_nilad((((int)val->reg+6)+10));
 chn_emit_nilad(CHN_STORE);
}
static void chn_emit_inst(Inst* inst) {
 switch (inst->op) {
 case MOV:
 chn_load_val(&inst->src);
 chn_store_reg(&inst->dst);
 break;
 case ADD:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_emit_nilad(CHN_ADD);
 chn_modulo_once();
 chn_store_reg(&inst->dst);
 break;
 case SUB:
 chn_load_val(&inst->dst);
 chn_emit_nilad(((4)+10));
 chn_emit_monad(CHN_LOAD,0);
 chn_load_val(&inst->src);
 chn_emit_nilad(CHN_SUB);
 chn_emit_nilad(CHN_ADD);
 chn_modulo_once();
 chn_store_reg(&inst->dst);
 break;
 case LOAD:
 chn_load_val(&inst->src);
 chn_read_memory();
 chn_store_reg(&inst->dst);
 break;
 case STORE:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_write_memory();
 break;
 case PUTC:
 chn_load_val(&inst->src);
 chn_putc();
 break;
 case GETC:
 chn_getc();
 chn_store_reg(&inst->dst);
 break;
 case EXIT:
 chn_output();
 chn_exit();
 break;
 case DUMP:
 break;
 case EQ:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_eq();
 chn_emit_nilad(((0)+10));
 chn_emit_nilad(CHN_ADD);
 chn_store_reg(&inst->dst);
 break;
 case NE:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_ne();
 chn_store_reg(&inst->dst);
 break;
 case LT:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_lt();
 chn_store_reg(&inst->dst);
 break;
 case GT:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_gt();
 chn_store_reg(&inst->dst);
 break;
 case LE:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_le();
 chn_store_reg(&inst->dst);
 break;
 case GE:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_ge();
 chn_store_reg(&inst->dst);
 break;
 case JEQ:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_eq();
 chn_load_val(&inst->jmp);
 chn_jump(chn_cur_pc);
 break;
 case JNE:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_ne();
 chn_load_val(&inst->jmp);
 chn_jump(chn_cur_pc);
 break;
 case JLT:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_lt();
 chn_load_val(&inst->jmp);
 chn_jump(chn_cur_pc);
 break;
 case JGT:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_gt();
 chn_load_val(&inst->jmp);
 chn_jump(chn_cur_pc);
 break;
 case JLE:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_le();
 chn_load_val(&inst->jmp);
 chn_jump(chn_cur_pc);
 break;
 case JGE:
 chn_load_val(&inst->dst);
 chn_load_val(&inst->src);
 chn_ge();
 chn_load_val(&inst->jmp);
 chn_jump(chn_cur_pc);
 break;
 case JMP:
 chn_emit_nilad(((1)+10));
 chn_load_val(&inst->jmp);
 chn_jump(chn_cur_pc);
 break;
 default:
 error("oops");
 }
}
void target_chn(Module* module) {
 init_state_chn(module->data);
 int num_funcs = emit_chunked_main_loop(module->text,
 chn_emit_func_prologue,
 chn_emit_func_epilogue,
 chn_emit_pc_change,
 chn_emit_inst);
 chn_output();
 chn_exit();
 chn_emit_jump_table_initializer();
 chn_emit_whole_program();
}
