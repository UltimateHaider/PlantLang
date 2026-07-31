#include <plant_compat.h>
tx_t make_node(tx_t ty, tx_t val, PlantArray* kids);
tx_t make_leaf(tx_t ty, tx_t val);
tx_t make_unary(tx_t ty, tx_t op, tx_t operand);
tx_t make_binary(tx_t ty, tx_t op, tx_t left, tx_t right);
tx_t is_keyword(tx_t wrd);
tx_t keyword_to_type(tx_t wrd);
tx_t char_type(tx_t ch);
tx_t is_alnum(tx_t ch);
tx_t is_alpha_start(tx_t ch);
PlantArray* match_ident_or_keyword(tx_t src, long i, long n);
PlantArray* match_number(tx_t src, long i, long n);
PlantArray* match_string(tx_t src, long i, long n);
long skip_comment(tx_t src, long i, long n);
PlantArray* scan_tokens(tx_t src);
tx_t tok_lex(PlantArray* tok);
tx_t tok_type(PlantArray* tok);
PlantArray* peek(PlantArray* tokens, long pos);
PlantArray* consume(PlantArray* tokens, long pos);
tx_t _first(PlantArray* pair);
long _second(PlantArray* pair);
tx_t is_eof(PlantArray* tokens, long pos);
PlantArray* collect_value(PlantArray* tokens, long start);
PlantArray* collect_until(PlantArray* tokens, long start, tx_t delim);
PlantArray* parse_create_stmt(PlantArray* tokens, long pos);
PlantArray* parse_show_stmt(PlantArray* tokens, long pos);
PlantArray* parse_give_stmt(PlantArray* tokens, long pos);
PlantArray* parse_set_stmt(PlantArray* tokens, long pos);
PlantArray* parse_let_stmt(PlantArray* tokens, long pos);
PlantArray* parse_reap_stmt(PlantArray* tokens, long pos);
PlantArray* parse_put_stmt(PlantArray* tokens, long pos);
PlantArray* parse_break_stmt(PlantArray* tokens, long pos);
PlantArray* parse_continue_stmt(PlantArray* tokens, long pos);
PlantArray* parse_if_stmt(PlantArray* tokens, long pos);
PlantArray* parse_season_stmt(PlantArray* tokens, long pos);
PlantArray* parse_statement(PlantArray* tokens, long pos);
PlantArray* parse_enum_decl(PlantArray* tokens, long pos);
PlantArray* parse_action_decl(PlantArray* tokens, long pos);
PlantArray* parse_declaration(PlantArray* tokens, long pos);
PlantArray* parse_program(PlantArray* tokens);
tx_t _substr(tx_t str, long start, long end);
tx_t _handle_func(tx_t expr, tx_t kw, tx_t cfn);
tx_t _handle_func_paren(tx_t expr, tx_t kw, tx_t cfn);
tx_t translate_expr(tx_t expr);
tx_t indent_str(long level);
tx_t generate_body(PlantArray* bd, long indent);
tx_t generate_node(tx_t node, long indent);
tx_t generate_c(PlantArray* ast);

tx_t make_node(tx_t ty, tx_t val, PlantArray* kids) {
  return plant_list_make((int64_t)(6), "type", ty, "value", val, "children", kids);
}

tx_t make_leaf(tx_t ty, tx_t val) {
  return make_node(ty,val,plant_list_make((int64_t)(0)));
}

tx_t make_unary(tx_t ty, tx_t op, tx_t operand) {
  return make_node(ty,op,plant_list_make((int64_t)(1), operand));
}

tx_t make_binary(tx_t ty, tx_t op, tx_t left, tx_t right) {
  return make_node(ty,op,plant_list_make((int64_t)(2), left, right));
}

