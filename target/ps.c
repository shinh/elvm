#include<ir/ir.h>
#include<target/util.h>

const char *ps_op_names[]={
    "eq", "ne", "lt", "gt", "le", "ge"
};

static void ps_init_state(int max_pc) {
    for(int i=0; i<7; i++){
        emit_line("/%s 0 def", reg_names[i]);
    }

    int ps_array_size_shift=15;
    int ps_array_size=1<<ps_array_size_shift;
    emit_line("/mem %d array def", 1 << (24 - ps_array_size_shift));
    emit_line("/zeros{");
    emit_line(" %d array dup", ps_array_size);
    emit_line(" 0 1 %d{", ps_array_size-1);
    emit_line("  0 put dup");
    emit_line(" }for");
    emit_line(" pop");
    emit_line("}def");
    emit_line("/mem_addr{");
    emit_line(" mem 1 index %d idiv get", ps_array_size);
    emit_line(" dup null eq{");
    emit_line("  pop zeros");
    emit_line("  mem 2 index %d idiv 2 index put", ps_array_size);
    emit_line(" }if");
    emit_line(" exch %d mod", ps_array_size);
    emit_line("}def");

    emit_line("/stdout (%%stdout) (w) file def");
    emit_line("/stdin (%%stdin) (r) file def");

    int num_funcs=max_pc/CHUNKED_FUNC_SIZE+1;
    emit_line("/func_table %d array def", num_funcs);
    emit_line("0 1 %d{", num_funcs-1);
    inc_indent();
    emit_line("func_table exch %d array put", CHUNKED_FUNC_SIZE);
    dec_indent();
    emit_line("}for");
}

static void ps_emit_func_prologue(int func_id) {
    emit_line("");
    emit_line("func_table %d get", func_id);
    // dummy
    emit_line("dup 0 {");
    inc_indent();
}

static void ps_emit_func_epilogue(void) {
    dec_indent();
    emit_line("} put");
    emit_line("pop");
}

static void ps_emit_pc_change(int pc) {
    dec_indent();
    emit_line("} put");
    emit_line("");
    emit_line("dup %d %d mod {", pc, CHUNKED_FUNC_SIZE);
    inc_indent();
}

static char* ps_value_str(Value* value) {
    switch(value->type){
        case REG:
            return format("%s", reg_names[value->reg]);
        case IMM:
            return format("%d", value->imm);
        default:
            error("invalid value");
            break;
    }
}

static void ps_emit_inst(Inst* inst) {
    int jumped=0;
    const char *reg_name;

    switch(inst->op){
        case MOV:
            emit_line("/%s %s def",
                    reg_names[inst->dst.reg], ps_value_str(&inst->src));
            break;
        case ADD:
            reg_name=reg_names[inst->dst.reg];
            emit_line("/%s %s %s add 16#FFFFFF and def",
                    reg_name, reg_name, ps_value_str(&inst->src));
            break;
        case SUB:
            reg_name=reg_names[inst->dst.reg];
            emit_line("/%s %s %s sub 16#FFFFFF and def",
                    reg_name, reg_name, ps_value_str(&inst->src));
            break;
        case LOAD:
            emit_line("/%s %s mem_addr get def",
                    reg_names[inst->dst.reg], ps_value_str(&inst->src));
            break;
        case STORE:
            emit_line("%s mem_addr %s put",
                    ps_value_str(&inst->src), reg_names[inst->dst.reg]);
            break;
        case EXIT:
            emit_line("quit");
            break;
        case JEQ:
        case JNE:
        case JLT:
        case JGT:
        case JLE:
        case JGE:
            emit_line("%s %s %s{",
                    reg_names[inst->dst.reg], ps_value_str(&inst->src),
                    ps_op_names[inst->op-JEQ]);
            emit_line(" /pc %s def", ps_value_str(&inst->jmp));
            emit_line("}{");
            if(inst->next) emit_line(" /pc %d def", inst->next->pc);
            emit_line("}ifelse");
            jumped=1;
            break;
        case JMP:
            emit_line("/pc %s def", ps_value_str(&inst->jmp));
            jumped=1;
            break;
        case PUTC:
            emit_line("stdout %s write", ps_value_str(&inst->src));
            break;
        case GETC:
            emit_line("/%s stdin read not{0}if def", reg_names[inst->dst.reg]);
            break;
        case EQ:
        case NE:
        case LT:
        case GT:
        case LE:
        case GE:
            reg_name=reg_names[inst->dst.reg];
            emit_line("/%s %s %s %s{1}{0}ifelse def",
                    reg_name, reg_name, ps_value_str(&inst->src),
                    ps_op_names[inst->op-EQ]);
            break;
        case DUMP:
            break;
        default:
            error("unknown operator");
            break;
    }
    if(!jumped && inst->next && inst->pc!=inst->next->pc){
        emit_line("/pc %d def", inst->next->pc);
    }
}

static int ps_max_pc(Inst* inst){
    int n=0;
    while(inst){
        if(inst->pc>n){
            n=inst->pc;
        }
        inst=inst->next;
    }
    return n;
}

void target_ps(Module *module) {
    ps_init_state(ps_max_pc(module->text));
    emit_chunked_main_loop(
            module->text,
            ps_emit_func_prologue,
            ps_emit_func_epilogue,
            ps_emit_pc_change,
            ps_emit_inst);
    emit_line("");

    Data *data=module->data;
    for(int mp=0; data; data=data->next, mp++){
        if(data->v){
            emit_line("%d mem_addr %d put", mp, data->v);
        }
    }
    emit_line("");

    emit_line("{");
    inc_indent();
    emit_line("func_table pc %d idiv get", CHUNKED_FUNC_SIZE);
    emit_line("pc %d mod get", CHUNKED_FUNC_SIZE);
    emit_line("exec");
    dec_indent();
    emit_line("}loop");
}
