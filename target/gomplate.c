#include <ir/ir.h>
#include <target/util.h>

#define INT64_MAX_STR "9223372036854775807"
#define CHAR_SIZE_STR "256"

// loop length = size ^ nest
#define RANGE_LOOP_NEST 6
#define RANGE_LOOP_SIZE 100

static const char* gomplate_value_str(Value *v) {
  if (v->type == REG) {
    return format("$%s", reg_names[v->reg]);
  } else if (v->type == IMM) {
    return format("%d", v->imm);
  } else {
    error("invalid src type");
  }
}

static const char* gomplate_cmp_str(Inst* inst) {
  int op = normalize_cond(inst->op, false);
  const char* fmt;
  switch (op) {
    case JEQ: fmt = "(eq $%s %s)"; break;
    case JNE: fmt = "(ne $%s %s)"; break;
    case JLT: fmt = "(lt $%s %s)"; break;
    case JGT: fmt = "(gt $%s %s)"; break;
    case JLE: fmt = "(le $%s %s)"; break;
    case JGE: fmt = "(ge $%s %s)"; break;
    default:
      error("oops");
  }
  return format(fmt, reg_names[inst->dst.reg], gomplate_value_str(&inst->src));
}

static void gomplate_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("{{- $%s = %s -}}", reg_names[inst->dst.reg], gomplate_value_str(&inst->src));
    break;

  case ADD:
    emit_line("{{- $%s = (math.Rem (math.Add (math.Rem (math.Add $%s %s) %d) %d) %d) -}}",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], gomplate_value_str(&inst->src),
              UINT_MAX + 1, UINT_MAX + 1, UINT_MAX + 1);
    break;

  case SUB:
    emit_line("{{- $%s = (math.Rem (math.Add (math.Rem (math.Sub $%s %s) %d) %d) %d) -}}",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], gomplate_value_str(&inst->src),
              UINT_MAX + 1, UINT_MAX + 1, UINT_MAX + 1);
    break;

  case LOAD:
    // coll.Index and coll.Has only handle string keys
    emit_str("{{- if coll.Has $mem (print %s) -}}", gomplate_value_str(&inst->src));
    emit_str("{{- $%s = $mem | coll.Index (print %s) -}}", reg_names[inst->dst.reg], gomplate_value_str(&inst->src));
    emit_str("{{- else -}}");
    emit_str("{{- $%s = 0 -}}", reg_names[inst->dst.reg]);
    emit_line("{{- end -}}");
    break;

  case STORE:
    emit_line("{{- $mem = $mem | coll.Merge (dict %s $%s) -}}", gomplate_value_str(&inst->src), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("{{- $out = printf \"%%s%%c\" $out (math.Rem %s " CHAR_SIZE_STR ") -}}", gomplate_value_str(&inst->src));
    break;

  case GETC:
    // read stdin if not yet
    emit_line("{{- if (eq $in \"\") -}}{{- $in = (datasource \"data\") -}}{{- end -}}");
    emit_line("{{- if (lt $cursor (len $in)) -}}{{- $%s = $in | coll.Index $cursor -}}{{- $cursor = math.Add $cursor 1 -}}{{- else -}}{{- $%s = 0 -}}{{- end -}}",
              reg_names[inst->dst.reg], reg_names[inst->dst.reg]);
    break;

  case EXIT:
    // render the whole buffer $out at the end
    // NOTE: $out should be rendered only by first EXIT
    emit_line("{{- if not $exited -}}{{$out}}{{- end -}}");
    emit_line("{{- $exited = true -}}");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("{{- if %s -}}{{- $%s = 1 -}}{{- else -}}{{- $%s = 0 -}}{{- end -}}",
              gomplate_cmp_str(inst), reg_names[inst->dst.reg], reg_names[inst->dst.reg]);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("{{- if %s -}}{{- $pc = math.Sub %s 1 -}}{{- end -}}",
              gomplate_cmp_str(inst), gomplate_value_str(&inst->jmp));
    break;

  case JMP:
    emit_line("{{- $pc = math.Sub %s 1 -}}", gomplate_value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

static void gomplate_init_state(Data* data) {
  // stdin should not be read until it is required, otherwise program will hang up
  emit_line("{{- $in := \"\" -}}");
  emit_line("{{- $out := \"\" -}}");
  emit_line("{{- $cursor := 0 -}}");

  // registers
  for (int i = 0; i < 7; i++) {
    emit_line("{{- $%s := 0 -}}", reg_names[i]);
  }

  // memory
  emit_str("{{- $mem :=  coll.Dict ");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_str("%d %d ", mp, data->v);
    }
  }
  emit_line("-}}");
}

static void gomplate_unpack_vars() {
  emit_line("{{- $in := $ | coll.Index \"in\" -}}");
  emit_line("{{- $out := $ | coll.Index \"out\" -}}");
  emit_line("{{- $cursor := $ | coll.Index \"cursor\" -}}");
  emit_line("{{- $mem := $ | coll.Index \"mem\" -}}");
  for (int i = 0; i < 7; i++) {
    emit_line("{{- $%s := $ | coll.Index \"%s\" -}}", reg_names[i], reg_names[i]);
  }
}

static void gomplate_pack_vars() {
  emit_str("{{- $args := coll.Dict ");
  emit_str("\"in\" $in ");
  emit_str("\"out\" $out ");
  emit_str("\"cursor\" $cursor ");
  emit_str("\"mem\" $mem ");
  for (int i = 0; i < 7; i++) {
    emit_str("\"%s\" $%s ", reg_names[i], reg_names[i]);
  }
  emit_line("-}}");
}

static void gomplate_start_range() {
  for (int i = 0; i < RANGE_LOOP_NEST; i++) {
    emit_line("{{- range math.Seq %d -}}", RANGE_LOOP_SIZE);
  }
}

static void gomplate_end_range() {
  for (int i = 0; i < RANGE_LOOP_NEST; i++) {
    emit_line("{{- if $exited -}}{{- break -}}{{- end -}}{{- end -}}");
  }
}

void target_gomplate(Module* module) {
  emit_line("{{- define \"Step\" -}}");
  inc_indent();
  gomplate_unpack_vars();
  emit_line("{{- $exited := false -}}");

  // NOTE: use range instead of template recursion to avoid stack memory limit
  emit_line("");
  gomplate_start_range();
  inc_indent();

  emit_line("{{- if false -}}");
  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      dec_indent();
      emit_line("{{- else if (eq $pc %d) -}}", inst->pc);
      prev_pc = inst->pc;
      inc_indent();
    }
    gomplate_emit_inst(inst);
  }
  // end of if
  emit_line("{{- end -}}");
  emit_line("{{- $pc = (math.Add $pc 1) -}}");

  // end of range
  dec_indent();
  gomplate_end_range();

  // call recursively if loop ends
  emit_line("");
  emit_line("{{- if not $exited -}}");
  inc_indent();
  gomplate_pack_vars();
  emit_line("{{- tmpl.Exec \"Step\" $args -}}");
  dec_indent();
  emit_line("{{- end -}}");

  // end of define
  dec_indent();
  emit_line("{{- end -}}");

  // main
  emit_line("");
  gomplate_init_state(module->data);
  gomplate_pack_vars();
  emit_line("{{- tmpl.Exec \"Step\" $args -}}");
}