tx_t is_keyword(tx_t wrd) {
  if (strcmp(wrd, "LET") == 0) {
    return 1;
  }
  if (strcmp(wrd, "CREATE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "MATCH") == 0) {
    return 1;
  }
  if (strcmp(wrd, "IF") == 0) {
    return 1;
  }
  if (strcmp(wrd, "ELSE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "LOOP") == 0) {
    return 1;
  }
  if (strcmp(wrd, "RETURN") == 0) {
    return 1;
  }
  if (strcmp(wrd, "TYPE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "SHOW") == 0) {
    return 1;
  }
  if (strcmp(wrd, "ACTION") == 0) {
    return 1;
  }
  if (strcmp(wrd, "REAP") == 0) {
    return 1;
  }
  if (strcmp(wrd, "GIVE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "SET") == 0) {
    return 1;
  }
  if (strcmp(wrd, "PUT") == 0) {
    return 1;
  }
  if (strcmp(wrd, "TAKE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "FOR") == 0) {
    return 1;
  }
  if (strcmp(wrd, "CYCLE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "SEASON") == 0) {
    return 1;
  }
  if (strcmp(wrd, "WEATHER") == 0) {
    return 1;
  }
  if (strcmp(wrd, "SHELTER") == 0) {
    return 1;
  }
  if (strcmp(wrd, "CALM") == 0) {
    return 1;
  }
  if (strcmp(wrd, "AND") == 0) {
    return 1;
  }
  if (strcmp(wrd, "OR") == 0) {
    return 1;
  }
  if (strcmp(wrd, "NOT") == 0) {
    return 1;
  }
  if (strcmp(wrd, "IS") == 0) {
    return 1;
  }
  if (strcmp(wrd, "TRUE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "FALSE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "TO") == 0) {
    return 1;
  }
  if (strcmp(wrd, "FROM") == 0) {
    return 1;
  }
  if (strcmp(wrd, "AS") == 0) {
    return 1;
  }
  if (strcmp(wrd, "IN") == 0) {
    return 1;
  }
  if (strcmp(wrd, "SPLIT") == 0) {
    return 1;
  }
  if (strcmp(wrd, "JOIN") == 0) {
    return 1;
  }
  if (strcmp(wrd, "ENUM") == 0) {
    return 1;
  }
  if (strcmp(wrd, "CONST") == 0) {
    return 1;
  }
  if (strcmp(wrd, "FUNCTION") == 0) {
    return 1;
  }
  if (strcmp(wrd, "IMPORT") == 0) {
    return 1;
  }
  if (strcmp(wrd, "OPTION") == 0) {
    return 1;
  }
  if (strcmp(wrd, "RESULT") == 0) {
    return 1;
  }
  if (strcmp(wrd, "GREATER") == 0) {
    return 1;
  }
  if (strcmp(wrd, "LESS") == 0) {
    return 1;
  }
  if (strcmp(wrd, "THAN") == 0) {
    return 1;
  }
  if (strcmp(wrd, "COUNT") == 0) {
    return 1;
  }
  if (strcmp(wrd, "FIRST") == 0) {
    return 1;
  }
  if (strcmp(wrd, "LAST") == 0) {
    return 1;
  }
  if (strcmp(wrd, "SUM") == 0) {
    return 1;
  }
  if (strcmp(wrd, "NULL") == 0) {
    return 1;
  }
  if (strcmp(wrd, "ISNT") == 0) {
    return 1;
  }
  if (strcmp(wrd, "TEST") == 0) {
    return 1;
  }
  if (strcmp(wrd, "BREAK") == 0) {
    return 1;
  }
  if (strcmp(wrd, "CONTINUE") == 0) {
    return 1;
  }
  if (strcmp(wrd, "INTO") == 0) {
    return 1;
  }
  return 0;
}

tx_t keyword_to_type(tx_t wrd) {
  if (strcmp(wrd, "LET") == 0) {
    return "LET";
  }
  if (strcmp(wrd, "CREATE") == 0) {
    return "CREATE";
  }
  if (strcmp(wrd, "MATCH") == 0) {
    return "MATCH";
  }
  if (strcmp(wrd, "IF") == 0) {
    return "IF";
  }
  if (strcmp(wrd, "ELSE") == 0) {
    return "ELSE";
  }
  if (strcmp(wrd, "LOOP") == 0) {
    return "LOOP";
  }
  if (strcmp(wrd, "RETURN") == 0) {
    return "RETURN";
  }
  if (strcmp(wrd, "TYPE") == 0) {
    return "TYPE";
  }
  if (strcmp(wrd, "SHOW") == 0) {
    return "SHOW";
  }
  if (strcmp(wrd, "ACTION") == 0) {
    return "ACTION";
  }
  if (strcmp(wrd, "REAP") == 0) {
    return "REAP";
  }
  if (strcmp(wrd, "GIVE") == 0) {
    return "GIVE";
  }
  if (strcmp(wrd, "SET") == 0) {
    return "SET";
  }
  if (strcmp(wrd, "PUT") == 0) {
    return "PUT";
  }
  if (strcmp(wrd, "TAKE") == 0) {
    return "TAKE";
  }
  if (strcmp(wrd, "FOR") == 0) {
    return "FOR";
  }
  if (strcmp(wrd, "CYCLE") == 0) {
    return "CYCLE";
  }
  if (strcmp(wrd, "SEASON") == 0) {
    return "SEASON";
  }
  if (strcmp(wrd, "WEATHER") == 0) {
    return "WEATHER";
  }
  if (strcmp(wrd, "SHELTER") == 0) {
    return "SHELTER";
  }
  if (strcmp(wrd, "CALM") == 0) {
    return "CALM";
  }
  if (strcmp(wrd, "AND") == 0) {
    return "AND";
  }
  if (strcmp(wrd, "OR") == 0) {
    return "OR";
  }
  if (strcmp(wrd, "NOT") == 0) {
    return "NOT";
  }
  if (strcmp(wrd, "IS") == 0) {
    return "IS";
  }
  if (strcmp(wrd, "TRUE") == 0) {
    return "TRUE";
  }
  if (strcmp(wrd, "FALSE") == 0) {
    return "FALSE";
  }
  if (strcmp(wrd, "TO") == 0) {
    return "TO";
  }
  if (strcmp(wrd, "FROM") == 0) {
    return "FROM";
  }
  if (strcmp(wrd, "AS") == 0) {
    return "AS";
  }
  if (strcmp(wrd, "IN") == 0) {
    return "IN";
  }
  if (strcmp(wrd, "SPLIT") == 0) {
    return "SPLIT";
  }
  if (strcmp(wrd, "JOIN") == 0) {
    return "JOIN";
  }
  if (strcmp(wrd, "ENUM") == 0) {
    return "ENUM";
  }
  if (strcmp(wrd, "CONST") == 0) {
    return "CONST";
  }
  if (strcmp(wrd, "FUNCTION") == 0) {
    return "FUNCTION";
  }
  if (strcmp(wrd, "IMPORT") == 0) {
    return "IMPORT";
  }
  if (strcmp(wrd, "OPTION") == 0) {
    return "OPTION";
  }
  if (strcmp(wrd, "RESULT") == 0) {
    return "RESULT";
  }
  if (strcmp(wrd, "GREATER") == 0) {
    return "GREATER";
  }
  if (strcmp(wrd, "LESS") == 0) {
    return "LESS";
  }
  if (strcmp(wrd, "THAN") == 0) {
    return "THAN";
  }
  if (strcmp(wrd, "COUNT") == 0) {
    return "COUNT";
  }
  if (strcmp(wrd, "FIRST") == 0) {
    return "FIRST";
  }
  if (strcmp(wrd, "LAST") == 0) {
    return "LAST";
  }
  if (strcmp(wrd, "SUM") == 0) {
    return "SUM";
  }
  if (strcmp(wrd, "NULL") == 0) {
    return "NULL";
  }
  if (strcmp(wrd, "ISNT") == 0) {
    return "ISNT";
  }
  if (strcmp(wrd, "TEST") == 0) {
    return "TEST";
  }
  if (strcmp(wrd, "BREAK") == 0) {
    return "BREAK";
  }
  if (strcmp(wrd, "CONTINUE") == 0) {
    return "CONTINUE";
  }
  if (strcmp(wrd, "INTO") == 0) {
    return "INTO";
  }
  return "IDENT";
}

tx_t char_type(tx_t ch) {
  if (strcmp(ch, "+") == 0) {
    return "PLUS";
  }
  if (strcmp(ch, "-") == 0) {
    return "MINUS";
  }
  if (strcmp(ch, "*") == 0) {
    return "STAR";
  }
  if (strcmp(ch, "/") == 0) {
    return "SLASH";
  }
  if (strcmp(ch, "%") == 0) {
    return "PERCENT";
  }
  if (strcmp(ch, "=") == 0) {
    return "EQUAL";
  }
  if (strcmp(ch, ":") == 0) {
    return "COLON";
  }
  if (strcmp(ch, "<") == 0) {
    return "LESS";
  }
  if (strcmp(ch, ">") == 0) {
    return "GREATER";
  }
  if (strcmp(ch, "!") == 0) {
    return "BANG";
  }
  if (strcmp(ch, "?") == 0) {
    return "QUESTION";
  }
  if (strcmp(ch, "&") == 0) {
    return "AMPERSAND";
  }
  if (strcmp(ch, "|") == 0) {
    return "PIPE";
  }
  if (strcmp(ch, ";") == 0) {
    return "SEMI";
  }
  if (strcmp(ch, "(") == 0) {
    return "LPAREN";
  }
  if (strcmp(ch, ")") == 0) {
    return "RPAREN";
  }
  if (strcmp(ch, "{") == 0) {
    return "LBRACE";
  }
  if (strcmp(ch, "}") == 0) {
    return "RBRACE";
  }
  if (strcmp(ch, "[") == 0) {
    return "LBRACKET";
  }
  if (strcmp(ch, "]") == 0) {
    return "RBRACKET";
  }
  if (strcmp(ch, ",") == 0) {
    return "COMMA";
  }
  if (strcmp(ch, ".") == 0) {
    return "DOT";
  }
  if (strcmp(ch, "_") == 0) {
    return "WILDCARD";
  }
  if (strcmp(ch, "#") == 0) {
    return "HASH";
  }
  return "";
}

tx_t is_alnum(tx_t ch) {
  if (strcmp(ch, "a") >= 0 && strcmp(ch, "z") <= 0) {
    return 1;
  }
  if (strcmp(ch, "A") >= 0 && strcmp(ch, "Z") <= 0) {
    return 1;
  }
  if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0) {
    return 1;
  }
  if (strcmp(ch, "_") == 0) {
    return 1;
  }
  return 0;
}

tx_t is_alpha_start(tx_t ch) {
  if (strcmp(ch, "a") >= 0 && strcmp(ch, "z") <= 0) {
    return 1;
  }
  if (strcmp(ch, "A") >= 0 && strcmp(ch, "Z") <= 0) {
    return 1;
  }
  if (strcmp(ch, "_") == 0) {
    return 1;
  }
  return 0;
}

PlantArray* match_ident_or_keyword(tx_t src, long i, long n) {
  tx_t wd = "";
  long ni = 0;
  tx_t ok = "";
  wd = "";
  ni = i;
  while (ni<n) {
    ok = is_alnum(_at(src, ni));
    if (!ok) {
      break;
    }
    wd = _cat(wd, _at(src, ni));
    ni = ni+1;
  }
  return plant_list_make((int64_t)(2), wd, ni);
}

PlantArray* match_number(tx_t src, long i, long n) {
  tx_t num = "";
  long ni = 0;
  num = "";
  ni = i;
  while (ni<n && strcmp(_at(src, ni), "0") >= 0 && strcmp(_at(src, ni), "9") <= 0) {
    num = _cat(num, _at(src, ni));
    ni = ni+1;
  }
  return plant_list_make((int64_t)(2), num, ni);
}

PlantArray* match_string(tx_t src, long i, long n) {
  tx_t val = "";
  long si = 0;
  int done = "";
  val = "";
  si = i;
  done = 0;
  if (si<n && strcmp(_at(src, si), "\"") == 0) {
    si = si+1;
  }
  while (!done && si<n) {
    if (strcmp(_at(src, si), "\"") == 0) {
      done = 1;
    }
    if (strcmp(_at(src, si), "\"") == 0) {
      continue;
    }
    if (strcmp(_at(src, si), "\\") == 0) {
      si = si+1;
    }
    if (si<n) {
      val = _cat(val, _at(src, si));
    }
    if (si<n) {
      si = si+1;
    }
  }
  if (si<n && strcmp(_at(src, si), "\"") == 0) {
    si = si+1;
  }
  return plant_list_make((int64_t)(2), val, si);
}

long skip_comment(tx_t src, long i, long n) {
  while (i<n && strcmp(_at(src, i), "\n") != 0) {
    i = i+1;
  }
  return i;
}

PlantArray* scan_tokens(tx_t src) {
  PlantArray* tokens = NULL;
  long i = 0;
  long n = 0;
  tx_t ch = "";
  PlantArray* si = NULL;
  tx_t ok = "";
  tx_t tok_ty = "";
  tokens = plant_list_make((int64_t)(0));
  i = 0;
  n = strlen(src);
  while (i<n) {
    ch = _at(src, i);
    if (strcmp(ch, " ") == 0 || strcmp(ch, "\t") == 0 || strcmp(ch, "\r") == 0) {
      i = i+1;
    }
    if (strcmp(ch, " ") == 0 || strcmp(ch, "\t") == 0 || strcmp(ch, "\r") == 0) {
      continue;
    }
    if (strcmp(ch, "\n") == 0) {
      i = i+1;
    }
    if (strcmp(ch, "\n") == 0) {
      continue;
    }
    if (strcmp(ch, "#") == 0) {
      si = skip_comment(src, i+1, n);
    }
    if (strcmp(ch, "#") == 0) {
      i = si;
    }
    if (strcmp(ch, "#") == 0) {
      continue;
    }
    if (strcmp(ch, "\"") == 0) {
      si = match_string(src, i, n);
    }
    if (strcmp(ch, "\"") == 0) {
      tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), "STRING", plant_list_get(si, 0)));
    }
    if (strcmp(ch, "\"") == 0) {
      i = plant_list_get(si, 1);
    }
    if (strcmp(ch, "\"") == 0) {
      continue;
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0 && i+1<n && strcmp(_at(src, i+1), "\\") == 0) {
      si = match_number(src, i, n);
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0 && i+1<n && strcmp(_at(src, i+1), "\\") == 0) {
      tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), "DEPTH", plant_list_get(si, 0)));
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0 && i+1<n && strcmp(_at(src, i+1), "\\") == 0) {
      i = plant_list_get(si, 1)+1;
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0 && i+1<n && strcmp(_at(src, i+1), "\\") == 0 && i<n && strcmp(_at(src, i), " ") == 0) {
      i = i+1;
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0 && i+1<n && strcmp(_at(src, i+1), "\\") == 0) {
      continue;
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0) {
      si = match_number(src, i, n);
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0) {
      tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), "NUMBER", plant_list_get(si, 0)));
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0) {
      i = plant_list_get(si, 1);
    }
    if (strcmp(ch, "0") >= 0 && strcmp(ch, "9") <= 0) {
      continue;
    }
    ok = is_alpha_start(ch);
    if (ok) {
      si = match_ident_or_keyword(src, i, n);
    }
    if (ok) {
      tok_ty = keyword_to_type(plant_list_get(si, 0));
    }
    if (ok) {
      tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), tok_ty, plant_list_get(si, 0)));
    }
    if (ok) {
      i = plant_list_get(si, 1);
    }
    if (ok) {
      continue;
    }
    if (strcmp(ch, ".") == 0 && i+1<n && strcmp(_at(src, i+1), ".") == 0) {
      tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), "DOT_DOT", ".."));
    }
    if (strcmp(ch, ".") == 0 && i+1<n && strcmp(_at(src, i+1), ".") == 0) {
      i = i+2;
    }
    if (strcmp(ch, ".") == 0 && i+1<n && strcmp(_at(src, i+1), ".") == 0) {
      continue;
    }
    if (strcmp(ch, "-") == 0 && i+1<n && strcmp(_at(src, i+1), ">") == 0) {
      tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), "ARROW", "->"));
    }
    if (strcmp(ch, "-") == 0 && i+1<n && strcmp(_at(src, i+1), ">") == 0) {
      i = i+2;
    }
    if (strcmp(ch, "-") == 0 && i+1<n && strcmp(_at(src, i+1), ">") == 0) {
      continue;
    }
    if (strcmp(ch, "*") == 0 && i+1<n && strcmp(_at(src, i+1), "*") == 0) {
      tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), "STAR_STAR", "**"));
    }
    if (strcmp(ch, "*") == 0 && i+1<n && strcmp(_at(src, i+1), "*") == 0) {
      i = i+2;
    }
    if (strcmp(ch, "*") == 0 && i+1<n && strcmp(_at(src, i+1), "*") == 0) {
      continue;
    }
    tok_ty = char_type(ch);
    if (strcmp(tok_ty, "") != 0) {
      tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), tok_ty, ch));
    }
    if (strcmp(tok_ty, "") != 0) {
      i = i+1;
    }
    if (strcmp(tok_ty, "") != 0) {
      continue;
    }
    tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), "ERROR", ch));
    i = i+1;
  }
  tokens = plant_list_push(tokens, plant_list_make((int64_t)(2), "EOF", ""));
  return tokens;
}

