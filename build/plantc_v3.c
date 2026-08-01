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
tx_t parse_create_stmt(PlantArray* tokens, long pos);
tx_t parse_show_stmt(PlantArray* tokens, long pos);
tx_t parse_give_stmt(PlantArray* tokens, long pos);
tx_t parse_set_stmt(PlantArray* tokens, long pos);
tx_t parse_let_stmt(PlantArray* tokens, long pos);
tx_t parse_reap_stmt(PlantArray* tokens, long pos);
tx_t parse_put_stmt(PlantArray* tokens, long pos);
tx_t parse_break_stmt(PlantArray* tokens, long pos);
tx_t parse_continue_stmt(PlantArray* tokens, long pos);
tx_t parse_if_stmt(PlantArray* tokens, long pos);
tx_t parse_season_stmt(PlantArray* tokens, long pos);
tx_t parse_statement(PlantArray* tokens, long pos);
tx_t parse_enum_decl(PlantArray* tokens, long pos);
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
tx_t generate_body(PlantArray* bd, long indent);
tx_t generate_node(tx_t node, long indent);
tx_t generate_c(PlantArray* ast);

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
  tx_t tp = "";
  tx_t tp_name = "";
  tx_t p5 = "";
  tx_t rp = "";
  tx_t p6 = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t eq_pair = "";
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
        tp = consume(tokens, p4);
        tp_name = tok_lex(plant_list_get(tp,  0 ));
        p5 = _second(tp);
        rp = consume(tokens, p5);
        p6 = _second(rp);
        vtype = tp_name;
        p3 = p6;
    }
    tok2 = peek(tokens, p3);
    lx2 = tok_lex(tok2);
    if (strcmp(lx2,"=") == 0) {
        eq_pair = consume(tokens, p3);
        p4 = _second(eq_pair);
        vpair = collect_value(tokens, p4);
        tx_t expr = plant_list_get(vpair,  0 );
        p5 = _second(vpair);
        return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "create_stmt" , "target" , id_name , "var_type" , vtype , "value" , expr ) , p5 );
    }
    if (strcmp(lx2,"TO") == 0) {
        to_pair = consume(tokens, p3);
        p4 = _second(to_pair);
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
  tx_t tp = "";
  tx_t tp_name = "";
  tx_t p5 = "";
  tx_t rp = "";
  tx_t p6 = "";
  tx_t tok2 = "";
  tx_t lx2 = "";
  tx_t eq_pair = "";
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
        tp = consume(tokens, p4);
        tp_name = tok_lex(plant_list_get(tp,  0 ));
        p5 = _second(tp);
        rp = consume(tokens, p5);
        p6 = _second(rp);
        vtype = tp_name;
        p3 = p6;
    }
    tok2 = peek(tokens, p3);
    lx2 = tok_lex(tok2);
    if (strcmp(lx2,"=") == 0) {
        eq_pair = consume(tokens, p3);
        p4 = _second(eq_pair);
        vpair = collect_value(tokens, p4);
        tx_t expr = plant_list_get(vpair,  0 );
        p5 = _second(vpair);
        return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "let_stmt" , "target" , id_name , "var_type" , vtype , "value" , expr ) , p5 );
    }
    if (strcmp(lx2,"TO") == 0) {
        to_pair = consume(tokens, p3);
        p4 = _second(to_pair);
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
  tx_t tok0 = "";
  tx_t lx0 = "";
  tx_t ty0 = "";
  tx_t com0 = "";
  tx_t is_eof_flag = "";
  tx_t tok = "";
  tx_t lx = "";
  tx_t ty = "";
  tx_t dot = "";
  tx_t p6 = "";
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
    PlantArray* args = plant_list_make ( 0 );
    while (1) {
        tok0 = peek(tokens, p5);
        lx0 = tok_lex(tok0);
        ty0 = tok_type(tok0);
        if (strcmp(lx0,",") == 0 && strcmp(ty0,"STRING") != 0) {
            com0 = consume(tokens, p5);
            p5 = _second(com0);
        }
        is_eof_flag = is_eof(tokens, p5);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "reap_stmt" , "target" , var_name , "action" , act_name , "args" , args ) , p5 );
        }
        tok = peek(tokens, p5);
        lx = tok_lex(tok);
        ty = tok_type(tok);
        if (strcmp(lx,".") == 0 && strcmp(ty,"STRING") != 0) {
            dot = consume(tokens, p5);
            p6 = _second(dot);
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "reap_stmt" , "target" , var_name , "action" , act_name , "args" , args ) , p6 );
        }
        tx_t arg_text = "";
        long adepth = 0;
        while (1) {
            atok = peek(tokens, p5);
            alx = tok_lex(atok);
            atype = tok_type(atok);
            if (strcmp(atype,"STRING") == 0) {
                alx = _cat(_cat("\"", alx), "\"");
            }
            if (strcmp(alx,",") == 0 && adepth == 0) {
                break;
            }
            if (strcmp(alx,".") == 0 && adepth == 0) {
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
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "reap_stmt" , "target" , var_name , "action" , act_name , "args" , args ) , p6 );
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
tx_t parse_action_decl(PlantArray* tokens, long pos) {
  tx_t pair = "";
  tx_t p2 = "";
  tx_t name_pair = "";
  tx_t aname = "";
  tx_t p3 = "";
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
  tx_t pt_pair = "";
  tx_t pt = "";
  tx_t p7 = "";
  tx_t rp2 = "";
  tx_t p8 = "";
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
  tx_t after_tok = "";
  tx_t after_lx = "";
  tx_t com_pair = "";
  tx_t dot_pair = "";
  tx_t tok4 = "";
  tx_t lx4 = "";
  tx_t slash = "";
  tx_t end = "";
  tx_t dot = "";
  tx_t stmt_pair = "";
    pair = consume(tokens, pos);
    p2 = _second(pair);
    name_pair = consume(tokens, p2);
    aname = tok_lex(plant_list_get(name_pair,  0 ));
    p3 = _second(name_pair);
    lp = consume(tokens, p3);
    p4 = _second(lp);
    PlantArray* params = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p4);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "action_decl" , "name" , aname , "params" , params , "body" , plant_list_make ( 0 ) ) , p4 );
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
            pt_pair = consume(tokens, p6);
            pt = tok_lex(plant_list_get(pt_pair,  0 ));
            p7 = _second(pt_pair);
            rp2 = consume(tokens, p7);
            p8 = _second(rp2);
            params = plant_list_push(params, plant_list_make ( 4 , "name" , pn , "type" , pt ));
            p5 = p8;
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
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "action_decl" , "name" , aname , "params" , params , "body" , plant_list_make ( 0 ) ) , p5 );
        }
    }
    PlantArray* body = plant_list_make ( 0 );
    while (1) {
        is_eof_flag = is_eof(tokens, p5);
        if (is_eof_flag) {
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "action_decl" , "name" , aname , "params" , params , "body" , body ) , p5 );
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
            return plant_list_make ( 2 , plant_list_make ( 8 , "type" , "action_decl" , "name" , aname , "params" , params , "body" , body ) , p8 );
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
        if (strcmp(ch,"0") >= 0 && strcmp(ch,"9") <= 0) {
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
tx_t generate_body(PlantArray* bd, long indent) {
  tx_t node_code = "";
    tx_t res = "";
    long i = 0;
    tx_t node_el = "";
    while (i < plant_array_length(bd)) {
        node_el = plant_list_get(bd, i);
        node_code = generate_node(node_el, indent);
        if (strcmp(node_code,"") > 0) {
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
  tx_t target = "";
  tx_t vtype = "";
  tx_t item = "";
  tx_t citem = "";
  tx_t tgt = "";
  tx_t act = "";
  tx_t arg0 = "";
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
  tx_t ename = "";
    ntype = _map_get(node, "type");
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
        val = _map_get(node, "value");
        cval = translate_expr(val);
        cval = _handle_cat(cval);
        isel = indent_str(indent);
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
        val = _map_get(node, "value");
        cval = translate_expr(val);
        isel = indent_str(indent);
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
        PlantArray* args = _map_get ( node , "args" );
        tx_t argstr = "";
        long ai = 0;
        tx_t arg_el = "";
        while (ai < plant_array_length(args)) {
            arg_el = plant_list_get(args, ai);
            tx_t aexpr = arg_el;
            arg0 = substring(arg_el, 0, 1);
            if (strcmp(arg0,"\"") != 0) {
                aexpr = translate_expr(arg_el);
                aexpr = _handle_cat(aexpr);
            }
            if (ai > 0) {
                argstr = _cat(argstr, ", ");
            }
            argstr = _cat(argstr, aexpr);
            ai = ai+1;
        }
        isel = indent_str(indent);
        if (strcmp(tgt,"_") == 0) {
            return _cat(_cat(_cat(_cat(_cat(isel, "  "), act), "("), argstr), ");\n");
        }
        if (strcmp(tgt,"_") != 0) {
            return _cat(_cat(_cat(_cat(_cat(_cat(_cat(isel, "  "), tgt), " = "), act), "("), argstr), ");\n");
        }
    }
    if (strcmp(ntype,"if_stmt") == 0) {
        cond = _map_get(node, "cond");
        bd = _map_get(node, "body");
        ccond = translate_expr(cond);
        ccond = handle_strcmp(ccond);
        isel = indent_str(indent);
        tx_t ccode = _cat(_cat(_cat(isel, "  if ("), ccond), ") {\n");
        bcode = generate_body(bd, indent+2);
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
        bcode = generate_body(bd, indent+2);
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
            bcode = generate_body(bd, indent+2);
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
            bcode = generate_body(bd, indent+4);
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
                bcode = generate_body(cbody, indent+4);
                ccode = _cat(_cat(_cat(ccode, bcode), isel), "      }\n");
            }
            if (strcmp(binding,"") == 0 || strcmp(binding,"null") == 0) {
                bcode = generate_body(cbody, indent+4);
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
    if (strcmp(ntype,"action_decl") == 0) {
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
            tx_t ctype = "tx_t";
            if (strcmp(ptype,"NUM") == 0) {
                ctype = "long";
            }
            if (strcmp(ptype,"FACT") == 0) {
                ctype = "int";
            }
            if (strcmp(ptype,"LIST") == 0) {
                ctype = "PlantArray*";
            }
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
        bcode = generate_body(bd, 1);
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
tx_t generate_c(PlantArray* ast) {
  tx_t ntype = "";
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
    i = 0;
    while (i < plant_array_length(ast)) {
        node_el = plant_list_get(ast, i);
        ntype = _map_get(node_el, "type");
        if (strcmp(ntype,"action_decl") == 0) {
            aname = _map_get(node_el, "name");
            PlantArray* params2 = _map_get ( node_el , "params" );
            paramstr = "";
            pi = 0;
            while (pi < plant_array_length(params2)) {
                param_el = plant_list_get(params2, pi);
                pname = _map_get(param_el, "name");
                ptype = _map_get(param_el, "type");
                ctype = "tx_t";
                if (strcmp(ptype,"NUM") == 0) {
                    ctype = "long";
                }
                if (strcmp(ptype,"FACT") == 0) {
                    ctype = "int";
                }
                if (strcmp(ptype,"LIST") == 0) {
                    ctype = "PlantArray*";
                }
                if (pi > 0) {
                    paramstr = _cat(paramstr, ", ");
                }
                paramstr = _cat(_cat(_cat(paramstr, ctype), " "), pname);
                pi = pi+1;
            }
            pro_code = _cat(_cat(_cat(_cat(_cat(pro_code, "tx_t "), aname), "("), paramstr), ");\n");
        }
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
        if (strcmp(ntype,"action_decl") == 0 || strcmp(ntype,"enum_decl") == 0) {
            nd_code = generate_node(node_el, 0);
            decl_code = _cat(decl_code, nd_code);
            has_decl = 1;
        }
        if (strcmp(ntype,"action_decl") != 0 && strcmp(ntype,"enum_decl") != 0) {
            ns_code = generate_node(node_el, 0);
            stmt_code = _cat(stmt_code, ns_code);
            has_stmt = 1;
        }
        i = i+1;
    }
    if (has_stmt) {
        stmt_code = _cat(_cat(_cat("int main(int argc, char **argv) {\n  plant_init_cli(argc, argv);\n", dcode), stmt_code), "  return 0;\n}\n");
    }
    return _cat(_cat(_cat(_cat(header, pro_code), "\n"), decl_code), stmt_code);
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
      plant_print("Chloroplast 0.46.4 (pure native)");
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
