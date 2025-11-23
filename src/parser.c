#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "parser.h"
#include "lexer.h"
#include "error.h"

// --- スコープ・変数管理 ---
typedef struct LVar LVar;
struct LVar {
    LVar *next;
    char *name;
    int id;
};
LVar *locals = NULL;
int var_counter = 0;

LVar *find_lvar(char *name) {
    for (LVar *v = locals; v; v = v->next) {
        if (strcmp(v->name, name) == 0) return v;
    }
    return NULL;
}

int register_lvar(char *name) {
    for (LVar *v = locals; v; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            error(ERR_SEMANTIC, "変数「%s」は既に宣言されています", name);
        }
    }
    LVar *v = calloc(1, sizeof(LVar));
    v->name = strdup(name);
    v->id = ++var_counter;
    v->next = locals;
    locals = v;
    return v->id;
}

// --- ノード生成 ---
Node *new_node(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}
Node *new_num(double val) {
    Node *node = new_node(ND_LITERAL);
    node->val = val;
    return node;
}
Node *new_var_node(char *name) {
    LVar *lvar = find_lvar(name);
    if (!lvar) error(ERR_SEMANTIC, "未定義の変数「%s」が参照されています", name);
    Node *node = new_node(ND_VAR);
    node->name = strdup(name);
    node->var_id = lvar->id;
    return node;
}
Node *new_str_lit_node(char *content) {
    Node *node = new_node(ND_STR_LIT);
    char fmt[2048] = {0};
    int *ids = calloc(128, sizeof(int));
    int argc = 0;
    char *p = content;
    int len = strlen(content);
    int no_newline = 0;
    if (len >= 3 && strcmp(content + len - 3, "：") == 0) no_newline = 1;

    while (*p) {
        if (strncmp(p, "”", 3) == 0) {
            p += 3;
            char *start = p;
            char *end = strstr(start, "”");
            if (end) {
                int var_len = end - start;
                char var_name[256] = {0};
                strncpy(var_name, start, var_len);
                LVar *lvar = find_lvar(var_name);
                if (!lvar) error(ERR_SEMANTIC, "文字列内で未定義の変数「%s」が使われています", var_name);
                ids[argc++] = lvar->id;
                strcat(fmt, "%f");
                p = end + 3;
            } else {
                strcat(fmt, "”");
            }
        } 
        else if (*p == '"') { strcat(fmt, "\\\""); p++; }
        else if (*p == '%') { strcat(fmt, "%%"); p++; }
        else if (*p == '\\') { strcat(fmt, "\\\\"); p++; }
        else { strncat(fmt, p, 1); p++; }
    }
    if (!no_newline) strcat(fmt, "\\n");
    node->strVal = strdup(fmt);
    node->args = ids;
    node->argc = argc;
    return node;
}

// --- トークン操作・空白チェックヘルパー ---

// 直前の空白・改行禁止をチェックする関数
// context: エラーメッセージ用の文脈（例: "「。」の前"）
void check_no_space(const char *context) {
    if (current_token.has_space_before || current_token.has_newline_before) {
        error(ERR_SYNTAX, "%sには空白・改行を入れてはいけません (Token: %s)", context, current_token.str);
    }
}

// 直前の空白または改行必須をチェックする関数
// context: エラーメッセージ用の文脈（例: "「かつ」の前"）
void check_has_space(const char *context) {
    if (!current_token.has_space_before && !current_token.has_newline_before) {
        error(ERR_SYNTAX, "%sには空白または改行が必要です (Token: %s)", context, current_token.str);
    }
}

// 指定されたトークンであることを確認し、消費する
void expect(TokenType type, FILE *fp) {
    if (current_token.type == type) {
        getNextToken(fp);
    } else {
        error(ERR_SYNTAX, "「%s」が期待されていましたが、「%s」が代わりに発見されました (Token: %s)", getTokenName(type), getTokenName(current_token.type), current_token.str);
    }
}


// --- 構文解析関数 ---

Node *parse_program(FILE *fp);
Node *parse_statements_block(FILE *fp);
Node *parse_statement(FILE *fp);
Node *parse_simple_statement(FILE *fp);
Node *parse_loop_or_if_statement(FILE *fp);
Node *parse_conditional_block(FILE *fp, NodeKind kind);
Node *parse_if_statement_block(FILE *fp);
Node *parse_simple_statement_suffix(FILE *fp, char *name);
Node *parse_simple_statement_suffix_wo(FILE *fp, char *name);
Node *parse_simple_statement_suffix_ni(FILE *fp, char *name);
Node *parse_simple_statement_suffix_kara(FILE *fp, char *name);
Node *parse_condition_expression(FILE *fp);
Node *parse_condition_term(FILE *fp);
Node *parse_condition_factor(FILE *fp);
Node *parse_simple_condition(FILE *fp);
Node *parse_comparison_op(FILE *fp, Node *lhs, Node *rhs);
Node *parse_value(FILE *fp);