tx_t tok_lex(PlantArray* tok) {
  return plant_list_get(tok, 1);
}

tx_t tok_type(PlantArray* tok) {
  return plant_list_get(tok, 0);
}

PlantArray* peek(PlantArray* tokens, long pos) {
  if (pos<(tokens)->count) {
    return plant_list_get(tokens, pos);
  }
  return plant_list_make((int64_t)(2), NULL, "");
}

PlantArray* consume(PlantArray* tokens, long pos) {
  return plant_list_make((int64_t)(2), plant_list_get(tokens, pos), pos+1);
}

tx_t _first(PlantArray* pair) {
  return plant_list_get(pair, 0);
}

long _second(PlantArray* pair) {
  return plant_list_get(pair, 1);
}

tx_t is_eof(PlantArray* tokens, long pos) {
  PlantArray* tok = NULL;
  tx_t tp = "";
  if (pos>=(tokens)->count) {
    return 1;
  }
  tok = peek(tokens, pos);
  tp = tok_type(tok);
  if (strcmp(tp, "EOF") == 0) {
    return 1;
  }
  return 0;
}

PlantArray* collect_value(PlantArray* tokens, long start) {
  tx_t text = "";
  long p2 = 0;
  long depth = 0;
  tx_t is_eof_flag = "";
  PlantArray* tok = NULL;
  tx_t lx = "";
  PlantArray* cpair = NULL;
  text = "";
  p2 = start;
  depth = 0;
  while (1) {
    is_eof_flag = is_eof(tokens, p2);
    if (is_eof_flag) {
      return plant_list_make((int64_t)(2), text, p2);
    }
    tok = peek(tokens, p2);
    lx = tok_lex(tok);
    if (strcmp(lx, ".") == 0 && depth == 0) {
      cpair = consume(tokens, p2);
      p2 = _second(cpair);
      return plant_list_make((int64_t)(2), text, p2);
    }
    if (strcmp(lx, "(") == 0) {
      depth = depth+1;
    }
    if (strcmp(lx, ")") == 0) {
      depth = depth-1;
    }
    if (strcmp(text, "") > 0) {
      text = _cat(text, " ");
    }
    text = _cat(text, lx);
    cpair = consume(tokens, p2);
    p2 = _second(cpair);
  }
}

PlantArray* collect_until(PlantArray* tokens, long start, tx_t delim) {
  tx_t text = "";
  long p2 = 0;
  long depth = 0;
  tx_t is_eof_flag = "";
  PlantArray* tok = NULL;
  tx_t lx = "";
  PlantArray* cpair = NULL;
  text = "";
  p2 = start;
  depth = 0;
  while (1) {
    is_eof_flag = is_eof(tokens, p2);
    if (is_eof_flag) {
      return plant_list_make((int64_t)(2), text, p2);
    }
    tok = peek(tokens, p2);
    lx = tok_lex(tok);
    if (lx == delim && depth == 0) {
      return plant_list_make((int64_t)(2), text, p2);
    }
    if (strcmp(lx, "(") == 0) {
      depth = depth+1;
    }
    if (strcmp(lx, ")") == 0) {
      depth = depth-1;
    }
    if (strcmp(text, "") > 0) {
      text = _cat(text, " ");
    }
    text = _cat(text, lx);
    cpair = consume(tokens, p2);
    p2 = _second(cpair);
  }
}

