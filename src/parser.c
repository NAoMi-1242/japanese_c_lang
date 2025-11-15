#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lexer.h"
#include "error.h"

// --- スコープ・変数管理 (Parser側で実施) ---
// 変数名とIDを紐付けるリスト
typedef struct LVar LVar;
struct LVar {
    LVar *next;
    char *name;
    int id;     // 発行された一意なID
};

LVar *locals = NULL;     // 現在のスコープで見える変数リスト (スタック構造)
int var_counter = 0;     // グローバルなIDカウンタ

// 変数を検索 (現在のスコープから順に遡る)
LVar *find_lvar(char *name) {
    for (LVar *v = locals; v; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            return v;
        }
    }
    return NULL;
}

// 変数を登録 (新規ID発行)
// 常にリストの先頭に追加する
int register_lvar(char *name) {
    LVar *v = calloc(1, sizeof(LVar));
    v->name = strdup(name);
    v->id = ++var_counter; // 新しいIDを発行
    v->next = locals;
    locals = v;
    return v->id;
}

// --- ノード生成ヘルパー ---

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

// 既存変数の参照ノードを作成 (未定義ならエラー)
Node *new_var_node(char *name) {
    LVar *lvar = find_lvar(name);
    if (!lvar) {
        // エラー種別: 意味解析エラー (未定義変数)
        error(ERR_SEMANTIC, "未定義の変数「%s」が参照されています", name);
    }

    Node *node = new_node(ND_VAR);
    node->name = strdup(name);
    node->var_id = lvar->id; // 解決済みのIDをセット
    return node;
}

// 文字列リテラル解析 (埋め込み変数の解決とフォーマット文字列生成)
Node *new_str_lit_node(char *content) {
    Node *node = new_node(ND_STR_LIT);
    
    char fmt[2048] = {0};
    int *ids = calloc(128, sizeof(int)); // 最大128個の埋め込みに対応
    int argc = 0;
    
    char *p = content;
    
    // 末尾が「：」なら改行しない
    int len = strlen(content);
    int no_newline = 0;
    if (len >= 3 && strcmp(content + len - 3, "：") == 0) {
        no_newline = 1;
    }

    while (*p) {
        // 変数埋め込み "”" の開始判定
        if (strncmp(p, "”", 3) == 0) {
            p += 3;
            char *start = p;
            char *end = strstr(start, "”");
            
            if (end) {
                // 変数名を切り出し
                int var_len = end - start;
                char var_name[256] = {0};
                strncpy(var_name, start, var_len);

                // 変数を検索しIDを取得
                LVar *lvar = find_lvar(var_name);
                if (!lvar) {
                    // エラー種別: 意味解析エラー (文字列内で未定義変数)
                    error(ERR_SEMANTIC, "文字列内で未定義の変数「%s」が使われています", var_name);
                }
                
                // IDリストに追加
                ids[argc++] = lvar->id;
                
                // フォーマット文字列には %f を追加
                strcat(fmt, "%f");
                p = end + 3;
            } else {
                // 閉じカッコがない場合はそのまま出力
                strcat(fmt, "”");
            }
        } 
        // エスケープ処理 (C言語のprintf用にエスケープ)
        else if (*p == '"') { strcat(fmt, "\\\""); p++; }
        else if (*p == '%') { strcat(fmt, "%%"); p++; }
        else if (*p == '\\') { strcat(fmt, "\\\\"); p++; }
        else { 
            // 通常文字
            strncat(fmt, p, 1); 
            p++; 
        }
    }

    // 改行制御
    if (!no_newline) strcat(fmt, "\\n");

    node->strVal = strdup(fmt);
    node->args = ids;
    node->argc = argc;
    return node;
}

// --- トークン操作ヘルパー ---

int consume(TokenType type, FILE *fp) {
    if (token == type) {
        getNextToken(fp); 
        return 1;
    }
    return 0;
}

void expect(TokenType type, FILE *fp) {
    if (token != type) {
        // エラー種別: 構文エラー
        error(ERR_SYNTAX, "予期しないトークンです (Token: %s)", tokenStr);
    }
    getNextToken(fp);
}