Node *parse_program(FILE *fp) {
    expect(TK_MAIN, fp);
    Node *node = new_node(ND_PROGRAM);
    node->next = parse_statements_block(fp);
    return node;
}

Node *parse_statements_block(FILE *fp) {
    LVar *scope_snapshot = locals;
    expect(TK_LBRACE, fp);

    Node head; head.next = NULL;
    Node *cur = &head;

    while (
        current_token.type == TK_VARIABLE || 
        current_token.type == TK_PRINT_LIT || 
        current_token.type == TK_LITERAL ||
        current_token.type == TK_LOOP || 
        current_token.type == TK_IF
    ) {
        cur->next = parse_statement(fp);
        cur = cur->next;
    }

    expect(TK_RBRACE, fp);
    locals = scope_snapshot;
    return head.next;
}

Node *parse_statement(FILE *fp) {
    Node *node;
    if (current_token.type == TK_VARIABLE || current_token.type == TK_PRINT_LIT || current_token.type == TK_LITERAL) {
        node = parse_simple_statement(fp);
        
        if (current_token.type == TK_PERIOD) {
            check_no_space("文末の「。」の前");
        }
        expect(TK_PERIOD, fp); 
    } else if (current_token.type == TK_LOOP || current_token.type == TK_IF) {
        node = parse_loop_or_if_statement(fp);
    } else {
        error(ERR_SYNTAX, "文が期待されています (Token: %s)", current_token.str);
    }
    return node;
}

Node *parse_simple_statement(FILE *fp) {
    Node *node;

    if (current_token.type == TK_VARIABLE) {
        char *name = strdup(current_token.str);
        getNextToken(fp);
        // 変数直後の空白チェックは各suffix関数内で行う
        node = parse_simple_statement_suffix(fp, name);
    } 
    else if (current_token.type == TK_PRINT_LIT || current_token.type == TK_LITERAL) {
        Node *val;
        if (current_token.type == TK_LITERAL) val = new_num(atof(current_token.str));
        else val = new_str_lit_node(current_token.str);
        getNextToken(fp);
        
        if (current_token.type == TK_OUTPUT) {
            check_no_space("「と出力する」の前");
        }
        expect(TK_OUTPUT, fp);
        
        node = new_node(ND_OUTPUT);
        node->lhs = val;
    } 
    else {
        error(ERR_SYNTAX, "不明な単文です");
    }
    return node;
}

Node *parse_loop_or_if_statement(FILE *fp) {
    Node *node;
    if (current_token.type == TK_LOOP) {
        getNextToken(fp);
        node = parse_conditional_block(fp, ND_LOOP);
    } else if (current_token.type == TK_IF) {
        getNextToken(fp);
        node = parse_if_statement_block(fp);
    } else {
        error(ERR_SYNTAX, "不明なブロック文です");
    }
    return node;
}

Node *parse_conditional_block(FILE *fp, NodeKind kind) {
    expect(TK_LPAR, fp);
    Node *cond = parse_condition_expression(fp);
    expect(TK_RPAR, fp);
    
    Node *body = parse_statements_block(fp);

    Node *node = new_node(kind);
    node->cond = cond;
    node->then = body;
    return node;
}

Node *parse_if_statement_block(FILE *fp) {
    Node *node = parse_conditional_block(fp, ND_IF);
    Node *curr = node;

    while (current_token.type == TK_ELSEIF) {
        getNextToken(fp);
        Node *elif_node = parse_conditional_block(fp, ND_ELSEIF);
        curr->els = elif_node;
        curr = elif_node;
    }
    
    if (current_token.type == TK_ELSE) {
        getNextToken(fp);
        curr->els = parse_statements_block(fp);
    }

    return node;
}

Node *parse_simple_statement_suffix(FILE *fp, char *name) {
    if (current_token.type == TK_WO) {
        check_no_space("助詞「を」の前");
        getNextToken(fp);
        check_no_space("助詞「を」の後");
        return parse_simple_statement_suffix_wo(fp, name);
    } else if (current_token.type == TK_NI) {
        check_no_space("助詞「に」の前");
        getNextToken(fp);
        check_no_space("助詞「に」の後");
        return parse_simple_statement_suffix_ni(fp, name);
    } else if (current_token.type == TK_KARA) {
        check_no_space("助詞「から」の前");
        getNextToken(fp);
        check_no_space("助詞「から」の後");
        return parse_simple_statement_suffix_kara(fp, name);
    } else {
        error(ERR_SYNTAX, "「を」「に」「から」が期待されています");
    }
    return NULL;
}

Node *parse_simple_statement_suffix_wo(FILE *fp, char *name) {
    Node *val = parse_value(fp);
    
    if (current_token.type == TK_DECLARE) {
        check_no_space("「で宣言する」の前");
        getNextToken(fp);
        int id = register_lvar(name);
        Node *target = new_node(ND_VAR);
        target->name = name;
        target->var_id = id;
        return new_binary(ND_DECLARE, target, val);
    } else if (current_token.type == TK_DIV) {
        check_no_space("「でわる」の前");
        getNextToken(fp);
        Node *target = new_var_node(name);
        return new_binary(ND_DIV, target, val);
    } else {
        error(ERR_SYNTAX, "「で宣言する」または「でわる」が期待されています");
    }
    return NULL;
}