PlantArray* parse_create_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* id_pair = NULL;
  tx_t id_name = "";
  long p3 = 0;
  PlantArray* tok = NULL;
  tx_t lx = "";
  tx_t vtype = "";
  PlantArray* lp = NULL;
  long p4 = 0;
  PlantArray* tp = NULL;
  tx_t tp_name = "";
  long p5 = 0;
  PlantArray* rp = NULL;
  long p6 = 0;
  PlantArray* tok2 = NULL;
  tx_t lx2 = "";
  PlantArray* eq_pair = NULL;
  PlantArray* vpair = NULL;
  tx_t expr = "";
  PlantArray* to_pair = NULL;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  id_pair = consume(tokens, p2);
  id_name = tok_lex(plant_list_get(id_pair, 0));
  p3 = _second(id_pair);
  tok = peek(tokens, p3);
  lx = tok_lex(tok);
  vtype = "";
  if (strcmp(lx, "(") == 0) {
    lp = consume(tokens, p3);
    p4 = _second(lp);
    tp = consume(tokens, p4);
    tp_name = tok_lex(plant_list_get(tp, 0));
    p5 = _second(tp);
    rp = consume(tokens, p5);
    p6 = _second(rp);
    vtype = tp_name;
    p3 = p6;
  }
  tok2 = peek(tokens, p3);
  lx2 = tok_lex(tok2);
  if (strcmp(lx2, "=") == 0) {
    eq_pair = consume(tokens, p3);
    p4 = _second(eq_pair);
    vpair = collect_value(tokens, p4);
    expr = plant_list_get(vpair, 0);
    p5 = _second(vpair);
    return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "create_stmt", "target", id_name, "var_type", vtype, "value", expr), p5);
  }
  if (strcmp(lx2, "TO") == 0) {
    to_pair = consume(tokens, p3);
    p4 = _second(to_pair);
    vpair = collect_value(tokens, p4);
    expr = plant_list_get(vpair, 0);
    p5 = _second(vpair);
    return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "create_stmt", "target", id_name, "var_type", vtype, "value", expr), p5);
  }
  vpair = collect_value(tokens, p3);
  expr = plant_list_get(vpair, 0);
  p4 = _second(vpair);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "create_stmt", "target", id_name, "var_type", vtype, "value", expr), p4);
}

PlantArray* parse_show_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* vpair = NULL;
  tx_t expr = "";
  long p3 = 0;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  vpair = collect_value(tokens, p2);
  expr = plant_list_get(vpair, 0);
  p3 = _second(vpair);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(4), "type", "show_stmt", "value", expr), p3);
}

PlantArray* parse_give_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* vpair = NULL;
  tx_t expr = "";
  long p3 = 0;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  vpair = collect_value(tokens, p2);
  expr = plant_list_get(vpair, 0);
  p3 = _second(vpair);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(4), "type", "give_stmt", "value", expr), p3);
}

PlantArray* parse_set_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* id_pair = NULL;
  tx_t id_name = "";
  long p3 = 0;
  PlantArray* eq = NULL;
  long p4 = 0;
  PlantArray* vpair = NULL;
  tx_t expr = "";
  long p5 = 0;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  id_pair = consume(tokens, p2);
  id_name = tok_lex(plant_list_get(id_pair, 0));
  p3 = _second(id_pair);
  eq = consume(tokens, p3);
  p4 = _second(eq);
  vpair = collect_value(tokens, p4);
  expr = plant_list_get(vpair, 0);
  p5 = _second(vpair);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(6), "type", "set_stmt", "target", id_name, "value", expr), p5);
}

PlantArray* parse_let_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* id_pair = NULL;
  tx_t id_name = "";
  long p3 = 0;
  PlantArray* tok = NULL;
  tx_t lx = "";
  tx_t vtype = "";
  PlantArray* lp = NULL;
  long p4 = 0;
  PlantArray* tp = NULL;
  tx_t tp_name = "";
  long p5 = 0;
  PlantArray* rp = NULL;
  long p6 = 0;
  PlantArray* tok2 = NULL;
  tx_t lx2 = "";
  PlantArray* eq_pair = NULL;
  PlantArray* vpair = NULL;
  tx_t expr = "";
  PlantArray* to_pair = NULL;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  id_pair = consume(tokens, p2);
  id_name = tok_lex(plant_list_get(id_pair, 0));
  p3 = _second(id_pair);
  tok = peek(tokens, p3);
  lx = tok_lex(tok);
  vtype = "";
  if (strcmp(lx, "(") == 0) {
    lp = consume(tokens, p3);
    p4 = _second(lp);
    tp = consume(tokens, p4);
    tp_name = tok_lex(plant_list_get(tp, 0));
    p5 = _second(tp);
    rp = consume(tokens, p5);
    p6 = _second(rp);
    vtype = tp_name;
    p3 = p6;
  }
  tok2 = peek(tokens, p3);
  lx2 = tok_lex(tok2);
  if (strcmp(lx2, "=") == 0) {
    eq_pair = consume(tokens, p3);
    p4 = _second(eq_pair);
    vpair = collect_value(tokens, p4);
    expr = plant_list_get(vpair, 0);
    p5 = _second(vpair);
    return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "let_stmt", "target", id_name, "var_type", vtype, "value", expr), p5);
  }
  if (strcmp(lx2, "TO") == 0) {
    to_pair = consume(tokens, p3);
    p4 = _second(to_pair);
    vpair = collect_value(tokens, p4);
    expr = plant_list_get(vpair, 0);
    p5 = _second(vpair);
    return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "let_stmt", "target", id_name, "var_type", vtype, "value", expr), p5);
  }
  vpair = collect_value(tokens, p3);
  expr = plant_list_get(vpair, 0);
  p4 = _second(vpair);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "let_stmt", "target", id_name, "var_type", vtype, "value", expr), p4);
}

PlantArray* parse_reap_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* var_pair = NULL;
  tx_t var_name = "";
  long p3 = 0;
  PlantArray* from_pair = NULL;
  long p4 = 0;
  PlantArray* act_pair = NULL;
  tx_t act_name = "";
  long p5 = 0;
  PlantArray* args = NULL;
  PlantArray* tok0 = NULL;
  tx_t lx0 = "";
  PlantArray* com0 = NULL;
  tx_t is_eof_flag = "";
  PlantArray* tok = NULL;
  tx_t lx = "";
  PlantArray* dot = NULL;
  long p6 = 0;
  PlantArray* vpair = NULL;
  tx_t expr = "";
  PlantArray* tok2 = NULL;
  tx_t lx2 = "";
  PlantArray* com = NULL;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  var_pair = consume(tokens, p2);
  var_name = tok_lex(plant_list_get(var_pair, 0));
  p3 = _second(var_pair);
  from_pair = consume(tokens, p3);
  p4 = _second(from_pair);
  act_pair = consume(tokens, p4);
  act_name = tok_lex(plant_list_get(act_pair, 0));
  p5 = _second(act_pair);
  args = plant_list_make((int64_t)(0));
  while (1) {
    tok0 = peek(tokens, p5);
    lx0 = tok_lex(tok0);
    if (strcmp(lx0, ",") == 0) {
      com0 = consume(tokens, p5);
      p5 = _second(com0);
    }
    is_eof_flag = is_eof(tokens, p5);
    if (is_eof_flag) {
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "reap_stmt", "target", var_name, "action", act_name, "args", args), p5);
    }
    tok = peek(tokens, p5);
    lx = tok_lex(tok);
    if (strcmp(lx, ".") == 0) {
      dot = consume(tokens, p5);
      p6 = _second(dot);
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "reap_stmt", "target", var_name, "action", act_name, "args", args), p6);
    }
    vpair = collect_value(tokens, p5);
    expr = plant_list_get(vpair, 0);
    p6 = _second(vpair);
    args = plant_list_push(args, expr);
    p5 = p6;
    tok2 = peek(tokens, p5);
    lx2 = tok_lex(tok2);
    if (strcmp(lx2, ",") == 0) {
      com = consume(tokens, p5);
      p5 = _second(com);
    }
    if (strcmp(lx2, ".") == 0) {
      dot = consume(tokens, p5);
      p6 = _second(dot);
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "reap_stmt", "target", var_name, "action", act_name, "args", args), p6);
    }
  }
}

