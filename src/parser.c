#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lexer.h"

// --- エラー処理 ---
void error(const char *fmt) {
    fprintf(stderr, "Parse Error: %s (Token: %d, Val: %s)\n", fmt, token, tokenStr);
    exit(1);
}

// --- ノード生成ヘルパー関数 ---

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

Node *new_var(char *name) {
    Node *node = new_node(ND_VAR);
    node->name = strdup(name);
    return node;
}

Node *new_str_lit(char *str) {
    Node *node = new_node(ND_STR_LIT);
    node->strVal = strdup(str);
    return node;
}

// --- トークン操作ヘルパー ---

// 期待するトークンなら読み進めて true を返す
int consume(TokenType type, FILE *fp) {
    if (token == type) {
        getNextToken(fp); 
        return 1;
    }
    return 0;
}

// 期待するトークンでなければエラー
void expect(TokenType type, FILE *fp) {
    if (token != type) {
        error("Unexpected token");
    }
    getNextToken(fp);
}

// --- 構文解析関数 (プロトタイプ宣言) ---
Node *parse_statement_list(FILE *fp);
Node *parse_statement(FILE *fp);
Node *parse_simple_statement(FILE *fp);
Node *parse_block_statement(FILE *fp);
Node *parse_condition_expression(FILE *fp);

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
    Node head;
    head.next = NULL;
    Node *cur = &head;

    // ブロック終了 "}" または EOF が来るまで繰り返す
    while (token != TK_RBRACE && token != TK_EOF) {
        cur->next = parse_statement(fp);
        cur = cur->next;
    }

    return head.next;
}

// statement ::= block_statement | simple_statement "。"
Node *parse_statement(FILE *fp) {
    // ブロック文 (ループ, もし)
    if (token == TK_LOOP || token == TK_IF) {
        return parse_block_statement(fp);
    }
    
    // 単文
    Node *node = parse_simple_statement(fp);
    expect(TK_PERIOD, fp); // 文末の「。」をチェック
    return node;
}

// 値 (リテラル or 変数) をパースする
Node *parse_value(FILE *fp) {
    if (token == TK_LITERAL) {
        double val = atof(tokenStr);
        getNextToken(fp);
        return new_num(val);
    } else if (token == TK_VARIABLE) {
        Node *node = new_var(tokenStr);
        getNextToken(fp);
        return node;
    }
    error("Expected number or variable");
    return NULL;
}

// simple_statement
Node *parse_simple_statement(FILE *fp) {
    // 1. 出力文: (LIT | PRINT_LIT) "と出力する"
    if (token == TK_LITERAL || token == TK_PRINT_LIT) {
        Node *node;
        if (token == TK_LITERAL) {
            node = new_num(atof(tokenStr));
        } else {
            node = new_str_lit(tokenStr);
        }
        getNextToken(fp);
        
        expect(TK_OUTPUT, fp);
        
        Node *outNode = new_node(ND_OUTPUT);
        outNode->lhs = node; // 左辺に出力内容を持たせる
        return outNode;
    }

    // 2. 変数操作: VARIABLE statement_suffix
    if (token == TK_VARIABLE) {
        Node *target = new_var(tokenStr);
        getNextToken(fp); // 変数を消費

        // サフィックス分岐
        if (token == TK_WO) { // ...を
            getNextToken(fp);
            Node *val = parse_value(fp);
            
            if (token == TK_DECLARE) { // ...で宣言する
                getNextToken(fp);
                return new_binary(ND_DECLARE, target, val);
            } else if (token == TK_DIV) { // ...でわる
                getNextToken(fp);
                return new_binary(ND_DIV, target, val);
            }
        }
        else if (token == TK_NI) { // ...に
            getNextToken(fp);
            
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
            Node *val = parse_value(fp);
            expect(TK_SUB, fp); // をひく
            return new_binary(ND_SUB, target, val);
        }
    }

    error("Unknown statement");
    return NULL;
}

// 条件式のパース (再帰下降)
// simple_condition ::= VAR "が" (LIT|VAR) OP
Node *parse_simple_condition(FILE *fp) {
    if (token != TK_VARIABLE) error("Condition must start with variable");
    Node *lhs = new_var(tokenStr);
    getNextToken(fp);

    expect(TK_GA, fp);

    Node *rhs = parse_value(fp);

    NodeKind kind;
    if (token == TK_OP_GE) kind = ND_GE;
    else if (token == TK_OP_LE) kind = ND_LE;
    else if (token == TK_OP_GT) kind = ND_GT;
    else if (token == TK_OP_LT) kind = ND_LT;
    else if (token == TK_OP_EQ) kind = ND_EQ;
    else if (token == TK_OP_NE) kind = ND_NE;
    else error("Unknown operator in condition");

    getNextToken(fp); // 演算子を消費
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
    // factor相当の処理 (simple_condition or (expression))
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
            getNextToken(fp); // "ではなく"
            expect(TK_LPAR, fp);
            Node *elif_cond = parse_condition_expression(fp);
            expect(TK_RPAR, fp);
            expect(TK_LBRACE, fp);
            Node *elif_body = parse_statement_list(fp);
            expect(TK_RBRACE, fp);

            // else if は ND_ELSEIF ノードとして作成
            Node *elif_node = new_node(ND_ELSEIF); 
            elif_node->cond = elif_cond;
            elif_node->then = elif_body;
            
            curr->els = elif_node;
            curr = elif_node;
        }

        if (token == TK_ELSE) {
            getNextToken(fp); // "ではない"
            expect(TK_LBRACE, fp);
            Node *else_body = parse_statement_list(fp);
            expect(TK_RBRACE, fp);
            curr->els = else_body;
        }

        return node;
    }

    error("Expected block statement");
    return NULL;
}