Node *parse_simple_statement_suffix_ni(FILE *fp, char *name) {
    Node *target = new_var_node(name);

    if (current_token.type == TK_INPUT) {
        getNextToken(fp);
        Node *node = new_node(ND_INPUT);
        node->lhs = target;
        return node;
    } 
    
    Node *val = parse_value(fp);
    
    if (current_token.type == TK_ASSIGN) {
        check_no_space("「を代入する」の前");
        getNextToken(fp);
        return new_binary(ND_ASSIGN, target, val);
    } else if (current_token.type == TK_ADD) {
        check_no_space("「をたす」の前");
        getNextToken(fp);
        return new_binary(ND_ADD, target, val);
    } else if (current_token.type == TK_MUL) {
        check_no_space("「をかける」の前");
        getNextToken(fp);
        return new_binary(ND_MUL, target, val);
    } else {
        error(ERR_SYNTAX, "「入力する」「を代入する」「をたす」「をかける」が期待されています");
    }
    return NULL;
}

Node *parse_simple_statement_suffix_kara(FILE *fp, char *name) {
    Node *target = new_var_node(name);
    Node *val = parse_value(fp);
    
    if (current_token.type == TK_SUB) {
        check_no_space("「をひく」の前");
    }
    expect(TK_SUB, fp); 
    return new_binary(ND_SUB, target, val);
}

Node *parse_condition_expression(FILE *fp) {
    Node *node = parse_condition_term(fp);

    while (current_token.type == TK_OR) {
        check_has_space("「または」の前");
        getNextToken(fp);
        check_has_space("「または」の後");
        
        node = new_binary(ND_OR, node, parse_condition_term(fp));
    }
    return node;
}

Node *parse_condition_term(FILE *fp) {
    Node *node = parse_condition_factor(fp);

    while (current_token.type == TK_AND) {
        check_has_space("「かつ」の前");
        getNextToken(fp);
        check_has_space("「かつ」の後");
        
        node = new_binary(ND_AND, node, parse_condition_factor(fp));
    }
    return node;
}

Node *parse_condition_factor(FILE *fp) {
    Node *node;
    if (current_token.type == TK_LPAR) {
        getNextToken(fp);
        node = parse_condition_expression(fp);
        expect(TK_RPAR, fp);
    } 
    else if (current_token.type == TK_LITERAL || current_token.type == TK_VARIABLE) {
        node = parse_simple_condition(fp);
    } 
    else {
        error(ERR_SYNTAX, "条件式または「（」が期待されています (Token: %s)", current_token.str);
    }
    return node;
}

Node *parse_simple_condition(FILE *fp) {
    Node *lhs = parse_value(fp);
    
    if (current_token.type == TK_GA) {
        check_no_space("「が」の前");
    }
    expect(TK_GA, fp);
    check_no_space("「が」の後");

    Node *rhs = parse_value(fp);
    
    // 比較演算子の前は空白禁止 (詳細はparse_comparison_op内でチェック)
    return parse_comparison_op(fp, lhs, rhs);
}

Node *parse_comparison_op(FILE *fp, Node *lhs, Node *rhs) {
    if (current_token.type == TK_OP_GE) { 
        check_no_space("「以上か」の前");
        getNextToken(fp); return new_binary(ND_GE, lhs, rhs); 
    }
    if (current_token.type == TK_OP_LE) { 
        check_no_space("「以下か」の前");
        getNextToken(fp); return new_binary(ND_LE, lhs, rhs); 
    }
    if (current_token.type == TK_OP_GT) { 
        check_no_space("「より大きいか」の前");
        getNextToken(fp); return new_binary(ND_GT, lhs, rhs); 
    }
    if (current_token.type == TK_OP_LT) { 
        check_no_space("「より小さいか」の前");
        getNextToken(fp); return new_binary(ND_LT, lhs, rhs); 
    }
    if (current_token.type == TK_OP_EQ) { 
        check_no_space("「と一緒か」の前");
        getNextToken(fp); return new_binary(ND_EQ, lhs, rhs); 
    }
    if (current_token.type == TK_OP_NE) { 
        check_no_space("「と違うか」の前");
        getNextToken(fp); return new_binary(ND_NE, lhs, rhs); 
    }
    
    error(ERR_SYNTAX, "比較演算子（「以上か」など）が期待されています");
    return NULL;
}

Node *parse_value(FILE *fp) {
    if (current_token.type == TK_LITERAL) {
        Node *node = new_num(atof(current_token.str));
        getNextToken(fp);
        return node;
    } else if (current_token.type == TK_VARIABLE) {
        Node *node = new_var_node(current_token.str);
        getNextToken(fp);
        return node;
    } else {
        error(ERR_SYNTAX, "数値または変数が期待されています");
    }
    return NULL;
}