PlantArray* parse_put_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* vpair = NULL;
  tx_t item = "";
  long p3 = 0;
  PlantArray* into_pair = NULL;
  long p4 = 0;
  PlantArray* tpair = NULL;
  tx_t target = "";
  long p5 = 0;
  PlantArray* dot_pair = NULL;
  long p6 = 0;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  vpair = collect_until(tokens, p2, "INTO");
  item = plant_list_get(vpair, 0);
  p3 = _second(vpair);
  into_pair = consume(tokens, p3);
  p4 = _second(into_pair);
  tpair = collect_until(tokens, p4, ".");
  target = plant_list_get(tpair, 0);
  p5 = _second(tpair);
  dot_pair = consume(tokens, p5);
  p6 = _second(dot_pair);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(6), "type", "put_stmt", "item", item, "target", target), p6);
}

PlantArray* parse_break_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* tok = NULL;
  tx_t lx = "";
  PlantArray* drop = NULL;
  PlantArray* dot_pair = NULL;
  long p3 = 0;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  if (p2<(tokens)->count) {
    tok = peek(tokens, p2);
    lx = tok_lex(tok);
    if (strcmp(lx, "0") == 0) {
      drop = consume(tokens, p2);
      p2 = _second(drop);
    }
  }
  dot_pair = consume(tokens, p2);
  p3 = _second(dot_pair);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(2), "type", "break_stmt"), p3);
}

PlantArray* parse_continue_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* dot_pair = NULL;
  long p3 = 0;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  dot_pair = consume(tokens, p2);
  p3 = _second(dot_pair);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(2), "type", "continue_stmt"), p3);
}

PlantArray* parse_if_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* cpair = NULL;
  tx_t cond = "";
  long p3 = 0;
  PlantArray* com = NULL;
  long p4 = 0;
  PlantArray* stmt_pair = NULL;
  tx_t body = "";
  long p5 = 0;
  PlantArray* slash = NULL;
  long p6 = 0;
  PlantArray* if_close = NULL;
  long p7 = 0;
  PlantArray* dot = NULL;
  long p8 = 0;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  cpair = collect_until(tokens, p2, ",");
  cond = plant_list_get(cpair, 0);
  p3 = _second(cpair);
  com = consume(tokens, p3);
  p4 = _second(com);
  stmt_pair = parse_statement(tokens, p4);
  body = plant_list_get(stmt_pair, 0);
  p5 = _second(stmt_pair);
  slash = consume(tokens, p5);
  p6 = _second(slash);
  if_close = consume(tokens, p6);
  p7 = _second(if_close);
  dot = consume(tokens, p7);
  p8 = _second(dot);
  return plant_list_make((int64_t)(2), plant_list_make((int64_t)(6), "type", "if_stmt", "cond", cond, "body", plant_list_make((int64_t)(1), body)), p8);
}

PlantArray* parse_season_stmt(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* cpair = NULL;
  tx_t cond = "";
  long p3 = 0;
  PlantArray* com = NULL;
  long p4 = 0;
  PlantArray* body = NULL;
  tx_t is_eof_flag = "";
  PlantArray* tok = NULL;
  tx_t lx = "";
  PlantArray* slash = NULL;
  long p5 = 0;
  PlantArray* season_close = NULL;
  long p6 = 0;
  PlantArray* dot = NULL;
  long p7 = 0;
  PlantArray* stmt_pair = NULL;
  tx_t stmt = "";
  pair = consume(tokens, pos);
  p2 = _second(pair);
  cpair = collect_until(tokens, p2, ",");
  cond = plant_list_get(cpair, 0);
  p3 = _second(cpair);
  com = consume(tokens, p3);
  p4 = _second(com);
  body = plant_list_make((int64_t)(0));
  while (1) {
    is_eof_flag = is_eof(tokens, p4);
    if (is_eof_flag) {
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(6), "type", "season_stmt", "cond", cond, "body", body), p4);
    }
    tok = peek(tokens, p4);
    lx = tok_lex(tok);
    if (strcmp(lx, "/") == 0) {
      slash = consume(tokens, p4);
      p5 = _second(slash);
      season_close = consume(tokens, p5);
      p6 = _second(season_close);
      dot = consume(tokens, p6);
      p7 = _second(dot);
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(6), "type", "season_stmt", "cond", cond, "body", body), p7);
    }
    stmt_pair = parse_statement(tokens, p4);
    stmt = plant_list_get(stmt_pair, 0);
    p4 = _second(stmt_pair);
    if (strcmp(stmt, "") > 0) {
      body = plant_list_push(body, stmt);
    }
  }
}

PlantArray* parse_statement(PlantArray* tokens, long pos) {
  PlantArray* tok = NULL;
  tx_t tp = "";
  PlantArray* drop_pair = NULL;
  tx_t lx = "";
  PlantArray* r = NULL;
  while (1) {
    tok = peek(tokens, pos);
    tp = tok_type(tok);
    if (strcmp(tp, "DEPTH") != 0) {
      break;
    }
    drop_pair = consume(tokens, pos);
    pos = _second(drop_pair);
  }
  tok = peek(tokens, pos);
  lx = tok_lex(tok);
  if (strcmp(lx, "CREATE") == 0) {
    r = parse_create_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "SHOW") == 0) {
    r = parse_show_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "GIVE") == 0) {
    r = parse_give_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "SET") == 0) {
    r = parse_set_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "LET") == 0) {
    r = parse_let_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "IF") == 0) {
    r = parse_if_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "SEASON") == 0) {
    r = parse_season_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "REAP") == 0) {
    r = parse_reap_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "PUT") == 0) {
    r = parse_put_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "BREAK") == 0) {
    r = parse_break_stmt(tokens, pos);
    return r;
  }
  if (strcmp(lx, "CONTINUE") == 0) {
    r = parse_continue_stmt(tokens, pos);
    return r;
  }
  return plant_list_make((int64_t)(2), NULL, pos+1);
}

PlantArray* parse_enum_decl(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* name_pair = NULL;
  tx_t name = "";
  long p3 = 0;
  PlantArray* lbr = NULL;
  long p4 = 0;
  PlantArray* members = NULL;
  tx_t is_eof_flag = "";
  PlantArray* tok = NULL;
  tx_t lx = "";
  PlantArray* rbr = NULL;
  long p5 = 0;
  PlantArray* m_pair = NULL;
  tx_t mname = "";
  PlantArray* tok2 = NULL;
  tx_t lx2 = "";
  PlantArray* com = NULL;
  pair = consume(tokens, pos);
  p2 = _second(pair);
  name_pair = consume(tokens, p2);
  name = tok_lex(plant_list_get(name_pair, 0));
  p3 = _second(name_pair);
  lbr = consume(tokens, p3);
  p4 = _second(lbr);
  members = plant_list_make((int64_t)(0));
  while (1) {
    is_eof_flag = is_eof(tokens, p4);
    if (is_eof_flag) {
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(6), "type", "enum_decl", "name", name, "members", members), p4);
    }
    tok = peek(tokens, p4);
    lx = tok_lex(tok);
    if (strcmp(lx, "}") == 0) {
      rbr = consume(tokens, p4);
      p5 = _second(rbr);
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(6), "type", "enum_decl", "name", name, "members", members), p5);
    }
    m_pair = consume(tokens, p4);
    mname = tok_lex(plant_list_get(m_pair, 0));
    p5 = _second(m_pair);
    members = plant_list_push(members, mname);
    tok2 = peek(tokens, p5);
    lx2 = tok_lex(tok2);
    if (strcmp(lx2, ",") == 0) {
      com = consume(tokens, p5);
      p5 = _second(com);
    }
    p4 = p5;
  }
}