// --- 解析関数プロトタイプ ---
Node *parse_statement_list(FILE *fp);
Node *parse_statement(FILE *fp);
Node *parse_simple_statement(FILE *fp);
Node *parse_block_statement(FILE *fp);
Node *parse_condition_expression(FILE *fp);
Node *parse_value(FILE *fp);

// --- 実装 ---

// program ::= "メイン" "｛" statement_list "｝"
Node *parse_program(FILE *fp) {
    expect(TK_MAIN, fp);
    expect(TK_LBRACE, fp);

    Node *node = new_node(ND_PROGRAM);
    node->next = parse_statement_list(fp);

    expect(TK_RBRACE, fp);
    return node;
}

// statement_list ::= { statement }
Node *parse_statement_list(FILE *fp) {
    // スコープ開始：現在の変数の先頭を覚えておく
    LVar *scope_snapshot = locals;

    Node head;
    head.next = NULL;
    Node *cur = &head;

    while (token != TK_RBRACE && token != TK_EOF) {
        cur->next = parse_statement(fp);
        cur = cur->next;
    }

    // スコープ終了：リストを巻き戻す (ブロック内変数の破棄)
    locals = scope_snapshot;

    return head.next;
}

// statement
Node *parse_statement(FILE *fp) {
    if (token == TK_LOOP || token == TK_IF) {
        return parse_block_statement(fp);
    }
    
    Node *node = parse_simple_statement(fp);
    expect(TK_PERIOD, fp);
    return node;
}

// 値 (リテラル or 変数)
Node *parse_value(FILE *fp) {
    if (token == TK_LITERAL) {
        double val = atof(tokenStr);
        getNextToken(fp);
        return new_num(val);
    } else if (token == TK_VARIABLE) {
        // 変数ノード生成時に未定義チェックとID解決を行う
        Node *node = new_var_node(tokenStr);
        getNextToken(fp);
        return node;
    }
    // エラー種別: 構文エラー
    error(ERR_SYNTAX, "数値または変数が期待されています (Token: %s)", tokenStr);
    return NULL;
}

// simple_statement
Node *parse_simple_statement(FILE *fp) {
    // 1. 出力文
    if (token == TK_LITERAL || token == TK_PRINT_LIT) {
        Node *val;
        if (token == TK_LITERAL) {
            val = new_num(atof(tokenStr));
        } else {
            // 文字列リテラル解析 (埋め込み解決)
            val = new_str_lit_node(tokenStr);
        }
        getNextToken(fp);
        
        expect(TK_OUTPUT, fp);
        
        Node *outNode = new_node(ND_OUTPUT);
        outNode->lhs = val;
        return outNode;
    }

    // 2. 変数操作
    if (token == TK_VARIABLE) {
        char *name = strdup(tokenStr);
        getNextToken(fp); 

        // サフィックス分岐
        if (token == TK_WO) { // ...を
            getNextToken(fp);
            Node *val = parse_value(fp);
            
            if (token == TK_DECLARE) { // ...で宣言する
                getNextToken(fp);
                
                // 宣言処理: 新規IDを発行
                int id = register_lvar(name);
                
                Node *target = new_node(ND_VAR);
                target->name = name;
                target->var_id = id; // 新規ID

                return new_binary(ND_DECLARE, target, val);

            } else if (token == TK_DIV) { // ...でわる
                getNextToken(fp);
                Node *target = new_var_node(name); 
                return new_binary(ND_DIV, target, val);
            }
        }
        else if (token == TK_NI) { // ...に
            getNextToken(fp);
            
            // 対象変数は既存のもの (存在チェック)
            Node *target = new_var_node(name);

            if (token == TK_INPUT) { // 入力する
                getNextToken(fp);
                Node *inNode = new_node(ND_INPUT);
                inNode->lhs = target;
                return inNode;
            }

            Node *val = parse_value(fp);

            if (token == TK_ASSIGN) { // を代入する
                getNextToken(fp);
                return new_binary(ND_ASSIGN, target, val);
            } else if (token == TK_ADD) { // をたす
                getNextToken(fp);
                return new_binary(ND_ADD, target, val);
            } else if (token == TK_MUL) { // をかける
                getNextToken(fp);
                return new_binary(ND_MUL, target, val);
            }
        }
        else if (token == TK_KARA) { // ...から
            getNextToken(fp);
            Node *target = new_var_node(name); // 存在チェック
            Node *val = parse_value(fp);
            expect(TK_SUB, fp); 
            return new_binary(ND_SUB, target, val);
        }
    }

    // エラー種別: 構文エラー
    error(ERR_SYNTAX, "不明な文です (Token: %s)", tokenStr);
    return NULL;
}

