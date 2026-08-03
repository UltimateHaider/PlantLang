#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t is_keyword(tx_t wrd);
tx_t keyword_to_type(tx_t wrd);
tx_t char_type(tx_t ch);
tx_t is_alnum(tx_t ch);
tx_t is_alpha_start(tx_t ch);
tx_t match_ident_or_keyword(tx_t src, long i, long n);
tx_t match_number(tx_t src, long i, long n);
tx_t match_string(tx_t src, long i, long n);
tx_t skip_comment(tx_t src, long i, long n);
tx_t scan_tokens(tx_t src);
tx_t tok_lex(PlantArray* tok);
tx_t tok_type(PlantArray* tok);
tx_t peek(PlantArray* tokens, long pos);
tx_t consume(PlantArray* tokens, long pos);
tx_t _first(PlantArray* pair);
tx_t _second(PlantArray* pair);
tx_t is_eof(PlantArray* tokens, long pos);
tx_t escape_string(tx_t s);
tx_t collect_value(PlantArray* tokens, long start);
tx_t collect_until(PlantArray* tokens, long start, tx_t delim);
tx_t collect_type_text(PlantArray* tokens, long start, tx_t stopc, long stopcomma);
tx_t collect_args(PlantArray* tokens, long pos);
tx_t parse_await_stmt(PlantArray* tokens, long pos);
tx_t parse_start_stmt(PlantArray* tokens, long pos);
tx_t parse_async_in_stmt(PlantArray* tokens, long pos);
tx_t parse_cancel_stmt(PlantArray* tokens, long pos);
tx_t parse_trace_stmt(PlantArray* tokens, long pos);
tx_t parse_mission_stmt(PlantArray* tokens, long pos);
tx_t parse_create_stmt(PlantArray* tokens, long pos);
tx_t parse_show_stmt(PlantArray* tokens, long pos);
tx_t parse_give_stmt(PlantArray* tokens, long pos);
tx_t parse_set_stmt(PlantArray* tokens, long pos);
tx_t parse_let_stmt(PlantArray* tokens, long pos);
tx_t parse_closure(PlantArray* tokens, long pos);
tx_t parse_reap_stmt(PlantArray* tokens, long pos);
tx_t parse_call_stmt(PlantArray* tokens, long pos);
tx_t parse_put_stmt(PlantArray* tokens, long pos);
tx_t parse_break_stmt(PlantArray* tokens, long pos);
tx_t parse_continue_stmt(PlantArray* tokens, long pos);
tx_t parse_if_stmt(PlantArray* tokens, long pos);
tx_t parse_season_stmt(PlantArray* tokens, long pos);
tx_t parse_statement(PlantArray* tokens, long pos);
tx_t parse_enum_decl(PlantArray* tokens, long pos);
tx_t parse_struct_decl(PlantArray* tokens, long pos);
tx_t parse_action_decl(PlantArray* tokens, long pos);
tx_t parse_declaration(PlantArray* tokens, long pos);
tx_t map_add(PlantArray* m, tx_t k, tx_t v);
tx_t parse_program(PlantArray* tokens);
tx_t _substr(tx_t str, long start, long end);
tx_t _handle_func(tx_t expr, tx_t kw, tx_t cfn);
tx_t _handle_func_paren(tx_t expr, tx_t kw, tx_t cfn);
tx_t seg_is_numeric(tx_t seg, PlantArray* nums);
tx_t expr_is_numeric(tx_t e, PlantArray* nums);
tx_t _handle_cat(tx_t expr, PlantArray* nums, PlantArray* evars);
tx_t collect_declared_walk(PlantArray* bd, PlantArray* declared);
tx_t collect_used_walk(PlantArray* bd, PlantArray* used, PlantArray* declared);
tx_t collect_implicit(PlantArray* bd, PlantArray* params);
tx_t build_enum_registry(PlantArray* ast);
tx_t enum_members_of(PlantArray* reg, tx_t ty);
tx_t collect_enums_walk(tx_t bd, tx_t subst, tx_t reg, tx_t res);
tx_t collect_enums(tx_t bd, tx_t params, tx_t subst, tx_t reg);
tx_t enum_in_table(PlantArray* evars, tx_t name);
tx_t list_contains(PlantArray* lst, tx_t x);
tx_t collect_nums_walk(PlantArray* bd, PlantArray* subst, PlantArray* res);
tx_t collect_nums(PlantArray* bd, PlantArray* params, PlantArray* subst);
tx_t nums_from_avars(PlantArray* vars);
tx_t collect_nums_cb(PlantArray* bd, PlantArray* params, PlantArray* shads, PlantArray* subst);
tx_t async_argstr(PlantArray* args, PlantArray* sigs, tx_t act, PlantArray* nums, PlantArray* stvars, PlantArray* evars);
tx_t async_var_add(PlantArray* acc, tx_t name, tx_t ctype);
tx_t async_walk_decl(PlantArray* bd, PlantArray* acc);
tx_t async_collect_vars(PlantArray* bd, PlantArray* params);
tx_t async_split_phases(PlantArray* bd);
tx_t async_emit_state(tx_t name, PlantArray* vars);
tx_t async_emit_entry(tx_t name, PlantArray* params, tx_t prio);
tx_t async_emit_step(tx_t name, PlantArray* phases, PlantArray* vars, PlantArray* sigs, PlantArray* subst, PlantArray* clmap, PlantArray* stvars, PlantArray* evars);
tx_t translate_expr(tx_t expr);
tx_t indent_str(long level);
tx_t generate_body(PlantArray* bd, long indent, PlantArray* sigs, PlantArray* subst, PlantArray* clmap, tx_t actx, PlantArray* nums, PlantArray* stvars, PlantArray* evars);
tx_t generate_node(tx_t node, long indent, PlantArray* sigs, PlantArray* subst, PlantArray* clmap, tx_t actx, PlantArray* nums, PlantArray* stvars, PlantArray* evars);
tx_t type_base(tx_t ptype);
tx_t plant_ctype(tx_t ptype);
tx_t ffi_param_kind(tx_t ptype);
tx_t ffi_struct_name(tx_t typ);
tx_t ffi_struct_cname(tx_t typ);
tx_t is_struct_type(tx_t t);
tx_t collect_stvars_walk(PlantArray* bd, PlantArray* subst, PlantArray* res);
tx_t collect_stvars(PlantArray* bd, PlantArray* params, PlantArray* subst);
tx_t find_sig(PlantArray* sigs, tx_t name);
tx_t ffi_ctype(tx_t ptype);
tx_t is_bare_id(tx_t e);
tx_t expr_is_stringlike(tx_t e);
tx_t stvar_kind(PlantArray* stvars, tx_t name);
tx_t collect_cb_uses(PlantArray* bd, PlantArray* sigs, PlantArray* acc);
tx_t ffi_ret_ctype(tx_t ret);
tx_t struct_fields_at(PlantArray* tpl, PlantArray* args);
tx_t ffi_emit_struct_helpers(tx_t cname, PlantArray* flds);
tx_t trim(tx_t s);
tx_t subst_append(tx_t acc, tx_t w, PlantArray* subst);
tx_t subst_type(tx_t t, PlantArray* subst);
tx_t subst_reap_act(tx_t act, PlantArray* subst);
tx_t base_of(tx_t act);
tx_t ffi_topo_order(PlantArray* entries);
tx_t ffi_topo_emit_helpers(PlantArray* entries);
tx_t parse_type_args(tx_t act);
tx_t mangle(tx_t base, PlantArray* args);
tx_t find_template(PlantArray* templates, tx_t base);
tx_t find_struct(PlantArray* structs, tx_t name);
tx_t scan_type(tx_t t, PlantArray* subst, PlantArray* structs, PlantArray* acc);
tx_t scan_params(PlantArray* params, PlantArray* subst, PlantArray* structs, PlantArray* acc);
tx_t scan_fields(PlantArray* fields, PlantArray* subst, PlantArray* structs, PlantArray* acc);
tx_t collect_struct_insts(PlantArray* bd, PlantArray* subst, PlantArray* structs, PlantArray* acc);
tx_t struct_typedef(PlantArray* tpl, PlantArray* args);
tx_t key_in_acc(tx_t key, PlantArray* acc);
tx_t build_subst(PlantArray* generics, PlantArray* args);
tx_t collect_insts(PlantArray* bd, PlantArray* subst, PlantArray* templates, PlantArray* acc);
tx_t inst_fwddecl(tx_t inst, PlantArray* templates);
tx_t emit_inst(tx_t inst, PlantArray* templates, PlantArray* sigs, PlantArray* reg);
tx_t find_params(PlantArray* sigs, tx_t name);
tx_t is_ref_param(tx_t ptype);
tx_t is_ref_at(PlantArray* params, long idx);
tx_t find_node(PlantArray* ast, tx_t name);
tx_t find_ext_node(PlantArray* ast, tx_t name);
tx_t callee_add(PlantArray* acc, tx_t name);
tx_t callee_from_value(PlantArray* acc, tx_t val);
tx_t callees_of(PlantArray* bd);
tx_t async_reachable(PlantArray* ast);
tx_t generate_c(PlantArray* ast);
tx_t _cl_is_arg(tx_t arg);
tx_t _cl_map_get(PlantArray* clmap, tx_t key);
tx_t _cl_scopes(PlantArray* bd, PlantArray* scopes);
tx_t _cl_stamp_cnode(PlantArray* cnode, PlantArray* scopes, long cc, PlantArray* res);
tx_t _cl_walk(PlantArray* bd, PlantArray* scopes, PlantArray* clseq, PlantArray* clmap, long cid);
tx_t collect_closures(PlantArray* ast);
tx_t _cl_param_str(PlantArray* params);
tx_t _cl_emit_typedef(PlantArray* cnode);
tx_t _cl_emit_fn(PlantArray* cnode, PlantArray* sigs, PlantArray* subst, PlantArray* reg);


