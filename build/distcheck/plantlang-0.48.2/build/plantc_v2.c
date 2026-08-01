#include <plant_compat.h>

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
tx_t parse_create_stmt(PlantArray* tokens, long pos);
tx_t parse_show_stmt(PlantArray* tokens, long pos);
tx_t parse_give_stmt(PlantArray* tokens, long pos);
tx_t parse_set_stmt(PlantArray* tokens, long pos);
tx_t parse_let_stmt(PlantArray* tokens, long pos);
tx_t parse_closure(PlantArray* tokens, long pos);
tx_t parse_reap_stmt(PlantArray* tokens, long pos);
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
tx_t parse_program(PlantArray* tokens);
tx_t _substr(tx_t str, long start, long end);
tx_t _handle_func(tx_t expr, tx_t kw, tx_t cfn);
tx_t _handle_func_paren(tx_t expr, tx_t kw, tx_t cfn);
tx_t _handle_cat(tx_t expr);
tx_t collect_declared_walk(PlantArray* bd, PlantArray* declared);
tx_t collect_used_walk(PlantArray* bd, PlantArray* used, PlantArray* declared);
tx_t collect_implicit(PlantArray* bd, PlantArray* params);
tx_t translate_expr(tx_t expr);
tx_t indent_str(long level);
tx_t generate_body(PlantArray* bd, long indent, PlantArray* sigs, PlantArray* subst, PlantArray* clmap);
tx_t generate_node(tx_t node, long indent, PlantArray* sigs, PlantArray* subst, PlantArray* clmap);
tx_t type_base(tx_t ptype);
tx_t plant_ctype(tx_t ptype);
tx_t trim(tx_t s);
tx_t subst_append(tx_t acc, tx_t w, PlantArray* subst);
tx_t subst_type(tx_t t, PlantArray* subst);
tx_t subst_reap_act(tx_t act, PlantArray* subst);
tx_t base_of(tx_t act);
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
tx_t emit_inst(tx_t inst, PlantArray* templates, PlantArray* sigs);
tx_t find_params(PlantArray* sigs, tx_t name);
tx_t is_ref_param(tx_t ptype);
tx_t is_ref_at(PlantArray* params, long idx);
tx_t generate_c(PlantArray* ast);
tx_t _cl_is_arg(tx_t arg);
tx_t _cl_map_get(PlantArray* clmap, tx_t key);
tx_t _cl_scopes(PlantArray* bd, PlantArray* scopes);
tx_t _cl_stamp_cnode(PlantArray* cnode, PlantArray* scopes, long cc, PlantArray* res);
tx_t _cl_walk(PlantArray* bd, PlantArray* scopes, PlantArray* clseq, PlantArray* clmap, long cid);
tx_t collect_closures(PlantArray* ast);
tx_t _cl_param_str(PlantArray* params);
tx_t _cl_emit_typedef(PlantArray* cnode);
tx_t _cl_emit_fn(PlantArray* cnode, PlantArray* sigs, PlantArray* subst);

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
  tx_t lt_tok = "";
  tx_t lt_lx = "";
  tx_t lt_pair = "";
  tx_t rt_pair = "";
  tx_t rc_pair = "";
  tx_t re_pair = "";
  tx_t gt_pair = "";
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
    while (1) {
        is_eof_flag = is_eof(tokens, p4);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "action_decl" , "name" , aname , "generics" , generics , "params" , params , "body" , plant_list_make ( 0 ) ) , p4 );
        }
        tok = peek(tokens, p4);
        lx = tok_lex(tok);
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
        after_tok = peek(tokens, p5);
        after_lx = tok_lex(after_tok);
        if (strcmp(after_lx,",") == 0) {
            com_pair = consume(tokens, p5);
            p5 = _second(com_pair);
        }
        if (strcmp(after_lx,".") == 0) {
            dot_pair = consume(tokens, p5);
            p5 = _second(dot_pair);
            if (strcmp(ret_lx,"external") == 0) {
                return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "external_decl" , "name" , aname , "params" , params ) , p5 );
            }
            if (strcmp(ret_lx,"Result") == 0) {
                return plant_list_make ( 2 , plant_list_make ( 6 , "type" , "external_decl" , "name" , aname , "params" , params ) , p5 );
            }
            return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "action_decl" , "name" , aname , "generics" , generics , "params" , params , "body" , plant_list_make ( 0 ) ) , p5 );
        }
    }
    PlantArray* body = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p5);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "action_decl" , "name" , aname , "generics" , generics , "params" , params , "body" , body ) , p5 );
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
            return plant_list_make ( 2 , plant_list_make ( 10 , "type" , "action_decl" , "name" , aname , "generics" , generics , "params" , params , "body" , body ) , p8 );
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
    r = parse_statement(tokens, pos);
    return r;
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
    parts = strings_SPLIT(expr, _cat(kw, " "));
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
    parts = strings_SPLIT(expr, _cat(kw, " ("));
    if (plant_array_length(parts) == 1) {
        parts = strings_SPLIT(expr, _cat(kw, "("));
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
tx_t _handle_cat(tx_t expr) {
  tx_t isdigit = "";
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
    if (( plant_array_length(parts) ) == 0) {
        return expr;
    }
    if (has_str == 0 && has_digit == 1) {
        return strings_REPLACE ( expr , " + " , "+" );
    }
    seg = substring(expr, start, strlen( expr ));
    parts = plant_list_push(parts, seg);
    PlantArray* nparts = plant_list_make ( 0 );
    long ni = 0;
    tx_t pel2 = "";
    tx_t pel2b = "";
    long pd = 0;
    long pj = 0;
    tx_t pc = "";
    while (ni < plant_array_length(parts)) {
        pel2 = plant_list_get(parts, ni);
        pel2b = strings_REPLACE(pel2, " ", "");
        pd = 1;
        if (strcmp(pel2b,"") == 0) {
            pd = 0;
        }
        pj = 0;
        while (pj < strlen( pel2b )) {
            pc = char_at(pel2b, pj);
            if (strcmp(pc,"0") != 0 && strcmp(pc,"1") != 0 && strcmp(pc,"2") != 0 && strcmp(pc,"3") != 0 && strcmp(pc,"4") != 0 && strcmp(pc,"5") != 0 && strcmp(pc,"6") != 0 && strcmp(pc,"7") != 0 && strcmp(pc,"8") != 0 && strcmp(pc,"9") != 0) {
                pd = 0;
            }
            pj = pj+1;
        }
        if (pd == 1) {
            pel2 = _cat(_cat("_from_long(", pel2), ")");
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
tx_t translate_expr(tx_t expr) {
    tx_t e = expr;
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
    e = strings_REPLACE(e, " : ", "_");
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
tx_t generate_body(PlantArray* bd, long indent, PlantArray* sigs, PlantArray* subst, PlantArray* clmap) {
  tx_t node_code = "";
    tx_t res = "";
    long i = 0;
    tx_t node_el = "";
    while (i < plant_array_length(bd)) {
        node_el = plant_list_get(bd, i);
        node_code = generate_node(node_el, indent, sigs, subst, clmap);
        if (strcmp(node_code,"") > 0) {
            res = _cat(res, node_code);
        }
        i = i+1;
    }
    return res;
}
tx_t generate_node(tx_t node, long indent, PlantArray* sigs, PlantArray* subst, PlantArray* clmap) {
  tx_t ntype = "";
  tx_t val = "";
  tx_t cval = "";
  tx_t isel = "";
  tx_t target = "";
  tx_t vtype = "";
  tx_t cnd = "";
  tx_t envname = "";
  tx_t caps = "";
  tx_t moved = "";
  tx_t cid2 = "";
  tx_t item = "";
  tx_t citem = "";
  tx_t tgt = "";
  tx_t act = "";
  tx_t sact = "";
  tx_t base = "";
  tx_t gargs = "";
  tx_t mname = "";
  tx_t cvar = "";
  tx_t cfn = "";
  tx_t arg0 = "";
  tx_t rp = "";
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
  tx_t pname = "";
  tx_t ptype = "";
  tx_t ctype = "";
  tx_t ename = "";
    ntype = _map_get(node, "type");
    if (strcmp(clmap,"") == 0) {
        clmap = plant_list_make ( 0 );
    }
    if (strcmp(ntype,"show_stmt") == 0) {
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval);
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
        cval = _handle_cat(cval);
        if (strcmp(vtype,"NUM") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  long "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"FACT") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  int "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"LIST") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  PlantArray* "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"NUM") != 0 && strcmp(vtype,"FACT") != 0 && strcmp(vtype,"LIST") != 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  tx_t "), target), " = "), cval), ";\n");
        }
    }
    if (strcmp(ntype,"set_stmt") == 0) {
        target = _map_get(node, "target");
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval);
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
        cval = _handle_cat(cval);
        if (strcmp(vtype,"NUM") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  long "), target), " = "), cval), ";\n");
        }
        if (strcmp(vtype,"NUM") != 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  tx_t "), target), " = "), cval), ";\n");
        }
    }
    if (strcmp(ntype,"give_stmt") == 0) {
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval);
        isel = indent_str(indent);
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
        citem = _handle_cat(citem);
        isel = indent_str(indent);
        return _cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  "), target), " = plant_list_push("), target), ", "), citem), ");\n");
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
                    clpre = _cat(_cat(_cat(_cat(clpre, isel), "  tx_t __carg_"), ai), ";\n");
                    clpre = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, isel), "  { "), envname), "* __env_"), ai), " = ("), envname), "*)plant_env_alloc(sizeof("), envname), "));\n");
                    long cpi = 0;
                    tx_t capn = "";
                    tx_t capi = "";
                    while (cpi + 1 < plant_array_length(caps)) {
                        capn = plant_list_get(caps, cpi);
                        capi = plant_list_get(caps, cpi+1);
                        clpre = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, isel), "    __env_"), ai), "->"), capn), " = "), capi), ";\n");
                        cpi = cpi+2;
                    }
                    clpre = _cat(_cat(_cat(_cat(_cat(_cat(clpre, isel), "    __carg_"), ai), " = (tx_t)__env_"), ai), ";\n");
                    clpre = _cat(_cat(clpre, isel), "  }\n");
                    long cmi = 0;
                    tx_t cmv = "";
                    while (cmi < plant_array_length(moved)) {
                        cmv = plant_list_get(moved, cmi);
                        clclears = _cat(_cat(_cat(_cat(clclears, isel), "  "), cmv), " = 0;\n");
                        cmi = cmi+1;
                    }
                    aexpr = _cat("__carg_", ai);
                }
                if (strcmp(cndok,"0") == 0) {
                    aexpr = arg_el;
                }
            }
            if (strcmp(cl_ok,"0") == 0) {
                arg0 = substring(arg_el, 0, 1);
                if (strcmp(arg0,"\"") != 0) {
                    aexpr = translate_expr(arg_el);
                    aexpr = _handle_cat(aexpr);
                }
                rp = is_ref_at(fparams, ai);
                if (strcmp(rp,"1") == 0) {
                    aexpr = _cat("&", aexpr);
                }
            }
            if (ai > 0) {
                argstr = _cat(argstr, ", ");
            }
            argstr = _cat(argstr, aexpr);
            ai = ai+1;
        }
        if (strcmp(tgt,"_") == 0) {
            return _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, isel), "  "), callname), "("), clcall), argstr), ");\n"), clclears);
        }
        if (strcmp(tgt,"_") != 0) {
            return _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(clpre, isel), "  "), tgt), " = "), callname), "("), clcall), argstr), ");\n"), clclears);
        }
    }
    if (strcmp(ntype,"if_stmt") == 0) {
        cond = _map_get(node, "cond");
        bd = _map_get(node, "body");
        ccond = translate_expr(cond);
        ccond = handle_strcmp(ccond);
        isel = indent_str(indent);
        tx_t ccode = _cat(_cat(_cat(isel, "  if ("), ccond), ") {\n");
        bcode = generate_body(bd, indent+2, sigs, subst, clmap);
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
        bcode = generate_body(bd, indent+2, sigs, subst, clmap);
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
            bcode = generate_body(bd, indent+2, sigs, subst, clmap);
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
            bcode = generate_body(bd, indent+4, sigs, subst, clmap);
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
                bcode = generate_body(cbody, indent+4, sigs, subst, clmap);
                ccode = _cat(_cat(_cat(ccode, bcode), isel), "      }\n");
            }
            if (strcmp(binding,"") == 0 || strcmp(binding,"null") == 0) {
                bcode = generate_body(cbody, indent+4, sigs, subst, clmap);
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
            tx_t paramstr = "";
            long pi = 0;
            tx_t param_el = "";
            while (pi < plant_array_length(params)) {
                param_el = plant_list_get(params, pi);
                pname = _map_get(param_el, "name");
                ptype = _map_get(param_el, "type");
                ctype = plant_ctype(ptype);
                if (pi > 0) {
                    paramstr = _cat(paramstr, ", ");
                }
                paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
                pi = pi+1;
            }
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
            bcode = generate_body(bd, 1, sigs, subst, clmap);
            if (( plant_array_length(bd) ) == 0) {
                bcode = _cat(_cat("  return ", aname), ";\n");
            }
            if (( plant_array_length(bd) ) > 0) {
                long bd_count = plant_array_length(bd);
                long last_idx = bd_count - 1;
                tx_t last_nd = plant_list_get ( bd , last_idx );
                tx_t last_ty = _map_get ( last_nd , "type" );
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
    parts = strings_SPLIT(inner, ",");
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
        ctype = plant_ctype(fsub);
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
        ctype = plant_ctype(psub);
        if (pi > 0) {
            paramstr = _cat(paramstr, ", ");
        }
        paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
        pi = pi+1;
    }
    return _cat(_cat(_cat(_cat("tx_t ", mname), "("), paramstr), ");\n");
}
tx_t emit_inst(tx_t inst, PlantArray* templates, PlantArray* sigs) {
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
        ctype = plant_ctype(psub);
        if (pi > 0) {
            paramstr = _cat(paramstr, ", ");
        }
        paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
        pi = pi+1;
    }
    tx_t ccode = _cat(_cat(_cat(_cat("tx_t ", mname), "("), paramstr), ") {\n");
    PlantArray* implicit = collect_implicit ( bd , params );
    tx_t dcode = "";
    long di = 0;
    tx_t dv = "";
    while (di < plant_array_length(implicit)) {
        dv = plant_list_get(implicit, di);
        dcode = _cat(_cat(_cat(dcode, "  tx_t "), dv), " = \"\";\n");
        di = di+1;
    }
    bcode = generate_body(bd, 1, sigs, subst, plant_list_make ( 0 ));
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
tx_t generate_c(PlantArray* ast) {
  tx_t ntype = "";
  tx_t sname = "";
  tx_t ibase = "";
  tx_t iargs = "";
  tx_t itpl = "";
  tx_t igns = "";
  tx_t insub = "";
  tx_t ipms = "";
  tx_t ibd = "";
  tx_t stdef = "";
  tx_t ibtrim = "";
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
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"action_decl") == 0 || strcmp(ntype,"external_decl") == 0) {
            aname = _map_get(node_el, "name");
            PlantArray* params2 = _map_get ( node_el , "params" );
            sigs = plant_list_push(sigs, plant_list_make ( 4 , "name" , aname , "params" , params2 ));
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
    tx_t struct_code = "";
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"struct_decl") == 0) {
            PlantArray* sgens4 = _map_get ( node_el , "generics" );
            if (plant_array_length(sgens4) == 0) {
                PlantArray* eargs = plant_list_make ( 0 );
                stdef = struct_typedef(node_el, eargs);
                struct_code = _cat(struct_code, stdef);
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
            stdef = struct_typedef(itpl, iargs);
            struct_code = _cat(struct_code, stdef);
        }
        i = i+1;
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
                paramstr = "";
                pi = 0;
                while (pi < plant_array_length(params3)) {
                    param_el = plant_list_get(params3, pi);
                    pname = _map_get(param_el, "name");
                    ptype = _map_get(param_el, "type");
                    ctype = plant_ctype(ptype);
                    if (pi > 0) {
                        paramstr = _cat(paramstr, ", ");
                    }
                    paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
                    pi = pi+1;
                }
                pro_code = _cat(_cat(_cat(_cat(_cat(pro_code, "tx_t "), aname), "("), paramstr), ");\n");
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
        fd = _cl_emit_fn(cnode, sigs, esub);
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
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"action_decl") == 0) {
            PlantArray* gens4 = _map_get ( node_el , "generics" );
            if (plant_array_length(gens4) == 0) {
                aname = _map_get(node_el, "name");
                cmact = _cl_map_get(clmaps, aname);
                nd_code = generate_node(node_el, 0, sigs, esub, cmact);
                decl_code = _cat(decl_code, nd_code);
                has_decl = 1;
            }
        }
        if (strcmp(ntype,"enum_decl") == 0) {
            nd_code = generate_node(node_el, 0, sigs, esub, plant_list_make ( 0 ));
            decl_code = _cat(decl_code, nd_code);
            has_decl = 1;
        }
        if (strcmp(ntype,"action_decl") != 0 && strcmp(ntype,"enum_decl") != 0 && strcmp(ntype,"external_decl") != 0 && strcmp(ntype,"struct_decl") != 0) {
            ns_code = generate_node(node_el, 0, sigs, esub, plant_list_make ( 0 ));
            stmt_code = _cat(stmt_code, ns_code);
            has_stmt = 1;
        }
        i = i+1;
    }
    i = 0;
    while (i < plant_array_length(insts)) {
        node_el = plant_list_get(insts, i);
        nd_code = emit_inst(node_el, templates, sigs);
        decl_code = _cat(decl_code, nd_code);
        has_decl = 1;
        i = i+1;
    }
    if (has_stmt) {
        stmt_code = _cat(_cat(_cat("int main(int argc, char **argv) {\n  plant_init_cli(argc, argv);\n", dcode), stmt_code), "  return 0;\n}\n");
    }
    return _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(header, struct_code), cltype_code), pro_code), clfwd_code), "\n"), cldef_code), "\n"), decl_code), stmt_code);
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
        }
        if (strcmp(sty,"let_stmt") == 0) {
            stg = _map_get(st, "target");
            stv = _map_get(st, "var_type");
            stc = plant_ctype(stv);
            res = plant_list_push(res, stg);
            res = plant_list_push(res, stc);
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
        if (strcmp(str_eq ( capm3 , "MOVE" ),"1") == 0) {
            capsflat = plant_list_push(capsflat, capn3);
            capsflat = plant_list_push(capsflat, capn3);
            moved3 = plant_list_push(moved3, capn3);
        }
        if (strcmp(str_eq ( capm3 , "REF" ),"1") == 0) {
            capsflat = plant_list_push(capsflat, capn3);
            capsflat = plant_list_push(capsflat, _cat("&", capn3));
        }
        shads3 = plant_list_push(shads3, plant_list_make ( 6 , "name" , capn3 , "ctype" , capct , "mode" , capm3 ));
        cpsc = plant_list_push(cpsc, capn3);
        cpsc = plant_list_push(cpsc, capct);
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
tx_t _cl_emit_fn(PlantArray* cnode, PlantArray* sigs, PlantArray* subst) {
  tx_t fname = "";
  tx_t envn2 = "";
  tx_t params = "";
  tx_t bk = "";
  tx_t shads = "";
  tx_t cm2 = "";
  tx_t pstr = "";
  tx_t sname5 = "";
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
        sname5 = _map_get(se5, "name");
        if (strcmp(str_eq ( sm5 , "MOVE" ),"1") == 0) {
            fnc = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(fnc, "  "), sct5), " "), sname5), " = (("), envn2), "*)env)->"), sname5), ";\n");
        }
        if (strcmp(str_eq ( sm5 , "REF" ),"1") == 0) {
            fnc = _cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(_cat(fnc, "  "), sct5), " "), sname5), " = *(( "), sct5), "*)(("), envn2), "*)env)->"), sname5), ");\n");
        }
        si5 = si5+1;
    }
    if (strcmp(str_eq ( bk , "expr" ),"1") == 0) {
        bx = _map_get(cnode, "body");
        cx3 = translate_expr(bx);
        cx3 = _handle_cat(cx3);
        fnc = _cat(_cat(_cat(fnc, "  return "), cx3), ";\n");
    }
    if (strcmp(str_eq ( bk , "block" ),"1") == 0) {
        bb = _map_get(cnode, "body");
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
        bc3 = generate_body(bb, 1, sigs, subst, cm2);
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
      plant_print("Chloroplast 0.48.2 (pure native)");
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
  return 0;
}