PlantArray* parse_action_decl(PlantArray* tokens, long pos) {
  PlantArray* pair = NULL;
  long p2 = 0;
  PlantArray* name_pair = NULL;
  tx_t aname = "";
  long p3 = 0;
  PlantArray* lp = NULL;
  long p4 = 0;
  PlantArray* params = NULL;
  tx_t is_eof_flag = "";
  PlantArray* tok = NULL;
  tx_t lx = "";
  PlantArray* rp = NULL;
  long p5 = 0;
  long brk = 0;
  PlantArray* pn_pair = NULL;
  tx_t pn = "";
  PlantArray* tok2 = NULL;
  tx_t lx2 = "";
  PlantArray* lp2 = NULL;
  long p6 = 0;
  PlantArray* pt_pair = NULL;
  tx_t pt = "";
  long p7 = 0;
  PlantArray* rp2 = NULL;
  long p8 = 0;
  PlantArray* tok3 = NULL;
  tx_t lx3 = "";
  PlantArray* com = NULL;
  PlantArray* arrow_tok = NULL;
  tx_t arrow_lx = "";
  PlantArray* arrow_pair = NULL;
  PlantArray* ret_pair = NULL;
  PlantArray* after_tok = NULL;
  tx_t after_lx = "";
  PlantArray* com_pair = NULL;
  PlantArray* dot_pair = NULL;
  PlantArray* body = NULL;
  PlantArray* tok4 = NULL;
  tx_t lx4 = "";
  PlantArray* slash = NULL;
  PlantArray* end = NULL;
  PlantArray* dot = NULL;
  PlantArray* stmt_pair = NULL;
  tx_t stmt = "";
  pair = consume(tokens, pos);
  p2 = _second(pair);
  name_pair = consume(tokens, p2);
  aname = tok_lex(plant_list_get(name_pair, 0));
  p3 = _second(name_pair);
  lp = consume(tokens, p3);
  p4 = _second(lp);
  params = plant_list_make((int64_t)(0));
  while (1) {
    is_eof_flag = is_eof(tokens, p4);
    if (is_eof_flag) {
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "action_decl", "name", aname, "params", params, "body", plant_list_make((int64_t)(0))), p4);
    }
    tok = peek(tokens, p4);
    lx = tok_lex(tok);
    if (strcmp(lx, ")") == 0) {
      rp = consume(tokens, p4);
      p5 = _second(rp);
      brk = 0;
      break;
    }
    pn_pair = consume(tokens, p4);
    pn = tok_lex(plant_list_get(pn_pair, 0));
    p5 = _second(pn_pair);
    tok2 = peek(tokens, p5);
    lx2 = tok_lex(tok2);
    if (strcmp(lx2, "(") == 0) {
      lp2 = consume(tokens, p5);
      p6 = _second(lp2);
      pt_pair = consume(tokens, p6);
      pt = tok_lex(plant_list_get(pt_pair, 0));
      p7 = _second(pt_pair);
      rp2 = consume(tokens, p7);
      p8 = _second(rp2);
      params = plant_list_push(params, plant_list_make((int64_t)(4), "name", pn, "type", pt));
      p5 = p8;
    }
    tok3 = peek(tokens, p5);
    lx3 = tok_lex(tok3);
    if (strcmp(lx3, ",") == 0) {
      com = consume(tokens, p5);
      p5 = _second(com);
    }
    p4 = p5;
  }
  arrow_tok = peek(tokens, p5);
  arrow_lx = tok_lex(arrow_tok);
  if (strcmp(arrow_lx, "->") == 0) {
    arrow_pair = consume(tokens, p5);
    p5 = _second(arrow_pair);
    ret_pair = consume(tokens, p5);
    p5 = _second(ret_pair);
    after_tok = peek(tokens, p5);
    fprintf(stderr, "DEBUG arrow_lx=[%s] after_lx=[%s] p5=%ld count=%ld\n", arrow_lx, after_lx, p5, tokens->count);
    after_lx = tok_lex(after_tok);
    if (strcmp(after_lx, ",") == 0) {
      com_pair = consume(tokens, p5);
      p5 = _second(com_pair);
    }
    if (strcmp(after_lx, ".") == 0) {
      dot_pair = consume(tokens, p5);
      p5 = _second(dot_pair);
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "action_decl", "name", aname, "params", params, "body", plant_list_make((int64_t)(0))), p5);
    }
  }
  body = plant_list_make((int64_t)(0));
  while (1) {
    is_eof_flag = is_eof(tokens, p5);
    if (is_eof_flag) {
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "action_decl", "name", aname, "params", params, "body", body), p5);
    }
    tok4 = peek(tokens, p5);
    lx4 = tok_lex(tok4);
    if (strcmp(lx4, "/") == 0) {
      slash = consume(tokens, p5);
      p6 = _second(slash);
      end = consume(tokens, p6);
      p7 = _second(end);
      dot = consume(tokens, p7);
      p8 = _second(dot);
      return plant_list_make((int64_t)(2), plant_list_make((int64_t)(8), "type", "action_decl", "name", aname, "params", params, "body", body), p8);
    }
    stmt_pair = parse_statement(tokens, p5);
    stmt = plant_list_get(stmt_pair, 0);
    p5 = _second(stmt_pair);
    if (strcmp(stmt, "") > 0) {
      body = plant_list_push(body, stmt);
    }
  }
}

PlantArray* parse_declaration(PlantArray* tokens, long pos) {
  PlantArray* tok = NULL;
  tx_t lx = "";
  PlantArray* r = NULL;
  tok = peek(tokens, pos);
  lx = tok_lex(tok);
  if (strcmp(lx, "ENUM") == 0) {
    r = parse_enum_decl(tokens, pos);
    return r;
  }
  if (strcmp(lx, "ACTION") == 0) {
    r = parse_action_decl(tokens, pos);
    return r;
  }
  r = parse_statement(tokens, pos);
  return r;
}

PlantArray* parse_program(PlantArray* tokens) {
  long pos = 0;
  PlantArray* nodes = NULL;
  tx_t is_eof_flag = "";
  PlantArray* d_pair = NULL;
  tx_t decl = "";
  long pos2 = 0;
  pos = 0;
  nodes = plant_list_make((int64_t)(0));
  while (1) {
    is_eof_flag = is_eof(tokens, pos);
    if (is_eof_flag) {
      return plant_list_make((int64_t)(4), "type", "program", "body", nodes);
    }
    d_pair = parse_declaration(tokens, pos);
    decl = plant_list_get(d_pair, 0);
    pos2 = _second(d_pair);
    if (pos2<=pos) {
      return plant_list_make((int64_t)(4), "type", "program", "body", nodes);
    }
    pos = pos2;
    if (strcmp(decl, "") > 0) {
      nodes = plant_list_push(nodes, decl);
    }
  }
}

tx_t _substr(tx_t str, long start, long end) {
  tx_t res = "";
  long idx = 0;
  res = "";
  idx = start;
  while (idx<end) {
    res = _cat(res, _at(str, idx));
    idx = idx+1;
  }
  return res;
}

tx_t _handle_func(tx_t expr, tx_t kw, tx_t cfn) {
  PlantArray* parts = NULL;
  tx_t res = "";
  long idx = 0;
  tx_t cur = "";
  long n = 0;
  long pos = 0;
  tx_t vname = "";
  tx_t rest = "";
  parts = strings_SPLIT(expr, _cat(kw, " "));
  if (plant_array_length(parts == 1)) {
    return expr;
  }
  res = plant_list_get(parts, 0);
  idx = 1;
  while (idx<(parts)->count) {
    cur = plant_list_get(parts, idx);
    n = strlen(cur);
    pos = 0;
    while (pos<n && strcmp(_at(cur, pos), " ") != 0 && strcmp(_at(cur, pos), "+") != 0 && strcmp(_at(cur, pos), ")") != 0 && strcmp(_at(cur, pos), "(") != 0) {
      pos = pos+1;
    }
    vname = _substr(cur, 0, pos);
    rest = _substr(cur, pos, n);
    res = _cat(_cat(_cat(_cat(_cat(res, cfn), "("), vname), ")"), rest);
    idx = idx+1;
  }
  return res;
}

tx_t _handle_func_paren(tx_t expr, tx_t kw, tx_t cfn) {
  PlantArray* parts = NULL;
  tx_t res = "";
  long idx = 0;
  parts = strings_SPLIT(expr, _cat(kw, "("));
  if (plant_array_length(parts == 1)) {
    return expr;
  }
  res = plant_list_get(parts, 0);
  idx = 1;
  while (idx<(parts)->count) {
    res = _cat(_cat(_cat(res, cfn), "("), plant_list_get(parts, idx));
    idx = idx+1;
  }
  return res;
}