tx_t is_keyword(tx_t wrd) {
    if (strcmp(wrd,"LET") == 0) {
        return 1;
    }
    if (strcmp(wrd,"CREATE") == 0) {
        return 1;
    }
    if (strcmp(wrd,"MATCH") == 0) {
        return 1;
    }
    if (strcmp(wrd,"IF") == 0) {
        return 1;
    }
    if (strcmp(wrd,"ELSE") == 0) {
        return 1;
    }
    if (strcmp(wrd,"LOOP") == 0) {
        return 1;
    }
    if (strcmp(wrd,"RETURN") == 0) {
        return 1;
    }
    if (strcmp(wrd,"TYPE") == 0) {
        return 1;
    }
    if (strcmp(wrd,"SHOW") == 0) {
        return 1;
    }
    if (strcmp(wrd,"ACTION") == 0) {
        return 1;
    }
    if (strcmp(wrd,"REAP") == 0) {
        return 1;
    }
    if (strcmp(wrd,"GIVE") == 0) {
        return 1;
    }
    if (strcmp(wrd,"SET") == 0) {
        return 1;
    }
    if (strcmp(wrd,"PUT") == 0) {
        return 1;
    }
    if (strcmp(wrd,"TAKE") == 0) {
        return 1;
    }
    if (strcmp(wrd,"FOR") == 0) {
        return 1;
    }
    if (strcmp(wrd,"CYCLE") == 0) {
        return 1;
    }
    if (strcmp(wrd,"SEASON") == 0) {
        return 1;
    }
    if (strcmp(wrd,"WEATHER") == 0) {
        return 1;
    }
    if (strcmp(wrd,"SHELTER") == 0) {
        return 1;
    }
    if (strcmp(wrd,"CALM") == 0) {
        return 1;
    }
    if (strcmp(wrd,"AND") == 0) {
        return 1;
    }
    if (strcmp(wrd,"OR") == 0) {
        return 1;
    }
    if (strcmp(wrd,"NOT") == 0) {
        return 1;
    }
    if (strcmp(wrd,"IS") == 0) {
        return 1;
    }
    if (strcmp(wrd,"1") == 0) {
        return 1;
    }
    if (strcmp(wrd,"0") == 0) {
        return 1;
    }
    if (strcmp(wrd,"TO") == 0) {
        return 1;
    }
    if (strcmp(wrd,"FROM") == 0) {
        return 1;
    }
    if (strcmp(wrd,"AS") == 0) {
        return 1;
    }
    if (strcmp(wrd,"IN") == 0) {
        return 1;
    }
    if (strcmp(wrd,"SPLIT") == 0) {
        return 1;
    }
    if (strcmp(wrd,"JOIN") == 0) {
        return 1;
    }
    if (strcmp(wrd,"ENUM") == 0) {
        return 1;
    }
    if (strcmp(wrd,"CONST") == 0) {
        return 1;
    }
    if (strcmp(wrd,"FUNCTION") == 0) {
        return 1;
    }
    if (strcmp(wrd,"IMPORT") == 0) {
        return 1;
    }
    if (strcmp(wrd,"OPTION") == 0) {
        return 1;
    }
    if (strcmp(wrd,"RESULT") == 0) {
        return 1;
    }
    if (strcmp(wrd,"GREATER") == 0) {
        return 1;
    }
    if (strcmp(wrd,"LESS") == 0) {
        return 1;
    }
    if (strcmp(wrd,"THAN") == 0) {
        return 1;
    }
    if (strcmp(wrd,"COUNT") == 0) {
        return 1;
    }
    if (strcmp(wrd,"FIRST") == 0) {
        return 1;
    }
    if (strcmp(wrd,"LAST") == 0) {
        return 1;
    }
    if (strcmp(wrd,"SUM") == 0) {
        return 1;
    }
    if (strcmp(wrd,"NULL") == 0) {
        return 1;
    }
    if (strcmp(wrd,"!=") == 0) {
        return 1;
    }
    if (strcmp(wrd,"TEST") == 0) {
        return 1;
    }
    if (strcmp(wrd,"BREAK") == 0) {
        return 1;
    }
    if (strcmp(wrd,"CONTINUE") == 0) {
        return 1;
    }
    if (strcmp(wrd,"INTO") == 0) {
        return 1;
    }
    return 0;
}
tx_t keyword_to_type(tx_t wrd) {
    if (strcmp(wrd,"LET") == 0) {
        return "LET";
    }
    if (strcmp(wrd,"CREATE") == 0) {
        return "CREATE";
    }
    if (strcmp(wrd,"MATCH") == 0) {
        return "MATCH";
    }
    if (strcmp(wrd,"IF") == 0) {
        return "IF";
    }
    if (strcmp(wrd,"ELSE") == 0) {
        return "ELSE";
    }
    if (strcmp(wrd,"LOOP") == 0) {
        return "LOOP";
    }
    if (strcmp(wrd,"RETURN") == 0) {
        return "RETURN";
    }
    if (strcmp(wrd,"TYPE") == 0) {
        return "TYPE";
    }
    if (strcmp(wrd,"SHOW") == 0) {
        return "SHOW";
    }
    if (strcmp(wrd,"ACTION") == 0) {
        return "ACTION";
    }
    if (strcmp(wrd,"REAP") == 0) {
        return "REAP";
    }
    if (strcmp(wrd,"GIVE") == 0) {
        return "GIVE";
    }
    if (strcmp(wrd,"SET") == 0) {
        return "SET";
    }
    if (strcmp(wrd,"PUT") == 0) {
        return "PUT";
    }
    if (strcmp(wrd,"TAKE") == 0) {
        return "TAKE";
    }
    if (strcmp(wrd,"FOR") == 0) {
        return "FOR";
    }
    if (strcmp(wrd,"CYCLE") == 0) {
        return "CYCLE";
    }
    if (strcmp(wrd,"SEASON") == 0) {
        return "SEASON";
    }
    if (strcmp(wrd,"WEATHER") == 0) {
        return "WEATHER";
    }
    if (strcmp(wrd,"SHELTER") == 0) {
        return "SHELTER";
    }
    if (strcmp(wrd,"CALM") == 0) {
        return "CALM";
    }
    if (strcmp(wrd,"AND") == 0) {
        return "AND";
    }
    if (strcmp(wrd,"OR") == 0) {
        return "OR";
    }
    if (strcmp(wrd,"NOT") == 0) {
        return "NOT";
    }
    if (strcmp(wrd,"IS") == 0) {
        return "IS";
    }
    if (strcmp(wrd,"1") == 0) {
        return "1";
    }
    if (strcmp(wrd,"0") == 0) {
        return "0";
    }
    if (strcmp(wrd,"TO") == 0) {
        return "TO";
    }
    if (strcmp(wrd,"FROM") == 0) {
        return "FROM";
    }
    if (strcmp(wrd,"AS") == 0) {
        return "AS";
    }
    if (strcmp(wrd,"IN") == 0) {
        return "IN";
    }
    if (strcmp(wrd,"SPLIT") == 0) {
        return "SPLIT";
    }
    if (strcmp(wrd,"JOIN") == 0) {
        return "JOIN";
    }
    if (strcmp(wrd,"ENUM") == 0) {
        return "ENUM";
    }
    if (strcmp(wrd,"CONST") == 0) {
        return "CONST";
    }
    if (strcmp(wrd,"FUNCTION") == 0) {
        return "FUNCTION";
    }
    if (strcmp(wrd,"IMPORT") == 0) {
        return "IMPORT";
    }
    if (strcmp(wrd,"OPTION") == 0) {
        return "OPTION";
    }
    if (strcmp(wrd,"RESULT") == 0) {
        return "RESULT";
    }
    if (strcmp(wrd,"GREATER") == 0) {
        return "GREATER";
    }
    if (strcmp(wrd,"LESS") == 0) {
        return "LESS";
    }
    if (strcmp(wrd,"THAN") == 0) {
        return "THAN";
    }
    if (strcmp(wrd,"COUNT") == 0) {
        return "COUNT";
    }
    if (strcmp(wrd,"FIRST") == 0) {
        return "FIRST";
    }
    if (strcmp(wrd,"LAST") == 0) {
        return "LAST";
    }
    if (strcmp(wrd,"SUM") == 0) {
        return "SUM";
    }
    if (strcmp(wrd,"NULL") == 0) {
        return "NULL";
    }
    if (strcmp(wrd,"!=") == 0) {
        return "!=";
    }
    if (strcmp(wrd,"TEST") == 0) {
        return "TEST";
    }
    if (strcmp(wrd,"BREAK") == 0) {
        return "BREAK";
    }
    if (strcmp(wrd,"CONTINUE") == 0) {
        return "CONTINUE";
    }
    if (strcmp(wrd,"INTO") == 0) {
        return "INTO";
    }
    return "IDENT";
}
tx_t char_type(tx_t ch) {
    if (strcmp(ch,"+") == 0) {
        return "PLUS";
    }
    if (strcmp(ch,"-") == 0) {
        return "MINUS";
    }
    if (strcmp(ch,"*") == 0) {
        return "STAR";
    }
    if (strcmp(ch,"/") == 0) {
        return "SLASH";
    }
    if (strcmp(ch,"%") == 0) {
        return "PERCENT";
    }
    if (strcmp(ch,"=") == 0) {
        return "EQUAL";
    }
    if (strcmp(ch,":") == 0) {
        return "COLON";
    }
    if (strcmp(ch,"<") == 0) {
        return "LESS";
    }
    if (strcmp(ch,">") == 0) {
        return "GREATER";
    }
    if (strcmp(ch,"!") == 0) {
        return "BANG";
    }
    if (strcmp(ch,"?") == 0) {
        return "QUESTION";
    }
    if (strcmp(ch,"&") == 0) {
        return "AMPERSAND";
    }
    if (strcmp(ch,"|") == 0) {
        return "PIPE";
    }
    if (strcmp(ch,";") == 0) {
        return "SEMI";
    }
    if (strcmp(ch,"(") == 0) {
        return "LPAREN";
    }
    if (strcmp(ch,")") == 0) {
        return "RPAREN";
    }
    if (strcmp(ch,"{") == 0) {
        return "LBRACE";
    }
    if (strcmp(ch,"}") == 0) {
        return "RBRACE";
    }
    if (strcmp(ch,"[") == 0) {
        return "LBRACKET";
    }
    if (strcmp(ch,"]") == 0) {
        return "RBRACKET";
    }
    if (strcmp(ch,",") == 0) {
        return "COMMA";
    }
    if (strcmp(ch,".") == 0) {
        return "DOT";
    }
    if (strcmp(ch,"_") == 0) {
        return "WILDCARD";
    }
    if (strcmp(ch,"#") == 0) {
        return "HASH";
    }
    return "";
}
tx_t is_alnum(tx_t ch) {
    if (strcmp(ch,"a") >= 0 && strcmp(ch,"z") <= 0) {
        return 1;
    }
    if (strcmp(ch,"A") >= 0 && strcmp(ch,"Z") <= 0) {
        return 1;
    }
    if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0) {
        return 1;
    }
    if (strcmp(ch,"_") == 0) {
        return 1;
    }
    return 0;
}
tx_t is_alpha_start(tx_t ch) {
    if (strcmp(ch,"a") >= 0 && strcmp(ch,"z") <= 0) {
        return 1;
    }
    if (strcmp(ch,"A") >= 0 && strcmp(ch,"Z") <= 0) {
        return 1;
    }
    if (strcmp(ch,"_") == 0) {
        return 1;
    }
    return 0;
}
tx_t match_ident_or_keyword(tx_t src, long i, long n) {
  tx_t ok = "";
    tx_t wd = "";
    long ni = i;
    tx_t ch = "";
    while (ni < n) {
        ok = is_alnum(char_at ( src , ni ));
        if (!ok) {
            break;
        }
        ch = char_at(src, ni);
        wd = _cat(wd, ch);
        ni = ni+1;
    }
    if (strcmp(wd,"null") == 0) {
        wd = "NULL";
    }
    return plant_list_make ( 2 , wd , ni );
}
tx_t match_number(tx_t src, long i, long n) {
    tx_t num = "";
    long ni = i;
    tx_t ch = "";
    while (ni < n && strcmp(char_at ( src , ni ),"0") >= 0 && strcmp(char_at ( src , ni ),"9") <= 0) {
        ch = char_at(src, ni);
        num = _cat(num, ch);
        ni = ni+1;
    }
    return plant_list_make ( 2 , num , ni );
}
tx_t match_string(tx_t src, long i, long n) {
    tx_t val = "";
    long si = i;
    int done = 0;
    tx_t ch = "";
    if (si < n && strcmp(char_at ( src , si ),"\"") == 0) {
        si = si+1;
    }
    while (!done && si < n) {
        ch = char_at(src, si);
        if (strcmp(ch,"\"") == 0) {
            done = 1;
        }
        if (strcmp(ch,"\"") == 0) {
            continue;
        }
        if (strcmp(ch,"\\") == 0) {
            si = si+1;
            if (si < n) {
                ch = char_at(src, si);
                if (strcmp(ch,"n") == 0) {
                    ch = "\n";
                }
                if (strcmp(ch,"t") == 0) {
                    ch = "\t";
                }
                if (strcmp(ch,"r") == 0) {
                    ch = "\r";
                }
            }
        }
        if (si < n) {
            val = _cat(val, ch);
            si = si+1;
        }
    }
    if (si < n && strcmp(char_at ( src , si ),"\"") == 0) {
        si = si+1;
    }
    return plant_list_make ( 2 , val , si );
}
tx_t skip_comment(tx_t src, long i, long n) {
    while (i < n && strcmp(char_at ( src , i ),"\n") != 0) {
        i = i+1;
    }
    return i;
}
tx_t scan_tokens(tx_t src) {
  tx_t si = "";
  tx_t ok = "";
  tx_t tok_ty = "";
    PlantArray* tokens = plant_list_make ( 0 );
    long i = 0;
    long n = strlen( src );
    tx_t ch = "";
    while (i < n) {
        ch = char_at(src, i);
        if (strcmp(ch," ") == 0 || strcmp(ch,"\t") == 0 || strcmp(ch,"\r") == 0) {
            i = i+1;
        }
        if (strcmp(ch," ") == 0 || strcmp(ch,"\t") == 0 || strcmp(ch,"\r") == 0) {
            continue;
        }
        if (strcmp(ch,"\n") == 0) {
            i = i+1;
        }
        if (strcmp(ch,"\n") == 0) {
            continue;
        }
        if (strcmp(ch,"#") == 0) {
            si = skip_comment(src, i+1, n);
        }
        if (strcmp(ch,"#") == 0) {
            i = si;
        }
        if (strcmp(ch,"#") == 0) {
            continue;
        }
        if (strcmp(ch,"\"") == 0) {
            si = match_string(src, i, n);
        }
        if (strcmp(ch,"\"") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "STRING" , plant_list_get(si,  0 ) ));
        }
        if (strcmp(ch,"\"") == 0) {
            i = plant_list_get(si,  1 );
        }
        if (strcmp(ch,"\"") == 0) {
            continue;
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"\\") == 0) {
            si = match_number(src, i, n);
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"\\") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "DEPTH" , plant_list_get(si,  0 ) ));
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"\\") == 0) {
            i = plant_list_get(si,  1 )+1;
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"\\") == 0 && i < n && strcmp(char_at ( src , i )," ") == 0) {
            i = i+1;
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"\\") == 0) {
            continue;
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0) {
            si = match_number(src, i, n);
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "NUMBER" , plant_list_get(si,  0 ) ));
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0) {
            i = plant_list_get(si,  1 );
        }
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0) {
            continue;
        }
        ok = is_alpha_start(ch);
        if (ok) {
            si = match_ident_or_keyword(src, i, n);
        }
        if (ok) {
            tok_ty = keyword_to_type(plant_list_get(si,  0 ));
        }
        if (ok) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , tok_ty , plant_list_get(si,  0 ) ));
        }
        if (ok) {
            i = plant_list_get(si,  1 );
        }
        if (ok) {
            continue;
        }
        if (strcmp(ch,".") == 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),".") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "DOT_DOT" , ".." ));
            i = i+2;
            continue;
        }
        if (strcmp(ch,"-") == 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),">") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "ARROW" , "->" ));
            i = i+2;
            continue;
        }
        if (strcmp(ch,">") == 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"=") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "GREATER" , ">=" ));
            i = i+2;
            continue;
        }
        if (strcmp(ch,"<") == 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"=") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "LESS" , "<=" ));
            i = i+2;
            continue;
        }
        if (strcmp(ch,"=") == 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"=") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "EQUAL" , "==" ));
            i = i+2;
            continue;
        }
        if (strcmp(ch,"!") == 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"=") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "BANG" , "!=" ));
            i = i+2;
            continue;
        }
        if (strcmp(ch,"*") == 0 && i + 1 < n && strcmp(char_at ( src , i + 1 ),"*") == 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , "**" , "**" ));
            i = i+2;
            continue;
        }
        tok_ty = char_type(ch);
        if (strcmp(tok_ty,"") != 0) {
            tokens = plant_list_push(tokens, plant_list_make ( 2 , tok_ty , ch ));
        }
        if (strcmp(tok_ty,"") != 0) {
            i = i+1;
        }
        if (strcmp(tok_ty,"") != 0) {
            continue;
        }
        tokens = plant_list_push(tokens, plant_list_make ( 2 , "ERROR" , ch ));
        i = i+1;
    }
    tokens = plant_list_push(tokens, plant_list_make ( 2 , "EOF" , "" ));
    return tokens;
}
tx_t tok_lex(PlantArray* tok) {
    return plant_list_get(tok,  1 );
}
tx_t tok_type(PlantArray* tok) {
    return plant_list_get(tok,  0 );
}
tx_t peek(PlantArray* tokens, long pos) {
    if (pos < plant_array_length(tokens)) {
        return plant_list_get(tokens,  pos );
    }
    return plant_list_make ( 2 , NULL , "" );
}
tx_t consume(PlantArray* tokens, long pos) {
    return plant_list_make ( 2 , plant_list_get(tokens,  pos ) , pos + 1 );
}
tx_t _first(PlantArray* pair) {
    return plant_list_get(pair,  0 );
}
tx_t _second(PlantArray* pair) {
    return plant_list_get(pair,  1 );
}
tx_t is_eof(PlantArray* tokens, long pos) {
  tx_t tok = "";
  tx_t tp = "";
    if (pos >= plant_array_length(tokens)) {
        return 1;
    }
    tok = peek(tokens, pos);
    tp = tok_type(tok);
    if (strcmp(tp,"EOF") == 0) {
        return 1;
    }
    return 0;
}
tx_t escape_string(tx_t s) {
    tx_t r = "";
    long ei = 0;
    long en = strlen( s );
    tx_t ec = "";
    while (ei < en) {
        ec = char_at(s, ei);
        if (strcmp(ec,"\\") == 0) {
            r = _cat(_cat(r, "\\"), "\\");
        }
        if (strcmp(ec,"\"") == 0) {
            r = _cat(_cat(r, "\\"), "\"");
        }
        if (strcmp(ec,"\n") == 0) {
            r = _cat(_cat(r, "\\"), "n");
        }
        if (strcmp(ec,"\t") == 0) {
            r = _cat(_cat(r, "\\"), "t");
        }
        if (strcmp(ec,"\r") == 0) {
            r = _cat(_cat(r, "\\"), "r");
        }
        if (strcmp(ec,"\\") != 0 && strcmp(ec,"\"") != 0 && strcmp(ec,"\n") != 0 && strcmp(ec,"\t") != 0 && strcmp(ec,"\r") != 0) {
            r = _cat(r, ec);
        }
        ei = ei+1;
    }
    return r;
}
tx_t collect_value(PlantArray* tokens, long start) {
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t tt = "";
  tx_t cpair = "";
    tx_t text = "";
    long p2 = start;
    long depth = 0;
    while (1) {
        is_eof_flag = is_eof(tokens, p2);
        if (is_eof_flag) {
            return plant_list_make ( 2 , text , p2 );
        }
        tok = peek(tokens, p2);
        lx = tok_lex(tok);
        tt = tok_type(tok);
        if (strcmp(tt,"STRING") == 0) {
            lx = escape_string(lx);
            lx = _cat(_cat("\"", lx), "\"");
        }
        if (strcmp(lx,".") == 0 && depth == 0) {
            cpair = consume(tokens, p2);
            p2 = _second(cpair);
            return plant_list_make ( 2 , text , p2 );
        }
        if (strcmp(lx,")") == 0 && depth == 0) {
            return plant_list_make ( 2 , text , p2 );
        }
        if (strcmp(lx,"(") == 0) {
            depth = depth+1;
        }
        if (strcmp(lx,")") == 0) {
            depth = depth - 1;
        }
        if (strcmp(text,"") > 0) {
            text = _cat(text, " ");
        }
        text = _cat(text, lx);
        cpair = consume(tokens, p2);
        p2 = _second(cpair);
    }
  return collect_value;
}
tx_t collect_until(PlantArray* tokens, long start, tx_t delim) {
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t tt = "";
  tx_t cpair = "";
    tx_t text = "";
    long p2 = start;
    long depth = 0;
    while (1) {
        is_eof_flag = is_eof(tokens, p2);
        if (is_eof_flag) {
            return plant_list_make ( 2 , text , p2 );
        }
        tok = peek(tokens, p2);
        lx = tok_lex(tok);
        tt = tok_type(tok);
        if (strcmp(tt,"STRING") == 0) {
            lx = escape_string(lx);
            lx = _cat(_cat("\"", lx), "\"");
        }
        if (strcmp(_cat ( "" , lx ),_cat ( "" , delim )) == 0 && depth == 0) {
            return plant_list_make ( 2 , text , p2 );
        }
        if (strcmp(lx,"(") == 0) {
            depth = depth+1;
        }
        if (strcmp(lx,")") == 0) {
            depth = depth - 1;
        }
        if (strcmp(text,"") > 0) {
            text = _cat(text, " ");
        }
        text = _cat(text, lx);
        cpair = consume(tokens, p2);
        p2 = _second(cpair);
    }
  return collect_until;
}
tx_t collect_type_text(PlantArray* tokens, long start, tx_t stopc, long stopcomma) {
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t cpair = "";
    tx_t text = "";
    long p2 = start;
    long pdepth = 0;
    long bdepth = 0;
    while (1) {
        is_eof_flag = is_eof(tokens, p2);
        if (is_eof_flag) {
            return plant_list_make ( 2 , text , p2 );
        }
        tok = peek(tokens, p2);
        lx = tok_lex(tok);
        if (pdepth == 0 && bdepth == 0) {
            if (strcmp(_cat ( "" , lx ),_cat ( "" , stopc )) == 0) {
                return plant_list_make ( 2 , text , p2 );
            }
            if (stopcomma == 1 && strcmp(lx,",") == 0) {
                return plant_list_make ( 2 , text , p2 );
            }
        }
        if (strcmp(lx,"(") == 0) {
            pdepth = pdepth+1;
        }
        if (strcmp(lx,")") == 0) {
            pdepth = pdepth - 1;
        }
        if (strcmp(lx,"[") == 0) {
            bdepth = bdepth+1;
        }
        if (strcmp(lx,"]") == 0) {
            bdepth = bdepth - 1;
        }
        if (strcmp(text,"") > 0) {
            text = _cat(text, " ");
        }
        text = _cat(text, lx);
        cpair = consume(tokens, p2);
        p2 = _second(cpair);
    }
  return collect_type_text;
}
tx_t collect_args(PlantArray* tokens, long pos) {
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t ty = "";
  tx_t com = "";
  tx_t atok = "";
  tx_t alx = "";
  tx_t atype = "";
  tx_t cp = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t com2 = "";
    PlantArray* args = plant_list_make ( 0 );
    long p5 = pos;
    while (1) {
        is_eof_flag = is_eof(tokens, p5);
        if (is_eof_flag) {
            return plant_list_make ( 2 , args , p5 );
        }
        tok = peek(tokens, p5);
        lx = tok_lex(tok);
        ty = tok_type(tok);
        if (strcmp(lx,",") == 0 && strcmp(ty,"STRING") != 0) {
            com = consume(tokens, p5);
            p5 = _second(com);
        }
        if (strcmp(lx,".") == 0 && strcmp(ty,"STRING") != 0) {
            return plant_list_make ( 2 , args , p5 );
        }
        if (strcmp(lx,")") == 0 && strcmp(ty,"STRING") != 0) {
            return plant_list_make ( 2 , args , p5 );
        }
        tx_t arg_text = "";
        long adepth = 0;
        while (1) {
            atok = peek(tokens, p5);
            alx = tok_lex(atok);
            atype = tok_type(atok);
            if (strcmp(atype,"STRING") == 0) {
                alx = escape_string(alx);
                alx = _cat(_cat("\"", alx), "\"");
            }
            if (strcmp(alx,",") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,".") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,")") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,"(") == 0) {
                adepth = adepth+1;
            }
            if (strcmp(alx,")") == 0) {
                adepth = adepth - 1;
            }
            if (strcmp(arg_text,"") > 0) {
                arg_text = _cat(arg_text, " ");
            }
            arg_text = _cat(arg_text, alx);
            cp = consume(tokens, p5);
            p5 = _second(cp);
        }
        args = plant_list_push(args, arg_text);
        tok2 = peek(tokens, p5);
        lx2 = tok_lex(tok2);
        if (strcmp(lx2,",") == 0) {
            com2 = consume(tokens, p5);
            p5 = _second(com2);
        }
    }
  return collect_args;
}
tx_t parse_await_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t act_pair = "";
  tx_t act_name = "";
  tx_t p3 = "";
  tx_t ap = "";
  tx_t p4 = "";
  tx_t dot = "";
  tx_t p5 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    act_pair = consume(tokens, p2);
    act_name = tok_lex(plant_list_get(act_pair,  0 ));
    p3 = _second(act_pair);
    ap = collect_args(tokens, p3);
    PlantArray* args = plant_list_get(ap,  0 );
    p4 = _second(ap);
    dot = consume(tokens, p4);
    p5 = _second(dot);
    return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "await_stmt" , "action" , act_name , "args" , args ) , p5 );
}
tx_t parse_start_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t act_pair = "";
  tx_t act_name = "";
  tx_t p3 = "";
  tx_t ap = "";
  tx_t p4 = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t in_pair = "";
  tx_t p5 = "";
  tx_t ctx_pair = "";
  tx_t p6 = "";
  tx_t dot = "";
  tx_t p7 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    act_pair = consume(tokens, p2);
    act_name = tok_lex(plant_list_get(act_pair,  0 ));
    p3 = _second(act_pair);
    ap = collect_args(tokens, p3);
    PlantArray* args = plant_list_get(ap,  0 );
    p4 = _second(ap);
    tx_t ctx_name = "";
    tok = peek(tokens, p4);
    lx = tok_lex(tok);
    if (strcmp(lx,"IN") == 0) {
        in_pair = consume(tokens, p4);
        p5 = _second(in_pair);
        ctx_pair = consume(tokens, p5);
        ctx_name = tok_lex(plant_list_get(ctx_pair,  0 ));
        p6 = _second(ctx_pair);
        dot = consume(tokens, p6);
        p7 = _second(dot);
        return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "start_stmt" , "action" , act_name , "args" , args , "ctx" , ctx_name ) , p7 );
    }
    dot = consume(tokens, p4);
    p5 = _second(dot);
    return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "start_stmt" , "action" , act_name , "args" , args , "ctx" , ctx_name ) , p5 );
}
tx_t parse_async_in_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t in_pair = "";
  tx_t p3 = "";
  tx_t ctx_pair = "";
  tx_t ctx_name = "";
  tx_t p4 = "";
  tx_t com = "";
  tx_t p5 = "";
  tx_t act_pair = "";
  tx_t act_name = "";
  tx_t p6 = "";
  tx_t ap = "";
  tx_t p7 = "";
  tx_t dot = "";
  tx_t p8 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    in_pair = consume(tokens, p2);
    p3 = _second(in_pair);
    ctx_pair = consume(tokens, p3);
    ctx_name = tok_lex(plant_list_get(ctx_pair,  0 ));
    p4 = _second(ctx_pair);
    com = consume(tokens, p4);
    p5 = _second(com);
    act_pair = consume(tokens, p5);
    act_name = tok_lex(plant_list_get(act_pair,  0 ));
    p6 = _second(act_pair);
    ap = collect_args(tokens, p6);
    PlantArray* args = plant_list_get(ap,  0 );
    p7 = _second(ap);
    dot = consume(tokens, p7);
    p8 = _second(dot);
    return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "start_stmt" , "action" , act_name , "args" , args , "ctx" , ctx_name ) , p8 );
}
tx_t parse_cancel_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t vpair = "";
  tx_t p3 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    vpair = collect_value(tokens, p2);
    tx_t expr = plant_list_get(vpair,  0 );
    p3 = _second(vpair);
    return plant_list_make ( 2 , plant_list_make ( 4 , "type" , "cancel_stmt" , "value" , expr ) , p3 );
}
tx_t parse_trace_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t lv_pair = "";
  tx_t lv = "";
  tx_t p3 = "";
  tx_t vpair = "";
  tx_t p4 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    lv_pair = consume(tokens, p2);
    lv = tok_lex(plant_list_get(lv_pair,  0 ));
    p3 = _second(lv_pair);
    vpair = collect_value(tokens, p3);
    tx_t expr = plant_list_get(vpair,  0 );
    p4 = _second(vpair);
    return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "trace_stmt" , "level" , lv , "value" , expr ) , p4 );
}
tx_t parse_mission_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t cfg_pair = "";
  tx_t p3 = "";
  tx_t key_pair = "";
  tx_t cfg_key = "";
  tx_t p4 = "";
  tx_t eq_tok = "";
  tx_t eq_lx = "";
  tx_t eq_pair = "";
  tx_t val_pair = "";
  tx_t cfg_val = "";
  tx_t p5 = "";
  tx_t dot = "";
  tx_t p6 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    cfg_pair = consume(tokens, p2);
    p3 = _second(cfg_pair);
    key_pair = consume(tokens, p3);
    cfg_key = tok_lex(plant_list_get(key_pair,  0 ));
    p4 = _second(key_pair);
    eq_tok = peek(tokens, p4);
    eq_lx = tok_lex(eq_tok);
    if (strcmp(eq_lx,"=") == 0 || strcmp(eq_lx,"IS") == 0) {
        eq_pair = consume(tokens, p4);
        p4 = _second(eq_pair);
    }
    val_pair = consume(tokens, p4);
    cfg_val = tok_lex(plant_list_get(val_pair,  0 ));
    p5 = _second(val_pair);
    dot = consume(tokens, p5);
    p6 = _second(dot);
    return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "config_stmt" , "key" , cfg_key , "value" , cfg_val ) , p6 );
}
tx_t parse_create_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t id_pair = "";
  tx_t id_name = "";
  tx_t p3 = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t lp = "";
  tx_t p4 = "";
  tx_t tv = "";
  tx_t tt = "";
  tx_t p5 = "";
  tx_t rp = "";
  tx_t p6 = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t eq_pair = "";
  tx_t cb_tok = "";
  tx_t cb_lx = "";
  tx_t clp = "";
  tx_t cnode = "";
  tx_t dotp = "";
  tx_t vpair = "";
  tx_t to_pair = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    id_pair = consume(tokens, p2);
    id_name = tok_lex(plant_list_get(id_pair,  0 ));
    p3 = _second(id_pair);
    tok = peek(tokens, p3);
    lx = tok_lex(tok);
    tx_t vtype = "";
    if (strcmp(lx,"(") == 0) {
        lp = consume(tokens, p3);
        p4 = _second(lp);
        tv = collect_type_text(tokens, p4, ")", 0);
        tt = _first(tv);
        p5 = _second(tv);
        rp = consume(tokens, p5);
        p6 = _second(rp);
        vtype = tt;
        p3 = p6;
    }
    tok2 = peek(tokens, p3);
    lx2 = tok_lex(tok2);
    if (strcmp(lx2,"=") == 0) {
        eq_pair = consume(tokens, p3);
        p4 = _second(eq_pair);
        cb_tok = peek(tokens, p4);
        cb_lx = tok_lex(cb_tok);
        if (strcmp(cb_lx,"[") == 0) {
            clp = parse_closure(tokens, p4);
            cnode = _first(clp);
            p5 = _second(clp);
            if (plant_array_length(cnode) > 0) {
                dotp = consume(tokens, p5);
                p6 = _second(dotp);
                return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "create_stmt" , "target" , id_name , "var_type" , vtype , "value" , "" , "closure" , cnode ) , p6 );
            }
        }
        vpair = collect_value(tokens, p4);
        tx_t expr = plant_list_get(vpair,  0 );
        p5 = _second(vpair);
        return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "create_stmt" , "target" , id_name , "var_type" , vtype , "value" , expr ) , p5 );
    }
    if (strcmp(lx2,"TO") == 0) {
        to_pair = consume(tokens, p3);
        p4 = _second(to_pair);
        cb_tok = peek(tokens, p4);
        cb_lx = tok_lex(cb_tok);
        if (strcmp(cb_lx,"[") == 0) {
            clp = parse_closure(tokens, p4);
            cnode = _first(clp);
            p5 = _second(clp);
            if (plant_array_length(cnode) > 0) {
                dotp = consume(tokens, p5);
                p6 = _second(dotp);
                return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "create_stmt" , "target" , id_name , "var_type" , vtype , "value" , "" , "closure" , cnode ) , p6 );
            }
        }
        vpair = collect_value(tokens, p4);
        tx_t expr = plant_list_get(vpair,  0 );
        p5 = _second(vpair);
        return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "create_stmt" , "target" , id_name , "var_type" , vtype , "value" , expr ) , p5 );
    }
    vpair = collect_value(tokens, p3);
    tx_t expr = plant_list_get(vpair,  0 );
    p4 = _second(vpair);
    return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "create_stmt" , "target" , id_name , "var_type" , vtype , "value" , expr ) , p4 );
}
tx_t parse_show_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t vpair = "";
  tx_t p3 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    vpair = collect_value(tokens, p2);
    tx_t expr = plant_list_get(vpair,  0 );
    p3 = _second(vpair);
    return plant_list_make ( 2 , plant_list_make ( 4 , "type" , "show_stmt" , "value" , expr ) , p3 );
}
tx_t parse_give_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t vpair = "";
  tx_t p3 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    vpair = collect_value(tokens, p2);
    tx_t expr = plant_list_get(vpair,  0 );
    p3 = _second(vpair);
    return plant_list_make ( 2 , plant_list_make ( 4 , "type" , "give_stmt" , "value" , expr ) , p3 );
}
tx_t parse_set_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t id_pair = "";
  tx_t id_name = "";
  tx_t p3 = "";
  tx_t eq = "";
  tx_t p4 = "";
  tx_t vpair = "";
  tx_t p5 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    id_pair = consume(tokens, p2);
    id_name = tok_lex(plant_list_get(id_pair,  0 ));
    p3 = _second(id_pair);
    eq = consume(tokens, p3);
    p4 = _second(eq);
    vpair = collect_value(tokens, p4);
    tx_t expr = plant_list_get(vpair,  0 );
    p5 = _second(vpair);
    return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "set_stmt" , "target" , id_name , "value" , expr ) , p5 );
}
tx_t parse_let_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t id_pair = "";
  tx_t id_name = "";
  tx_t p3 = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t lp = "";
  tx_t p4 = "";
  tx_t tv = "";
  tx_t tt = "";
  tx_t p5 = "";
  tx_t rp = "";
  tx_t p6 = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t eq_pair = "";
  tx_t cb_tok = "";
  tx_t cb_lx = "";
  tx_t clp = "";
  tx_t cnode = "";
  tx_t dotp = "";
  tx_t vpair = "";
  tx_t to_pair = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    id_pair = consume(tokens, p2);
    id_name = tok_lex(plant_list_get(id_pair,  0 ));
    p3 = _second(id_pair);
    tok = peek(tokens, p3);
    lx = tok_lex(tok);
    tx_t vtype = "";
    if (strcmp(lx,"(") == 0) {
        lp = consume(tokens, p3);
        p4 = _second(lp);
        tv = collect_type_text(tokens, p4, ")", 0);
        tt = _first(tv);
        p5 = _second(tv);
        rp = consume(tokens, p5);
        p6 = _second(rp);
        vtype = tt;
        p3 = p6;
    }
    tok2 = peek(tokens, p3);
    lx2 = tok_lex(tok2);
    if (strcmp(lx2,"=") == 0) {
        eq_pair = consume(tokens, p3);
        p4 = _second(eq_pair);
        cb_tok = peek(tokens, p4);
        cb_lx = tok_lex(cb_tok);
        if (strcmp(cb_lx,"[") == 0) {
            clp = parse_closure(tokens, p4);
            cnode = _first(clp);
            p5 = _second(clp);
            if (plant_array_length(cnode) > 0) {
                dotp = consume(tokens, p5);
                p6 = _second(dotp);
                return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "let_stmt" , "target" , id_name , "var_type" , vtype , "value" , "" , "closure" , cnode ) , p6 );
            }
        }
        vpair = collect_value(tokens, p4);
        tx_t expr = plant_list_get(vpair,  0 );
        p5 = _second(vpair);
        return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "let_stmt" , "target" , id_name , "var_type" , vtype , "value" , expr ) , p5 );
    }
    if (strcmp(lx2,"TO") == 0) {
        to_pair = consume(tokens, p3);
        p4 = _second(to_pair);
        cb_tok = peek(tokens, p4);
        cb_lx = tok_lex(cb_tok);
        if (strcmp(cb_lx,"[") == 0) {
            clp = parse_closure(tokens, p4);
            cnode = _first(clp);
            p5 = _second(clp);
            if (plant_array_length(cnode) > 0) {
                dotp = consume(tokens, p5);
                p6 = _second(dotp);
                return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "let_stmt" , "target" , id_name , "var_type" , vtype , "value" , "" , "closure" , cnode ) , p6 );
            }
        }
        vpair = collect_value(tokens, p4);
        tx_t expr = plant_list_get(vpair,  0 );
        p5 = _second(vpair);
        return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "let_stmt" , "target" , id_name , "var_type" , vtype , "value" , expr ) , p5 );
    }
    vpair = collect_value(tokens, p3);
    tx_t expr = plant_list_get(vpair,  0 );
    p4 = _second(vpair);
    return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "let_stmt" , "target" , id_name , "var_type" , vtype , "value" , expr ) , p4 );
}
tx_t parse_closure(PlantArray* tokens, long pos) {
  tx_t lb = "";
  tx_t p2 = "";
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t rb = "";
  tx_t mode_pair = "";
  tx_t mode = "";
  tx_t name_pair = "";
  tx_t cap_name = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t com = "";
  tx_t ptok = "";
  tx_t plx = "";
  tx_t lp = "";
  tx_t p3 = "";
  tx_t rp = "";
  tx_t p4 = "";
  tx_t pn_pair = "";
  tx_t pn = "";
  tx_t lp2 = "";
  tx_t p5 = "";
  tx_t ptv = "";
  tx_t pt = "";
  tx_t rp2 = "";
  tx_t tok3 = "";
  tx_t lx3 = "";
  tx_t com2 = "";
  tx_t atok = "";
  tx_t alx = "";
  tx_t ap = "";
  tx_t stok = "";
  tx_t sty = "";
  tx_t sp = "";
  tx_t btok = "";
  tx_t blx = "";
  tx_t btok2 = "";
  tx_t blx2 = "";
  tx_t bp = "";
  tx_t p6 = "";
  tx_t btok3 = "";
  tx_t blx3 = "";
  tx_t brp = "";
  tx_t d_pair = "";
  tx_t etok = "";
  tx_t elx = "";
  tx_t ety = "";
  tx_t ep = "";
    long start_pos = pos;
    lb = consume(tokens, pos);
    p2 = _second(lb);
    PlantArray* captures = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p2);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 0 ) , start_pos );
        }
        tok = peek(tokens, p2);
        lx = tok_lex(tok);
        if (strcmp(lx,"]") == 0) {
            rb = consume(tokens, p2);
            p2 = _second(rb);
            break;
        }
        mode_pair = consume(tokens, p2);
        mode = tok_lex(plant_list_get(mode_pair,  0 ));
        p2 = _second(mode_pair);
        if (strcmp(mode,"MOVE") != 0 && strcmp(mode,"REF") != 0) {
            return plant_list_make ( 2 , plant_list_make ( 0 ) , start_pos );
        }
        name_pair = consume(tokens, p2);
        cap_name = tok_lex(plant_list_get(name_pair,  0 ));
        p2 = _second(name_pair);
        captures = plant_list_push(captures, plant_list_make ( 4 , "name" , cap_name , "mode" , mode ));
        tok2 = peek(tokens, p2);
        lx2 = tok_lex(tok2);
        if (strcmp(lx2,",") == 0) {
            com = consume(tokens, p2);
            p2 = _second(com);
        }
    }
    ptok = peek(tokens, p2);
    plx = tok_lex(ptok);
    if (strcmp(plx,"(") != 0) {
        return plant_list_make ( 2 , plant_list_make ( 0 ) , start_pos );
    }
    lp = consume(tokens, p2);
    p3 = _second(lp);
    PlantArray* params = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p3);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 0 ) , start_pos );
        }
        tok = peek(tokens, p3);
        lx = tok_lex(tok);
        if (strcmp(lx,")") == 0) {
            rp = consume(tokens, p3);
            p4 = _second(rp);
            break;
        }
        pn_pair = consume(tokens, p3);
        pn = tok_lex(plant_list_get(pn_pair,  0 ));
        p4 = _second(pn_pair);
        tok2 = peek(tokens, p4);
        lx2 = tok_lex(tok2);
        if (strcmp(lx2,"(") == 0) {
            lp2 = consume(tokens, p4);
            p5 = _second(lp2);
            ptv = collect_type_text(tokens, p5, ")", 1);
            pt = _first(ptv);
            p5 = _second(ptv);
            rp2 = consume(tokens, p5);
            p5 = _second(rp2);
            params = plant_list_push(params, plant_list_make ( 4 , "name" , pn , "type" , pt ));
            p4 = p5;
        }
        if (strcmp(lx2,"(") != 0) {
            params = plant_list_push(params, plant_list_make ( 4 , "name" , pn , "type" , "" ));
        }
        tok3 = peek(tokens, p4);
        lx3 = tok_lex(tok3);
        if (strcmp(lx3,",") == 0) {
            com2 = consume(tokens, p4);
            p4 = _second(com2);
        }
        p3 = p4;
    }
    atok = peek(tokens, p4);
    alx = tok_lex(atok);
    if (strcmp(alx,"->") != 0) {
        return plant_list_make ( 2 , plant_list_make ( 0 ) , start_pos );
    }
    ap = consume(tokens, p4);
    p5 = _second(ap);
    stok = peek(tokens, p5);
    sty = tok_type(stok);
    if (strcmp(sty,"MINUS") == 0) {
        sp = consume(tokens, p5);
        p5 = _second(sp);
    }
    tx_t body_kind = "expr";
    btok = peek(tokens, p5);
    blx = tok_lex(btok);
    PlantArray* body = plant_list_make ( 0 );
    if (strcmp(blx,"(") == 0) {
        btok2 = peek(tokens, p5+1);
        blx2 = tok_lex(btok2);
        if (strcmp(blx2,"CREATE") == 0 || strcmp(blx2,"SHOW") == 0 || strcmp(blx2,"GIVE") == 0 || strcmp(blx2,"SET") == 0 || strcmp(blx2,"LET") == 0 || strcmp(blx2,"IF") == 0 || strcmp(blx2,"SEASON") == 0 || strcmp(blx2,"REAP") == 0 || strcmp(blx2,"PUT") == 0 || strcmp(blx2,"BREAK") == 0 || strcmp(blx2,"CONTINUE") == 0) {
            body_kind = "block";
            bp = consume(tokens, p5);
            p6 = _second(bp);
            PlantArray* stmts = plant_list_make ( 0 );
            while (1) {
                is_eof_flag = is_eof(tokens, p6);
                if (is_eof_flag) {
                    break;
                }
                btok3 = peek(tokens, p6);
                blx3 = tok_lex(btok3);
                if (strcmp(blx3,")") == 0) {
                    brp = consume(tokens, p6);
                    p6 = _second(brp);
                    break;
                }
                d_pair = parse_statement(tokens, p6);
                tx_t decl = plant_list_get(d_pair,  0 );
                p6 = _second(d_pair);
                if (strcmp(decl,"") > 0) {
                    stmts = plant_list_push(stmts, decl);
                }
            }
            body = stmts;
            p5 = p6;
        }
    }
    if (strcmp(body_kind,"expr") == 0) {
        tx_t text = "";
        long bd = 0;
        long pd = 0;
        while (1) {
            is_eof_flag = is_eof(tokens, p5);
            if (is_eof_flag) {
                break;
            }
            etok = peek(tokens, p5);
            elx = tok_lex(etok);
            ety = tok_type(etok);
            if (strcmp(ety,"STRING") == 0) {
                elx = escape_string(elx);
                elx = _cat(_cat("\"", elx), "\"");
            }
            if (strcmp(elx,",") == 0 && pd == 0 && bd == 0) {
                break;
            }
            if (strcmp(elx,".") == 0 && pd == 0 && bd == 0) {
                break;
            }
            if (strcmp(elx,"(") == 0) {
                pd = pd+1;
            }
            if (strcmp(elx,")") == 0) {
                pd = pd - 1;
            }
            if (strcmp(elx,"[") == 0) {
                bd = bd+1;
            }
            if (strcmp(elx,"]") == 0) {
                bd = bd - 1;
            }
            if (strcmp(text,"") > 0) {
                text = _cat(text, " ");
            }
            text = _cat(text, elx);
            ep = consume(tokens, p5);
            p5 = _second(ep);
        }
        body = text;
    }
    PlantArray* cnode = plant_list_make ( 12 , "type" , "closure" , "params" , params , "captures" , captures , "body" , body , "bkind" , body_kind );
    return plant_list_make ( 2 , cnode , p5 );
}
tx_t parse_reap_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t var_pair = "";
  tx_t var_name = "";
  tx_t p3 = "";
  tx_t from_pair = "";
  tx_t p4 = "";
  tx_t act_pair = "";
  tx_t act_name = "";
  tx_t p5 = "";
  tx_t next_tok = "";
  tx_t next_lx = "";
  tx_t next_ty = "";
  tx_t colon_pair = "";
  tx_t func_pair = "";
  tx_t func_name = "";
  tx_t ga_tok = "";
  tx_t ga_lx = "";
  tx_t ga_lb = "";
  tx_t gv = "";
  tx_t gtext = "";
  tx_t ga_rb = "";
  tx_t tok0 = "";
  tx_t lx0 = "";
  tx_t ty0 = "";
  tx_t com0 = "";
  tx_t ctok = "";
  tx_t clx = "";
  tx_t clp = "";
  tx_t cnode = "";
  tx_t p6 = "";
  tx_t ctok2 = "";
  tx_t clx2 = "";
  tx_t ccom = "";
  tx_t cdot = "";
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t ty = "";
  tx_t dot = "";
  tx_t atok = "";
  tx_t alx = "";
  tx_t atype = "";
  tx_t cp = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t com = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    var_pair = consume(tokens, p2);
    var_name = tok_lex(plant_list_get(var_pair,  0 ));
    p3 = _second(var_pair);
    from_pair = consume(tokens, p3);
    p4 = _second(from_pair);
    act_pair = consume(tokens, p4);
    act_name = tok_lex(plant_list_get(act_pair,  0 ));
    p5 = _second(act_pair);
    next_tok = peek(tokens, p5);
    next_lx = tok_lex(next_tok);
    next_ty = tok_type(next_tok);
    if (strcmp(next_ty,"COLON") == 0) {
        act_name = _cat(act_name, ":");
        colon_pair = consume(tokens, p5);
        p5 = _second(colon_pair);
        func_pair = consume(tokens, p5);
        func_name = tok_lex(plant_list_get(func_pair,  0 ));
        act_name = _cat(act_name, func_name);
        p5 = _second(func_pair);
    }
    ga_tok = peek(tokens, p5);
    ga_lx = tok_lex(ga_tok);
    if (strcmp(ga_lx,"[") == 0) {
        ga_lb = consume(tokens, p5);
        p5 = _second(ga_lb);
        gv = collect_type_text(tokens, p5, "]", 0);
        gtext = _first(gv);
        p5 = _second(gv);
        ga_rb = consume(tokens, p5);
        p5 = _second(ga_rb);
        act_name = _cat(_cat(_cat(act_name, "["), gtext), "]");
    }
    PlantArray* args = plant_list_make ( 0 );
    PlantArray* clargs = plant_list_make ( 0 );
    while (1) {
        tok0 = peek(tokens, p5);
        lx0 = tok_lex(tok0);
        ty0 = tok_type(tok0);
        if (strcmp(lx0,",") == 0 && strcmp(ty0,"STRING") != 0) {
            com0 = consume(tokens, p5);
            p5 = _second(com0);
        }
        ctok = peek(tokens, p5);
        clx = tok_lex(ctok);
        if (strcmp(clx,"[") == 0) {
            clp = parse_closure(tokens, p5);
            cnode = _first(clp);
            p6 = _second(clp);
            if (plant_array_length(cnode) > 0) {
                args = plant_list_push(args, "@@CLOSURE@@");
                clargs = plant_list_push(clargs, cnode);
                p5 = p6;
                ctok2 = peek(tokens, p5);
                clx2 = tok_lex(ctok2);
                if (strcmp(clx2,",") == 0) {
                    ccom = consume(tokens, p5);
                    p5 = _second(ccom);
                }
                if (strcmp(clx2,".") == 0) {
                    cdot = consume(tokens, p5);
                    p6 = _second(cdot);
                    return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "reap_stmt" , "target" , var_name , "action" , act_name , "args" , args , "clargs" , clargs ) , p6 );
                }
            }
        }
        is_eof_flag = is_eof(tokens, p5);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "reap_stmt" , "target" , var_name , "action" , act_name , "args" , args , "clargs" , clargs ) , p5 );
        }
        tok = peek(tokens, p5);
        lx = tok_lex(tok);
        ty = tok_type(tok);
        if (strcmp(lx,".") == 0 && strcmp(ty,"STRING") != 0) {
            dot = consume(tokens, p5);
            p6 = _second(dot);
            return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "reap_stmt" , "target" , var_name , "action" , act_name , "args" , args , "clargs" , clargs ) , p6 );
        }
        tx_t arg_text = "";
        long adepth = 0;
        while (1) {
            atok = peek(tokens, p5);
            alx = tok_lex(atok);
            atype = tok_type(atok);
            if (strcmp(atype,"STRING") == 0) {
                alx = escape_string(alx);
                alx = _cat(_cat("\"", alx), "\"");
            }
            if (strcmp(alx,",") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,".") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,")") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,"(") == 0) {
                adepth = adepth+1;
            }
            if (strcmp(alx,")") == 0) {
                adepth = adepth - 1;
            }
            if (strcmp(arg_text,"") > 0) {
                arg_text = _cat(arg_text, " ");
            }
            arg_text = _cat(arg_text, alx);
            cp = consume(tokens, p5);
            p5 = _second(cp);
        }
        args = plant_list_push(args, arg_text);
        tok2 = peek(tokens, p5);
        lx2 = tok_lex(tok2);
        if (strcmp(lx2,",") == 0) {
            com = consume(tokens, p5);
            p5 = _second(com);
        }
        if (strcmp(lx2,".") == 0) {
            dot = consume(tokens, p5);
            p6 = _second(dot);
            return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "reap_stmt" , "target" , var_name , "action" , act_name , "args" , args , "clargs" , clargs ) , p6 );
        }
    }
  return parse_reap_stmt;
}
tx_t parse_call_stmt(PlantArray* tokens, long pos) {
  tx_t act_pair = "";
  tx_t act_name = "";
  tx_t p5 = "";
  tx_t next_tok = "";
  tx_t next_ty = "";
  tx_t colon_pair = "";
  tx_t func_pair = "";
  tx_t func_name = "";
  tx_t ga_tok = "";
  tx_t ga_lx = "";
  tx_t ga_lb = "";
  tx_t gv = "";
  tx_t gtext = "";
  tx_t ga_rb = "";
  tx_t lpar = "";
  tx_t tok0 = "";
  tx_t lx0 = "";
  tx_t ty0 = "";
  tx_t com0 = "";
  tx_t ctok = "";
  tx_t clx = "";
  tx_t clp = "";
  tx_t cnode = "";
  tx_t p6 = "";
  tx_t ctok2 = "";
  tx_t clx2 = "";
  tx_t ccom = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t rp = "";
  tx_t dot = "";
  tx_t atok = "";
  tx_t alx = "";
  tx_t atype = "";
  tx_t cp = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t com = "";
    act_pair = consume(tokens, pos);
    act_name = tok_lex(plant_list_get(act_pair,  0 ));
    p5 = _second(act_pair);
    next_tok = peek(tokens, p5);
    next_ty = tok_type(next_tok);
    if (strcmp(next_ty,"COLON") == 0) {
        act_name = _cat(act_name, ":");
        colon_pair = consume(tokens, p5);
        p5 = _second(colon_pair);
        func_pair = consume(tokens, p5);
        func_name = tok_lex(plant_list_get(func_pair,  0 ));
        act_name = _cat(act_name, func_name);
        p5 = _second(func_pair);
    }
    ga_tok = peek(tokens, p5);
    ga_lx = tok_lex(ga_tok);
    if (strcmp(ga_lx,"[") == 0) {
        ga_lb = consume(tokens, p5);
        p5 = _second(ga_lb);
        gv = collect_type_text(tokens, p5, "]", 0);
        gtext = _first(gv);
        p5 = _second(gv);
        ga_rb = consume(tokens, p5);
        p5 = _second(ga_rb);
        act_name = _cat(_cat(_cat(act_name, "["), gtext), "]");
    }
    lpar = consume(tokens, p5);
    p5 = _second(lpar);
    PlantArray* args = plant_list_make ( 0 );
    PlantArray* clargs = plant_list_make ( 0 );
    while (1) {
        tok0 = peek(tokens, p5);
        lx0 = tok_lex(tok0);
        ty0 = tok_type(tok0);
        if (strcmp(lx0,",") == 0 && strcmp(ty0,"STRING") != 0) {
            com0 = consume(tokens, p5);
            p5 = _second(com0);
        }
        ctok = peek(tokens, p5);
        clx = tok_lex(ctok);
        if (strcmp(clx,"[") == 0) {
            clp = parse_closure(tokens, p5);
            cnode = _first(clp);
            p6 = _second(clp);
            if (plant_array_length(cnode) > 0) {
                args = plant_list_push(args, "@@CLOSURE@@");
                clargs = plant_list_push(clargs, cnode);
                p5 = p6;
                ctok2 = peek(tokens, p5);
                clx2 = tok_lex(ctok2);
                if (strcmp(clx2,",") == 0) {
                    ccom = consume(tokens, p5);
                    p5 = _second(ccom);
                }
            }
        }
        tok = peek(tokens, p5);
        lx = tok_lex(tok);
        if (strcmp(lx,")") == 0) {
            rp = consume(tokens, p5);
            p5 = _second(rp);
            dot = consume(tokens, p5);
            p6 = _second(dot);
            return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "call_stmt" , "action" , act_name , "args" , args , "clargs" , clargs ) , p6 );
        }
        tx_t arg_text = "";
        long adepth = 0;
        while (1) {
            atok = peek(tokens, p5);
            alx = tok_lex(atok);
            atype = tok_type(atok);
            if (strcmp(atype,"STRING") == 0) {
                alx = escape_string(alx);
                alx = _cat(_cat("\"", alx), "\"");
            }
            if (strcmp(alx,",") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,")") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,"(") == 0) {
                adepth = adepth+1;
            }
            if (strcmp(alx,")") == 0) {
                adepth = adepth - 1;
            }
            if (strcmp(arg_text,"") > 0) {
                arg_text = _cat(arg_text, " ");
            }
            arg_text = _cat(arg_text, alx);
            cp = consume(tokens, p5);
            p5 = _second(cp);
        }
        args = plant_list_push(args, arg_text);
        tok2 = peek(tokens, p5);
        lx2 = tok_lex(tok2);
        if (strcmp(lx2,",") == 0) {
            com = consume(tokens, p5);
            p5 = _second(com);
        }
    }
  return parse_call_stmt;
}
tx_t parse_put_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t vpair = "";
  tx_t p3 = "";
  tx_t into_pair = "";
  tx_t p4 = "";
  tx_t tpair = "";
  tx_t p5 = "";
  tx_t dot_pair = "";
  tx_t p6 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    vpair = collect_until(tokens, p2, "INTO");
    tx_t item = plant_list_get(vpair,  0 );
    p3 = _second(vpair);
    into_pair = consume(tokens, p3);
    p4 = _second(into_pair);
    tpair = collect_until(tokens, p4, ".");
    tx_t target = plant_list_get(tpair,  0 );
    p5 = _second(tpair);
    dot_pair = consume(tokens, p5);
    p6 = _second(dot_pair);
    return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "put_stmt" , "item" , item , "target" , target ) , p6 );
}
tx_t parse_break_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t drop = "";
  tx_t dot_pair = "";
  tx_t p3 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    if (p2 < plant_array_length(tokens)) {
        tok = peek(tokens, p2);
        lx = tok_lex(tok);
        if (strcmp(lx,"0") == 0) {
            drop = consume(tokens, p2);
            p2 = _second(drop);
        }
    }
    dot_pair = consume(tokens, p2);
    p3 = _second(dot_pair);
    return plant_list_make ( 2 , plant_list_make ( 2 , "type" , "break_stmt" ) , p3 );
}
tx_t parse_continue_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t dot_pair = "";
  tx_t p3 = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    dot_pair = consume(tokens, p2);
    p3 = _second(dot_pair);
    return plant_list_make ( 2 , plant_list_make ( 2 , "type" , "continue_stmt" ) , p3 );
}
tx_t parse_if_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t cpair = "";
  tx_t p3 = "";
  tx_t com = "";
  tx_t p4 = "";
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t slash = "";
  tx_t p5 = "";
  tx_t if_close = "";
  tx_t p6 = "";
  tx_t dot = "";
  tx_t p7 = "";
  tx_t stmt_pair = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    cpair = collect_until(tokens, p2, ",");
    tx_t cond = plant_list_get(cpair,  0 );
    p3 = _second(cpair);
    com = consume(tokens, p3);
    p4 = _second(com);
    PlantArray* body = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p4);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "if_stmt" , "cond" , cond , "body" , body ) , p4 );
        }
        tok = peek(tokens, p4);
        lx = tok_lex(tok);
        if (strcmp(lx,"/") == 0) {
            slash = consume(tokens, p4);
            p5 = _second(slash);
            if_close = consume(tokens, p5);
            p6 = _second(if_close);
            dot = consume(tokens, p6);
            p7 = _second(dot);
            return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "if_stmt" , "cond" , cond , "body" , body ) , p7 );
        }
        stmt_pair = parse_statement(tokens, p4);
        tx_t stmt = plant_list_get(stmt_pair,  0 );
        p4 = _second(stmt_pair);
        if (strcmp(stmt,"") > 0) {
            body = plant_list_push(body, stmt);
        }
    }
  return parse_if_stmt;
}
tx_t parse_season_stmt(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t cpair = "";
  tx_t p3 = "";
  tx_t com = "";
  tx_t p4 = "";
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t slash = "";
  tx_t p5 = "";
  tx_t season_close = "";
  tx_t p6 = "";
  tx_t dot = "";
  tx_t p7 = "";
  tx_t stmt_pair = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    cpair = collect_until(tokens, p2, ",");
    tx_t cond = plant_list_get(cpair,  0 );
    p3 = _second(cpair);
    com = consume(tokens, p3);
    p4 = _second(com);
    PlantArray* body = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p4);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "season_stmt" , "cond" , cond , "body" , body ) , p4 );
        }
        tok = peek(tokens, p4);
        lx = tok_lex(tok);
        if (strcmp(lx,"/") == 0) {
            slash = consume(tokens, p4);
            p5 = _second(slash);
            season_close = consume(tokens, p5);
            p6 = _second(season_close);
            dot = consume(tokens, p6);
            p7 = _second(dot);
            return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "season_stmt" , "cond" , cond , "body" , body ) , p7 );
        }
        stmt_pair = parse_statement(tokens, p4);
        tx_t stmt = plant_list_get(stmt_pair,  0 );
        p4 = _second(stmt_pair);
        if (strcmp(stmt,"") > 0) {
            body = plant_list_push(body, stmt);
        }
    }
  return parse_season_stmt;
}
tx_t parse_statement(PlantArray* tokens, long pos) {
  tx_t tok = "";
  tx_t tp = "";
  tx_t drop_pair = "";
  tx_t lx = "";
  tx_t r = "";
  tx_t nx = "";
  tx_t nx_ty = "";
  tx_t nx2 = "";
  tx_t nx2_ty = "";
  tx_t nx3 = "";
  tx_t nx3_ty = "";
    while (1) {
        tok = peek(tokens, pos);
        tp = tok_type(tok);
        if (strcmp(tp,"DEPTH") != 0) {
            break;
        }
        drop_pair = consume(tokens, pos);
        pos = _second(drop_pair);
    }
    tok = peek(tokens, pos);
    lx = tok_lex(tok);
    if (strcmp(lx,"CREATE") == 0) {
        r = parse_create_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"SHOW") == 0) {
        r = parse_show_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"GIVE") == 0) {
        r = parse_give_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"SET") == 0) {
        r = parse_set_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"LET") == 0) {
        r = parse_let_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"IF") == 0) {
        r = parse_if_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"SEASON") == 0) {
        r = parse_season_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"REAP") == 0) {
        r = parse_reap_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"PUT") == 0) {
        r = parse_put_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"BREAK") == 0) {
        r = parse_break_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"CONTINUE") == 0) {
        r = parse_continue_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"AWAIT") == 0) {
        r = parse_await_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"START") == 0) {
        r = parse_start_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"ASYNC") == 0) {
        r = parse_async_in_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"CANCEL") == 0) {
        r = parse_cancel_stmt(tokens, pos);
        return r;
    }
    if (strcmp(lx,"TRACE") == 0) {
        r = parse_trace_stmt(tokens, pos);
        return r;
    }
    nx = peek(tokens, pos+1);
    nx_ty = tok_type(nx);
    if (strcmp(nx_ty,"LPAREN") == 0) {
        r = parse_call_stmt(tokens, pos);
        return r;
    }
    nx2 = peek(tokens, pos+1);
    nx2_ty = tok_type(nx2);
    if (strcmp(nx2_ty,"COLON") == 0) {
        nx3 = peek(tokens, pos+2);
        nx3_ty = tok_type(nx3);
        if (strcmp(nx3_ty,"LPAREN") == 0) {
            r = parse_call_stmt(tokens, pos);
            return r;
        }
    }
    return plant_list_make ( 2 , NULL , pos + 1 );
}
tx_t parse_enum_decl(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t name_pair = "";
  tx_t name = "";
  tx_t p3 = "";
  tx_t lbr = "";
  tx_t p4 = "";
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t rbr = "";
  tx_t p5 = "";
  tx_t m_pair = "";
  tx_t mname = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t com = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    name_pair = consume(tokens, p2);
    name = tok_lex(plant_list_get(name_pair,  0 ));
    p3 = _second(name_pair);
    lbr = consume(tokens, p3);
    p4 = _second(lbr);
    PlantArray* members = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p4);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "enum_decl" , "name" , name , "members" , members ) , p4 );
        }
        tok = peek(tokens, p4);
        lx = tok_lex(tok);
        if (strcmp(lx,"}") == 0) {
            rbr = consume(tokens, p4);
            p5 = _second(rbr);
            return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "enum_decl" , "name" , name , "members" , members ) , p5 );
        }
        m_pair = consume(tokens, p4);
        mname = tok_lex(plant_list_get(m_pair,  0 ));
        p5 = _second(m_pair);
        members = plant_list_push(members, mname);
        tok2 = peek(tokens, p5);
        lx2 = tok_lex(tok2);
        if (strcmp(lx2,",") == 0) {
            com = consume(tokens, p5);
            p5 = _second(com);
        }
        p4 = p5;
    }
  return parse_enum_decl;
}
tx_t parse_struct_decl(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t name_pair = "";
  tx_t sname = "";
  tx_t p3 = "";
  tx_t sg_tok = "";
  tx_t sg_lx = "";
  tx_t sg_lb = "";
  tx_t sgv = "";
  tx_t sgtext = "";
  tx_t sg_rb = "";
  tx_t sgparts = "";
  tx_t sget = "";
  tx_t lb = "";
  tx_t p4 = "";
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t rb = "";
  tx_t p5 = "";
  tx_t fn_pair = "";
  tx_t fname = "";
  tx_t col_pair = "";
  tx_t p6 = "";
  tx_t ftv = "";
  tx_t ftype = "";
  tx_t p7 = "";
  tx_t ftypet = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t com = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    name_pair = consume(tokens, p2);
    sname = tok_lex(plant_list_get(name_pair,  0 ));
    p3 = _second(name_pair);
    PlantArray* generics = plant_list_make ( 0 );
    sg_tok = peek(tokens, p3);
    sg_lx = tok_lex(sg_tok);
    if (strcmp(sg_lx,"[") == 0) {
        sg_lb = consume(tokens, p3);
        p3 = _second(sg_lb);
        sgv = collect_type_text(tokens, p3, "]", 0);
        sgtext = _first(sgv);
        p3 = _second(sgv);
        sg_rb = consume(tokens, p3);
        p3 = _second(sg_rb);
        sgparts = strings_SPLIT(sgtext, ",");
        long sgi = 0;
        tx_t sge = "";
        while (sgi < plant_array_length(sgparts)) {
            sge = plant_list_get(sgparts, sgi);
            sget = trim(sge);
            if (strcmp(sget,"") > 0) {
                generics = plant_list_push(generics, sget);
            }
            sgi = sgi+1;
        }
    }
    lb = consume(tokens, p3);
    p4 = _second(lb);
    PlantArray* fields = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p4);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "struct_decl" , "name" , sname , "generics" , generics , "fields" , fields ) , p4 );
        }
        tok = peek(tokens, p4);
        lx = tok_lex(tok);
        if (strcmp(lx,"}") == 0) {
            rb = consume(tokens, p4);
            p5 = _second(rb);
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "struct_decl" , "name" , sname , "generics" , generics , "fields" , fields ) , p5 );
        }
        fn_pair = consume(tokens, p4);
        fname = tok_lex(plant_list_get(fn_pair,  0 ));
        p5 = _second(fn_pair);
        col_pair = consume(tokens, p5);
        p6 = _second(col_pair);
        ftv = collect_type_text(tokens, p6, "}", 1);
        ftype = _first(ftv);
        p7 = _second(ftv);
        ftypet = trim(ftype);
        fields = plant_list_push(fields, plant_list_make ( 4 , "name" , fname , "type" , ftypet ));
        tok2 = peek(tokens, p7);
        lx2 = tok_lex(tok2);
        if (strcmp(lx2,",") == 0) {
            com = consume(tokens, p7);
            p7 = _second(com);
        }
        p4 = p7;
    }
  return parse_struct_decl;
}
tx_t parse_action_decl(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t name_pair = "";
  tx_t aname = "";
  tx_t p3 = "";
  tx_t ga_tok = "";
  tx_t ga_lx = "";
  tx_t ga_lb = "";
  tx_t gv = "";
  tx_t gtext = "";
  tx_t ga_rb = "";
  tx_t gparts = "";
  tx_t ge2t = "";
  tx_t lp = "";
  tx_t p4 = "";
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t tokv2 = "";
  tx_t lxv2 = "";
  tx_t ctok2 = "";
  tx_t clx2 = "";
  tx_t tokv3 = "";
  tx_t lxv3 = "";
  tx_t rp = "";
  tx_t p5 = "";
  tx_t pn_pair = "";
  tx_t pn = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t lp2 = "";
  tx_t p6 = "";
  tx_t ptv = "";
  tx_t pt = "";
  tx_t p7 = "";
  tx_t rp2 = "";
  tx_t tok3 = "";
  tx_t lx3 = "";
  tx_t com = "";
  tx_t arrow_tok = "";
  tx_t arrow_lx = "";
  tx_t arrow_pair = "";
  tx_t skip_tok = "";
  tx_t skip_ty = "";
  tx_t skip_pair = "";
  tx_t ret_pair = "";
  tx_t ret_lx = "";
  tx_t rtv = "";
  tx_t rtt = "";
  tx_t vst = "";
  tx_t vsl = "";
  tx_t vsp = "";
  tx_t lt_tok = "";
  tx_t lt_lx = "";
  tx_t lt_pair = "";
  tx_t rt_pair = "";
  tx_t rc_pair = "";
  tx_t re_pair = "";
  tx_t gt_pair = "";
  tx_t pr_tok = "";
  tx_t pr_lx = "";
  tx_t pr_pair = "";
  tx_t lv_pair = "";
  tx_t lv_lx = "";
  tx_t after_tok = "";
  tx_t after_lx = "";
  tx_t com_pair = "";
  tx_t dot_pair = "";
  tx_t tok4 = "";
  tx_t lx4 = "";
  tx_t slash = "";
  tx_t end = "";
  tx_t dot = "";
  tx_t p8 = "";
  tx_t stmt_pair = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    name_pair = consume(tokens, p2);
    aname = tok_lex(plant_list_get(name_pair,  0 ));
    p3 = _second(name_pair);
    tx_t prio = "1";
    PlantArray* generics = plant_list_make ( 0 );
    ga_tok = peek(tokens, p3);
    ga_lx = tok_lex(ga_tok);
    if (strcmp(ga_lx,"[") == 0) {
        ga_lb = consume(tokens, p3);
        p3 = _second(ga_lb);
        gv = collect_type_text(tokens, p3, "]", 0);
        gtext = _first(gv);
        p3 = _second(gv);
        ga_rb = consume(tokens, p3);
        p3 = _second(ga_rb);
        gparts = strings_SPLIT(gtext, ",");
        long gi2 = 0;
        tx_t ge2 = "";
        while (gi2 < plant_array_length(gparts)) {
            ge2 = plant_list_get(gparts, gi2);
            ge2t = trim(ge2);
            if (strcmp(ge2t,"") > 0) {
                generics = plant_list_push(generics, ge2t);
            }
            gi2 = gi2+1;
        }
    }
    lp = consume(tokens, p3);
    p4 = _second(lp);
    PlantArray* params = plant_list_make ( 0 );
    tx_t vararg = "0";
    while (1) {
        is_eof_flag = is_eof(tokens, p4);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 12 , "type" , "action_decl" , "name" , aname , "generics" , generics , "params" , params , "body" , plant_list_make ( 0 ) , "prio" , prio ) , p4 );
        }
        tok = peek(tokens, p4);
        lx = tok_lex(tok);
        if (strcmp(lx,"..") == 0) {
            tokv2 = peek(tokens, p4+1);
            lxv2 = tok_lex(tokv2);
            if (strcmp(lxv2,".") == 0) {
                vararg = "1";
                p4 = p4+2;
                ctok2 = peek(tokens, p4);
                clx2 = tok_lex(ctok2);
                if (strcmp(clx2,")") == 0) {
                    p4 = p4+1;
                }
                if (strcmp(clx2,",") == 0) {
                    p4 = p4+2;
                }
                p5 = p4;
                break;
            }
        }
        if (strcmp(lx,".") == 0) {
            tokv2 = peek(tokens, p4+1);
            lxv2 = tok_lex(tokv2);
            if (strcmp(lxv2,"..") == 0) {
                vararg = "1";
                p4 = p4+2;
                ctok2 = peek(tokens, p4);
                clx2 = tok_lex(ctok2);
                if (strcmp(clx2,")") == 0) {
                    p4 = p4+1;
                }
                if (strcmp(clx2,",") == 0) {
                    p4 = p4+2;
                }
                p5 = p4;
                break;
            }
            if (strcmp(lxv2,".") == 0) {
                tokv3 = peek(tokens, p4+2);
                lxv3 = tok_lex(tokv3);
                if (strcmp(lxv3,".") == 0) {
                    vararg = "1";
                    p4 = p4+3;
                    ctok2 = peek(tokens, p4);
                    clx2 = tok_lex(ctok2);
                    if (strcmp(clx2,")") == 0) {
                        p4 = p4+1;
                    }
                    if (strcmp(clx2,",") == 0) {
                        p4 = p4+2;
                    }
                    p5 = p4;
                    break;
                }
            }
        }
        if (strcmp(lx,")") == 0) {
            rp = consume(tokens, p4);
            p5 = _second(rp);
            long brk = 0;
            break;
        }
        pn_pair = consume(tokens, p4);
        pn = tok_lex(plant_list_get(pn_pair,  0 ));
        p5 = _second(pn_pair);
        tok2 = peek(tokens, p5);
        lx2 = tok_lex(tok2);
        if (strcmp(lx2,"(") == 0) {
            lp2 = consume(tokens, p5);
            p6 = _second(lp2);
            ptv = collect_type_text(tokens, p6, ")", 1);
            pt = _first(ptv);
            p7 = _second(ptv);
            pt = strings_REPLACE(pt, " *", "*");
            pt = strings_REPLACE(pt, "* ", "*");
            rp2 = consume(tokens, p7);
            p7 = _second(rp2);
            params = plant_list_push(params, plant_list_make ( 4 , "name" , pn , "type" , pt ));
            p5 = p7;
        }
        if (strcmp(lx2,"(") != 0) {
            params = plant_list_push(params, plant_list_make ( 4 , "name" , pn , "type" , "" ));
        }
        tok3 = peek(tokens, p5);
        lx3 = tok_lex(tok3);
        if (strcmp(lx3,",") == 0) {
            com = consume(tokens, p5);
            p5 = _second(com);
        }
        p4 = p5;
    }
    arrow_tok = peek(tokens, p5);
    arrow_lx = tok_lex(arrow_tok);
    if (strcmp(arrow_lx,"->") == 0) {
        arrow_pair = consume(tokens, p5);
        p5 = _second(arrow_pair);
        skip_tok = peek(tokens, p5);
        skip_ty = tok_type(skip_tok);
        if (strcmp(skip_ty,"MINUS") == 0) {
            skip_pair = consume(tokens, p5);
            p5 = _second(skip_pair);
        }
        ret_pair = consume(tokens, p5);
        ret_lx = tok_lex(plant_list_get(ret_pair,  0 ));
        p5 = _second(ret_pair);
        tx_t ret = "";
        if (strcmp(ret_lx,"Result") == 0) {
            ret = "Result";
        }
        if (strcmp(ret_lx,"STRUCT") == 0) {
            rtv = collect_type_text(tokens, p5, ".", 0);
            rtt = _first(rtv);
            p5 = _second(rtv);
            ret = _cat("STRUCT ", rtt);
        }
        if (strcmp(ret_lx,"void") == 0) {
            vst = peek(tokens, p5);
            vsl = tok_lex(vst);
            if (strcmp(vsl,"*") == 0) {
                vsp = consume(tokens, p5);
                p5 = _second(vsp);
                ret = "void*";
            }
        }
        if (strcmp(ret_lx,"Result") == 0) {
            lt_tok = peek(tokens, p5);
            lt_lx = tok_lex(lt_tok);
            if (strcmp(lt_lx,"<") == 0) {
                lt_pair = consume(tokens, p5);
                p5 = _second(lt_pair);
                rt_pair = consume(tokens, p5);
                p5 = _second(rt_pair);
                rc_pair = consume(tokens, p5);
                p5 = _second(rc_pair);
                re_pair = consume(tokens, p5);
                p5 = _second(re_pair);
                gt_pair = consume(tokens, p5);
                p5 = _second(gt_pair);
            }
        }
        pr_tok = peek(tokens, p5);
        pr_lx = tok_lex(pr_tok);
        if (strcmp(pr_lx,"PRIORITY") == 0) {
            pr_pair = consume(tokens, p5);
            p5 = _second(pr_pair);
            lv_pair = consume(tokens, p5);
            lv_lx = tok_lex(plant_list_get(lv_pair,  0 ));
            p5 = _second(lv_pair);
            if (strcmp(lv_lx,"HIGH") == 0) {
                prio = "0";
            }
            if (strcmp(lv_lx,"NORMAL") == 0) {
                prio = "1";
            }
            if (strcmp(lv_lx,"LOW") == 0) {
                prio = "2";
            }
        }
        after_tok = peek(tokens, p5);
        after_lx = tok_lex(after_tok);
        if (strcmp(after_lx,",") == 0) {
            com_pair = consume(tokens, p5);
            p5 = _second(com_pair);
        }
        if (strcmp(after_lx,".") == 0) {
            dot_pair = consume(tokens, p5);
            p5 = _second(dot_pair);
            if (strcmp(ret_lx,"external") == 0 || strcmp(ret_lx,"Result") == 0 || strcmp(ret_lx,"STRUCT") == 0 || strcmp(ret,"void*") == 0) {
                return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "external_decl" , "name" , aname , "params" , params , "ret" , ret , "varargs" , vararg ) , p5 );
            }
            return plant_list_make ( 2 , plant_list_make ( 12 , "type" , "action_decl" , "name" , aname , "generics" , generics , "params" , params , "body" , plant_list_make ( 0 ) , "prio" , prio ) , p5 );
        }
    }
    PlantArray* body = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p5);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 12 , "type" , "action_decl" , "name" , aname , "generics" , generics , "params" , params , "body" , body , "prio" , prio ) , p5 );
        }
        tok4 = peek(tokens, p5);
        lx4 = tok_lex(tok4);
        if (strcmp(lx4,"/") == 0) {
            slash = consume(tokens, p5);
            p6 = _second(slash);
            end = consume(tokens, p6);
            p7 = _second(end);
            dot = consume(tokens, p7);
            p8 = _second(dot);
            return plant_list_make ( 2 , plant_list_make ( 12 , "type" , "action_decl" , "name" , aname , "generics" , generics , "params" , params , "body" , body , "prio" , prio ) , p8 );
        }
        stmt_pair = parse_statement(tokens, p5);
        tx_t stmt = plant_list_get(stmt_pair,  0 );
        p5 = _second(stmt_pair);
        if (strcmp(stmt,"") > 0) {
            body = plant_list_push(body, stmt);
        }
    }
  return parse_action_decl;
}
tx_t parse_declaration(PlantArray* tokens, long pos) {
  tx_t tok = "";
  tx_t lx = "";
  tx_t r = "";
  tx_t pair = "";
  tx_t nx = "";
  tx_t nd = "";
  tx_t np = "";
  tx_t nt = "";
  tx_t nd2 = "";
    tok = peek(tokens, pos);
    lx = tok_lex(tok);
    if (strcmp(lx,"ENUM") == 0) {
        r = parse_enum_decl(tokens, pos);
        return r;
    }
    if (strcmp(lx,"STRUCT") == 0) {
        r = parse_struct_decl(tokens, pos);
        return r;
    }
    if (strcmp(lx,"ACTION") == 0) {
        r = parse_action_decl(tokens, pos);
        return r;
    }
    if (strcmp(lx,"ASYNC") == 0) {
        pair = peek(tokens, pos+1);
        nx = tok_lex(pair);
        if (strcmp(nx,"IN") == 0) {
            r = parse_async_in_stmt(tokens, pos);
            return r;
        }
        r = parse_action_decl(tokens, pos+1);
        nd = _first(r);
        np = _second(r);
        nt = _map_get(nd, "type");
        if (strcmp(nt,"action_decl") == 0) {
            nd2 = map_add(nd, "async", "1");
            return plant_list_make ( 2 , nd2 , np );
        }
        return r;
    }
    if (strcmp(lx,"MISSION") == 0) {
        r = parse_mission_stmt(tokens, pos);
        return r;
    }
    r = parse_statement(tokens, pos);
    return r;
}
tx_t map_add(PlantArray* m, tx_t k, tx_t v) {
  tx_t me3 = "";
    PlantArray* out = plant_list_make ( 0 );
    long mi3 = 0;
    while (mi3 < plant_array_length(m)) {
        me3 = plant_list_get(m, mi3);
        out = plant_list_push(out, me3);
        mi3 = mi3+1;
    }
    out = plant_list_push(out, k);
    out = plant_list_push(out, v);
    return out;
}
tx_t parse_program(PlantArray* tokens) {
  tx_t is_eof_flag = "";
  tx_t d_pair = "";
  tx_t pos2 = "";
    long pos = 0;
    PlantArray* nodes = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, pos);
        if (is_eof_flag) {
            return plant_list_make ( 4 , "type" , "program" , "body" , nodes );
        }
        d_pair = parse_declaration(tokens, pos);
        tx_t decl = plant_list_get(d_pair,  0 );
        pos2 = _second(d_pair);
        if (pos2 <= pos) {
            return plant_list_make ( 4 , "type" , "program" , "body" , nodes );
        }
        pos = pos2;
        if (strcmp(decl,"") > 0) {
            nodes = plant_list_push(nodes, decl);
        }
    }
  return parse_program;
}
tx_t _substr(tx_t str, long start, long end) {
    return substring ( str , start , end );
}
tx_t _handle_func(tx_t expr, tx_t kw, tx_t cfn) {
  tx_t parts = "";
  tx_t p0 = "";
  tx_t cur = "";
  tx_t pos = "";
  tx_t vname = "";
  tx_t rest = "";
    parts = strings_SPLIT((tx_t)expr, _cat(kw, " "));
    if (plant_array_length(parts) == 1) {
        return expr;
    }
    p0 = plant_list_get(parts, 0);
    tx_t res = p0;
    long idx = 1;
    while (idx < plant_array_length(parts)) {
        cur = plant_list_get(parts, idx);
        pos = find_any(cur, " +)(");
        if (pos == - 1) {
            pos = strlen( cur );
        }
        vname = substring(cur, 0, pos);
        rest = substring(cur, pos, strlen( cur ));
        res = _cat(_cat(_cat(_cat(_cat(res, cfn), "("), vname), ")"), rest);
        idx = idx+1;
    }
    return res;
}
tx_t _handle_func_paren(tx_t expr, tx_t kw, tx_t cfn) {
  tx_t parts = "";
  tx_t p0 = "";
  tx_t p = "";
    parts = strings_SPLIT((tx_t)expr, _cat(kw, " ("));
    if (plant_array_length(parts) == 1) {
        parts = strings_SPLIT((tx_t)expr, _cat(kw, "("));
    }
    if (plant_array_length(parts) == 1) {
        return expr;
    }
    p0 = plant_list_get(parts, 0);
    tx_t res = p0;
    long idx = 1;
    while (idx < plant_array_length(parts)) {
        p = plant_list_get(parts, idx);
        res = _cat(_cat(_cat(res, cfn), "("), p);
        idx = idx+1;
    }
    return res;
}
tx_t seg_is_numeric(tx_t seg, PlantArray* nums) {
  tx_t sb = "";
  tx_t pre0 = "";
  tx_t pre1 = "";
  tx_t pre2 = "";
  tx_t pre3 = "";
  tx_t pre4 = "";
  tx_t pre5 = "";
  tx_t pre6 = "";
  tx_t pre7 = "";
  tx_t pre8 = "";
  tx_t isid = "";
  tx_t isdig2 = "";
  tx_t isop = "";
  tx_t mf = "";
    sb = strings_REPLACE((tx_t)seg, " ", "");
    if (strcmp(sb,"") == 0) {
        return 0;
    }
    pre0 = substring(sb, 0, 7);
    if (strcmp(pre0,"strlen(") == 0) {
        return 1;
    }
    pre1 = substring(sb, 0, 19);
    if (strcmp(pre1,"plant_array_length(") == 0) {
        return 1;
    }
    pre2 = substring(sb, 0, 9);
    if (strcmp(pre2,"_to_long(") == 0) {
        return 1;
    }
    pre3 = substring(sb, 0, 9);
    if (strcmp(pre3,"json_len(") == 0) {
        return 1;
    }
    pre4 = substring(sb, 0, 10);
    if (strcmp(pre4,"json_kind(") == 0) {
        return 1;
    }
    pre5 = substring(sb, 0, 9);
    if (strcmp(pre5,"set_size(") == 0) {
        return 1;
    }
    pre6 = substring(sb, 0, 11);
    if (strcmp(pre6,"queue_size(") == 0) {
        return 1;
    }
    pre7 = substring(sb, 0, 11);
    if (strcmp(pre7,"stack_size(") == 0) {
        return 1;
    }
    pre8 = substring(sb, 0, 14);
    if (strcmp(pre8,"ffi_last_error(") == 0) {
        return 1;
    }
    long si = 0;
    tx_t sc = "";
    tx_t tok = "";
    long toknum = 1;
    while (si < strlen( sb )) {
        sc = char_at(sb, si);
        isid = find_any(sc, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
        if (isid != - 1) {
            tok = _cat(tok, sc);
            isdig2 = find_any(sc, "0123456789");
            if (isdig2 == - 1) {
                toknum = 0;
            }
        }
        if (isid == - 1) {
            isop = find_any(sc, "+-*/%^<>=!&|()");
            if (isop == - 1) {
                return 0;
            }
            if (strcmp(tok,"") > 0) {
                if (toknum == 0) {
                    mf = list_contains(nums, tok);
                    if (mf == 0) {
                        return 0;
                    }
                }
                tok = "";
                toknum = 1;
            }
        }
        si = si+1;
    }
    if (strcmp(tok,"") > 0) {
        if (toknum == 0) {
            mf = list_contains(nums, tok);
            if (mf == 0) {
                return 0;
            }
        }
    }
    return 1;
}
tx_t expr_is_numeric(tx_t e, PlantArray* nums) {
  tx_t q0 = "";
  tx_t sn = "";
    q0 = substring(e, 0, 1);
    if (strcmp(q0,"\"") == 0) {
        return 0;
    }
    sn = seg_is_numeric(e, nums);
    return sn;
}
tx_t _handle_cat(tx_t expr, PlantArray* nums, PlantArray* evars) {
  tx_t isdigit = "";
  tx_t sg0 = "";
  tx_t sgn = "";
  tx_t snm = "";
    PlantArray* parts = plant_list_make ( 0 );
    long depth = 0;
    long instr = 0;
    long i = 0;
    tx_t ch = "";
    tx_t c0 = "";
    tx_t c1 = "";
    tx_t seg = "";
    long start = 0;
    tx_t res = "";
    long pi = 0;
    tx_t pel = "";
    long has_str = 0;
    long has_digit = 0;
    while (i < strlen( expr )) {
        ch = char_at(expr, i);
        if (strcmp(ch,"\"") == 0) {
            instr = 1 - instr;
            has_str = 1;
        }
        if (instr == 1 && strcmp(ch,"\\") == 0) {
            i = i+1;
        }
        isdigit = find_any(ch, "0123456789");
        if (isdigit != - 1) {
            has_digit = 1;
        }
        if (instr == 0 && strcmp(ch,"(") == 0) {
            depth = depth+1;
        }
        if (instr == 0 && strcmp(ch,")") == 0) {
            depth = depth - 1;
        }
        if (instr == 0 && strcmp(ch,"+") == 0 && depth == 0) {
            c0 = char_at(expr, i - 1);
            c1 = char_at(expr, i+1);
            if (strcmp(c0," ") == 0 && strcmp(c1," ") == 0) {
                seg = substring(expr, start, i - 1);
                parts = plant_list_push(parts, seg);
                start = i+2;
            }
        }
        i = i+1;
    }
    seg = substring(expr, start, strlen( expr ));
    if (( plant_array_length(parts) ) == 0) {
        return expr;
    }
    if (has_str == 0 && has_digit == 1) {
        return strings_REPLACE ( expr , " + " , "+" );
    }
    long allnum = 1;
    if (has_str == 0 && has_digit == 0) {
        long ni0 = 0;
        tx_t pe0 = "";
        while (ni0 < plant_array_length(parts)) {
            pe0 = plant_list_get(parts, ni0);
            sg0 = seg_is_numeric(pe0, nums);
            if (sg0 == 0) {
                allnum = 0;
            }
            ni0 = ni0+1;
        }
        sg0 = seg_is_numeric(seg, nums);
        if (sg0 == 0) {
            allnum = 0;
        }
        if (allnum == 1) {
            return strings_REPLACE ( expr , " + " , "+" );
        }
    }
    parts = plant_list_push(parts, seg);
    PlantArray* nparts = plant_list_make ( 0 );
    long ni = 0;
    tx_t pel2 = "";
    while (ni < plant_array_length(parts)) {
        pel2 = plant_list_get(parts, ni);
        sgn = seg_is_numeric(pel2, nums);
        if (sgn == 1) {
            pel2 = _cat(_cat("_from_long(", pel2), ")");
        }
        snm = enum_in_table(evars, pel2);
        if (sgn == 0 && strcmp(snm,"") != 0) {
            pel2 = _cat(_cat(_cat(_cat("_from_enum(", pel2), ", \""), snm), "\")");
        }
        nparts = plant_list_push(nparts, pel2);
        ni = ni+1;
    }
    res = plant_list_get(nparts,  0 );
    pi = 1;
    while (pi < plant_array_length(nparts)) {
        pel = plant_list_get(nparts, pi);
        res = _cat(_cat(_cat(_cat("_cat(", res), ", "), pel), ")");
        pi = pi+1;
    }
    return res;
}
tx_t collect_declared_walk(PlantArray* bd, PlantArray* declared) {
  tx_t tgt = "";
  tx_t sub_bd = "";
  tx_t sub_ret = "";
    long wi = 0;
    tx_t nd = "";
    tx_t ty = "";
    while (wi < plant_array_length(bd)) {
        nd = plant_list_get(bd, wi);
        ty = _map_get(nd, "type");
        if (strcmp(ty,"create_stmt") == 0 || strcmp(ty,"let_stmt") == 0) {
            tgt = _map_get(nd, "target");
            declared = plant_list_push(declared, tgt);
        }
        if (strcmp(ty,"if_stmt") == 0 || strcmp(ty,"season_stmt") == 0) {
            sub_bd = _map_get(nd, "body");
            sub_ret = collect_declared_walk(sub_bd, declared);
        }
        wi = wi+1;
    }
    return "ok";
}
tx_t collect_used_walk(PlantArray* bd, PlantArray* used, PlantArray* declared) {
  tx_t tgt = "";
  tx_t sub_bd = "";
  tx_t sub_ret = "";
    long wi = 0;
    tx_t nd = "";
    tx_t ty = "";
    while (wi < plant_array_length(bd)) {
        nd = plant_list_get(bd, wi);
        ty = _map_get(nd, "type");
        if (strcmp(ty,"reap_stmt") == 0) {
            tgt = _map_get(nd, "target");
            long found = 0;
            long fi = 0;
            tx_t fe = "";
            while (fi < plant_array_length(declared)) {
                fe = plant_list_get(declared, fi);
                if (strcmp(str_eq ( fe , tgt ),"1") == 0) {
                    found = 1;
                }
                fi = fi+1;
            }
            fi = 0;
            while (fi < plant_array_length(used)) {
                fe = plant_list_get(used, fi);
                if (strcmp(str_eq ( fe , tgt ),"1") == 0) {
                    found = 1;
                }
                fi = fi+1;
            }
            if (!found) {
                used = plant_list_push(used, tgt);
            }
        }
        if (strcmp(ty,"if_stmt") == 0 || strcmp(ty,"season_stmt") == 0) {
            sub_bd = _map_get(nd, "body");
            sub_ret = collect_used_walk(sub_bd, used, declared);
        }
        wi = wi+1;
    }
    return "ok";
}
tx_t collect_implicit(PlantArray* bd, PlantArray* params) {
  tx_t sub_ret = "";
    PlantArray* used = plant_list_make ( 0 );
    PlantArray* declared = plant_list_make ( 0 );
    long ci = 0;
    tx_t pn = "";
    while (ci < plant_array_length(params)) {
        pn = _map_get(plant_list_get(params,  ci ), "name");
        declared = plant_list_push(declared, pn);
        ci = ci+1;
    }
    sub_ret = collect_declared_walk(bd, declared);
    sub_ret = collect_used_walk(bd, used, declared);
    return used;
}
tx_t build_enum_registry(PlantArray* ast) {
  tx_t enm = "";
    PlantArray* reg = plant_list_make ( 0 );
    long ei = 0;
    tx_t ne = "";
    tx_t nty = "";
    while (ei < plant_array_length(ast)) {
        ne = plant_list_get(ast, ei);
        nty = _map_get(ne, "type");
        if (strcmp(nty,"enum_decl") == 0) {
            enm = _map_get(ne, "name");
            PlantArray* mems = _map_get ( ne , "members" );
            tx_t csv = "";
            long mi = 0;
            while (mi < plant_array_length(mems)) {
                if (mi > 0) {
                    csv = _cat(csv, ",");
                }
                csv = _cat(csv, plant_list_get ( mems , mi ));
                mi = mi+1;
            }
            reg = plant_list_push(reg, enm);
            reg = plant_list_push(reg, csv);
        }
        ei = ei+1;
    }
    return reg;
}
tx_t enum_members_of(PlantArray* reg, tx_t ty) {
    tx_t res = "";
    long ri = 0;
    tx_t rn = "";
    tx_t rv = "";
    while (ri + 1 < plant_array_length(reg)) {
        rn = plant_list_get(reg, ri);
        rv = plant_list_get(reg, ri+1);
        if (strcmp(str_eq ( rn , ty ),"1") == 0) {
            res = rv;
        }
        ri = ri+2;
    }
    return res;
}
tx_t collect_enums_walk(tx_t bd, tx_t subst, tx_t reg, tx_t res) {
  tx_t wfound = "";
  tx_t wret = "";
    long wi = 0;
    tx_t wnd = "";
    tx_t wty = "";
    tx_t wtg = "";
    tx_t wvt = "";
    tx_t wbs = "";
    tx_t wms = "";
    while (wi < plant_array_length(bd)) {
        wnd = plant_list_get(bd, wi);
        wty = _map_get(wnd, "type");
        if (strcmp(wty,"create_stmt") == 0 || strcmp(wty,"let_stmt") == 0) {
            wtg = _map_get(wnd, "target");
            wvt = _map_get(wnd, "var_type");
            wbs = subst_type(wvt, subst);
            wms = enum_members_of(reg, wbs);
            if (strcmp(wms,"") > 0) {
                wfound = list_contains(res, wtg);
                if (wfound == 0) {
                    res = plant_list_push(res, wtg);
                    res = plant_list_push(res, wms);
                }
            }
        }
        if (strcmp(wty,"if_stmt") == 0 || strcmp(wty,"season_stmt") == 0 || strcmp(wty,"cycle_stmt") == 0 || strcmp(wty,"match_stmt") == 0) {
            PlantArray* sbl = _map_get ( wnd , "body" );
            wret = collect_enums_walk(sbl, subst, reg, res);
        }
        wi = wi+1;
    }
    return res;
}
tx_t collect_enums(tx_t bd, tx_t params, tx_t subst, tx_t reg) {
  tx_t pfound = "";
  tx_t mf = "";
  tx_t wr = "";
    PlantArray* res = plant_list_make ( 0 );
    long pi = 0;
    tx_t pn2 = "";
    tx_t pt2 = "";
    tx_t psu2 = "";
    tx_t pm2 = "";
    while (pi < plant_array_length(params)) {
        pn2 = _map_get(plant_list_get(params,  pi ), "name");
        pt2 = _map_get(plant_list_get(params,  pi ), "type");
        psu2 = subst_type(pt2, subst);
        pm2 = enum_members_of(reg, psu2);
        if (strcmp(pm2,"") != 0) {
            pfound = list_contains(res, pn2);
            if (pfound == 0) {
                res = plant_list_push(res, pn2);
                res = plant_list_push(res, pm2);
            }
        }
        pi = pi+1;
    }
    long ri = 0;
    tx_t rn = "";
    tx_t rv = "";
    while (ri + 1 < plant_array_length(reg)) {
        rn = plant_list_get(reg, ri);
        rv = plant_list_get(reg, ri+1);
        PlantArray* ms = plant_list_make ( 0 );
        ms = strings_SPLIT((tx_t)rv, ",");
        long mi = 0;
        tx_t mname = "";
        while (mi < plant_array_length(ms)) {
            mname = plant_list_get(ms, mi);
            if (strcmp(mname,"") > 0) {
                mf = list_contains(res, mname);
                if (mf == 0) {
                    res = plant_list_push(res, mname);
                    res = plant_list_push(res, rv);
                }
            }
            mi = mi+1;
        }
        ri = ri+2;
    }
    wr = collect_enums_walk(bd, subst, reg, res);
    return res;
}
tx_t enum_in_table(PlantArray* evars, tx_t name) {
    tx_t res = "";
    long ej = 0;
    tx_t ekey = "";
    tx_t eval = "";
    while (ej + 1 < plant_array_length(evars)) {
        ekey = plant_list_get(evars, ej);
        eval = plant_list_get(evars, ej+1);
        if (strcmp(str_eq ( ekey , name ),"1") == 0) {
            res = eval;
        }
        ej = ej+2;
    }
    return res;
}
tx_t list_contains(PlantArray* lst, tx_t x) {
    long found = 0;
    long li = 0;
    tx_t le = "";
    while (li < plant_array_length(lst)) {
        le = plant_list_get(lst, li);
        if (strcmp(str_eq ( le , x ),"1") == 0) {
            found = 1;
        }
        li = li+1;
    }
    return found;
}
tx_t collect_nums_walk(PlantArray* bd, PlantArray* subst, PlantArray* res) {
  tx_t wfound = "";
  tx_t wbd2 = "";
  tx_t wret2 = "";
    long wi = 0;
    tx_t wnd = "";
    tx_t wty = "";
    tx_t wtg = "";
    tx_t wvt = "";
    tx_t wbs = "";
    tx_t wbb = "";
    while (wi < plant_array_length(bd)) {
        wnd = plant_list_get(bd, wi);
        wty = _map_get(wnd, "type");
        if (strcmp(wty,"create_stmt") == 0 || strcmp(wty,"let_stmt") == 0) {
            wtg = _map_get(wnd, "target");
            wvt = _map_get(wnd, "var_type");
            wbs = subst_type(wvt, subst);
            wbb = type_base(wbs);
            if (strcmp(wbb,"NUM") == 0 || strcmp(wbb,"FACT") == 0) {
                wfound = list_contains(res, wtg);
                if (wfound == 0) {
                    res = plant_list_push(res, wtg);
                }
            }
        }
        if (strcmp(wty,"if_stmt") == 0 || strcmp(wty,"season_stmt") == 0 || strcmp(wty,"cycle_stmt") == 0 || strcmp(wty,"match_stmt") == 0) {
            wbd2 = _map_get(wnd, "body");
            wret2 = collect_nums_walk(wbd2, subst, res);
        }
        wi = wi+1;
    }
    return res;
}
tx_t collect_nums(PlantArray* bd, PlantArray* params, PlantArray* subst) {
  tx_t ret = "";
    PlantArray* res = plant_list_make ( 0 );
    long pi = 0;
    tx_t pnd = "";
    tx_t pn2 = "";
    tx_t pty2 = "";
    tx_t psu2 = "";
    tx_t pbs2 = "";
    while (pi < plant_array_length(params)) {
        pnd = plant_list_get(params, pi);
        pn2 = _map_get(pnd, "name");
        pty2 = _map_get(pnd, "type");
        psu2 = subst_type(pty2, subst);
        pbs2 = type_base(psu2);
        if (strcmp(pbs2,"NUM") == 0 || strcmp(pbs2,"FACT") == 0) {
            res = plant_list_push(res, pn2);
        }
        pi = pi+1;
    }
    ret = collect_nums_walk(bd, subst, res);
    return res;
}
tx_t nums_from_avars(PlantArray* vars) {
  tx_t vf = "";
    PlantArray* res = plant_list_make ( 0 );
    long vi = 0;
    tx_t vn = "";
    tx_t vt = "";
    while (vi + 1 < plant_array_length(vars)) {
        vn = plant_list_get(vars, vi);
        vt = plant_list_get(vars, vi+1);
        if (strcmp(vt,"long") == 0 || strcmp(vt,"int") == 0) {
            vf = list_contains(res, vn);
            if (vf == 0) {
                res = plant_list_push(res, vn);
            }
        }
        vi = vi+2;
    }
    return res;
}
tx_t collect_nums_cb(PlantArray* bd, PlantArray* params, PlantArray* shads, PlantArray* subst) {
  tx_t sf7 = "";
  tx_t ret7 = "";
    PlantArray* res = plant_list_make ( 0 );
    long ci = 0;
    tx_t cnd = "";
    tx_t cn2 = "";
    tx_t cty2 = "";
    tx_t csu2 = "";
    tx_t cbs2 = "";
    while (ci < plant_array_length(params)) {
        cnd = plant_list_get(params, ci);
        cn2 = _map_get(cnd, "name");
        cty2 = _map_get(cnd, "type");
        csu2 = subst_type(cty2, subst);
        cbs2 = type_base(csu2);
        if (strcmp(cbs2,"NUM") == 0 || strcmp(cbs2,"FACT") == 0) {
            res = plant_list_push(res, cn2);
        }
        ci = ci+1;
    }
    long si7 = 0;
    tx_t se7 = "";
    tx_t sn7 = "";
    tx_t sc7 = "";
    while (si7 < plant_array_length(shads)) {
        se7 = plant_list_get(shads, si7);
        sn7 = _map_get(se7, "name");
        sc7 = _map_get(se7, "ctype");
        if (strcmp(sc7,"long") == 0 || strcmp(sc7,"int") == 0) {
            sf7 = list_contains(res, sn7);
            if (sf7 == 0) {
                res = plant_list_push(res, sn7);
            }
        }
        si7 = si7+1;
    }
    ret7 = collect_nums_walk(bd, subst, res);
    return res;
}
tx_t async_argstr(PlantArray* args, PlantArray* sigs, tx_t act, PlantArray* nums, PlantArray* stvars, PlantArray* evars) {
  tx_t fparams = "";
  tx_t a0 = "";
  tx_t rp = "";
  tx_t fp_el = "";
  tx_t fp_ty = "";
  tx_t fp_kind = "";
  tx_t fpx = "";
  tx_t fpk2 = "";
  tx_t e0 = "";
  tx_t cnm3 = "";
  tx_t amp0 = "";
  tx_t fs2 = "";
    tx_t argstr = "";
    fparams = find_params(sigs, act);
    long ai = 0;
    tx_t ael = "";
    while (ai < plant_array_length(args)) {
        ael = plant_list_get(args, ai);
        tx_t aex = ael;
        a0 = substring(ael, 0, 1);
        if (strcmp(a0,"\"") != 0) {
            aex = translate_expr(ael);
            aex = _handle_cat(aex, nums, evars);
        }
        rp = is_ref_at(fparams, ai);
        if (strcmp(rp,"1") == 0) {
            aex = _cat("&", aex);
        }
        if (ai < plant_array_length(fparams)) {
            fp_el = plant_list_get(fparams, ai);
            fp_ty = _map_get(fp_el, "type");
            fp_kind = ffi_param_kind(fp_ty);
            if (strcmp(fp_kind,"struct_val") == 0) {
                fpx = is_bare_id(ael);
                fpk2 = stvar_kind(stvars, ael);
                if (strcmp(fpx,"1") == 0 && strcmp(fpk2,"") > 0) {
                    if (strcmp(fpk2,"struct_ref") == 0) {
                        aex = _cat("*", aex);
                    }
                }
                if (strcmp(fpx,"0") == 0 || strcmp(fpk2,"") == 0) {
                    e0 = substring(ael, 0, 1);
                    if (strcmp(e0,"\"") != 0) {
                        cnm3 = ffi_struct_cname(fp_ty);
                        aex = _cat(_cat(_cat(_cat("plant_map_to_", cnm3), "("), aex), ")");
                    }
                }
            }
            if (strcmp(fp_kind,"struct_ref") == 0) {
                fpx = is_bare_id(ael);
                fpk2 = stvar_kind(stvars, ael);
                amp0 = substring(aex, 0, 1);
                if (strcmp(amp0,"&") == 0) {
                    aex = substring ( aex , 1 , strlen( aex ) );
                }
                if (strcmp(fpx,"1") == 0 && strcmp(fpk2,"struct_val") == 0) {
                    aex = _cat("&", aex);
                }
                if (strcmp(fpx,"0") == 0 || strcmp(fpk2,"") == 0) {
                    e0 = substring(ael, 0, 1);
                    if (strcmp(e0,"\"") != 0) {
                        cnm3 = ffi_struct_cname(fp_ty);
                        aex = _cat(_cat(_cat(_cat("plant_map_to_ref_", cnm3), "("), aex), ")");
                    }
                }
            }
            if (strcmp(fp_kind,"callback") == 0) {
                fpx = is_bare_id(ael);
                if (strcmp(fpx,"1") == 0) {
                    fpk2 = stvar_kind(stvars, ael);
                    if (strcmp(fpk2,"") == 0) {
                        fs2 = find_sig(sigs, ael);
                        if (plant_array_length(fs2) > 0) {
                            aex = _cat(_cat(_cat(_cat("plant_cb_ensure(\"", ael), "\", plant_cbw_"), ael), ")");
                        }
                    }
                }
            }
        }
        if (ai > 0) {
            argstr = _cat(argstr, ", ");
        }
        argstr = _cat(argstr, aex);
        ai = ai+1;
    }
    return argstr;
}
tx_t async_var_add(PlantArray* acc, tx_t name, tx_t ctype) {
    long found = 0;
    long fi = 0;
    tx_t fe = "";
    while (fi < plant_array_length(acc)) {
        fe = plant_list_get(acc, fi);
        if (strcmp(str_eq ( fe , name ),"1") == 0) {
            found = 1;
        }
        fi = fi+1;
    }
    if (!found) {
        acc = plant_list_push(acc, name);
        acc = plant_list_push(acc, ctype);
    }
    return acc;
}
tx_t async_walk_decl(PlantArray* bd, PlantArray* acc) {
  tx_t tg = "";
  tx_t vt = "";
  tx_t ct = "";
  tx_t sb = "";
  tx_t cl2 = "";
  tx_t cb = "";
    long wi = 0;
    tx_t nd = "";
    tx_t ty = "";
    while (wi < plant_array_length(bd)) {
        nd = plant_list_get(bd, wi);
        ty = _map_get(nd, "type");
        if (strcmp(ty,"create_stmt") == 0 || strcmp(ty,"let_stmt") == 0) {
            tg = _map_get(nd, "target");
            vt = _map_get(nd, "var_type");
            ct = ffi_ctype(vt);
            acc = async_var_add(acc, tg, ct);
        }
        if (strcmp(ty,"if_stmt") == 0 || strcmp(ty,"season_stmt") == 0) {
            sb = _map_get(nd, "body");
            acc = async_walk_decl(sb, acc);
        }
        if (strcmp(ty,"cycle_stmt") == 0) {
            sb = _map_get(nd, "body");
            acc = async_walk_decl(sb, acc);
        }
        if (strcmp(ty,"match_stmt") == 0) {
            PlantArray* cl = _map_get ( nd , "clauses" );
            long ci = 0;
            while (ci < plant_array_length(cl)) {
                cl2 = plant_list_get(cl, ci);
                cb = _map_get(cl2, "bodyStatements");
                acc = async_walk_decl(cb, acc);
                ci = ci+1;
            }
        }
        wi = wi+1;
    }
    return acc;
}
tx_t async_collect_vars(PlantArray* bd, PlantArray* params) {
  tx_t pn = "";
  tx_t pt = "";
  tx_t ct = "";
  tx_t imp = "";
  tx_t iv = "";
    PlantArray* acc = plant_list_make ( 0 );
    long pi = 0;
    tx_t pe = "";
    while (pi < plant_array_length(params)) {
        pe = plant_list_get(params, pi);
        pn = _map_get(pe, "name");
        pt = _map_get(pe, "type");
        ct = ffi_ctype(pt);
        acc = async_var_add(acc, pn, ct);
        pi = pi+1;
    }
    acc = async_walk_decl(bd, acc);
    imp = collect_implicit(bd, params);
    long ii = 0;
    while (ii < plant_array_length(imp)) {
        iv = plant_list_get(imp, ii);
        acc = async_var_add(acc, iv, "tx_t");
        ii = ii+1;
    }
    return acc;
}
tx_t async_split_phases(PlantArray* bd) {
    PlantArray* phases = plant_list_make ( 0 );
    PlantArray* cur = plant_list_make ( 0 );
    long wi = 0;
    tx_t nd = "";
    tx_t ty = "";
    while (wi < plant_array_length(bd)) {
        nd = plant_list_get(bd, wi);
        ty = _map_get(nd, "type");
        if (strcmp(ty,"await_stmt") == 0) {
            cur = plant_list_push(cur, nd);
            phases = plant_list_push(phases, cur);
            cur = plant_list_make ( 0 );
        }
        if (strcmp(ty,"await_stmt") != 0) {
            cur = plant_list_push(cur, nd);
        }
        wi = wi+1;
    }
    phases = plant_list_push(phases, cur);
    return phases;
}
tx_t async_emit_state(tx_t name, PlantArray* vars) {
    tx_t code = "typedef struct {\n";
    code = _cat(code, "  tx_t __self;\n");
    code = _cat(code, "  long __pc;\n");
    long vi = 0;
    tx_t vn = "";
    tx_t vt = "";
    while (vi + 1 < plant_array_length(vars)) {
        vn = plant_list_get(vars, vi);
        vt = plant_list_get(vars, vi+1);
        code = _cat(_cat(_cat(_cat(_cat(code, "  "), vt), " "), vn), ";\n");
        vi = vi+2;
    }
    code = _cat(_cat(_cat(code, "} plant_a_"), name), "_state;\n\n");
    return code;
}
tx_t async_emit_entry(tx_t name, PlantArray* params, tx_t prio) {
  tx_t pn = "";
  tx_t pt = "";
  tx_t ct = "";
  tx_t pe2 = "";
  tx_t pn2 = "";
    tx_t pstr = "";
    long pi = 0;
    tx_t pe = "";
    while (pi < plant_array_length(params)) {
        pe = plant_list_get(params, pi);
        pn = _map_get(pe, "name");
        pt = _map_get(pe, "type");
        ct = ffi_ctype(pt);
        if (pi > 0) {
            pstr = _cat(pstr, ", ");
        }
        pstr = _cat(_cat(_cat(pstr, ct), " "), pn);
        pi = pi+1;
    }
    tx_t code = _cat(_cat("tx_t ", name), "(tx_t __parent, tx_t __ctx");
    if (strcmp(pstr,"") > 0) {
        code = _cat(_cat(code, ", "), pstr);
    }
    code = _cat(code, ") {\n");
    code = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(code, "  plant_a_"), name), "_state* s = (plant_a_"), name), "_state*)plant_async_alloc_state(sizeof(plant_a_"), name), "_state), \""), name), "\");\n");
    long ci = 0;
    while (ci < plant_array_length(params)) {
        pe2 = plant_list_get(params, ci);
        pn2 = _map_get(pe2, "name");
        code = _cat(_cat(_cat(_cat(_cat(code, "  s->"), pn2), " = "), pn2), ";\n");
        ci = ci+1;
    }
    code = _cat(_cat(_cat(_cat(_cat(_cat(_cat(code, "  return plant_async_register((tx_t)s, plant_a_"), name), "_step, __parent, __ctx, "), prio), ", -1, -1, 0, \""), name), "\");\n");
    code = _cat(code, "}\n\n");
    return code;
}
tx_t async_emit_step(tx_t name, PlantArray* phases, PlantArray* vars, PlantArray* sigs, PlantArray* subst, PlantArray* clmap, PlantArray* stvars, PlantArray* evars) {
  tx_t phd = "";
  tx_t ls = "";
  tx_t lcode = "";
  tx_t awnd = "";
  tx_t act_s = "";
  tx_t arg_s = "";
  tx_t tcode = "";
    tx_t code = _cat(_cat("static int plant_a_", name), "_step(tx_t st) {\n");
    code = _cat(_cat(_cat(_cat(_cat(code, "  plant_a_"), name), "_state* s = (plant_a_"), name), "_state*)st;\n");
    code = _cat(code, "  if (s->__pc > 0) plant_async_await_result(st);\n");
    long vi = 0;
    tx_t vn = "";
    tx_t vt = "";
    while (vi + 1 < plant_array_length(vars)) {
        vn = plant_list_get(vars, vi);
        vt = plant_list_get(vars, vi+1);
        code = _cat(_cat(_cat(_cat(_cat(_cat(_cat(code, "  "), vt), " "), vn), " = s->"), vn), ";\n");
        vi = vi+2;
    }
    code = _cat(code, "  switch (s->__pc) {\n");
    PlantArray* nums_s = nums_from_avars ( vars );
    long ph = 0;
    while (ph < plant_array_length(phases)) {
        tx_t phs = _from_long ( ph );
        code = _cat(_cat(_cat(_cat(_cat(_cat(_cat(code, "    case "), phs), ": goto plant_a_"), name), "_L"), phs), ";\n");
        ph = ph+1;
    }
    code = _cat(code, "  }\n");
    long ph2 = 0;
    while (ph2 < plant_array_length(phases)) {
        tx_t ph2s = _from_long ( ph2 );
        code = _cat(_cat(_cat(_cat(_cat(code, "  plant_a_"), name), "_L"), ph2s), ":\n");
        phd = plant_list_get(phases, ph2);
        long nph = plant_array_length(phases);
        long nst = plant_array_length(phd);
        long nxt = ph2+1;
        if (nxt < nph) {
            PlantArray* lead = plant_list_make ( 0 );
            long lj = 0;
            while (lj < nst - 1) {
                ls = plant_list_get(phd, lj);
                lead = plant_list_push(lead, ls);
                lj = lj+1;
            }
            lcode = generate_body(lead, 1, sigs, subst, clmap, "a", nums_s, stvars, evars);
            code = _cat(code, lcode);
            long aw_last = nst - 1;
            awnd = plant_list_get(phd, aw_last);
            act_s = _map_get(awnd, "action");
            PlantArray* awargs = _map_get ( awnd , "args" );
            arg_s = async_argstr(awargs, sigs, act_s, nums_s, stvars, evars);
            long vi2 = 0;
            tx_t vn2 = "";
            while (vi2 + 1 < plant_array_length(vars)) {
                vn2 = plant_list_get(vars, vi2);
                code = _cat(_cat(_cat(_cat(_cat(code, "  s->"), vn2), " = "), vn2), ";\n");
                vi2 = vi2+2;
            }
            tx_t nxts = _from_long ( nxt );
            code = _cat(_cat(_cat(code, "  s->__pc = "), nxts), ";\n");
            code = _cat(_cat(_cat(code, "  plant_async_suspend(st, "), act_s), "(st, 0");
            if (strcmp(arg_s,"") > 0) {
                code = _cat(_cat(code, ", "), arg_s);
            }
            code = _cat(code, "));\n  return 0;\n");
        }
        if (nxt == nph) {
            tcode = generate_body(phd, 1, sigs, subst, clmap, "a", nums_s, stvars, evars);
            code = _cat(code, tcode);
        }
        ph2 = ph2+1;
    }
    code = _cat(_cat(_cat(code, "  plant_async_finish(st, \""), name), "\");\n");
    code = _cat(code, "  return 1;\n");
    code = _cat(code, "}\n\n");
    return code;
}
tx_t translate_expr(tx_t expr) {
  tx_t e0 = "";
  tx_t e1 = "";
  tx_t cm = "";
  tx_t anm = "";
  tx_t args1 = "";
    tx_t e = expr;
    e0 = substring(e, 0, 6);
    if (strcmp(e0,"START ") == 0) {
        e1 = substring(e, 6, strlen( e ));
        cm = find_any(e1, ",");
        if (cm == - 1) {
            return _cat(e1, "(0, 0)");
        }
        anm = substring(e1, 0, cm);
        args1 = substring(e1, cm+1, strlen( e1 ));
        return _cat(_cat(_cat(anm, "(0, 0, "), args1), ")");
    }
    e = strings_REPLACE((tx_t)e, "GREATER THAN OR EQUAL", ">=");
    e = strings_REPLACE((tx_t)e, "LESS THAN OR EQUAL", "<=");
    e = strings_REPLACE((tx_t)e, "GREATER THAN", ">");
    e = strings_REPLACE((tx_t)e, "LESS THAN", "<");
    e = strings_REPLACE((tx_t)e, "ISNT", "!=");
    e = strings_REPLACE((tx_t)e, "STAR_STAR", "**");
    e = strings_REPLACE((tx_t)e, " AND ", " && ");
    e = strings_REPLACE((tx_t)e, " OR ", " || ");
    e = strings_REPLACE((tx_t)e, "NOT ", "!");
    e = strings_REPLACE((tx_t)e, " IS ", " == ");
    e = _handle_func(e, "COUNT", "plant_array_length");
    e = _handle_func_paren(e, "LEN", "strlen");
    e = _handle_func(e, "TEST", "!");
    e = strings_REPLACE((tx_t)e, "TRUE", "1");
    e = strings_REPLACE((tx_t)e, "FALSE", "0");
    e = strings_REPLACE((tx_t)e, "NULL", "NULL");
    e = strings_REPLACE((tx_t)e, " : ", "_");
    e = handle_brackets(e);
    return e;
}
tx_t indent_str(long level) {
    tx_t res = "";
    long i = 0;
    while (i < level) {
        res = _cat(res, "  ");
        i = i+1;
    }
    return res;
}
tx_t generate_body(PlantArray* bd, long indent, PlantArray* sigs, PlantArray* subst, PlantArray* clmap, tx_t actx, PlantArray* nums, PlantArray* stvars, PlantArray* evars) {
  tx_t node_code = "";
    tx_t res = "";
    long i = 0;
    tx_t node_el = "";
    while (i < plant_array_length(bd)) {
        node_el = plant_list_get(bd, i);
        node_code = generate_node(node_el, indent, sigs, subst, clmap, actx, nums, stvars, evars);
        if (strcmp(node_code,"") > 0) {
            res = _cat(res, node_code);
        }
        i = i+1;
    }
    return res;
}
tx_t generate_node(tx_t node, long indent, PlantArray* sigs, PlantArray* subst, PlantArray* clmap, tx_t actx, PlantArray* nums, PlantArray* stvars, PlantArray* evars) {
  tx_t ntype = "";
  tx_t val = "";
  tx_t cval = "";
  tx_t isn2 = "";
  tx_t snm2 = "";
  tx_t isel = "";
  tx_t target = "";
  tx_t vtype = "";
  tx_t cnd = "";
  tx_t envname = "";
  tx_t caps = "";
  tx_t moved = "";
  tx_t cid2 = "";
  tx_t vst2 = "";
  tx_t vct2 = "";
  tx_t vst3 = "";
  tx_t vst4 = "";
  tx_t vct4 = "";
  tx_t vst5 = "";
  tx_t item = "";
  tx_t citem = "";
  tx_t act = "";
  tx_t ctx_n = "";
  tx_t lv = "";
  tx_t key = "";
  tx_t ca2 = "";
  tx_t ccode3 = "";
  tx_t tgt = "";
  tx_t sact = "";
  tx_t base = "";
  tx_t gargs = "";
  tx_t mname = "";
  tx_t cvar = "";
  tx_t cfn = "";
  tx_t arg0 = "";
  tx_t fp_el = "";
  tx_t fk2 = "";
  tx_t fpx = "";
  tx_t fpk2 = "";
  tx_t e0 = "";
  tx_t cnm4 = "";
  tx_t amp0 = "";
  tx_t fs2 = "";
  tx_t rp3 = "";
  tx_t sv2 = "";
  tx_t sf2 = "";
  tx_t sig2 = "";
  tx_t rcnm = "";
  tx_t stk2 = "";
  tx_t cond = "";
  tx_t bd = "";
  tx_t ccond = "";
  tx_t bcode = "";
  tx_t ivar = "";
  tx_t fromExpr = "";
  tx_t toExpr = "";
  tx_t stepExpr = "";
  tx_t listExpr = "";
  tx_t indexVar = "";
  tx_t cfrom = "";
  tx_t cto = "";
  tx_t cstep = "";
  tx_t clist = "";
  tx_t subj = "";
  tx_t csubj = "";
  tx_t vname = "";
  tx_t binding = "";
  tx_t cbody = "";
  tx_t aname = "";
  tx_t prio_s = "";
  tx_t st_code = "";
  tx_t en_code = "";
  tx_t stp_code = "";
  tx_t pname = "";
  tx_t ptype = "";
  tx_t ctype = "";
  tx_t drn2 = "";
  tx_t ename = "";
    ntype = _map_get(node, "type");
    if (strcmp(clmap,"") == 0) {
        clmap = plant_list_make ( 0 );
    }
    if (strcmp(evars,"") == 0) {
        evars = plant_list_make ( 0 );
    }
    if (strcmp(ntype,"show_stmt") == 0) {
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval, nums, evars);
        isn2 = expr_is_numeric(cval, nums);
        if (isn2 == 1) {
            cval = _cat(_cat("_from_long(", cval), ")");
        }
        snm2 = enum_in_table(evars, cval);
        if (isn2 == 0 && strcmp(snm2,"") != 0) {
            cval = _cat(_cat(_cat(_cat("_from_enum(", cval), ", \""), snm2), "\")");
        }
        isel = indent_str(indent);
        return _cat(_cat(_cat(isel, "  plant_print("), cval), ");\n");
    }
    if (strcmp(ntype,"create_stmt") == 0) {
        target = _map_get(node, "target");
        vtype = _map_get(node, "var_type");
        vtype = subst_type(vtype, subst);
        isel = indent_str(indent);
        cnd = _map_get(node, "closure");
        if (strcmp(cnd,"") > 0) {
            envname = _map_get(cnd, "envname");
            caps = _map_get(cnd, "clcaps");
            moved = _map_get(cnd, "moved");
            cid2 = _map_get(cnd, "cid");
            tx_t ccode2 = _cat(_cat(_cat(isel, "  tx_t "), target), " = (tx_t)0;\n");
            if (strcmp(actx,"a") == 0) {
                ccode2 = _cat(_cat(_cat(isel, "  "), target), " = (tx_t)0;\n");
            }
            ccode2 = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(ccode2, isel), "  { "), envname), "* __env_"), cid2), " = ("), envname), "*)plant_env_alloc(sizeof("), envname), "));\n");
            long cpi = 0;
            tx_t capn = "";
            tx_t capi = "";
            while (cpi + 1 < plant_array_length(caps)) {
                capn = plant_list_get(caps, cpi);
                capi = plant_list_get(caps, cpi+1);
                ccode2 = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(ccode2, isel), "    __env_"), cid2), "->"), capn), " = "), capi), ";\n");
                cpi = cpi+2;
            }
            ccode2 = _cat(_cat(_cat(_cat(_cat(_cat(ccode2, isel), "    "), target), " = (tx_t)__env_"), cid2), ";\n");
            ccode2 = _cat(_cat(ccode2, isel), "  }\n");
            long cmi = 0;
            tx_t cmv = "";
            while (cmi < plant_array_length(moved)) {
                cmv = plant_list_get(moved, cmi);
                ccode2 = _cat(_cat(_cat(_cat(ccode2, isel), "  "), cmv), " = 0;\n");
                cmi = cmi+1;
            }
            return ccode2;
        }
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval, nums, evars);
        if (strcmp(actx,"a") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"NUM") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  long "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"FACT") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  int "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"LIST") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  PlantArray* "), target), " = "), cval), ";\n");
        }
        vst2 = is_struct_type(vtype);
        if (strcmp(vst2,"1") == 0) {
            vct2 = ffi_ctype(vtype);
            if (strcmp(actx,"a") == 0) {
                return _cat(_cat(_cat(_cat(_cat(isel, "  "), target), " = "), cval), ";\n");
            }
            return _cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  "), vct2), " "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"NUM") != 0 && strcmp(vtype,"FACT") != 0 && strcmp(vtype,"LIST") != 0) {
            vst3 = is_struct_type(vtype);
            if (strcmp(vst3,"1") != 0) {
                return _cat(_cat(_cat(_cat(_cat(isel, "  tx_t "), target), " = "), cval), ";\n");
            }
        }
    }
    if (strcmp(ntype,"set_stmt") == 0) {
        target = _map_get(node, "target");
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval, nums, evars);
        isel = indent_str(indent);
        return _cat(_cat(_cat(_cat(_cat(isel, "  "), target), " = "), cval), ";\n");
    }
    if (strcmp(ntype,"let_stmt") == 0) {
        target = _map_get(node, "target");
        vtype = _map_get(node, "var_type");
        vtype = subst_type(vtype, subst);
        isel = indent_str(indent);
        cnd = _map_get(node, "closure");
        if (strcmp(cnd,"") > 0) {
            envname = _map_get(cnd, "envname");
            caps = _map_get(cnd, "clcaps");
            moved = _map_get(cnd, "moved");
            cid2 = _map_get(cnd, "cid");
            tx_t ccode2 = _cat(_cat(_cat(isel, "  tx_t "), target), " = (tx_t)0;\n");
            if (strcmp(actx,"a") == 0) {
                ccode2 = _cat(_cat(_cat(isel, "  "), target), " = (tx_t)0;\n");
            }
            ccode2 = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(ccode2, isel), "  { "), envname), "* __env_"), cid2), " = ("), envname), "*)plant_env_alloc(sizeof("), envname), "));\n");
            long cpi = 0;
            tx_t capn = "";
            tx_t capi = "";
            while (cpi + 1 < plant_array_length(caps)) {
                capn = plant_list_get(caps, cpi);
                capi = plant_list_get(caps, cpi+1);
                ccode2 = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(ccode2, isel), "    __env_"), cid2), "->"), capn), " = "), capi), ";\n");
                cpi = cpi+2;
            }
            ccode2 = _cat(_cat(_cat(_cat(_cat(_cat(ccode2, isel), "    "), target), " = (tx_t)__env_"), cid2), ";\n");
            ccode2 = _cat(_cat(ccode2, isel), "  }\n");
            long cmi = 0;
            tx_t cmv = "";
            while (cmi < plant_array_length(moved)) {
                cmv = plant_list_get(moved, cmi);
                ccode2 = _cat(_cat(_cat(_cat(ccode2, isel), "  "), cmv), " = 0;\n");
                cmi = cmi+1;
            }
            return ccode2;
        }
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval, nums, evars);
        if (strcmp(actx,"a") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"NUM") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  long "), target), " = "), cval), ";\n");
        }
        vst4 = is_struct_type(vtype);
        if (strcmp(vst4,"1") == 0) {
            vct4 = ffi_ctype(vtype);
            if (strcmp(actx,"a") == 0) {
                return _cat(_cat(_cat(_cat(_cat(isel, "  "), target), " = "), cval), ";\n");
            }
            return _cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  "), vct4), " "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"NUM") != 0) {
            vst5 = is_struct_type(vtype);
            if (strcmp(vst5,"1") != 0) {
                return _cat(_cat(_cat(_cat(_cat(isel, "  tx_t "), target), " = "), cval), ";\n");
            }
        }
    }
    if (strcmp(ntype,"give_stmt") == 0) {
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval, nums, evars);
        isel = indent_str(indent);
        if (strcmp(actx,"a") == 0) {
            return _cat(_cat(_cat(isel, "  plant_async_finish(st, "), cval), ");\n  return 1;\n");
        }
        return _cat(_cat(_cat(isel, "  return "), cval), ";\n");
    }
    if (strcmp(ntype,"break_stmt") == 0) {
        isel = indent_str(indent);
        return _cat(isel, "  break;\n");
    }
    if (strcmp(ntype,"continue_stmt") == 0) {
        isel = indent_str(indent);
        return _cat(isel, "  continue;\n");
    }
    if (strcmp(ntype,"put_stmt") == 0) {
        item = _map_get(node, "item");
        target = _map_get(node, "target");
        citem = translate_expr(item);
        citem = _handle_cat(citem, nums, evars);
        isel = indent_str(indent);
        return _cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  "), target), " = plant_list_push("), target), ", "), citem), ");\n");
    }
    if (strcmp(ntype,"await_stmt") == 0) {
        return "#error AWAIT must be a top-level statement of an ASYNC ACTION body\n";
    }
    if (strcmp(ntype,"start_stmt") == 0) {
        act = _map_get(node, "action");
        act = strings_REPLACE(act, ":", "_");
        tx_t ctx_s = "0";
        ctx_n = _map_get(node, "ctx");
        if (strcmp(ctx_n,"") != 0 && strcmp(ctx_n,"null") != 0) {
            ctx_s = ctx_n;
        }
        PlantArray* args_s = _map_get ( node , "args" );
        tx_t argstr_s = "";
        argstr_s = async_argstr(args_s, sigs, act, nums, stvars, evars);
        isel = indent_str(indent);
        if (strcmp(argstr_s,"") > 0) {
            return _cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  "), act), "(0, "), ctx_s), ", "), argstr_s), ");\n");
        }
        if (strcmp(argstr_s,"") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  "), act), "(0, "), ctx_s), ");\n");
        }
    }
    if (strcmp(ntype,"cancel_stmt") == 0) {
        val = _map_get(node, "value");
        cval = translate_expr(val);
        isel = indent_str(indent);
        return _cat(_cat(_cat(isel, "  plant_async_cancel("), cval), ");\n");
    }
    if (strcmp(ntype,"trace_stmt") == 0) {
        lv = _map_get(node, "level");
        tx_t lvnum = "0";
        if (strcmp(lv,"DEBUG") == 0) {
            lvnum = "1";
        }
        if (strcmp(lv,"PERF") == 0) {
            lvnum = "2";
        }
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval, nums, evars);
        isel = indent_str(indent);
        return _cat(_cat(_cat(_cat(_cat(isel, "  plant_trace("), lvnum), ", \"\", "), cval), ");\n");
    }
    if (strcmp(ntype,"config_stmt") == 0) {
        key = _map_get(node, "key");
        val = _map_get(node, "value");
        isel = indent_str(indent);
        return _cat(_cat(_cat(_cat(_cat(isel, "  plant_async_config(\""), key), "\", \""), val), "\");\n");
    }
    if (strcmp(ntype,"call_stmt") == 0) {
        ca2 = _map_get(node, "action");
        PlantArray* cargs2 = _map_get ( node , "args" );
        PlantArray* ccl2 = _map_get ( node , "clargs" );
        if (strcmp(ccl2,"") == 0) {
            ccl2 = plant_list_make ( 0 );
        }
        PlantArray* cnode2 = plant_list_make ( 10 , "type" , "reap_stmt" , "target" , "_" , "action" , ca2 , "args" , cargs2 , "clargs" , ccl2 );
        ccode3 = generate_node(cnode2, indent, sigs, subst, clmap, actx, nums, stvars, evars);
        return ccode3;
    }
    if (strcmp(ntype,"reap_stmt") == 0) {
        tgt = _map_get(node, "target");
        act = _map_get(node, "action");
        act = strings_REPLACE(act, ":", "_");
        tx_t callname = act;
        PlantArray* fparams = plant_list_make ( 0 );
        long gi = - 1;
        gi = find_any(act, "[");
        if (gi != - 1) {
            sact = subst_reap_act(act, subst);
            base = base_of(sact);
            gargs = parse_type_args(sact);
            mname = mangle(base, gargs);
            callname = mname;
            fparams = find_params(sigs, base);
        }
        if (gi == - 1) {
            fparams = find_params(sigs, act);
        }
        cvar = _cl_map_get(clmap, act);
        tx_t clcall = "";
        if (strcmp(cvar,"") > 0) {
            cfn = _map_get(cvar, "fnname");
            callname = cfn;
            clcall = _cat(_cat("(tx_t)", act), ", ");
        }
        PlantArray* args = _map_get ( node , "args" );
        PlantArray* clargs = plant_list_make ( 0 );
        clargs = _map_get(node, "clargs");
        if (strcmp(clargs,"") == 0) {
            clargs = plant_list_make ( 0 );
        }
        tx_t argstr = "";
        tx_t clpre = "";
        tx_t clclears = "";
        long ai = 0;
        tx_t arg_el = "";
        isel = indent_str(indent);
        while (ai < plant_array_length(args)) {
            arg_el = plant_list_get(args, ai);
            tx_t aexpr = arg_el;
            tx_t cl_ok = "0";
            cl_ok = _cl_is_arg(arg_el);
            if (strcmp(cl_ok,"1") == 0) {
                cnd = plant_list_get(clargs, ai);
                tx_t cndok = "0";
                if (strcmp(cnd,"") > 0) {
                    cndok = "1";
                }
                if (strcmp(cndok,"1") == 0) {
                    envname = _map_get(cnd, "envname");
                    caps = _map_get(cnd, "clcaps");
                    moved = _map_get(cnd, "moved");
                    clpre = _cat(_cat(_cat(_cat(clpre, isel), "  tx_t __carg_"), _from_long(ai)), ";\n");
                    clpre = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, isel), "  { "), envname), "* __env_"), _from_long(ai)), " = ("), envname), "*)plant_env_alloc(sizeof("), envname), "));\n");
                    long cpi = 0;
                    tx_t capn = "";
                    tx_t capi = "";
                    while (cpi + 1 < plant_array_length(caps)) {
                        capn = plant_list_get(caps, cpi);
                        capi = plant_list_get(caps, cpi+1);
                        clpre = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, isel), "    __env_"), _from_long(ai)), "->"), capn), " = "), capi), ";\n");
                        cpi = cpi+2;
                    }
                    clpre = _cat(_cat(_cat(_cat(_cat(_cat(clpre, isel), "    __carg_"), _from_long(ai)), " = (tx_t)__env_"), _from_long(ai)), ";\n");
                    clpre = _cat(_cat(clpre, isel), "  }\n");
                    long cmi = 0;
                    tx_t cmv = "";
                    while (cmi < plant_array_length(moved)) {
                        cmv = plant_list_get(moved, cmi);
                        clclears = _cat(_cat(_cat(_cat(clclears, isel), "  "), cmv), " = 0;\n");
                        cmi = cmi+1;
                    }
                    aexpr = _cat("__carg_", _from_long(ai));
                }
                if (strcmp(cndok,"0") == 0) {
                    aexpr = arg_el;
                }
            }
            if (strcmp(cl_ok,"0") == 0) {
                arg0 = substring(arg_el, 0, 1);
                if (strcmp(arg0,"\"") != 0) {
                    aexpr = translate_expr(arg_el);
                    aexpr = _handle_cat(aexpr, nums, evars);
                }
                tx_t fp_ty = "";
                if (ai < plant_array_length(fparams)) {
                    fp_el = plant_list_get(fparams, ai);
                    fp_ty = _map_get(fp_el, "type");
                }
                fk2 = ffi_param_kind(fp_ty);
                if (strcmp(fk2,"struct_val") == 0) {
                    fpx = is_bare_id(arg_el);
                    fpk2 = stvar_kind(stvars, arg_el);
                    if (strcmp(fpx,"1") == 0 && strcmp(fpk2,"struct_ref") == 0) {
                        aexpr = _cat("*", aexpr);
                    }
                    if (strcmp(fpx,"0") == 0 || strcmp(fpk2,"") == 0) {
                        e0 = substring(arg_el, 0, 1);
                        if (strcmp(e0,"\"") != 0) {
                            cnm4 = ffi_struct_cname(fp_ty);
                            aexpr = _cat(_cat(_cat(_cat("plant_map_to_", cnm4), "("), aexpr), ")");
                        }
                    }
                }
                if (strcmp(fk2,"struct_ref") == 0) {
                    fpx = is_bare_id(arg_el);
                    fpk2 = stvar_kind(stvars, arg_el);
                    amp0 = substring(aexpr, 0, 1);
                    if (strcmp(amp0,"&") == 0) {
                        aexpr = substring ( aexpr , 1 , strlen( aexpr ) );
                    }
                    if (strcmp(fpx,"1") == 0 && strcmp(fpk2,"struct_val") == 0) {
                        aexpr = _cat("&", aexpr);
                    }
                    if (strcmp(fpx,"0") == 0 || strcmp(fpk2,"") == 0) {
                        e0 = substring(arg_el, 0, 1);
                        if (strcmp(e0,"\"") != 0) {
                            cnm4 = ffi_struct_cname(fp_ty);
                            aexpr = _cat(_cat(_cat(_cat("plant_map_to_ref_", cnm4), "("), aexpr), ")");
                        }
                    }
                }
                if (strcmp(fk2,"callback") == 0) {
                    fpx = is_bare_id(arg_el);
                    if (strcmp(fpx,"1") == 0) {
                        fpk2 = stvar_kind(stvars, arg_el);
                        if (strcmp(fpk2,"") == 0) {
                            fs2 = find_sig(sigs, arg_el);
                            if (plant_array_length(fs2) > 0) {
                                aexpr = _cat(_cat(_cat(_cat("plant_cb_ensure(\"", arg_el), "\", plant_cbw_"), arg_el), ")");
                            }
                        }
                    }
                }
                if (strcmp(fk2,"plain") == 0) {
                    rp3 = is_ref_at(fparams, ai);
                    if (strcmp(rp3,"1") == 0) {
                        aexpr = _cat("&", aexpr);
                    }
                }
                if (strcmp(fk2,"voidptr") == 0) {
                    aexpr = _cat("(void*)", aexpr);
                }
                if (ai >= plant_array_length(fparams)) {
                    sv2 = expr_is_stringlike(aexpr);
                    if (strcmp(sv2,"0") == 0) {
                        sf2 = list_contains(nums, arg_el);
                        if (sf2 == 0) {
                            aexpr = _cat("(tx_t)", aexpr);
                        }
                    }
                }
            }
            if (ai > 0) {
                argstr = _cat(argstr, ", ");
            }
            argstr = _cat(argstr, aexpr);
            ai = ai+1;
        }
        tx_t ffi_ret = "";
        tx_t ffi_rtk = "";
        sig2 = find_sig(sigs, act);
        if (plant_array_length(sig2) > 0) {
            ffi_ret = _map_get(sig2, "ret");
            ffi_rtk = ffi_param_kind(ffi_ret);
        }
        tx_t call_expr = _cat(_cat(_cat(_cat(callname, "("), clcall), argstr), ")");
        tx_t ffi_pref = "";
        if (strcmp(ffi_rtk,"struct_val") == 0) {
            rcnm = ffi_struct_cname(ffi_ret);
            ffi_pref = _cat(_cat(ffi_pref, isel), "  plant_ffi_errno = 0;\n");
            if (strcmp(tgt,"_") == 0) {
                return _cat(_cat(_cat(_cat(_cat(_cat(clpre, ffi_pref), isel), "  "), call_expr), ";\n"), clclears);
            }
            stk2 = stvar_kind(stvars, tgt);
            if (strcmp(stk2,"") > 0) {
                return _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, ffi_pref), isel), "  "), tgt), " = plant_"), rcnm), "_to_map("), call_expr), ");\n"), clclears);
            }
            return _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, ffi_pref), isel), "  "), tgt), " = plant_"), rcnm), "_to_map("), call_expr), ");\n"), clclears);
        }
        if (strcmp(ffi_rtk,"struct_val") != 0) {
            if (strcmp(ffi_rtk,"voidptr") == 0 || strcmp(ffi_rtk,"struct_ref") == 0) {
                ffi_pref = _cat(_cat(ffi_pref, isel), "  plant_ffi_errno = 0;\n");
            }
            if (strcmp(tgt,"_") == 0) {
                return _cat(_cat(_cat(_cat(_cat(_cat(clpre, ffi_pref), isel), "  "), call_expr), ";\n"), clclears);
            }
            if (strcmp(tgt,"_") != 0) {
                return _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, ffi_pref), isel), "  "), tgt), " = "), call_expr), ";\n"), clclears);
            }
        }
    }
    if (strcmp(ntype,"if_stmt") == 0) {
        cond = _map_get(node, "cond");
        bd = _map_get(node, "body");
        ccond = translate_expr(cond);
        ccond = handle_strcmp(ccond);
        isel = indent_str(indent);
        tx_t ccode = _cat(_cat(_cat(isel, "  if ("), ccond), ") {\n");
        bcode = generate_body(bd, indent+2, sigs, subst, clmap, actx, nums, stvars, evars);
        ccode = _cat(_cat(_cat(ccode, bcode), isel), "  }\n");
        return ccode;
    }
    if (strcmp(ntype,"season_stmt") == 0) {
        cond = _map_get(node, "cond");
        bd = _map_get(node, "body");
        ccond = translate_expr(cond);
        ccond = handle_strcmp(ccond);
        isel = indent_str(indent);
        tx_t ccode = _cat(_cat(_cat(isel, "  while ("), ccond), ") {\n");
        bcode = generate_body(bd, indent+2, sigs, subst, clmap, actx, nums, stvars, evars);
        ccode = _cat(_cat(_cat(ccode, bcode), isel), "  }\n");
        return ccode;
    }
    if (strcmp(ntype,"cycle_stmt") == 0) {
        ivar = _map_get(node, "iterVar");
        fromExpr = _map_get(node, "fromExpr");
        toExpr = _map_get(node, "toExpr");
        stepExpr = _map_get(node, "stepExpr");
        listExpr = _map_get(node, "listExpr");
        indexVar = _map_get(node, "indexVar");
        bd = _map_get(node, "body");
        isel = indent_str(indent);
        tx_t ccode = "";
        if (strcmp(fromExpr,"") != 0) {
            cfrom = translate_expr(fromExpr);
            cto = translate_expr(toExpr);
            tx_t stepstr = "1";
            if (strcmp(stepExpr,"") > 0 && strcmp(stepExpr,"null") != 0) {
                cstep = translate_expr(stepExpr);
                stepstr = cstep;
            }
            ccode = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  for (long "), ivar), " = "), cfrom), "; "), ivar), " <= "), cto), "; "), ivar), " += "), stepstr), ") {\n");
            bcode = generate_body(bd, indent+2, sigs, subst, clmap, actx, nums, stvars, evars);
            ccode = _cat(_cat(_cat(ccode, bcode), isel), "  }\n");
            return ccode;
        }
        if (strcmp(listExpr,"") != 0) {
            tx_t idxvar = "__cycle_i";
            if (strcmp(indexVar,"") != 0 && strcmp(indexVar,"null") != 0) {
                idxvar = indexVar;
            }
            clist = translate_expr(listExpr);
            ccode = _cat(isel, "  {\n");
            ccode = _cat(_cat(_cat(_cat(ccode, isel), "    long "), idxvar), " = 0;\n");
            ccode = _cat(_cat(_cat(_cat(_cat(_cat(ccode, isel), "    while ("), idxvar), " < plant_array_length("), clist), ")) {\n");
            ccode = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(ccode, isel), "      tx_t "), ivar), " = plant_array_get("), clist), ", "), idxvar), ");\n");
            bcode = generate_body(bd, indent+4, sigs, subst, clmap, actx, nums, stvars, evars);
            ccode = _cat(_cat(_cat(_cat(_cat(ccode, bcode), isel), "      "), idxvar), "++;\n");
            ccode = _cat(_cat(ccode, isel), "    }\n");
            ccode = _cat(_cat(ccode, isel), "  }\n");
            return ccode;
        }
        return "";
    }
    if (strcmp(ntype,"match_stmt") == 0) {
        subj = _map_get(node, "subjectExpr");
        PlantArray* clauses = _map_get ( node , "clauses" );
        csubj = translate_expr(subj);
        isel = indent_str(indent);
        tx_t ccode = _cat(_cat(_cat(isel, "  switch ("), csubj), ") {\n");
        long ci = 0;
        tx_t clause = "";
        while (ci < plant_array_length(clauses)) {
            clause = plant_list_get(clauses, ci);
            vname = _map_get(clause, "variantName");
            binding = _map_get(clause, "binding");
            cbody = _map_get(clause, "bodyStatements");
            ccode = _cat(_cat(_cat(_cat(ccode, isel), "    case "), vname), ":\n");
            if (strcmp(binding,"") > 0 && strcmp(binding,"null") != 0) {
                ccode = _cat(_cat(ccode, isel), "      {\n");
                ccode = _cat(_cat(_cat(_cat(_cat(_cat(ccode, isel), "        tx_t "), binding), " = "), csubj), ";\n");
                bcode = generate_body(cbody, indent+4, sigs, subst, clmap, actx, nums, stvars, evars);
                ccode = _cat(_cat(_cat(ccode, bcode), isel), "      }\n");
            }
            if (strcmp(binding,"") == 0 || strcmp(binding,"null") == 0) {
                bcode = generate_body(cbody, indent+4, sigs, subst, clmap, actx, nums, stvars, evars);
                ccode = _cat(ccode, bcode);
            }
            ccode = _cat(_cat(ccode, isel), "      break;\n");
            ci = ci+1;
        }
        ccode = _cat(_cat(ccode, isel), "  }\n");
        return ccode;
    }
    if (strcmp(ntype,"external_decl") == 0) {
        return "";
    }
    if (strcmp(ntype,"struct_decl") == 0) {
        return "";
    }
    if (strcmp(ntype,"action_decl") == 0) {
        PlantArray* gens_nd = _map_get ( node , "generics" );
        if (plant_array_length(gens_nd) == 0) {
            aname = _map_get(node, "name");
            PlantArray* params = _map_get ( node , "params" );
            bd = _map_get(node, "body");
            PlantArray* stvars_a = collect_stvars ( bd , params , subst );
            tx_t asy_mark = _map_get ( node , "async" );
            if (strcmp(asy_mark,"1") == 0) {
                prio_s = _map_get(node, "prio");
                PlantArray* avars = async_collect_vars ( bd , params );
                PlantArray* aphases = async_split_phases ( bd );
                PlantArray* evars_asy = collect_enums ( bd , params , subst , evars );
                st_code = async_emit_state(aname, avars);
                en_code = async_emit_entry(aname, params, prio_s);
                stp_code = async_emit_step(aname, aphases, avars, sigs, subst, clmap, stvars_a, evars_asy);
                return _cat(_cat(_cat(_cat(_cat("static int plant_a_", aname), "_step(tx_t st);\n\n"), st_code), en_code), stp_code);
            }
            tx_t paramstr = "";
            long pi = 0;
            tx_t param_el = "";
            while (pi < plant_array_length(params)) {
                param_el = plant_list_get(params, pi);
                pname = _map_get(param_el, "name");
                ptype = _map_get(param_el, "type");
                ctype = ffi_ctype(ptype);
                if (pi > 0) {
                    paramstr = _cat(paramstr, ", ");
                }
                paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
                pi = pi+1;
            }
            PlantArray* nums_a = collect_nums ( bd , params , subst );
            PlantArray* evars_a = collect_enums ( bd , params , subst , evars );
            tx_t ccode = _cat(_cat(_cat(_cat("tx_t ", aname), "("), paramstr), ") {\n");
            PlantArray* implicit = collect_implicit ( bd , params );
            tx_t dcode = "";
            long di = 0;
            tx_t dv = "";
            while (di < plant_array_length(implicit)) {
                dv = plant_list_get(implicit, di);
                dcode = _cat(_cat(_cat(dcode, "  tx_t "), dv), " = \"\";\n");
                di = di+1;
            }
            bcode = generate_body(bd, 1, sigs, subst, clmap, actx, nums_a, stvars_a, evars_a);
            drn2 = _map_get(node, "drain_after");
            if (( plant_array_length(bd) ) == 0) {
                if (strcmp(drn2,"1") == 0) {
                    bcode = "  plant_async_drain();\n";
                }
                bcode = _cat(_cat(_cat(bcode, "  return "), aname), ";\n");
            }
            if (( plant_array_length(bd) ) > 0) {
                long bd_count = plant_array_length(bd);
                long last_idx = bd_count - 1;
                tx_t last_nd = plant_list_get ( bd , last_idx );
                tx_t last_ty = _map_get ( last_nd , "type" );
                if (strcmp(drn2,"1") == 0 && strcmp(last_ty,"give_stmt") != 0) {
                    bcode = _cat(bcode, "  plant_async_drain();\n");
                }
                if (strcmp(last_ty,"give_stmt") != 0) {
                    bcode = _cat(_cat(_cat(bcode, "  return "), aname), ";\n");
                }
            }
            ccode = _cat(_cat(_cat(ccode, dcode), bcode), "}\n");
            return ccode;
        }
        return "";
    }
    if (strcmp(ntype,"enum_decl") == 0) {
        ename = _map_get(node, "name");
        PlantArray* members = _map_get ( node , "members" );
        tx_t ccode = "typedef enum {\n  ";
        long mi = 0;
        tx_t member_el = "";
        while (mi < plant_array_length(members)) {
            member_el = plant_list_get(members, mi);
            if (mi > 0) {
                ccode = _cat(ccode, ",\n  ");
            }
            ccode = _cat(ccode, member_el);
            mi = mi+1;
        }
        ccode = _cat(_cat(_cat(ccode, "\n} "), ename), ";\n");
        return ccode;
    }
    return "";
}
tx_t type_base(tx_t ptype) {
  tx_t bi = "";
  tx_t b = "";
  tx_t bt = "";
    bi = find_any(ptype, "[");
    if (bi == - 1) {
        return ptype;
    }
    b = substring(ptype, 0, bi);
    bt = trim(b);
    return bt;
}
tx_t plant_ctype(tx_t ptype) {
  tx_t base = "";
    base = type_base(ptype);
    if (strcmp(base,"NUM") == 0) {
        return "long";
    }
    if (strcmp(base,"FACT") == 0) {
        return "int";
    }
    if (strcmp(base,"LIST") == 0) {
        return "PlantArray*";
    }
    if (strcmp(base,"REF NUM") == 0) {
        return "long*";
    }
    if (strcmp(base,"REF FACT") == 0) {
        return "int*";
    }
    if (strcmp(base,"REF LIST") == 0) {
        return "PlantArray**";
    }
    if (strcmp(base,"REF TX") == 0) {
        return "tx_t*";
    }
    return "tx_t";
}
tx_t ffi_param_kind(tx_t ptype) {
  tx_t p0 = "";
  tx_t rest = "";
  tx_t r0 = "";
  tx_t s0 = "";
    p0 = substring(ptype, 0, 4);
    if (strcmp(p0,"REF ") == 0) {
        rest = substring(ptype, 4, strlen( ptype ));
        r0 = substring(rest, 0, 7);
        if (strcmp(r0,"STRUCT ") == 0) {
            return "struct_ref";
        }
        return "plain";
    }
    s0 = substring(ptype, 0, 7);
    if (strcmp(s0,"STRUCT ") == 0) {
        return "struct_val";
    }
    if (strcmp(str_eq ( ptype , "void*" ),"1") == 0) {
        return "voidptr";
    }
    if (strcmp(str_eq ( ptype , "CALLBACK" ),"1") == 0) {
        return "callback";
    }
    return "plain";
}
tx_t ffi_struct_name(tx_t typ) {
  tx_t p0 = "";
  tx_t rest = "";
  tx_t s0 = "";
    p0 = substring(typ, 0, 4);
    if (strcmp(p0,"REF ") == 0) {
        rest = substring(typ, 4, strlen( typ ));
        typ = rest;
    }
    s0 = substring(typ, 0, 7);
    if (strcmp(s0,"STRUCT ") == 0) {
        return substring ( typ , 7 , strlen( typ ) );
    }
    return typ;
}
tx_t ffi_struct_cname(tx_t typ) {
  tx_t p0 = "";
  tx_t s0 = "";
  tx_t b0 = "";
  tx_t bt0 = "";
  tx_t bi = "";
  tx_t gargs = "";
    p0 = substring(typ, 0, 4);
    if (strcmp(p0,"REF ") == 0) {
        typ = substring(typ, 4, strlen( typ ));
    }
    s0 = substring(typ, 0, 7);
    if (strcmp(s0,"STRUCT ") == 0) {
        typ = substring(typ, 7, strlen( typ ));
    }
    b0 = base_of(typ);
    bt0 = trim(b0);
    tx_t cname = _cat("plant_", bt0);
    bi = find_any(typ, "[");
    if (bi != - 1) {
        gargs = parse_type_args(typ);
        long ai = 0;
        tx_t av = "";
        while (ai < plant_array_length(gargs)) {
            av = plant_list_get(gargs, ai);
            cname = _cat(_cat(cname, "_"), av);
            ai = ai+1;
        }
    }
    return cname;
}
tx_t is_struct_type(tx_t t) {
  tx_t base = "";
  tx_t tb = "";
  tx_t rf = "";
    base = type_base(t);
    tb = trim(base);
    if (strcmp(tb,"NUM") == 0) {
        return "0";
    }
    if (strcmp(tb,"FACT") == 0) {
        return "0";
    }
    if (strcmp(tb,"TX") == 0) {
        return "0";
    }
    if (strcmp(tb,"LIST") == 0) {
        return "0";
    }
    if (strcmp(tb,"") == 0) {
        return "0";
    }
    rf = substring(tb, 0, 4);
    if (strcmp(rf,"REF ") == 0) {
        return "0";
    }
    return "1";
}
tx_t collect_stvars_walk(PlantArray* bd, PlantArray* subst, PlantArray* res) {
  tx_t wsf = "";
  tx_t wfound = "";
  tx_t wk2 = "";
  tx_t wbd2 = "";
  tx_t wret2 = "";
    long wi = 0;
    tx_t wnd = "";
    tx_t wty = "";
    tx_t wtg = "";
    tx_t wvt = "";
    tx_t wbs = "";
    while (wi < plant_array_length(bd)) {
        wnd = plant_list_get(bd, wi);
        wty = _map_get(wnd, "type");
        if (strcmp(wty,"create_stmt") == 0 || strcmp(wty,"let_stmt") == 0) {
            wtg = _map_get(wnd, "target");
            wvt = _map_get(wnd, "var_type");
            wbs = subst_type(wvt, subst);
            wsf = is_struct_type(wbs);
            if (strcmp(wsf,"1") == 0) {
                wfound = list_contains(res, wtg);
                if (wfound == 0) {
                    res = plant_list_push(res, wtg);
                    wk2 = ffi_param_kind(wbs);
                    res = plant_list_push(res, wk2);
                }
            }
        }
        if (strcmp(wty,"if_stmt") == 0 || strcmp(wty,"season_stmt") == 0 || strcmp(wty,"cycle_stmt") == 0 || strcmp(wty,"match_stmt") == 0) {
            wbd2 = _map_get(wnd, "body");
            wret2 = collect_stvars_walk(wbd2, subst, res);
        }
        wi = wi+1;
    }
    return res;
}
tx_t collect_stvars(PlantArray* bd, PlantArray* params, PlantArray* subst) {
  tx_t pbs2 = "";
  tx_t pf = "";
  tx_t pk2 = "";
  tx_t ret = "";
    PlantArray* res = plant_list_make ( 0 );
    long pi = 0;
    PlantArray* pnd = plant_list_make ( 0 );
    tx_t pn2 = "";
    tx_t pty2 = "";
    tx_t psu2 = "";
    while (pi < plant_array_length(params)) {
        pnd = plant_list_get(params, pi);
        pn2 = _map_get(pnd, "name");
        pty2 = _map_get(pnd, "type");
        psu2 = subst_type(pty2, subst);
        pbs2 = is_struct_type(psu2);
        if (strcmp(pbs2,"1") == 0) {
            pf = list_contains(res, pn2);
            if (pf == 0) {
                res = plant_list_push(res, pn2);
                pk2 = ffi_param_kind(psu2);
                res = plant_list_push(res, pk2);
            }
        }
        pi = pi+1;
    }
    ret = collect_stvars_walk(bd, subst, res);
    return res;
}
tx_t find_sig(PlantArray* sigs, tx_t name) {
    long fi = 0;
    tx_t fe = "";
    tx_t fn = "";
    PlantArray* found = plant_list_make ( 0 );
    while (fi < plant_array_length(sigs)) {
        fe = plant_list_get(sigs, fi);
        fn = _map_get(fe, "name");
        if (strcmp(str_eq ( fn , name ),"1") == 0) {
            found = fe;
        }
        fi = fi+1;
    }
    return found;
}
tx_t ffi_ctype(tx_t ptype) {
  tx_t k = "";
  tx_t sn = "";
    k = ffi_param_kind(ptype);
    if (strcmp(k,"struct_val") == 0) {
        sn = ffi_struct_name(ptype);
        return ffi_struct_cname ( sn );
    }
    if (strcmp(k,"struct_ref") == 0) {
        sn = ffi_struct_name(ptype);
        return _cat(ffi_struct_cname ( sn ), "*");
    }
    if (strcmp(k,"voidptr") == 0) {
        return "void*";
    }
    if (strcmp(k,"callback") == 0) {
        return "plant_cb_t";
    }
    return plant_ctype ( ptype );
}
tx_t is_bare_id(tx_t e) {
    long bi2 = 0;
    tx_t bc = "";
    while (bi2 < strlen( e )) {
        bc = char_at(e, bi2);
        if (strcmp(bc," ") == 0 || strcmp(bc,"(") == 0 || strcmp(bc,")") == 0 || strcmp(bc,"+") == 0 || strcmp(bc,"-") == 0 || strcmp(bc,"\"") == 0 || strcmp(bc,"[") == 0 || strcmp(bc,"]") == 0 || strcmp(bc,"@") == 0 || strcmp(bc,"*") == 0 || strcmp(bc,"&") == 0 || strcmp(bc,"=") == 0 || strcmp(bc,"!") == 0) {
            return "0";
        }
        bi2 = bi2+1;
    }
    return "1";
}
tx_t expr_is_stringlike(tx_t e) {
  tx_t q0 = "";
  tx_t c1 = "";
  tx_t s1 = "";
    q0 = substring(e, 0, 1);
    if (strcmp(q0,"\"") == 0) {
        return "1";
    }
    c1 = find_any(e, "_cat(");
    if (c1 != - 1) {
        return "1";
    }
    s1 = substring(e, 0, 11);
    if (strcmp(s1,"_from_long(") == 0) {
        return "1";
    }
    return "0";
}
tx_t stvar_kind(PlantArray* stvars, tx_t name) {
  tx_t sv1 = "";
  tx_t sk1 = "";
    long si = 0;
    while (si + 1 < plant_array_length(stvars)) {
        sv1 = plant_list_get(stvars, si);
        if (strcmp(str_eq ( sv1 , name ),"1") == 0) {
            sk1 = plant_list_get(stvars, si+1);
            return sk1;
        }
        si = si+2;
    }
    return "";
}
tx_t collect_cb_uses(PlantArray* bd, PlantArray* sigs, PlantArray* acc) {
  tx_t caact = "";
  tx_t cafp = "";
  tx_t kap = "";
  tx_t kpty = "";
  tx_t kk = "";
  tx_t karg = "";
  tx_t kb = "";
  tx_t kfound = "";
  tx_t sb = "";
  tx_t cl2 = "";
  tx_t cb9 = "";
    long wi = 0;
    tx_t nd = "";
    tx_t ty = "";
    while (wi < plant_array_length(bd)) {
        nd = plant_list_get(bd, wi);
        ty = _map_get(nd, "type");
        if (strcmp(ty,"reap_stmt") == 0 || strcmp(ty,"start_stmt") == 0 || strcmp(ty,"await_stmt") == 0) {
            caact = _map_get(nd, "action");
            if (strcmp(caact,"") > 0) {
                caact = strings_REPLACE(caact, ":", "_");
                cafp = find_params(sigs, caact);
                PlantArray* caargs = _map_get ( nd , "args" );
                long kai = 0;
                while (kai < plant_array_length(caargs)) {
                    if (kai < plant_array_length(cafp)) {
                        kap = plant_list_get(cafp, kai);
                        kpty = _map_get(kap, "type");
                        kk = ffi_param_kind(kpty);
                        if (strcmp(kk,"callback") == 0) {
                            karg = plant_list_get(caargs, kai);
                            kb = is_bare_id(karg);
                            if (strcmp(kb,"1") == 0) {
                                kfound = callee_add(acc, karg);
                                acc = kfound;
                            }
                        }
                    }
                    kai = kai+1;
                }
            }
        }
        if (strcmp(ty,"if_stmt") == 0 || strcmp(ty,"season_stmt") == 0 || strcmp(ty,"cycle_stmt") == 0) {
            sb = _map_get(nd, "body");
            acc = collect_cb_uses(sb, sigs, acc);
        }
        if (strcmp(ty,"match_stmt") == 0) {
            PlantArray* cl9 = _map_get ( nd , "clauses" );
            long ci9 = 0;
            while (ci9 < plant_array_length(cl9)) {
                cl2 = plant_list_get(cl9, ci9);
                cb9 = _map_get(cl2, "bodyStatements");
                acc = collect_cb_uses(cb9, sigs, acc);
                ci9 = ci9+1;
            }
        }
        wi = wi+1;
    }
    return acc;
}
tx_t ffi_ret_ctype(tx_t ret) {
  tx_t rk = "";
  tx_t sn = "";
    rk = ffi_param_kind(ret);
    if (strcmp(rk,"struct_val") == 0) {
        sn = ffi_struct_name(ret);
        return ffi_struct_cname ( sn );
    }
    if (strcmp(rk,"voidptr") == 0) {
        return "void*";
    }
    return "tx_t";
}
tx_t struct_fields_at(PlantArray* tpl, PlantArray* args) {
  tx_t fn9 = "";
  tx_t ft9 = "";
  tx_t fs9 = "";
    PlantArray* flds = plant_list_make ( 0 );
    PlantArray* sgens = _map_get ( tpl , "generics" );
    PlantArray* sfields = _map_get ( tpl , "fields" );
    PlantArray* fsub = build_subst ( sgens , args );
    long fi9 = 0;
    tx_t fel9 = "";
    while (fi9 < plant_array_length(sfields)) {
        fel9 = plant_list_get(sfields, fi9);
        fn9 = _map_get(fel9, "name");
        ft9 = _map_get(fel9, "type");
        fs9 = subst_type(ft9, fsub);
        flds = plant_list_push(flds, fn9);
        flds = plant_list_push(flds, fs9);
        fi9 = fi9+1;
    }
    return flds;
}
tx_t ffi_emit_struct_helpers(tx_t cname, PlantArray* flds) {
  tx_t fb8 = "";
  tx_t fs8 = "";
  tx_t es8 = "";
  tx_t fcn8 = "";
  tx_t fb7 = "";
  tx_t fs7 = "";
  tx_t es7 = "";
  tx_t fcn7 = "";
    tx_t h1 = "";
    tx_t h2 = "";
    tx_t h3 = "";
    tx_t h4 = "";
    tx_t hall = "";
    h1 = _cat(_cat(_cat(_cat("static ", cname), " plant_map_to_"), cname), "_d(tx_t m, long depth) {\n");
    h1 = _cat(_cat(_cat(h1, "  "), cname), " r;\n  memset(&r, 0, sizeof(r));\n");
    h1 = _cat(_cat(_cat(h1, "  if (depth > 3) { plant_ffi_errno = FFI_ERR_DEPTH; plant_ffi_debug_print(\"map_to_"), cname), ": depth limit\"); return r; }\n");
    h1 = _cat(h1, "  if (!m) { plant_ffi_errno = FFI_ERR_TYPE; return r; }\n");
    long fi8 = 0;
    tx_t fn8 = "";
    tx_t ft8 = "";
    while (fi8 + 1 < plant_array_length(flds)) {
        fn8 = plant_list_get(flds, fi8);
        ft8 = plant_list_get(flds, fi8+1);
        fb8 = type_base(ft8);
        if (strcmp(fb8,"NUM") == 0) {
            h1 = _cat(_cat(_cat(_cat(_cat(h1, "  r."), fn8), " = (long)(intptr_t)plant_map_get(m, \""), fn8), "\");\n");
        }
        if (strcmp(fb8,"FACT") == 0) {
            h1 = _cat(_cat(_cat(_cat(_cat(h1, "  r."), fn8), " = (int)(intptr_t)plant_map_get(m, \""), fn8), "\");\n");
        }
        if (strcmp(fb8,"TX") == 0) {
            h1 = _cat(_cat(_cat(_cat(_cat(h1, "  r."), fn8), " = plant_map_get(m, \""), fn8), "\");\n");
        }
        fs8 = is_struct_type(ft8);
        if (strcmp(fs8,"1") == 0) {
            es8 = substring(ft8, 0, 7);
            if (strcmp(es8,"STRUCT ") == 0) {
                fcn8 = ffi_struct_cname(ft8);
                h1 = _cat(_cat(_cat(_cat(_cat(_cat(_cat(h1, "  r."), fn8), " = plant_map_to_"), fcn8), "_d(plant_map_get(m, \""), fn8), "\"), depth + 1);\n");
            }
            if (strcmp(es8,"STRUCT ") != 0) {
                h1 = _cat(_cat(_cat(_cat(_cat(h1, "  r."), fn8), " = plant_map_get(m, \""), fn8), "\");\n");
            }
        }
        if (strcmp(fb8,"LIST") == 0) {
            h1 = _cat(h1, "  plant_ffi_errno = FFI_ERR_TYPE;\n  return r;\n");
        }
        fi8 = fi8+2;
    }
    h1 = _cat(h1, "  return r;\n}\n");
    h2 = _cat(_cat(_cat(_cat(_cat(_cat("static ", cname), " plant_map_to_"), cname), "(tx_t m) { return plant_map_to_"), cname), "_d(m, 1); }\n");
    h3 = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat("static void* plant_struct_alloc_copy_", cname), "("), cname), " v) { "), cname), "* r = ("), cname), "*)plant_alloc(sizeof("), cname), ")); *r = v; return (void*)r; }\n");
    h3 = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(h3, "static "), cname), "* plant_map_to_ref_"), cname), "(tx_t m) { return ("), cname), "*)plant_struct_alloc_copy_"), cname), "(plant_map_to_"), cname), "(m)); }\n");
    h4 = _cat(_cat(_cat(_cat("static tx_t plant_", cname), "_to_map("), cname), " v) {\n");
    h4 = _cat(h4, "  tx_t r = (tx_t)plant_map_create(8);\n");
    long fi7 = 0;
    tx_t fn7 = "";
    tx_t ft7 = "";
    while (fi7 + 1 < plant_array_length(flds)) {
        fn7 = plant_list_get(flds, fi7);
        ft7 = plant_list_get(flds, fi7+1);
        fb7 = type_base(ft7);
        if (strcmp(fb7,"NUM") == 0) {
            h4 = _cat(_cat(_cat(_cat(_cat(h4, "  plant_map_set(r, \""), fn7), "\", (void*)(intptr_t)v."), fn7), ");\n");
        }
        if (strcmp(fb7,"FACT") == 0) {
            h4 = _cat(_cat(_cat(_cat(_cat(h4, "  plant_map_set(r, \""), fn7), "\", (void*)(intptr_t)v."), fn7), ");\n");
        }
        if (strcmp(fb7,"TX") == 0) {
            h4 = _cat(_cat(_cat(_cat(_cat(h4, "  plant_map_set(r, \""), fn7), "\", v."), fn7), ");\n");
        }
        fs7 = is_struct_type(ft7);
        if (strcmp(fs7,"1") == 0) {
            es7 = substring(ft7, 0, 7);
            if (strcmp(es7,"STRUCT ") == 0) {
                fcn7 = ffi_struct_cname(ft7);
                h4 = _cat(_cat(_cat(_cat(_cat(_cat(_cat(h4, "  plant_map_set(r, \""), fn7), "\", plant_"), fcn7), "_to_map(v."), fn7), "));\n");
            }
            if (strcmp(es7,"STRUCT ") != 0) {
                h4 = _cat(_cat(_cat(_cat(_cat(h4, "  plant_map_set(r, \""), fn7), "\", v."), fn7), ");\n");
            }
        }
        if (strcmp(fb7,"LIST") == 0) {
            h4 = _cat(h4, "  plant_ffi_errno = FFI_ERR_TYPE;\n  return r;\n");
        }
        fi7 = fi7+2;
    }
    h4 = _cat(h4, "  return r;\n}\n");
    hall = _cat(_cat(h1, h2), "");
    hall = _cat(_cat(hall, h3), "");
    hall = _cat(_cat(hall, h4), "");
    return hall;
}
tx_t trim(tx_t s) {
  tx_t c = "";
    long st = 0;
    long en = strlen( s );
    while (st < en) {
        c = char_at(s, st);
        if (strcmp(c," ") != 0) {
            break;
        }
        st = st+1;
    }
    while (en > st) {
        c = char_at(s, en - 1);
        if (strcmp(c," ") != 0) {
            break;
        }
        en = en - 1;
    }
    if (en <= st) {
        return "";
    }
    return substring ( s , st , en );
}
tx_t subst_append(tx_t acc, tx_t w, PlantArray* subst) {
    long fi = 0;
    tx_t fk = "";
    tx_t fv = "";
    while (fi + 1 < plant_array_length(subst)) {
        fk = plant_list_get(subst, fi);
        fv = plant_list_get(subst, fi+1);
        if (strcmp(str_eq ( fk , w ),"1") == 0) {
            return _cat(acc, fv);
        }
        fi = fi+2;
    }
    return _cat(acc, w);
}
tx_t subst_type(tx_t t, PlantArray* subst) {
    tx_t res = "";
    tx_t w = "";
    long si = 0;
    tx_t ch = "";
    while (si < strlen( t )) {
        ch = char_at(t, si);
        if (strcmp(ch," ") == 0 || strcmp(ch,"[") == 0 || strcmp(ch,"]") == 0 || strcmp(ch,"(") == 0 || strcmp(ch,")") == 0 || strcmp(ch,",") == 0) {
            res = subst_append(res, w, subst);
            w = "";
            res = _cat(res, ch);
        }
        if (strcmp(ch," ") != 0 && strcmp(ch,"[") != 0 && strcmp(ch,"]") != 0 && strcmp(ch,"(") != 0 && strcmp(ch,")") != 0 && strcmp(ch,",") != 0) {
            w = _cat(w, ch);
        }
        si = si+1;
    }
    res = subst_append(res, w, subst);
    return res;
}
tx_t subst_reap_act(tx_t act, PlantArray* subst) {
  tx_t bi = "";
  tx_t head = "";
  tx_t tail = "";
  tx_t stail = "";
    bi = find_any(act, "[");
    if (bi == - 1) {
        return act;
    }
    head = substring(act, 0, bi);
    tail = substring(act, bi, strlen( act ));
    stail = subst_type(tail, subst);
    return _cat(head, stail);
}
tx_t base_of(tx_t act) {
  tx_t bi = "";
    bi = find_any(act, "[");
    if (bi == - 1) {
        return act;
    }
    return substring ( act , 0 , bi );
}
tx_t ffi_topo_order(PlantArray* entries) {
    PlantArray* tdone = plant_list_make ( 0 );
    long tprog = 1;
    long ti = 0;
    PlantArray* te9 = plant_list_make ( 0 );
    tx_t tscn = "";
    PlantArray* tflds2 = plant_list_make ( 0 );
    tx_t td1 = "0";
    tx_t td2 = "0";
    tx_t td3 = "0";
    long tj = 0;
    long tok = 1;
    tx_t ft9 = "";
    tx_t fs9 = "";
    tx_t fc9 = "";
    PlantArray* res = plant_list_make ( 0 );
    PlantArray* entries_scns = plant_list_make ( 0 );
    ti = 0;
    while (ti < plant_array_length(entries)) {
        te9 = plant_list_get(entries, ti);
        tscn = _map_get(te9, "scn");
        entries_scns = plant_list_push(entries_scns, tscn);
        ti = ti+1;
    }
    while (tprog == 1) {
        tprog = 0;
        ti = 0;
        while (ti < plant_array_length(entries)) {
            te9 = plant_list_get(entries, ti);
            tscn = _map_get(te9, "scn");
            td1 = key_in_acc(tscn, tdone);
            if (strcmp(td1,"1") == 0) {
                ti = ti+1;
                continue;
            }
            PlantArray* tflds2 = _map_get ( te9 , "flds" );
            tok = 1;
            tj = 0;
            while (tj < plant_array_length(tflds2)) {
                ft9 = plant_list_get(tflds2, tj);
                fs9 = is_struct_type(ft9);
                if (strcmp(fs9,"1") == 0) {
                    fc9 = ffi_struct_cname(ft9);
                    td2 = key_in_acc(fc9, entries_scns);
                    if (strcmp(td2,"1") == 0) {
                        td3 = key_in_acc(fc9, tdone);
                        if (strcmp(td3,"0") == 0) {
                            tok = 0;
                        }
                    }
                }
                tj = tj+1;
            }
            if (tok == 1) {
                res = plant_list_push(res, te9);
                tdone = plant_list_push(tdone, tscn);
                tprog = 1;
            }
            ti = ti+1;
        }
    }
    return res;
}
tx_t ffi_topo_emit_helpers(PlantArray* entries) {
  tx_t ord = "";
    ord = ffi_topo_order(entries);
    tx_t tfw = "";
    tx_t tdf = "";
    tx_t tscn = "";
    PlantArray* tflds2 = plant_list_make ( 0 );
    tx_t ffd1 = "";
    long ti = 0;
    PlantArray* te9 = plant_list_make ( 0 );
    while (ti < plant_array_length(ord)) {
        te9 = plant_list_get(ord, ti);
        tscn = _map_get(te9, "scn");
        tfw = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(tfw, "static "), tscn), " plant_map_to_"), tscn), "_d(tx_t m, long depth);\nstatic "), tscn), " plant_map_to_"), tscn), "(tx_t m);\nstatic void* plant_struct_alloc_copy_"), tscn), "("), tscn), " v);\nstatic "), tscn), "* plant_map_to_ref_"), tscn), "(tx_t m);\nstatic tx_t plant_"), tscn), "_to_map("), tscn), " v);\n");
        PlantArray* tflds2 = _map_get ( te9 , "flds" );
        ffd1 = ffi_emit_struct_helpers(tscn, tflds2);
        tdf = _cat(_cat(tdf, ffd1), "\n");
        ti = ti+1;
    }
    return _cat(tfw, tdf);
}
tx_t parse_type_args(tx_t act) {
  tx_t bi = "";
  tx_t ei = "";
  tx_t inner = "";
  tx_t parts = "";
    bi = find_any(act, "[");
    ei = find_any(act, "]");
    if (bi == - 1 || ei == - 1) {
        return plant_list_make ( 0 );
    }
    if (ei <= bi + 1) {
        return plant_list_make ( 0 );
    }
    inner = substring(act, bi+1, ei);
    parts = strings_SPLIT((tx_t)inner, ",");
    PlantArray* out = plant_list_make ( 0 );
    long oi = 0;
    tx_t pe = "";
    tx_t pt = "";
    while (oi < plant_array_length(parts)) {
        pe = plant_list_get(parts, oi);
        pt = trim(pe);
        if (strcmp(pt,"") > 0) {
            out = plant_list_push(out, pt);
        }
        oi = oi+1;
    }
    return out;
}
tx_t mangle(tx_t base, PlantArray* args) {
    tx_t res = _cat("plant_", base);
    long mi = 0;
    tx_t ae = "";
    while (mi < plant_array_length(args)) {
        ae = plant_list_get(args, mi);
        res = _cat(_cat(res, "_"), ae);
        mi = mi+1;
    }
    return res;
}
tx_t find_template(PlantArray* templates, tx_t base) {
    long fi = 0;
    tx_t fe = "";
    tx_t fn = "";
    PlantArray* found = plant_list_make ( 0 );
    while (fi < plant_array_length(templates)) {
        fe = plant_list_get(templates, fi);
        fn = _map_get(fe, "name");
        if (strcmp(str_eq ( fn , base ),"1") == 0) {
            found = fe;
        }
        fi = fi+1;
    }
    return found;
}
tx_t find_struct(PlantArray* structs, tx_t name) {
    long fi = 0;
    tx_t fe = "";
    tx_t fn = "";
    PlantArray* found = plant_list_make ( 0 );
    while (fi < plant_array_length(structs)) {
        fe = plant_list_get(structs, fi);
        fn = _map_get(fe, "name");
        if (strcmp(str_eq ( fn , name ),"1") == 0) {
            found = fe;
        }
        fi = fi+1;
    }
    return found;
}
tx_t scan_type(tx_t t, PlantArray* subst, PlantArray* structs, PlantArray* acc) {
  tx_t base = "";
  tx_t btrim = "";
  tx_t rf = "";
  tx_t tpl = "";
  tx_t args = "";
  tx_t found = "";
  tx_t generics = "";
  tx_t nsubst = "";
  tx_t fields = "";
    tx_t st = "";
    long bi = - 1;
    long ai2b = - 1;
    st = subst_type(t, subst);
    bi = find_any(st, "[");
    if (bi != - 1) {
        base = base_of(st);
        btrim = trim(base);
        rf = substring(btrim, 0, 4);
        if (strcmp(rf,"REF ") == 0) {
            btrim = substring(btrim, 4, strlen( btrim ));
        }
        tpl = find_struct(structs, btrim);
        if (plant_array_length(tpl) > 0) {
            args = parse_type_args(st);
            found = key_in_acc(st, acc);
            if (strcmp(found,"0") == 0) {
                acc = plant_list_push(acc, st);
                generics = _map_get(tpl, "generics");
                nsubst = build_subst(generics, args);
                fields = _map_get(tpl, "fields");
                long fi2 = 0;
                tx_t fv = "";
                while (fi2 < plant_array_length(fields)) {
                    fv = _map_get(plant_list_get(fields,  fi2 ), "type");
                    acc = scan_type(fv, nsubst, structs, acc);
                    fi2 = fi2+1;
                }
            }
        }
        args = parse_type_args(st);
        long ai2 = 0;
        tx_t av = "";
        while (ai2 < plant_array_length(args)) {
            av = plant_list_get(args, ai2);
            ai2b = find_any(av, "[");
            if (ai2b != - 1) {
                acc = scan_type(av, subst, structs, acc);
            }
            ai2 = ai2+1;
        }
    }
    return acc;
}
tx_t scan_params(PlantArray* params, PlantArray* subst, PlantArray* structs, PlantArray* acc) {
    long pi = 0;
    tx_t pv = "";
    while (pi < plant_array_length(params)) {
        pv = _map_get(plant_list_get(params,  pi ), "type");
        acc = scan_type(pv, subst, structs, acc);
        pi = pi+1;
    }
    return acc;
}
tx_t scan_fields(PlantArray* fields, PlantArray* subst, PlantArray* structs, PlantArray* acc) {
    long fi = 0;
    tx_t fv = "";
    while (fi < plant_array_length(fields)) {
        fv = _map_get(plant_list_get(fields,  fi ), "type");
        acc = scan_type(fv, subst, structs, acc);
        fi = fi+1;
    }
    return acc;
}
tx_t collect_struct_insts(PlantArray* bd, PlantArray* subst, PlantArray* structs, PlantArray* acc) {
    long ci = 0;
    tx_t nd = "";
    tx_t ty = "";
    tx_t vt = "";
    PlantArray* sub_bd = plant_list_make ( 0 );
    while (ci < plant_array_length(bd)) {
        nd = plant_list_get(bd, ci);
        ty = _map_get(nd, "type");
        if (strcmp(ty,"create_stmt") == 0 || strcmp(ty,"let_stmt") == 0) {
            vt = _map_get(nd, "var_type");
            if (strcmp(vt,"") > 0) {
                acc = scan_type(vt, subst, structs, acc);
            }
        }
        if (strcmp(ty,"if_stmt") == 0 || strcmp(ty,"season_stmt") == 0) {
            sub_bd = _map_get(nd, "body");
            acc = collect_struct_insts(sub_bd, subst, structs, acc);
        }
        ci = ci+1;
    }
    return acc;
}
tx_t struct_typedef(PlantArray* tpl, PlantArray* args) {
  tx_t sname = "";
  tx_t generics = "";
  tx_t fields = "";
  tx_t subst = "";
  tx_t sp8 = "";
  tx_t fk9 = "";
  tx_t base9 = "";
    sname = _map_get(tpl, "name");
    generics = _map_get(tpl, "generics");
    fields = _map_get(tpl, "fields");
    subst = build_subst(generics, args);
    tx_t tname = _cat("plant_", sname);
    long mi = 0;
    tx_t ae = "";
    while (mi < plant_array_length(args)) {
        ae = plant_list_get(args, mi);
        tname = _cat(_cat(tname, "_"), ae);
        mi = mi+1;
    }
    tx_t ccode = "typedef struct {\n";
    long fi = 0;
    PlantArray* fel = plant_list_make ( 0 );
    tx_t fname = "";
    tx_t ftype = "";
    tx_t fsub = "";
    tx_t ctype = "";
    while (fi < plant_array_length(fields)) {
        fel = plant_list_get(fields, fi);
        fname = _map_get(fel, "name");
        ftype = _map_get(fel, "type");
        fsub = subst_type(ftype, subst);
        sp8 = substring(fsub, 0, 7);
        if (strcmp(sp8,"STRUCT ") == 0) {
            ctype = ffi_ctype(fsub);
        }
        if (strcmp(sp8,"STRUCT ") != 0) {
            fk9 = ffi_param_kind(fsub);
            if (strcmp(fk9,"struct_ref") == 0) {
                ctype = ffi_ctype(fsub);
            }
            if (strcmp(fk9,"struct_ref") != 0) {
                base9 = type_base(fsub);
                if (strcmp(base9,"NUM") == 0 || strcmp(base9,"FACT") == 0 || strcmp(base9,"TX") == 0 || strcmp(base9,"LIST") == 0) {
                    ctype = plant_ctype(fsub);
                }
                if (strcmp(base9,"NUM") != 0 && strcmp(base9,"FACT") != 0 && strcmp(base9,"TX") != 0 && strcmp(base9,"LIST") != 0) {
                    ctype = "tx_t";
                }
            }
        }
        ccode = _cat(_cat(_cat(_cat(_cat(ccode, "  "), ctype), " "), fname), ";\n");
        fi = fi+1;
    }
    ccode = _cat(_cat(_cat(ccode, "} "), tname), ";\n");
    return ccode;
}
tx_t key_in_acc(tx_t key, PlantArray* acc) {
    long fi = 0;
    tx_t fe = "";
    while (fi < plant_array_length(acc)) {
        fe = plant_list_get(acc, fi);
        if (strcmp(str_eq ( fe , key ),"1") == 0) {
            return "1";
        }
        fi = fi+1;
    }
    return "0";
}
tx_t build_subst(PlantArray* generics, PlantArray* args) {
    PlantArray* subst = plant_list_make ( 0 );
    long zi = 0;
    tx_t gv = "";
    tx_t av = "";
    while (zi < plant_array_length(generics)) {
        if (zi < plant_array_length(args)) {
            gv = plant_list_get(generics, zi);
            av = plant_list_get(args, zi);
            subst = plant_list_push(subst, gv);
            subst = plant_list_push(subst, av);
        }
        zi = zi+1;
    }
    return subst;
}
tx_t collect_insts(PlantArray* bd, PlantArray* subst, PlantArray* templates, PlantArray* acc) {
  tx_t sact = "";
  tx_t base = "";
  tx_t tpl = "";
  tx_t found = "";
  tx_t args = "";
  tx_t generics = "";
  tx_t nsubst = "";
  tx_t tbd = "";
  tx_t sub_bd = "";
    long ci = 0;
    tx_t nd = "";
    tx_t ty = "";
    tx_t act = "";
    long gi = - 1;
    while (ci < plant_array_length(bd)) {
        nd = plant_list_get(bd, ci);
        ty = _map_get(nd, "type");
        if (strcmp(ty,"reap_stmt") == 0) {
            act = _map_get(nd, "action");
            gi = find_any(act, "[");
            if (gi != - 1) {
                sact = subst_reap_act(act, subst);
                base = base_of(sact);
                tpl = find_template(templates, base);
                if (plant_array_length(tpl) > 0) {
                    found = key_in_acc(sact, acc);
                    if (strcmp(found,"0") == 0) {
                        acc = plant_list_push(acc, sact);
                        args = parse_type_args(sact);
                        generics = _map_get(tpl, "generics");
                        nsubst = build_subst(generics, args);
                        tbd = _map_get(tpl, "body");
                        acc = collect_insts(tbd, nsubst, templates, acc);
                    }
                }
            }
        }
        if (strcmp(ty,"if_stmt") == 0 || strcmp(ty,"season_stmt") == 0) {
            sub_bd = _map_get(nd, "body");
            acc = collect_insts(sub_bd, subst, templates, acc);
        }
        ci = ci+1;
    }
    return acc;
}
tx_t inst_fwddecl(tx_t inst, PlantArray* templates) {
  tx_t base = "";
  tx_t args = "";
  tx_t tpl = "";
  tx_t generics = "";
  tx_t params = "";
  tx_t subst = "";
  tx_t mname = "";
    base = base_of(inst);
    args = parse_type_args(inst);
    tpl = find_template(templates, base);
    if (plant_array_length(tpl) == 0) {
        return "";
    }
    generics = _map_get(tpl, "generics");
    params = _map_get(tpl, "params");
    subst = build_subst(generics, args);
    mname = mangle(base, args);
    tx_t paramstr = "";
    long pi = 0;
    tx_t param_el = "";
    tx_t pname = "";
    tx_t ptype = "";
    tx_t psub = "";
    tx_t ctype = "";
    while (pi < plant_array_length(params)) {
        param_el = plant_list_get(params, pi);
        pname = _map_get(param_el, "name");
        ptype = _map_get(param_el, "type");
        psub = subst_type(ptype, subst);
        ctype = ffi_ctype(psub);
        if (pi > 0) {
            paramstr = _cat(paramstr, ", ");
        }
        paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
        pi = pi+1;
    }
    return _cat(_cat(_cat(_cat("tx_t ", mname), "("), paramstr), ");\n");
}
tx_t emit_inst(tx_t inst, PlantArray* templates, PlantArray* sigs, PlantArray* reg) {
  tx_t base = "";
  tx_t args = "";
  tx_t tpl = "";
  tx_t generics = "";
  tx_t params = "";
  tx_t bd = "";
  tx_t subst = "";
  tx_t mname = "";
  tx_t bcode = "";
    base = base_of(inst);
    args = parse_type_args(inst);
    tpl = find_template(templates, base);
    if (plant_array_length(tpl) == 0) {
        return "";
    }
    generics = _map_get(tpl, "generics");
    params = _map_get(tpl, "params");
    bd = _map_get(tpl, "body");
    subst = build_subst(generics, args);
    mname = mangle(base, args);
    tx_t paramstr = "";
    long pi = 0;
    tx_t param_el = "";
    tx_t pname = "";
    tx_t ptype = "";
    tx_t psub = "";
    tx_t ctype = "";
    while (pi < plant_array_length(params)) {
        param_el = plant_list_get(params, pi);
        pname = _map_get(param_el, "name");
        ptype = _map_get(param_el, "type");
        psub = subst_type(ptype, subst);
        ctype = ffi_ctype(psub);
        if (pi > 0) {
            paramstr = _cat(paramstr, ", ");
        }
        paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
        pi = pi+1;
    }
    tx_t ccode = _cat(_cat(_cat(_cat("tx_t ", mname), "("), paramstr), ") {\n");
    PlantArray* nums_m = collect_nums ( bd , params , subst );
    PlantArray* stvars_m = collect_stvars ( bd , params , subst );
    PlantArray* evars_m = collect_enums ( bd , params , subst , reg );
    PlantArray* implicit = collect_implicit ( bd , params );
    tx_t dcode = "";
    long di = 0;
    tx_t dv = "";
    while (di < plant_array_length(implicit)) {
        dv = plant_list_get(implicit, di);
        dcode = _cat(_cat(_cat(dcode, "  tx_t "), dv), " = \"\";\n");
        di = di+1;
    }
    bcode = generate_body(bd, 1, sigs, subst, plant_list_make ( 0 ), "", nums_m, stvars_m, evars_m);
    if (( plant_array_length(bd) ) == 0) {
        bcode = _cat(_cat("  return ", mname), ";\n");
    }
    if (( plant_array_length(bd) ) > 0) {
        long bd_count = plant_array_length(bd);
        long last_idx = bd_count - 1;
        tx_t last_nd = plant_list_get ( bd , last_idx );
        tx_t last_ty = _map_get ( last_nd , "type" );
        if (strcmp(last_ty,"give_stmt") != 0) {
            bcode = _cat(_cat(_cat(bcode, "  return "), mname), ";\n");
        }
    }
    ccode = _cat(_cat(_cat(ccode, dcode), bcode), "}\n");
    return ccode;
}
tx_t find_params(PlantArray* sigs, tx_t name) {
    long fi = 0;
    tx_t fe = "";
    tx_t fn = "";
    PlantArray* fp = plant_list_make ( 0 );
    while (fi < plant_array_length(sigs)) {
        fe = plant_list_get(sigs, fi);
        fn = _map_get(fe, "name");
        if (strcmp(str_eq ( fn , name ),"1") == 0) {
            fp = _map_get(fe, "params");
        }
        fi = fi+1;
    }
    return fp;
}
tx_t is_ref_param(tx_t ptype) {
  tx_t pf = "";
    pf = substring(ptype, 0, 4);
    if (strcmp(pf,"REF ") == 0) {
        return "1";
    }
    return "0";
}
tx_t is_ref_at(PlantArray* params, long idx) {
  tx_t pel = "";
  tx_t pty = "";
  tx_t rf = "";
    if (idx < plant_array_length(params)) {
        pel = plant_list_get(params, idx);
        pty = _map_get(pel, "type");
        rf = is_ref_param(pty);
        if (strcmp(rf,"1") == 0) {
            return "1";
        }
    }
    return "0";
}
tx_t find_node(PlantArray* ast, tx_t name) {
    long fi = 0;
    tx_t fe = "";
    tx_t fty = "";
    tx_t fnm = "";
    while (fi < plant_array_length(ast)) {
        fe = plant_list_get(ast, fi);
        fty = _map_get(fe, "type");
        if (strcmp(fty,"action_decl") == 0) {
            fnm = _map_get(fe, "name");
            if (strcmp(str_eq ( fnm , name ),"1") == 0) {
                return fe;
            }
        }
        fi = fi+1;
    }
    return "";
}
tx_t find_ext_node(PlantArray* ast, tx_t name) {
    long fei = 0;
    tx_t fee = "";
    tx_t fet = "";
    tx_t fen = "";
    while (fei < plant_array_length(ast)) {
        fee = plant_list_get(ast, fei);
        fet = _map_get(fee, "type");
        if (strcmp(fet,"external_decl") == 0) {
            fen = _map_get(fee, "name");
            if (strcmp(str_eq ( fen , name ),"1") == 0) {
                return fee;
            }
        }
        fei = fei+1;
    }
    return "";
}
tx_t callee_add(PlantArray* acc, tx_t name) {
    long cfound = 0;
    long ci = 0;
    tx_t ce = "";
    while (ci < plant_array_length(acc)) {
        ce = plant_list_get(acc, ci);
        if (strcmp(str_eq ( ce , name ),"1") == 0) {
            cfound = 1;
        }
        ci = ci+1;
    }
    if (!cfound) {
        acc = plant_list_push(acc, name);
    }
    return acc;
}
tx_t callee_from_value(PlantArray* acc, tx_t val) {
  tx_t v0 = "";
  tx_t ve = "";
  tx_t vcm = "";
  tx_t vn0 = "";
  tx_t vn1 = "";
  tx_t vn1b = "";
    v0 = substring(val, 0, 6);
    if (strcmp(v0,"START ") == 0) {
        ve = substring(val, 6, strlen( val ));
        vcm = find_any(ve, ",");
        if (vcm == - 1) {
            vn0 = trim(ve);
            acc = callee_add(acc, vn0);
        }
        if (vcm != - 1) {
            vn1 = substring(ve, 0, vcm);
            vn1b = trim(vn1);
            acc = callee_add(acc, vn1b);
        }
    }
    return acc;
}
tx_t callees_of(PlantArray* bd) {
  tx_t cbd2 = "";
  tx_t cret2 = "";
    PlantArray* acc = plant_list_make ( 0 );
    long ci = 0;
    tx_t cnd = "";
    tx_t cty = "";
    tx_t cact = "";
    tx_t cval = "";
    while (ci < plant_array_length(bd)) {
        cnd = plant_list_get(bd, ci);
        cty = _map_get(cnd, "type");
        if (strcmp(cty,"reap_stmt") == 0 || strcmp(cty,"start_stmt") == 0 || strcmp(cty,"await_stmt") == 0) {
            cact = _map_get(cnd, "action");
            if (strcmp(cact,"") > 0) {
                cact = strings_REPLACE(cact, ":", "_");
                acc = callee_add(acc, cact);
            }
        }
        if (strcmp(cty,"set_stmt") == 0 || strcmp(cty,"create_stmt") == 0 || strcmp(cty,"let_stmt") == 0 || strcmp(cty,"give_stmt") == 0 || strcmp(cty,"show_stmt") == 0 || strcmp(cty,"put_stmt") == 0 || strcmp(cty,"cancel_stmt") == 0 || strcmp(cty,"trace_stmt") == 0 || strcmp(cty,"config_stmt") == 0) {
            cval = _map_get(cnd, "value");
            if (strcmp(cval,"") > 0) {
                acc = callee_from_value(acc, cval);
            }
        }
        if (strcmp(cty,"if_stmt") == 0 || strcmp(cty,"season_stmt") == 0 || strcmp(cty,"cycle_stmt") == 0 || strcmp(cty,"match_stmt") == 0) {
            cbd2 = _map_get(cnd, "body");
            cret2 = callees_of(cbd2);
            long cj = 0;
            tx_t cje = "";
            while (cj < plant_array_length(cret2)) {
                cje = plant_list_get(cret2, cj);
                acc = callee_add(acc, cje);
                cj = cj+1;
            }
        }
        ci = ci+1;
    }
    return acc;
}
tx_t async_reachable(PlantArray* ast) {
  tx_t sfound = "";
  tx_t fnd = "";
  tx_t fas = "";
  tx_t fbod = "";
  tx_t callers2 = "";
  tx_t kf = "";
    PlantArray* queue = plant_list_make ( 0 );
    PlantArray* seen = plant_list_make ( 0 );
    long found = 0;
    queue = plant_list_push(queue, "main");
    long qi = 0;
    tx_t front = "";
    while (qi < plant_array_length(queue)) {
        front = plant_list_get(queue, qi);
        sfound = list_contains(seen, front);
        if (sfound == 0) {
            fnd = find_node(ast, front);
            if (strcmp(fnd,"") > 0) {
                fas = _map_get(fnd, "async");
                if (strcmp(fas,"1") == 0) {
                    found = 1;
                }
                fbod = _map_get(fnd, "body");
                callers2 = callees_of(fbod);
                long ck = 0;
                tx_t cke = "";
                while (ck < plant_array_length(callers2)) {
                    cke = plant_list_get(callers2, ck);
                    kf = list_contains(seen, cke);
                    if (kf == 0) {
                        queue = plant_list_push(queue, cke);
                    }
                    ck = ck+1;
                }
            }
            seen = plant_list_push(seen, front);
        }
        qi = qi+1;
    }
    return found;
}
tx_t generate_c(PlantArray* ast) {
  tx_t ntype = "";
  tx_t sname = "";
  tx_t fpel = "";
  tx_t fpty = "";
  tx_t fk7 = "";
  tx_t anm7 = "";
  tx_t cb2 = "";
  tx_t cb3 = "";
  tx_t ibase = "";
  tx_t iargs = "";
  tx_t itpl = "";
  tx_t igns = "";
  tx_t insub = "";
  tx_t ipms = "";
  tx_t ibd = "";
  tx_t sname4 = "";
  tx_t scn4 = "";
  tx_t stdef = "";
  tx_t ibtrim = "";
  tx_t scnA = "";
  tx_t fex = "";
  tx_t fnode = "";
  tx_t fretc = "";
  tx_t fkr2 = "";
  tx_t fcnx = "";
  tx_t fct2 = "";
  tx_t fccx = "";
  tx_t fk9 = "";
  tx_t snameQ = "";
  tx_t ibase2 = "";
  tx_t ibtrim2 = "";
  tx_t iargs2 = "";
  tx_t itpl2 = "";
  tx_t scnB = "";
  tx_t cba = "";
  tx_t asymark = "";
  tx_t inst_code = "";
  tx_t clres = "";
  tx_t clmaps = "";
  tx_t cllist = "";
  tx_t cnode = "";
  tx_t tc = "";
  tx_t fnn = "";
  tx_t cprm = "";
  tx_t pstr2 = "";
  tx_t fd = "";
  tx_t asy4 = "";
  tx_t cmact = "";
  tx_t nd_code = "";
  tx_t ns_code = "";
    tx_t header = "#include <plant_compat.h>\n\n";
    tx_t decl_code = "";
    tx_t stmt_code = "";
    long has_decl = 0;
    long has_stmt = 0;
    long i = 0;
    tx_t node_el = "";
    tx_t pro_code = "";
    long pi = 0;
    tx_t param_el = "";
    tx_t pname = "";
    tx_t ptype = "";
    tx_t ctype = "";
    tx_t aname = "";
    tx_t paramstr = "";
    PlantArray* sigs = plant_list_make ( 0 );
    PlantArray* templates = plant_list_make ( 0 );
    PlantArray* structs = plant_list_make ( 0 );
    PlantArray* eregs = build_enum_registry ( ast );
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"action_decl") == 0 || strcmp(ntype,"external_decl") == 0) {
            aname = _map_get(node_el, "name");
            PlantArray* params2 = _map_get ( node_el , "params" );
            tx_t rt2 = _map_get ( node_el , "ret" );
            tx_t vg2 = _map_get ( node_el , "varargs" );
            sigs = plant_list_push(sigs, plant_list_make ( 8 , "name" , aname , "params" , params2 , "ret" , rt2 , "varargs" , vg2 ));
        }
        if (strcmp(ntype,"action_decl") == 0) {
            aname = _map_get(node_el, "name");
            PlantArray* gens2 = _map_get ( node_el , "generics" );
            if (plant_array_length(gens2) > 0) {
                PlantArray* pms2 = _map_get ( node_el , "params" );
                PlantArray* bd2 = _map_get ( node_el , "body" );
                templates = plant_list_push(templates, plant_list_make ( 8 , "name" , aname , "generics" , gens2 , "params" , pms2 , "body" , bd2 ));
            }
        }
        if (strcmp(ntype,"struct_decl") == 0) {
            sname = _map_get(node_el, "name");
            PlantArray* sgens2 = _map_get ( node_el , "generics" );
            PlantArray* sfields2 = _map_get ( node_el , "fields" );
            structs = plant_list_push(structs, plant_list_make ( 6 , "name" , sname , "generics" , sgens2 , "fields" , sfields2 ));
        }
        i = i+1;
    }
    PlantArray* ffi_exts = plant_list_make ( 0 );
    PlantArray* cb_acts = plant_list_make ( 0 );
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"external_decl") == 0) {
            PlantArray* fparams7 = _map_get ( node_el , "params" );
            tx_t fret7 = _map_get ( node_el , "ret" );
            tx_t fvg7 = _map_get ( node_el , "varargs" );
            long ffi_ok = 0;
            if (strcmp(fret7,"") > 0) {
                ffi_ok = 1;
            }
            if (strcmp(fvg7,"1") == 0) {
                ffi_ok = 1;
            }
            long fai = 0;
            while (fai < plant_array_length(fparams7)) {
                fpel = plant_list_get(fparams7, fai);
                fpty = _map_get(fpel, "type");
                fk7 = ffi_param_kind(fpty);
                if (strcmp(fk7,"plain") != 0) {
                    ffi_ok = 1;
                }
                fai = fai+1;
            }
            if (ffi_ok == 1) {
                anm7 = _map_get(node_el, "name");
                ffi_exts = plant_list_push(ffi_exts, anm7);
            }
        }
        if (strcmp(ntype,"action_decl") == 0) {
            PlantArray* gsf = _map_get ( node_el , "generics" );
            if (plant_array_length(gsf) == 0) {
                PlantArray* bdf = _map_get ( node_el , "body" );
                cb2 = collect_cb_uses(bdf, sigs, cb_acts);
                cb_acts = cb2;
            }
        }
        if (strcmp(ntype,"action_decl") != 0 && strcmp(ntype,"enum_decl") != 0 && strcmp(ntype,"external_decl") != 0 && strcmp(ntype,"struct_decl") != 0) {
            cb3 = collect_cb_uses(plant_list_make ( 1 , node_el ), sigs, cb_acts);
            cb_acts = cb3;
        }
        i = i+1;
    }
    PlantArray* insts = plant_list_make ( 0 );
    PlantArray* esub = plant_list_make ( 0 );
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"action_decl") == 0) {
            PlantArray* gens5 = _map_get ( node_el , "generics" );
            if (plant_array_length(gens5) == 0) {
                PlantArray* bd3 = _map_get ( node_el , "body" );
                insts = collect_insts(bd3, esub, templates, insts);
            }
        }
        if (strcmp(ntype,"action_decl") != 0 && strcmp(ntype,"enum_decl") != 0 && strcmp(ntype,"external_decl") != 0 && strcmp(ntype,"struct_decl") != 0) {
            insts = collect_insts(plant_list_make ( 1 , node_el ), esub, templates, insts);
        }
        i = i+1;
    }
    PlantArray* structs_insts = plant_list_make ( 0 );
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"action_decl") == 0) {
            PlantArray* gs5 = _map_get ( node_el , "generics" );
            if (plant_array_length(gs5) == 0) {
                PlantArray* ps5 = _map_get ( node_el , "params" );
                PlantArray* bd5 = _map_get ( node_el , "body" );
                structs_insts = scan_params(ps5, esub, structs, structs_insts);
                structs_insts = collect_struct_insts(bd5, esub, structs, structs_insts);
            }
        }
        if (strcmp(ntype,"external_decl") == 0) {
            PlantArray* ps6 = _map_get ( node_el , "params" );
            structs_insts = scan_params(ps6, esub, structs, structs_insts);
        }
        if (strcmp(ntype,"struct_decl") == 0) {
            PlantArray* gs6 = _map_get ( node_el , "generics" );
            if (plant_array_length(gs6) == 0) {
                PlantArray* fs6 = _map_get ( node_el , "fields" );
                structs_insts = scan_fields(fs6, esub, structs, structs_insts);
            }
        }
        if (strcmp(ntype,"action_decl") != 0 && strcmp(ntype,"enum_decl") != 0 && strcmp(ntype,"external_decl") != 0 && strcmp(ntype,"struct_decl") != 0) {
            structs_insts = collect_struct_insts(plant_list_make ( 1 , node_el ), esub, structs, structs_insts);
        }
        i = i+1;
    }
    i = 0;
    while (i < plant_array_length(insts)) {
        node_el = plant_list_get(insts, i);
        ibase = base_of(node_el);
        iargs = parse_type_args(node_el);
        itpl = find_template(templates, ibase);
        if (plant_array_length(itpl) > 0) {
            igns = _map_get(itpl, "generics");
            insub = build_subst(igns, iargs);
            ipms = _map_get(itpl, "params");
            ibd = _map_get(itpl, "body");
            structs_insts = scan_params(ipms, insub, structs, structs_insts);
            structs_insts = collect_struct_insts(ibd, insub, structs, structs_insts);
        }
        i = i+1;
    }
    tx_t struct_code = "/*__PLANT_TYPES_BEGIN__*/\n#ifndef PLANT_TYPES_INCLUDED\n#define PLANT_TYPES_INCLUDED\n";
    PlantArray* ffi_tentries = plant_list_make ( 0 );
    PlantArray* ffi_torder = plant_list_make ( 0 );
    tx_t tscnX = "";
    tx_t tstdef = "";
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"struct_decl") == 0) {
            PlantArray* sgens4 = _map_get ( node_el , "generics" );
            if (plant_array_length(sgens4) == 0) {
                sname4 = _map_get(node_el, "name");
                scn4 = ffi_struct_cname(sname4);
                PlantArray* eargs = plant_list_make ( 0 );
                stdef = struct_typedef(node_el, eargs);
                PlantArray* sfldsT = struct_fields_at ( node_el , eargs );
                PlantArray* enT = plant_list_make ( 0 );
                enT = plant_list_push(enT, "scn");
                enT = plant_list_push(enT, scn4);
                enT = plant_list_push(enT, "flds");
                enT = plant_list_push(enT, sfldsT);
                enT = plant_list_push(enT, "stdef");
                enT = plant_list_push(enT, stdef);
                ffi_tentries = plant_list_push(ffi_tentries, enT);
            }
        }
        i = i+1;
    }
    i = 0;
    while (i < plant_array_length(structs_insts)) {
        node_el = plant_list_get(structs_insts, i);
        ibase = base_of(node_el);
        ibtrim = trim(ibase);
        iargs = parse_type_args(node_el);
        itpl = find_struct(structs, ibtrim);
        if (plant_array_length(itpl) > 0) {
            scnA = ffi_struct_cname(node_el);
            stdef = struct_typedef(itpl, iargs);
            PlantArray* sfldsT = struct_fields_at ( itpl , iargs );
            PlantArray* enT = plant_list_make ( 0 );
            enT = plant_list_push(enT, "scn");
            enT = plant_list_push(enT, scnA);
            enT = plant_list_push(enT, "flds");
            enT = plant_list_push(enT, sfldsT);
            enT = plant_list_push(enT, "stdef");
            enT = plant_list_push(enT, stdef);
            ffi_tentries = plant_list_push(ffi_tentries, enT);
        }
        i = i+1;
    }
    ffi_torder = ffi_topo_order(ffi_tentries);
    i = 0;
    while (i < plant_array_length(ffi_torder)) {
        node_el = plant_list_get(ffi_torder, i);
        tscnX = _map_get(node_el, "scn");
        struct_code = _cat(_cat(_cat(struct_code, "#define PLANT_STRUCT_"), tscnX), " 1\n");
        tstdef = _map_get(node_el, "stdef");
        struct_code = _cat(struct_code, tstdef);
        i = i+1;
    }
    long ffi_has_cb = 0;
    long ffi_has_vp = 0;
    long ffi_has_va = 0;
    i = 0;
    while (i < plant_array_length(ffi_exts)) {
        fex = plant_list_get(ffi_exts, i);
        fnode = find_ext_node(ast, fex);
        if (plant_array_length(fnode) > 0) {
            PlantArray* fpars = _map_get ( fnode , "params" );
            tx_t fret2 = _map_get ( fnode , "ret" );
            tx_t fvga = _map_get ( fnode , "varargs" );
            fretc = ffi_ret_ctype(fret2);
            long fexneed = 0;
            fkr2 = ffi_param_kind(fret2);
            if (strcmp(fkr2,"plain") == 0) {
                fexneed = 0;
            }
            if (strcmp(fkr2,"struct_val") == 0) {
                fexneed = 1;
            }
            if (strcmp(fkr2,"struct_ref") == 0) {
                fexneed = 1;
            }
            if (strcmp(fkr2,"callback") == 0) {
                fexneed = 1;
            }
            if (strcmp(fkr2,"voidptr") == 0) {
                fexneed = 1;
            }
            long fci = 0;
            tx_t fcel = "";
            while (fci < plant_array_length(fpars)) {
                fcel = plant_list_get(fpars, fci);
                fcnx = _map_get(fcel, "name");
                fct2 = _map_get(fcel, "type");
                fccx = ffi_ctype(fct2);
                fk9 = ffi_param_kind(fct2);
                if (strcmp(fk9,"callback") == 0) {
                    ffi_has_cb = 1;
                }
                if (strcmp(fk9,"voidptr") == 0) {
                    ffi_has_vp = 1;
                }
                if (strcmp(fk9,"struct_val") == 0) {
                    fexneed = 1;
                }
                if (strcmp(fk9,"struct_ref") == 0) {
                    fexneed = 1;
                }
                if (strcmp(fk9,"callback") == 0) {
                    fexneed = 1;
                }
                if (strcmp(fk9,"voidptr") == 0) {
                    fexneed = 1;
                }
                fci = fci+1;
            }
            if (strcmp(fvga,"1") == 0) {
                ffi_has_va = 1;
                fexneed = 1;
            }
            if (fexneed == 1) {
                struct_code = _cat(_cat(_cat(_cat(struct_code, fretc), " "), fex), "(");
                fci = 0;
                while (fci < plant_array_length(fpars)) {
                    fcel = plant_list_get(fpars, fci);
                    fcnx = _map_get(fcel, "name");
                    fct2 = _map_get(fcel, "type");
                    fccx = ffi_ctype(fct2);
                    if (fci > 0) {
                        struct_code = _cat(struct_code, ", ");
                    }
                    struct_code = _cat(_cat(_cat(struct_code, fccx), " "), fcnx);
                    fci = fci+1;
                }
                if (strcmp(fvga,"1") == 0) {
                    struct_code = _cat(struct_code, ", ...");
                }
                struct_code = _cat(struct_code, ");\n");
            }
        }
        i = i+1;
    }
    if (ffi_has_cb == 1) {
        struct_code = _cat(struct_code, "#define PLANT_FFI_HAS_CALLBACKS 1\n");
    }
    if (ffi_has_vp == 1) {
        struct_code = _cat(struct_code, "#define PLANT_FFI_HAS_VOIDPTR 1\n");
    }
    if (ffi_has_va == 1) {
        struct_code = _cat(struct_code, "#define PLANT_FFI_HAS_VARARGS 1\n");
    }
    struct_code = _cat(struct_code, "#endif\n/*__PLANT_TYPES_END__*/\n\n");
    PlantArray* ffi_entries = plant_list_make ( 0 );
    tx_t ffi_topo = "";
    PlantArray* enA = plant_list_make ( 0 );
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"struct_decl") == 0) {
            PlantArray* sgensQ = _map_get ( node_el , "generics" );
            if (plant_array_length(sgensQ) == 0) {
                snameQ = _map_get(node_el, "name");
                PlantArray* eargsQ = plant_list_make ( 0 );
                scnA = ffi_struct_cname(snameQ);
                PlantArray* sfldsA = struct_fields_at ( node_el , eargsQ );
                PlantArray* enA = plant_list_make ( 0 );
                enA = plant_list_push(enA, "scn");
                enA = plant_list_push(enA, scnA);
                enA = plant_list_push(enA, "flds");
                enA = plant_list_push(enA, sfldsA);
                ffi_entries = plant_list_push(ffi_entries, enA);
            }
        }
        i = i+1;
    }
    i = 0;
    while (i < plant_array_length(structs_insts)) {
        node_el = plant_list_get(structs_insts, i);
        ibase2 = base_of(node_el);
        ibtrim2 = trim(ibase2);
        iargs2 = parse_type_args(node_el);
        itpl2 = find_struct(structs, ibtrim2);
        if (plant_array_length(itpl2) > 0) {
            scnB = ffi_struct_cname(node_el);
            PlantArray* sfldsB = struct_fields_at ( itpl2 , iargs2 );
            PlantArray* enA = plant_list_make ( 0 );
            enA = plant_list_push(enA, "scn");
            enA = plant_list_push(enA, scnB);
            enA = plant_list_push(enA, "flds");
            enA = plant_list_push(enA, sfldsB);
            ffi_entries = plant_list_push(ffi_entries, enA);
        }
        i = i+1;
    }
    ffi_topo = ffi_topo_emit_helpers(ffi_entries);
    tx_t cb_code = "";
    PlantArray* cbnode = plant_list_make ( 0 );
    PlantArray* cbprms = plant_list_make ( 0 );
    tx_t cbp = "";
    tx_t cbt = "";
    tx_t cbtk = "";
    tx_t cbcn = "";
    long cbak = 0;
    tx_t cbcast = "";
    i = 0;
    while (i < plant_array_length(cb_acts)) {
        cba = plant_list_get(cb_acts, i);
        cbnode = find_node(ast, cba);
        PlantArray* cbprms = _map_get ( cbnode , "params" );
        cbcn = _cat(cba, "(");
        cbak = 0;
        while (cbak < plant_array_length(cbprms)) {
            cbp = plant_list_get(cbprms, cbak);
            cbt = _map_get(cbp, "type");
            cbtk = ffi_param_kind(cbt);
            cbcast = "";
            if (strcmp(cbtk,"plain") == 0 && strcmp(cbt,"NUM") == 0) {
                cbcast = "(long)(intptr_t)";
            }
            if (strcmp(cbtk,"plain") == 0 && strcmp(cbt,"FACT") == 0) {
                cbcast = "(int)(intptr_t)";
            }
            if (cbak > 0) {
                cbcn = _cat(cbcn, ", ");
            }
            if (cbak == 0 && plant_array_length(cbprms) == 1) {
                cbcn = _cat(_cat(cbcn, cbcast), "val");
            }
            if (cbak == 0 && plant_array_length(cbprms) == 2) {
                cbcn = _cat(_cat(cbcn, cbcast), "ctx");
            }
            if (cbak == 1) {
                cbcn = _cat(_cat(cbcn, cbcast), "val");
            }
            cbak = cbak+1;
        }
        cbcn = _cat(cbcn, ")");
        cb_code = _cat(_cat(_cat(_cat(_cat(cb_code, "static tx_t plant_cbw_"), cba), "(long ctx, tx_t val) { return "), cbcn), "; }\n");
        i = i+1;
    }
    if (plant_array_length(cb_acts) > 0) {
        cb_code = _cat(cb_code, "\n");
    }
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"action_decl") == 0) {
            aname = _map_get(node_el, "name");
            PlantArray* gens3 = _map_get ( node_el , "generics" );
            if (plant_array_length(gens3) == 0) {
                PlantArray* params3 = _map_get ( node_el , "params" );
                asymark = _map_get(node_el, "async");
                paramstr = "";
                pi = 0;
                while (pi < plant_array_length(params3)) {
                    param_el = plant_list_get(params3, pi);
                    pname = _map_get(param_el, "name");
                    ptype = _map_get(param_el, "type");
                    ctype = ffi_ctype(ptype);
                    if (pi > 0) {
                        paramstr = _cat(paramstr, ", ");
                    }
                    paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
                    pi = pi+1;
                }
                if (strcmp(asymark,"1") == 0) {
                    pro_code = _cat(_cat(_cat(pro_code, "tx_t "), aname), "(tx_t __parent, tx_t __ctx");
                    if (strcmp(paramstr,"") > 0) {
                        pro_code = _cat(_cat(pro_code, ", "), paramstr);
                    }
                    pro_code = _cat(pro_code, ");\n");
                }
                if (strcmp(asymark,"1") != 0) {
                    pro_code = _cat(_cat(_cat(_cat(_cat(pro_code, "tx_t "), aname), "("), paramstr), ");\n");
                }
            }
        }
        i = i+1;
    }
    i = 0;
    while (i < plant_array_length(insts)) {
        node_el = plant_list_get(insts, i);
        inst_code = inst_fwddecl(node_el, templates);
        pro_code = _cat(pro_code, inst_code);
        i = i+1;
    }
    clres = collect_closures(ast);
    clmaps = plant_list_get(clres, 0);
    cllist = plant_list_get(clres, 1);
    tx_t cltype_code = "";
    tx_t clfwd_code = "";
    tx_t cldef_code = "";
    tx_t enum_code = "";
    i = 0;
    while (i < plant_array_length(cllist)) {
        cnode = plant_list_get(cllist, i);
        tc = _cl_emit_typedef(cnode);
        cltype_code = _cat(_cat(cltype_code, tc), "");
        fnn = _map_get(cnode, "fnname");
        cprm = _map_get(cnode, "params");
        pstr2 = _cl_param_str(cprm);
        clfwd_code = _cat(_cat(_cat(clfwd_code, "tx_t "), fnn), "(tx_t env");
        if (strcmp(pstr2,"") > 0) {
            clfwd_code = _cat(_cat(clfwd_code, ", "), pstr2);
        }
        clfwd_code = _cat(clfwd_code, ");\n");
        fd = _cl_emit_fn(cnode, sigs, esub, eregs);
        cldef_code = _cat(_cat(cldef_code, fd), "");
        i = i+1;
    }
    PlantArray* implicit = collect_implicit ( ast , plant_list_make ( 0 ) );
    tx_t dcode = "";
    long di = 0;
    tx_t dv = "";
    while (di < plant_array_length(implicit)) {
        dv = plant_list_get(implicit, di);
        dcode = _cat(_cat(_cat(dcode, "  tx_t "), dv), " = \"\";\n");
        di = di+1;
    }
    long drn_main = async_reachable ( ast );
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"action_decl") == 0) {
            PlantArray* gens4 = _map_get ( node_el , "generics" );
            if (plant_array_length(gens4) == 0) {
                aname = _map_get(node_el, "name");
                asy4 = _map_get(node_el, "async");
                tx_t node_el2 = node_el;
                if (strcmp(str_eq ( aname , "main" ),"1") == 0 && drn_main == 1 && strcmp(asy4,"1") != 0) {
                    node_el2 = map_add(node_el, "drain_after", "1");
                }
                cmact = _cl_map_get(clmaps, aname);
                nd_code = generate_node(node_el2, 0, sigs, esub, cmact, "", plant_list_make ( 0 ), plant_list_make ( 0 ), eregs);
                decl_code = _cat(decl_code, nd_code);
                has_decl = 1;
            }
        }
        if (strcmp(ntype,"enum_decl") == 0) {
            nd_code = generate_node(node_el, 0, sigs, esub, plant_list_make ( 0 ), "", plant_list_make ( 0 ), plant_list_make ( 0 ), eregs);
            enum_code = _cat(enum_code, nd_code);
            has_decl = 1;
        }
        if (strcmp(ntype,"action_decl") != 0 && strcmp(ntype,"enum_decl") != 0 && strcmp(ntype,"external_decl") != 0 && strcmp(ntype,"struct_decl") != 0) {
            ns_code = generate_node(node_el, 0, sigs, esub, plant_list_make ( 0 ), "", plant_list_make ( 0 ), plant_list_make ( 0 ), eregs);
            stmt_code = _cat(stmt_code, ns_code);
            has_stmt = 1;
        }
        i = i+1;
    }
    i = 0;
    while (i < plant_array_length(insts)) {
        node_el = plant_list_get(insts, i);
        nd_code = emit_inst(node_el, templates, sigs, eregs);
        decl_code = _cat(decl_code, nd_code);
        has_decl = 1;
        i = i+1;
    }
    if (has_stmt) {
        stmt_code = _cat(_cat(_cat("int main(int argc, char **argv) {\n  plant_init_cli(argc, argv);\n", dcode), stmt_code), "  plant_async_drain();\n  return 0;\n}\n");
    }
    return _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(header, struct_code), ffi_topo), enum_code), cltype_code), pro_code), cb_code), clfwd_code), "\n"), cldef_code), "\n"), decl_code), stmt_code);
}
tx_t _cl_is_arg(tx_t arg) {
  tx_t pre = "";
    pre = substring(arg, 0, 11);
    if (strcmp(pre,"@@CLOSURE@@") == 0) {
        return "1";
    }
    return "0";
}
tx_t _cl_map_get(PlantArray* clmap, tx_t key) {
  tx_t mret = "";
    long mi2 = 0;
    tx_t me2 = "";
    while (mi2 < plant_array_length(clmap)) {
        me2 = plant_list_get(clmap, mi2);
        if (strcmp(str_eq ( me2 , key ),"1") == 0) {
            mret = plant_list_get(clmap, mi2+1);
            return mret;
        }
        mi2 = mi2+2;
    }
    return "";
}
tx_t _cl_scopes(PlantArray* bd, PlantArray* scopes) {
  tx_t sty = "";
  tx_t stg = "";
  tx_t stv = "";
  tx_t stc = "";
  tx_t oldc = "";
  tx_t iv = "";
  tx_t le = "";
  tx_t mcl = "";
  tx_t mb = "";
    long n2 = 0;
    tx_t st = "";
    PlantArray* res = scopes;
    while (n2 < plant_array_length(bd)) {
        st = plant_list_get(bd, n2);
        sty = _map_get(st, "type");
        if (strcmp(sty,"create_stmt") == 0) {
            stg = _map_get(st, "target");
            stv = _map_get(st, "var_type");
            stc = plant_ctype(stv);
            res = plant_list_push(res, stg);
            res = plant_list_push(res, stc);
            res = plant_list_push(res, _cat(stg, "#t"));
            res = plant_list_push(res, stv);
        }
        if (strcmp(sty,"let_stmt") == 0) {
            stg = _map_get(st, "target");
            stv = _map_get(st, "var_type");
            stc = plant_ctype(stv);
            res = plant_list_push(res, stg);
            res = plant_list_push(res, stc);
            res = plant_list_push(res, _cat(stg, "#t"));
            res = plant_list_push(res, stv);
        }
        if (strcmp(sty,"reap_stmt") == 0) {
            stg = _map_get(st, "target");
            if (strcmp(stg,"_") != 0) {
                oldc = _cl_map_get(res, stg);
                if (strcmp(oldc,"") == 0) {
                    res = plant_list_push(res, stg);
                    res = plant_list_push(res, "tx_t");
                }
            }
        }
        if (strcmp(sty,"cycle_stmt") == 0) {
            iv = _map_get(st, "iterVar");
            le = _map_get(st, "listExpr");
            if (strcmp(le,"") > 0) {
                res = plant_list_push(res, iv);
                res = plant_list_push(res, "tx_t");
            }
            if (strcmp(le,"") == 0) {
                res = plant_list_push(res, iv);
                res = plant_list_push(res, "long");
            }
        }
        if (strcmp(sty,"match_stmt") == 0) {
            mcl = _map_get(st, "clauses");
            long mn2 = 0;
            tx_t mel = "";
            while (mn2 < plant_array_length(mcl)) {
                mel = plant_list_get(mcl, mn2);
                mb = _map_get(mel, "binding");
                if (strcmp(mb,"") > 0 && strcmp(mb,"null") != 0) {
                    res = plant_list_push(res, mb);
                    res = plant_list_push(res, "tx_t");
                }
                mn2 = mn2+1;
            }
        }
        n2 = n2+1;
    }
    return res;
}
tx_t _cl_stamp_cnode(PlantArray* cnode, PlantArray* scopes, long cc, PlantArray* res) {
  tx_t cnp = "";
  tx_t cnb = "";
  tx_t ccap = "";
  tx_t cpn3 = "";
  tx_t cpt3 = "";
  tx_t cct3 = "";
  tx_t capn3 = "";
  tx_t capm3 = "";
  tx_t capct = "";
  tx_t capty = "";
  tx_t cnk = "";
  tx_t cres = "";
    cnp = _map_get(cnode, "params");
    cnb = _map_get(cnode, "body");
    ccap = _map_get(cnode, "captures");
    tx_t cid3 = _from_long ( cc );
    tx_t envn3 = _cat("plant_Env_", cid3);
    tx_t fnn3 = _cat(_cat("plant_Closure_", cid3), "_fn");
    PlantArray* capsflat = plant_list_make ( 0 );
    PlantArray* moved3 = plant_list_make ( 0 );
    PlantArray* shads3 = plant_list_make ( 0 );
    PlantArray* cpsc = plant_list_make ( 0 );
    long cpi3 = 0;
    tx_t cpe3 = "";
    while (cpi3 < plant_array_length(cnp)) {
        cpe3 = plant_list_get(cnp, cpi3);
        cpn3 = _map_get(cpe3, "name");
        cpt3 = _map_get(cpe3, "type");
        cct3 = plant_ctype(cpt3);
        cpsc = plant_list_push(cpsc, cpn3);
        cpsc = plant_list_push(cpsc, cct3);
        cpsc = plant_list_push(cpsc, _cat(cpn3, "#t"));
        cpsc = plant_list_push(cpsc, cpt3);
        cpi3 = cpi3+1;
    }
    long cci3 = 0;
    tx_t cce3 = "";
    while (cci3 < plant_array_length(ccap)) {
        cce3 = plant_list_get(ccap, cci3);
        capn3 = _map_get(cce3, "name");
        capm3 = _map_get(cce3, "mode");
        capct = _cl_map_get(scopes, capn3);
        if (strcmp(capct,"") == 0) {
            capct = "tx_t";
        }
        capty = _cl_map_get(scopes, _cat(capn3, "#t"));
        if (strcmp(capty,"") == 0) {
            capty = capct;
        }
        if (strcmp(str_eq ( capm3 , "MOVE" ),"1") == 0) {
            capsflat = plant_list_push(capsflat, capn3);
            capsflat = plant_list_push(capsflat, capn3);
            moved3 = plant_list_push(moved3, capn3);
        }
        if (strcmp(str_eq ( capm3 , "REF" ),"1") == 0) {
            capsflat = plant_list_push(capsflat, capn3);
            capsflat = plant_list_push(capsflat, _cat("&", capn3));
        }
        shads3 = plant_list_push(shads3, plant_list_make ( 8 , "name" , capn3 , "ctype" , capct , "mode" , capm3 , "ptype" , capty ));
        cpsc = plant_list_push(cpsc, capn3);
        cpsc = plant_list_push(cpsc, capct);
        cpsc = plant_list_push(cpsc, _cat(capn3, "#t"));
        cpsc = plant_list_push(cpsc, capty);
        cci3 = cci3+1;
    }
    PlantArray* cmap3 = plant_list_make ( 0 );
    long cck = cc+1;
    cnk = _map_get(cnode, "bkind");
    if (strcmp(str_eq ( cnk , "block" ),"1") == 0) {
        cres = _cl_walk(cnb, cpsc, res, plant_list_make ( 0 ), cc+1);
        res = plant_list_get(cres, 0);
        cmap3 = plant_list_get(cres, 1);
        cck = plant_list_get(cres, 2);
    }
    cnode = plant_list_push(cnode, "cid");
    cnode = plant_list_push(cnode, cid3);
    cnode = plant_list_push(cnode, "envname");
    cnode = plant_list_push(cnode, envn3);
    cnode = plant_list_push(cnode, "fnname");
    cnode = plant_list_push(cnode, fnn3);
    cnode = plant_list_push(cnode, "clcaps");
    cnode = plant_list_push(cnode, capsflat);
    cnode = plant_list_push(cnode, "moved");
    cnode = plant_list_push(cnode, moved3);
    cnode = plant_list_push(cnode, "shadows");
    cnode = plant_list_push(cnode, shads3);
    cnode = plant_list_push(cnode, "clmap");
    cnode = plant_list_push(cnode, cmap3);
    res = plant_list_push(res, cnode);
    return plant_list_make ( 2 , res , cck );
}
tx_t _cl_walk(PlantArray* bd, PlantArray* scopes, PlantArray* clseq, PlantArray* clmap, long cid) {
  tx_t sty3 = "";
  tx_t sc3 = "";
  tx_t cnode3 = "";
  tx_t tgt3 = "";
  tx_t cres = "";
  tx_t cla3 = "";
  tx_t bd3 = "";
  tx_t scn = "";
  tx_t mcl3 = "";
  tx_t mb3 = "";
  tx_t scm = "";
    long n3 = 0;
    tx_t st3 = "";
    PlantArray* res = clseq;
    PlantArray* rmap = clmap;
    long cc = cid;
    while (n3 < plant_array_length(bd)) {
        st3 = plant_list_get(bd, n3);
        sty3 = _map_get(st3, "type");
        sc3 = _cl_scopes(plant_list_make ( 1 , st3 ), scopes);
        cnode3 = _map_get(st3, "closure");
        if (strcmp(cnode3,"") > 0) {
            tgt3 = _map_get(st3, "target");
            cres = _cl_stamp_cnode(cnode3, sc3, cc, res);
            res = plant_list_get(cres, 0);
            cc = plant_list_get(cres, 1);
            rmap = plant_list_push(rmap, tgt3);
            rmap = plant_list_push(rmap, cnode3);
        }
        if (strcmp(sty3,"reap_stmt") == 0) {
            cla3 = _map_get(st3, "clargs");
            if (strcmp(cla3,"") > 0) {
                long clai = 0;
                tx_t clae = "";
                while (clai < plant_array_length(cla3)) {
                    clae = plant_list_get(cla3, clai);
                    cres = _cl_stamp_cnode(clae, sc3, cc, res);
                    res = plant_list_get(cres, 0);
                    cc = plant_list_get(cres, 1);
                    clai = clai+1;
                }
            }
        }
        bd3 = _map_get(st3, "body");
        if (strcmp(bd3,"") > 0) {
            scn = _cl_scopes(bd3, sc3);
            cres = _cl_walk(bd3, scn, res, rmap, cc);
            res = plant_list_get(cres, 0);
            rmap = plant_list_get(cres, 1);
            cc = plant_list_get(cres, 2);
        }
        if (strcmp(sty3,"match_stmt") == 0) {
            mcl3 = _map_get(st3, "clauses");
            long mn3 = 0;
            tx_t mel3 = "";
            while (mn3 < plant_array_length(mcl3)) {
                mel3 = plant_list_get(mcl3, mn3);
                mb3 = _map_get(mel3, "bodyStatements");
                scm = _cl_scopes(mb3, sc3);
                cres = _cl_walk(mb3, scm, res, rmap, cc);
                res = plant_list_get(cres, 0);
                rmap = plant_list_get(cres, 1);
                cc = plant_list_get(cres, 2);
                mn3 = mn3+1;
            }
        }
        n3 = n3+1;
    }
    return plant_list_make ( 3 , res , rmap , cc );
}
tx_t collect_closures(PlantArray* ast) {
  tx_t aty = "";
  tx_t agen = "";
  tx_t aname5 = "";
  tx_t apms5 = "";
  tx_t abd5 = "";
  tx_t apsc = "";
  tx_t apn6 = "";
  tx_t apt6 = "";
  tx_t act6 = "";
  tx_t imp5 = "";
  tx_t cres = "";
  tx_t cmap5 = "";
    PlantArray* clmaps = plant_list_make ( 0 );
    PlantArray* cllist = plant_list_make ( 0 );
    long ai5 = 0;
    tx_t ae5 = "";
    while (ai5 < plant_array_length(ast)) {
        ae5 = plant_list_get(ast, ai5);
        aty = _map_get(ae5, "type");
        if (strcmp(aty,"action_decl") == 0) {
            agen = _map_get(ae5, "generics");
            if (plant_array_length(agen) == 0) {
                aname5 = _map_get(ae5, "name");
                apms5 = _map_get(ae5, "params");
                abd5 = _map_get(ae5, "body");
                apsc = _cl_scopes(abd5, plant_list_make ( 0 ));
                long ai6 = 0;
                tx_t ae6 = "";
                while (ai6 < plant_array_length(apms5)) {
                    ae6 = plant_list_get(apms5, ai6);
                    apn6 = _map_get(ae6, "name");
                    apt6 = _map_get(ae6, "type");
                    act6 = plant_ctype(apt6);
                    apsc = plant_list_push(apsc, apn6);
                    apsc = plant_list_push(apsc, act6);
                    apsc = plant_list_push(apsc, _cat(apn6, "#t"));
                    apsc = plant_list_push(apsc, apt6);
                    ai6 = ai6+1;
                }
                imp5 = collect_implicit(abd5, apms5);
                long di5 = 0;
                tx_t dv5 = "";
                while (di5 < plant_array_length(imp5)) {
                    dv5 = plant_list_get(imp5, di5);
                    apsc = plant_list_push(apsc, dv5);
                    apsc = plant_list_push(apsc, "tx_t");
                    di5 = di5+1;
                }
                long cseedd = plant_array_length(cllist);
                cres = _cl_walk(abd5, apsc, cllist, plant_list_make ( 0 ), cseedd);
                cllist = plant_list_get(cres, 0);
                cmap5 = plant_list_get(cres, 1);
                clmaps = plant_list_push(clmaps, aname5);
                clmaps = plant_list_push(clmaps, cmap5);
            }
        }
        ai5 = ai5+1;
    }
    return plant_list_make ( 2 , clmaps , cllist );
}
tx_t _cl_param_str(PlantArray* params) {
  tx_t pn3 = "";
  tx_t pt3 = "";
  tx_t ct3 = "";
    tx_t pstr = "";
    long pi3 = 0;
    tx_t pe3 = "";
    while (pi3 < plant_array_length(params)) {
        pe3 = plant_list_get(params, pi3);
        pn3 = _map_get(pe3, "name");
        pt3 = _map_get(pe3, "type");
        ct3 = plant_ctype(pt3);
        if (pi3 > 0) {
            pstr = _cat(pstr, ", ");
        }
        pstr = _cat(_cat(_cat(pstr, ct3), " "), pn3);
        pi3 = pi3+1;
    }
    return pstr;
}
tx_t _cl_emit_typedef(PlantArray* cnode) {
  tx_t envn = "";
  tx_t shads = "";
  tx_t sct4 = "";
  tx_t sname4 = "";
    envn = _map_get(cnode, "envname");
    shads = _map_get(cnode, "shadows");
    tx_t tc = "typedef struct {\n";
    long si4 = 0;
    tx_t se4 = "";
    tx_t sm4 = "";
    while (si4 < plant_array_length(shads)) {
        se4 = plant_list_get(shads, si4);
        sm4 = _map_get(se4, "mode");
        sct4 = _map_get(se4, "ctype");
        sname4 = _map_get(se4, "name");
        if (strcmp(str_eq ( sm4 , "MOVE" ),"1") == 0) {
            tc = _cat(_cat(_cat(_cat(_cat(tc, "  "), sct4), " "), sname4), ";\n");
        }
        if (strcmp(str_eq ( sm4 , "REF" ),"1") == 0) {
            tc = _cat(_cat(_cat(tc, "  tx_t "), sname4), ";\n");
        }
        si4 = si4+1;
    }
    tc = _cat(_cat(_cat(tc, "} "), envn), ";\n");
    return tc;
}
tx_t _cl_emit_fn(PlantArray* cnode, PlantArray* sigs, PlantArray* subst, PlantArray* reg) {
  tx_t fname = "";
  tx_t envn2 = "";
  tx_t params = "";
  tx_t bk = "";
  tx_t shads = "";
  tx_t cm2 = "";
  tx_t pstr = "";
  tx_t snameQ = "";
  tx_t bx = "";
  tx_t cx3 = "";
  tx_t bb = "";
  tx_t imp6 = "";
  tx_t bc3 = "";
    fname = _map_get(cnode, "fnname");
    envn2 = _map_get(cnode, "envname");
    params = _map_get(cnode, "params");
    bk = _map_get(cnode, "bkind");
    shads = _map_get(cnode, "shadows");
    cm2 = _map_get(cnode, "clmap");
    pstr = _cl_param_str(params);
    PlantArray* cbpars2 = plant_list_make ( 0 );
    long cbi = 0;
    tx_t cbe = "";
    while (cbi < plant_array_length(params)) {
        cbe = plant_list_get(params, cbi);
        cbpars2 = plant_list_push(cbpars2, cbe);
        cbi = cbi+1;
    }
    long cbsi = 0;
    tx_t cbse = "";
    tx_t cbsn = "";
    tx_t cbty = "";
    while (cbsi < plant_array_length(shads)) {
        cbse = plant_list_get(shads, cbsi);
        cbsn = _map_get(cbse, "name");
        cbty = _map_get(cbse, "ptype");
        cbpars2 = plant_list_push(cbpars2, plant_list_make ( 4 , "name" , cbsn , "type" , cbty ));
        cbsi = cbsi+1;
    }
    tx_t fnc = _cat(_cat("tx_t ", fname), "(tx_t env");
    if (strcmp(pstr,"") > 0) {
        fnc = _cat(_cat(fnc, ", "), pstr);
    }
    fnc = _cat(fnc, ") {\n");
    long si5 = 0;
    tx_t se5 = "";
    tx_t sm5 = "";
    tx_t sct5 = "";
    while (si5 < plant_array_length(shads)) {
        se5 = plant_list_get(shads, si5);
        sm5 = _map_get(se5, "mode");
        sct5 = _map_get(se5, "ctype");
        snameQ = _map_get(se5, "name");
        if (strcmp(str_eq ( sm5 , "MOVE" ),"1") == 0) {
            fnc = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(fnc, "  "), sct5), " "), snameQ), " = (("), envn2), "*)env)->"), snameQ), ";\n");
        }
        if (strcmp(str_eq ( sm5 , "REF" ),"1") == 0) {
            fnc = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(fnc, "  "), sct5), " "), snameQ), " = *(( "), sct5), "*)(("), envn2), "*)env)->"), snameQ), ");\n");
        }
        si5 = si5+1;
    }
    if (strcmp(str_eq ( bk , "expr" ),"1") == 0) {
        bx = _map_get(cnode, "body");
        cx3 = translate_expr(bx);
        PlantArray* nums_c = collect_nums_cb ( plant_list_make ( 0 ) , params , shads , subst );
        PlantArray* evars_c = collect_enums ( plant_list_make ( 0 ) , cbpars2 , subst , reg );
        cx3 = _handle_cat(cx3, nums_c, evars_c);
        fnc = _cat(_cat(_cat(fnc, "  return "), cx3), ";\n");
    }
    if (strcmp(str_eq ( bk , "block" ),"1") == 0) {
        bb = _map_get(cnode, "body");
        PlantArray* nums_c = collect_nums_cb ( bb , params , shads , subst );
        PlantArray* stvars_c = collect_stvars_walk ( bb , subst , plant_list_make ( 0 ) );
        PlantArray* cbpars = params;
        long si6 = 0;
        tx_t se6 = "";
        while (si6 < plant_array_length(shads)) {
            se6 = plant_list_get(shads, si6);
            cbpars = plant_list_push(cbpars, se6);
            si6 = si6+1;
        }
        imp6 = collect_implicit(bb, cbpars);
        long di6 = 0;
        tx_t dv6 = "";
        while (di6 < plant_array_length(imp6)) {
            dv6 = plant_list_get(imp6, di6);
            fnc = _cat(_cat(_cat(fnc, "  tx_t "), dv6), " = \"\";\n");
            di6 = di6+1;
        }
        PlantArray* evars_c = collect_enums ( bb , cbpars2 , subst , reg );
        bc3 = generate_body(bb, 1, sigs, subst, cm2, "", nums_c, stvars_c, evars_c);
        fnc = _cat(_cat(fnc, bc3), "");
    }
    fnc = _cat(fnc, "}\n");
    return fnc;
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  tx_t arg0 = "";
  tx_t source_path = "";
  tx_t exists = "";
  tx_t source_text = "";
  tx_t tokens = "";
  tx_t program_ast = "";
  tx_t body = "";
  tx_t c_code = "";
  tx_t out_path = "";
  tx_t written = "";
  tx_t c_len = "";
  arg0 = get_cli_arg(0);
  if (strcmp(arg0,"-h") == 0 || strcmp(arg0,"--help") == 0) {
      plant_print("Chloroplast — Pure Native PlantLang compiler");
      plant_print("usage: Chloroplast <source.plant> [out.c]");
      plant_print("options:");
      plant_print("  -h, --help     show this help and exit");
      plant_print("  -v, --version  show version and exit");
      return 0;
  }
  if (strcmp(arg0,"-v") == 0 || strcmp(arg0,"--version") == 0) {
      plant_print("Chloroplast 0.48.9 (pure native)");
      return 0;
  }
  source_path = get_cli_arg(0);
  plant_print(_cat("input: ", source_path));
  exists = fs_EXISTS(source_path);
  if (strcmp(exists,"1") != 0) {
      plant_print(_cat("Error: file not found — ", source_path));
      return 1;
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
  if (strcmp(out_path,"") == 0) {
      out_path = strings_REPLACE(source_path, ".plant", ".c");
  }
  written = fs_WRITE(out_path, c_code);
  c_len = strings_LENGTH(c_code);
  plant_print(_cat(_cat(_cat("output: ", c_len), " bytes to "), out_path));
  plant_async_drain();
  return 0;
}