// --- 条件式のパース ---

Node *parse_simple_condition(FILE *fp) {
    // 左辺は値（変数orリテラル）
    Node *lhs = parse_value(fp);

    expect(TK_GA, fp);

    // 右辺
    Node *rhs = parse_value(fp);

    NodeKind kind;
    if (token == TK_OP_GE) kind = ND_GE;
    else if (token == TK_OP_LE) kind = ND_LE;
    else if (token == TK_OP_GT) kind = ND_GT;
    else if (token == TK_OP_LT) kind = ND_LT;
    else if (token == TK_OP_EQ) kind = ND_EQ;
    else if (token == TK_OP_NE) kind = ND_NE;
    // エラー種別: 構文エラー
    else error(ERR_SYNTAX, "不明な比較演算子です (Token: %s)", tokenStr);

    getNextToken(fp);
    return new_binary(kind, lhs, rhs);
}

Node *parse_condition_factor(FILE *fp);

// term { "または" term }
Node *parse_condition_expression(FILE *fp) {
    Node *node = parse_condition_factor(fp);
    while (token == TK_OR) {
        getNextToken(fp);
        node = new_binary(ND_OR, node, parse_condition_factor(fp));
    }
    return node;
}

// factor { "かつ" factor }
Node *parse_condition_term(FILE *fp) {
    Node *node;
    if (token == TK_LPAR) {
        getNextToken(fp);
        node = parse_condition_expression(fp);
        expect(TK_RPAR, fp);
    } else {
        node = parse_simple_condition(fp);
    }

    while (token == TK_AND) {
        getNextToken(fp);
        Node *rhs;
        if (token == TK_LPAR) {
            getNextToken(fp);
            rhs = parse_condition_expression(fp);
            expect(TK_RPAR, fp);
        } else {
            rhs = parse_simple_condition(fp);
        }
        node = new_binary(ND_AND, node, rhs);
    }
    return node;
}

// factorとtermの呼び出し階層を整理（OR -> AND -> Simple）
Node *parse_condition_factor(FILE *fp) {
    return parse_condition_term(fp);
}


// block_statement
Node *parse_block_statement(FILE *fp) {
    if (token == TK_LOOP) {
        getNextToken(fp);
        expect(TK_LPAR, fp);
        Node *cond = parse_condition_expression(fp);
        expect(TK_RPAR, fp);
        expect(TK_LBRACE, fp);
        Node *body = parse_statement_list(fp);
        expect(TK_RBRACE, fp);

        Node *node = new_node(ND_LOOP);
        node->cond = cond;
        node->then = body;
        return node;
    }

    if (token == TK_IF) {
        getNextToken(fp);
        expect(TK_LPAR, fp);
        Node *cond = parse_condition_expression(fp);
        expect(TK_RPAR, fp);
        expect(TK_LBRACE, fp);
        Node *then = parse_statement_list(fp);
        expect(TK_RBRACE, fp);

        Node *node = new_node(ND_IF);
        node->cond = cond;
        node->then = then;
        
        // else if / else の連鎖処理
        Node *curr = node;
        while (token == TK_ELSEIF) {
            getNextToken(fp); 
            expect(TK_LPAR, fp);
            Node *elif_cond = parse_condition_expression(fp);
            expect(TK_RPAR, fp);
            expect(TK_LBRACE, fp);
            Node *elif_body = parse_statement_list(fp);
            expect(TK_RBRACE, fp);

            Node *elif_node = new_node(ND_ELSEIF); 
            elif_node->cond = elif_cond;
            elif_node->then = elif_body;
            
            curr->els = elif_node;
            curr = elif_node;
        }

        if (token == TK_ELSE) {
            getNextToken(fp);
            expect(TK_LBRACE, fp);
            Node *else_body = parse_statement_list(fp);
            expect(TK_RBRACE, fp);
            curr->els = else_body;
        }

        return node;
    }

    // エラー種別: 構文エラー
    error(ERR_SYNTAX, "ブロック文が期待されています (Token: %s)", tokenStr);
    return NULL;
}