tx_t translate_expr(tx_t expr) {
  tx_t e = "";
  e = expr;
  e = strings_REPLACE(e, "GREATER THAN OR EQUAL", ">=");
  e = strings_REPLACE(e, "LESS THAN OR EQUAL", "<=");
  e = strings_REPLACE(e, "GREATER THAN", ">");
  e = strings_REPLACE(e, "LESS THAN", "<");
  e = strings_REPLACE(e, "ISNT", "!=");
  e = strings_REPLACE(e, "STAR_STAR", "**");
  e = strings_REPLACE(e, " AND ", " && ");
  e = strings_REPLACE(e, " OR ", " || ");
  e = strings_REPLACE(e, "NOT ", "!");
  e = strings_REPLACE(e, " IS ", " == ");
  e = _handle_func(e, "COUNT", "plant_array_length");
  e = _handle_func_paren(e, "LEN", "strlen");
  e = _handle_func(e, "TEST", "!");
  e = strings_REPLACE(e, "TRUE", "1");
  e = strings_REPLACE(e, "FALSE", "0");
  e = strings_REPLACE(e, "NULL", "NULL");
  return e;
}

tx_t indent_str(long level) {
  tx_t res = "";
  long i = 0;
  res = "";
  i = 0;
  while (i<level) {
    res = _cat(res, "  ");
    i = i+1;
  }
  return res;
}

tx_t generate_body(PlantArray* bd, long indent) {
  tx_t res = "";
  long i = 0;
  tx_t node_el = "";
  tx_t node_code = "";
  res = "";
  i = 0;
  while (i<(bd)->count) {
    node_el = plant_list_get(bd, i);
    node_code = generate_node(node_el, indent);
    if (strcmp(node_code, "") > 0) {
      res = _cat(res, node_code);
    }
    i = i+1;
  }
  return res;
}

tx_t generate_node(tx_t node, long indent) {
  tx_t ntype = "";
  tx_t val = "";
  tx_t cval = "";
  tx_t isel = "";
  tx_t has_ops = "";
  tx_t target = "";
  tx_t vtype = "";
  tx_t item = "";
  tx_t citem = "";
  tx_t tgt = "";
  tx_t act = "";
  PlantArray* args = NULL;
  tx_t argstr = "";
  long ai = 0;
  tx_t arg_el = "";
  tx_t aexpr = "";
  tx_t cond = "";
  tx_t bd = "";
  tx_t ccond = "";
  tx_t ccode = "";
  tx_t bcode = "";
  tx_t ivar = "";
  tx_t fromExpr = "";
  tx_t toExpr = "";
  tx_t stepExpr = "";
  tx_t listExpr = "";
  tx_t indexVar = "";
  tx_t cfrom = "";
  tx_t cto = "";
  tx_t stepstr = "";
  tx_t cstep = "";
  tx_t idxvar = "";
  tx_t clist = "";
  tx_t subj = "";
  PlantArray* clauses = NULL;
  tx_t csubj = "";
  long ci = 0;
  tx_t clause = "";
  tx_t vname = "";
  tx_t binding = "";
  tx_t cbody = "";
  tx_t aname = "";
  PlantArray* params = NULL;
  tx_t paramstr = "";
  long pi = 0;
  tx_t param_el = "";
  tx_t pname = "";
  tx_t ptype = "";
  tx_t ctype = "";
  tx_t ename = "";
  PlantArray* members = NULL;
  long mi = 0;
  tx_t member_el = "";
  ntype = _map_get(node, "type");
  if (strcmp(ntype, "show_stmt") == 0) {
    val = _map_get(node, "value");
    cval = translate_expr(val);
    isel = indent_str(indent);
    has_ops = strings_REPLACE(cval, " + ", "");
    has_ops = strings_REPLACE(has_ops, " == ", "");
    has_ops = strings_REPLACE(has_ops, " && ", "");
    has_ops = strings_REPLACE(has_ops, " || ", "");
    if (has_ops == cval) {
      cval = _cat(_cat("\"", cval), "\"");
    }
    return _cat(_cat(_cat(isel, "  plant_print("), cval), ");\n");
  }
  if (strcmp(ntype, "create_stmt") == 0) {
    target = _map_get(node, "target");
    vtype = _map_get(node, "var_type");
    val = _map_get(node, "value");
    cval = translate_expr(val);
    isel = indent_str(indent);
    if (strcmp(vtype, "NUM") == 0) {
      return _cat(_cat(_cat(_cat(_cat(isel, "  long "), target), " = "), cval), ";\n");
    }
    if (strcmp(vtype, "FACT") == 0) {
      return _cat(_cat(_cat(_cat(_cat(isel, "  int "), target), " = "), cval), ";\n");
    }
    if (strcmp(vtype, "NUM") != 0 && strcmp(vtype, "FACT") != 0) {
      return _cat(_cat(_cat(_cat(_cat(isel, "  tx_t "), target), " = "), cval), ";\n");
    }
  }
  if (strcmp(ntype, "set_stmt") == 0) {
    target = _map_get(node, "target");
    val = _map_get(node, "value");
    cval = translate_expr(val);
    isel = indent_str(indent);
    return _cat(_cat(_cat(_cat(_cat(isel, "  "), target), " = "), cval), ";\n");
  }
  if (strcmp(ntype, "let_stmt") == 0) {
    target = _map_get(node, "target");
    vtype = _map_get(node, "var_type");
    val = _map_get(node, "value");
    cval = translate_expr(val);
    isel = indent_str(indent);
    if (strcmp(vtype, "NUM") == 0) {
      return _cat(_cat(_cat(_cat(_cat(isel, "  long "), target), " = "), cval), ";\n");
    }
    if (strcmp(vtype, "NUM") != 0) {
      return _cat(_cat(_cat(_cat(_cat(isel, "  tx_t "), target), " = "), cval), ";\n");
    }
  }
  if (strcmp(ntype, "give_stmt") == 0) {
    val = _map_get(node, "value");
    cval = translate_expr(val);
    isel = indent_str(indent);
    return _cat(_cat(_cat(isel, "  return "), cval), ";\n");
  }
  if (strcmp(ntype, "break_stmt") == 0) {
    isel = indent_str(indent);
    return _cat(isel, "  break;\n");
  }
  if (strcmp(ntype, "continue_stmt") == 0) {
    isel = indent_str(indent);
    return _cat(isel, "  continue;\n");
  }
  if (strcmp(ntype, "put_stmt") == 0) {
    item = _map_get(node, "item");
    target = _map_get(node, "target");
    citem = translate_expr(item);
    isel = indent_str(indent);
    return _cat(_cat(_cat(_cat(_cat(isel, "  plant_array_push("), target), ", "), citem), ");\n");
  }
  if (strcmp(ntype, "reap_stmt") == 0) {
    tgt = _map_get(node, "target");
    act = _map_get(node, "action");
    args = _map_get(node,"args");
    argstr = "";
    ai = 0;
    while (ai<(args)->count) {
      arg_el = plant_list_get(args, ai);
      aexpr = translate_expr(arg_el);
      if (ai>0) {
        argstr = _cat(argstr, ", ");
      }
      argstr = _cat(argstr, aexpr);
      ai = ai+1;
    }
    isel = indent_str(indent);
    if (strcmp(tgt, "_") == 0) {
      return _cat(_cat(_cat(_cat(_cat(isel, "  "), act), "("), argstr), ");\n");
    }
    if (strcmp(tgt, "_") != 0) {
      return _cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  "), tgt), " = "), act), "("), argstr), ");\n");
    }
  }
  if (strcmp(ntype, "if_stmt") == 0) {
    cond = _map_get(node, "cond");
    bd = _map_get(node, "body");
    ccond = translate_expr(cond);
    isel = indent_str(indent);
    ccode = _cat(_cat(_cat(isel, "  if ("), ccond), ") \{\n");
    bcode = generate_body(bd, indent+2);
    ccode = _cat(_cat(_cat(ccode, bcode), isel), "  \}\n");
    return ccode;
  }
  if (strcmp(ntype, "season_stmt") == 0) {
    cond = _map_get(node, "cond");
    bd = _map_get(node, "body");
    ccond = translate_expr(cond);
    isel = indent_str(indent);
    ccode = _cat(_cat(_cat(isel, "  while ("), ccond), ") \{\n");
    bcode = generate_body(bd, indent+2);
    ccode = _cat(_cat(_cat(ccode, bcode), isel), "  \}\n");
    return ccode;
  }
  if (strcmp(ntype, "cycle_stmt") == 0) {
    ivar = _map_get(node, "iterVar");
    fromExpr = _map_get(node, "fromExpr");
    toExpr = _map_get(node, "toExpr");
    stepExpr = _map_get(node, "stepExpr");
    listExpr = _map_get(node, "listExpr");
    indexVar = _map_get(node, "indexVar");
    bd = _map_get(node, "body");
    isel = indent_str(indent);
    ccode = "";
    if (strcmp(fromExpr, "") != 0) {
      cfrom = translate_expr(fromExpr);
      cto = translate_expr(toExpr);
      stepstr = "1";
      if (strcmp(stepExpr, "") > 0 && strcmp(stepExpr, "null") != 0) {
        cstep = translate_expr(stepExpr);
        stepstr = cstep;
      }
      ccode = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  for (long "), ivar), " = "), cfrom), "; "), ivar), " <= "), cto), "; "), ivar), " += "), stepstr), ") \{\n");
      bcode = generate_body(bd, indent+2);
      ccode = _cat(_cat(_cat(ccode, bcode), isel), "  \}\n");
      return ccode;
    }
    if (strcmp(listExpr, "") != 0) {
      idxvar = "__cycle_i";
      if (strcmp(indexVar, "") != 0 && strcmp(indexVar, "null") != 0) {
        idxvar = indexVar;
      }
      clist = translate_expr(listExpr);
      ccode = _cat(isel, "  \{\n");
      ccode = _cat(_cat(_cat(_cat(ccode, isel), "    long "), idxvar), " = 0;\n");
      ccode = _cat(_cat(_cat(_cat(_cat(_cat(ccode, isel), "    while ("), idxvar), " < plant_array_length("), clist), ")) \{\n");
      ccode = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(ccode, isel), "      tx_t "), ivar), " = plant_array_get("), clist), ", "), idxvar), ");\n");
      bcode = generate_body(bd, indent+4);
      ccode = _cat(_cat(_cat(_cat(_cat(ccode, bcode), isel), "      "), idxvar), "++;\n");
      ccode = _cat(_cat(ccode, isel), "    }\n");
      ccode = _cat(_cat(ccode, isel), "  }\n");
      return ccode;
    }
    return "";
  }
  if (strcmp(ntype, "match_stmt") == 0) {
    subj = _map_get(node, "subjectExpr");
    clauses = _map_get(node,"clauses");
    csubj = translate_expr(subj);
    isel = indent_str(indent);
    ccode = _cat(_cat(_cat(isel, "  switch ("), csubj), ") \{\n");
    ci = 0;
    while (ci<(clauses)->count) {
      clause = plant_list_get(clauses, ci);
      vname = _map_get(clause, "variantName");
      binding = _map_get(clause, "binding");
      cbody = _map_get(clause, "bodyStatements");
      ccode = _cat(_cat(_cat(_cat(ccode, isel), "    case "), vname), ":\n");
      if (strcmp(binding, "") > 0 && strcmp(binding, "null") != 0) {
        ccode = _cat(_cat(ccode, isel), "      \{\n");
        ccode = _cat(_cat(_cat(_cat(_cat(_cat(ccode, isel), "        tx_t "), binding), " = "), csubj), ";\n");
        bcode = generate_body(cbody, indent+4);
        ccode = _cat(_cat(_cat(ccode, bcode), isel), "      \}\n");
      }
      if (strcmp(binding, "") == 0 || strcmp(binding, "null") == 0) {
        bcode = generate_body(cbody, indent+4);
        ccode = _cat(ccode, bcode);
      }
      ccode = _cat(_cat(ccode, isel), "      break;\n");
      ci = ci+1;
    }
    ccode = _cat(_cat(ccode, isel), "  \}\n");
    return ccode;
  }
  if (strcmp(ntype, "action_decl") == 0) {
    aname = _map_get(node, "name");
    params = _map_get(node,"params");
    bd = _map_get(node, "body");
    paramstr = "";
    pi = 0;
    while (pi<(params)->count) {
      param_el = plant_list_get(params, pi);
      pname = _map_get(param_el, "name");
      ptype = _map_get(param_el, "type");
      ctype = "tx_t";
      if (strcmp(ptype, "NUM") == 0) {
        ctype = "long";
      }
      if (strcmp(ptype, "FACT") == 0) {
        ctype = "int";
      }
      if (pi>0) {
        paramstr = _cat(paramstr, ", ");
      }
      paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
      pi = pi+1;
    }
    ccode = _cat(_cat(_cat(_cat("void ", aname), "("), paramstr), ") \{\n");
    bcode = generate_body(bd, 1);
    ccode = _cat(_cat(ccode, bcode), "\}\n");
    return ccode;
  }
  if (strcmp(ntype, "enum_decl") == 0) {
    ename = _map_get(node, "name");
    members = _map_get(node,"members");
    ccode = "typedef enum \{\n  ";
    mi = 0;
    while (mi<(members)->count) {
      member_el = plant_list_get(members, mi);
      if (mi>0) {
        ccode = _cat(ccode, ",\n  ");
      }
      ccode = _cat(ccode, member_el);
      mi = mi+1;
    }
    ccode = _cat(_cat(_cat(ccode, "\n\} "), ename), ";\n");
    return ccode;
  }
  return "";
}

tx_t generate_c(PlantArray* ast) {
  tx_t header = "";
  tx_t decl_code = "";
  tx_t stmt_code = "";
  long has_decl = 0;
  long has_stmt = 0;
  long i = 0;
  tx_t node_el = "";
  tx_t ntype = "";
  tx_t nd_code = "";
  tx_t ns_code = "";
  header = "#include <plant_compat.h>\n\n";
  decl_code = "";
  stmt_code = "";
  has_decl = 0;
  has_stmt = 0;
  i = 0;
  while (i<(ast)->count) {
    node_el = plant_list_get(ast, i);
    ntype = _map_get(node_el, "type");
    if (strcmp(ntype, "action_decl") == 0 || strcmp(ntype, "enum_decl") == 0) {
      nd_code = generate_node(node_el,0);
      decl_code = _cat(decl_code, nd_code);
      has_decl = 1;
    }
    if (strcmp(ntype, "action_decl") != 0 && strcmp(ntype, "enum_decl") != 0) {
      ns_code = generate_node(node_el,0);
      stmt_code = _cat(stmt_code, ns_code);
      has_stmt = 1;
    }
    i = i+1;
  }
  if (has_stmt) {
    stmt_code = _cat(_cat("int main(int argc, char **argv) {\n  plant_init_cli(argc, argv);\n", stmt_code), "  return 0;\n}\n");
  }
  return _cat(_cat(header, decl_code), stmt_code);
}

int main(int argc, char **argv) {
  tx_t source_path = "";
  tx_t exists = "";
  tx_t source_text = "";
  PlantArray* tokens = NULL;
  PlantArray* program_ast = NULL;
  tx_t body = "";
  tx_t c_code = "";
  tx_t out_path = "";
  tx_t written = "";
  tx_t c_len = "";
  plant_init_cli(argc, argv);
  source_path = get_cli_arg(0);
  plant_print(_cat("input: ", source_path));
  exists = fs_EXISTS(source_path);
  if (!exists) {
    plant_print(_cat("Error: file not found — ", source_path));
  }
  source_text = fs_READ(source_path);
  plant_print("tokenizing...");
  tokens = scan_tokens(source_text);
  plant_print("parsing...");
  program_ast = parse_program(tokens);
  body = _map_get(program_ast, "body");
  plant_print("generating C...");
  c_code = generate_c(body);
  out_path = get_cli_arg(1);
  if (strcmp(out_path, "") == 0) {
    out_path = strings_REPLACE(source_path, ".plant", ".c");
  }
  written = fs_WRITE(out_path, c_code);
  c_len = strings_LENGTH(c_code);
  plant_print(_cat(_cat(_cat("output: ", c_len), " bytes to "), out_path));
  return 0;
}
