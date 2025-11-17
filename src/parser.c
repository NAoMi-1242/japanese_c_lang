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
        if (strcmp(v->name, name) == 0) return v;
    }
    return NULL;
}

// 変数を登録 (新規ID発行)
// 常にリストの先頭に追加する
int register_lvar(char *name) {
    // 同一スコープ内での重複宣言チェック
    // localsの先頭から、次のスコープまでの範囲で同名変数を探す
    for (LVar *v = locals; v; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            error(ERR_SEMANTIC, "変数「%s」は既に宣言されています", name);
        }
    }
    
    LVar *v = calloc(1, sizeof(LVar));
    v->name = strdup(name);
    v->id = ++var_counter; // 新しいIDを発行
    v->next = locals;
    locals = v;
    return v->id;
}

// --- ノード生成ヘルパー (実装) ---

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

// --- トークン操作ヘルパー (expectのみ使用) ---

// 添付資料 の「終端記号」ルールを実装したもの
void expect(TokenType type, FILE *fp) {
    if (token == type) {
        getNextToken(fp);
    } else {
        const char *expected_name = getTokenName(type);
        
        // トークンの行番号でエラー報告
        error(
            ERR_SYNTAX,
            "「%s」が期待されていましたが、「%s」が代わりに発見されました (Token: %s)", 
            expected_name, getTokenName(token), tokenStr
        );
    }
}

// 空白・改行の消費 (BNF {ws_or_ln} の実装)
void consumeWsOrLn(FILE *fp) {
    while (token == TK_WS || token == TK_LN) {
        getNextToken(fp);
    }
}
// 必須の空白・改行 (BNF ws_or_ln の実装)
void expectWsOrLn(FILE *fp) {
    if (token != TK_WS && token != TK_LN) {
        error(ERR_SYNTAX, "空白または改行が必要です (Token: %s)", tokenStr);
    }
    // 1つ以上を消費
    while (token == TK_WS || token == TK_LN) {
        getNextToken(fp);
    }
}


// --- 構文解析関数 (BNF 準拠) ---

// プロトタイプ宣言 (BNFの関数対応)
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

// program ::= "メイン" statements_block
Node *parse_program(FILE *fp) {
    consumeWsOrLn(fp); // 開始前の空白を許容
    expect(TK_MAIN, fp);

    Node *node = new_node(ND_PROGRAM);
    node->next = parse_statements_block(fp);
    
    consumeWsOrLn(fp); // 終了後の空白を許容
    return node;
}

// statements_block ::= { ws_or_ln } "｛" { ws_or_ln } { statement { ws_or_ln } } "｝"
Node *parse_statements_block(FILE *fp) {
    LVar *scope_snapshot = locals;
    consumeWsOrLn(fp);
    expect(TK_LBRACE, fp);
    consumeWsOrLn(fp);

    Node head; head.next = NULL;
    Node *cur = &head;

    // { statement { ws_or_ln } } の実装
    // TK_RBRACE が来るまで文をパースし続ける
    // (calc.c の while (token != END) と同じ思想)
    while (token != TK_RBRACE && token != TK_EOF) {
        cur->next = parse_statement(fp);
        cur = cur->next;
        consumeWsOrLn(fp); // 文の後の空白・改行
    }

    expect(TK_RBRACE, fp);
    locals = scope_snapshot;
    return head.next;
}

// statement ::= simple_statement "。" | block_statement
Node *parse_statement(FILE *fp) {
    Node *node;

    // 最初に何が来るかで分岐する
    if (token == TK_VARIABLE || token == TK_PRINT_LIT || token == TK_LITERAL) {
        // simple_statement の場合
        node = parse_simple_statement(fp);
        expect(TK_PERIOD, fp); // "。"
    } else if (token == TK_LOOP || token == TK_IF) {
        // block_statement の場合
        node = parse_loop_or_if_statement(fp);
    } else {
        error(ERR_SYNTAX, "文が期待されています (Token: %s)", tokenStr);
    }
    return node;
}

// simple_statement ::= TOKEN_VARIABLE simple_statement_suffix
//                    | (TOKEN_PRINT_LITERAL | TOKEN_LITERAL) "と出力する"
Node *parse_simple_statement(FILE *fp) {
    Node *node;

    if (token == TK_VARIABLE) {
        char *name = strdup(tokenStr);
        getNextToken(fp);
        node = parse_simple_statement_suffix(fp, name);
    } 
    else if (token == TK_PRINT_LIT || token == TK_LITERAL) {
        Node *val;
        if (token == TK_LITERAL) val = new_num(atof(tokenStr));
        else val = new_str_lit_node(tokenStr);
        getNextToken(fp);
        
        expect(TK_OUTPUT, fp); // 「と出力する」
        
        node = new_node(ND_OUTPUT);
        node->lhs = val;
    } 
    else {
        // ここには来ないはず (parse_statementが分岐制御するため)
        error(ERR_SYNTAX, "不明な単文です");
    }
    return node;
}

// block_statement ::= "ループ" conditional_block | "もし" if_statement_block
Node *parse_loop_or_if_statement(FILE *fp) {
    Node *node;
    if (token == TK_LOOP) {
        getNextToken(fp);
        node = parse_conditional_block(fp, ND_LOOP);
    } else if (token == TK_IF) {
        getNextToken(fp);
        node = parse_if_statement_block(fp);
    } else {
        // ここには来ないはず
        error(ERR_SYNTAX, "不明なブロック文です");
    }
    return node;
}

// conditional_block ::= { TOKEN_WS } "（" condition_expression "）" statements_block
// (NodeKind kind は、ND_LOOP か ND_IF を受け取る)
Node *parse_conditional_block(FILE *fp, NodeKind kind) {
    while(token == TK_WS) getNextToken(fp); // { TOKEN_WS }
    
    expect(TK_LPAR, fp);
    Node *cond = parse_condition_expression(fp);
    expect(TK_RPAR, fp);
    
    Node *body = parse_statements_block(fp);

    Node *node = new_node(kind);
    node->cond = cond;
    node->then = body;
    return node;
}

// if_statement_block ::= conditional_block { elseif_statement } [ else_statement ]
Node *parse_if_statement_block(FILE *fp) {
    Node *node = parse_conditional_block(fp, ND_IF);
    Node *curr = node;

    // { elseif_statement }
    // 続く限り "ではなく" をパースする (whileループ)
    while(1) {
        consumeWsOrLn(fp); // { ws_or_ln }
        if (token == TK_ELSEIF) {
            getNextToken(fp);
            Node *elif_node = parse_conditional_block(fp, ND_ELSEIF);
            curr->els = elif_node;
            curr = elif_node;
        } else {
            break; // "ではなく" がなければループ終了
        }
    }
    
    // [ else_statement ]
    // "ではない" が1回来るかもしれない (if文)
    consumeWsOrLn(fp); // { ws_or_ln }
    if (token == TK_ELSE) {
        getNextToken(fp);
        curr->els = parse_statements_block(fp);
    }

    return node;
}

// simple_statement_suffix ::= "を" ... | "に" ... | "から" ...
Node *parse_simple_statement_suffix(FILE *fp, char *name) {
    if (token == TK_WO) {
        getNextToken(fp);
        return parse_simple_statement_suffix_wo(fp, name);
    } else if (token == TK_NI) {
        getNextToken(fp);
        return parse_simple_statement_suffix_ni(fp, name);
    } else if (token == TK_KARA) {
        getNextToken(fp);
        return parse_simple_statement_suffix_kara(fp, name);
    } else {
        error(ERR_SYNTAX, "「を」「に」「から」が期待されています");
    }
    return NULL;
}

// simple_statement_suffix_wo ::= (TOKEN_LITERAL | TOKEN_VARIABLE) "で宣言する"
//                       | (TOKEN_LITERAL | TOKEN_VARIABLE) "でわる"
Node *parse_simple_statement_suffix_wo(FILE *fp, char *name) {
    Node *val = parse_value(fp);

    if (token == TK_DECLARE) {
        getNextToken(fp);
        int id = register_lvar(name);
        Node *target = new_node(ND_VAR);
        target->name = name;
        target->var_id = id;
        return new_binary(ND_DECLARE, target, val);
    } else if (token == TK_DIV) {
        getNextToken(fp);
        Node *target = new_var_node(name);
        return new_binary(ND_DIV, target, val);
    } else {
        error(ERR_SYNTAX, "「で宣言する」または「でわる」が期待されています");
    }
    return NULL;
}

// simple_statement_suffix_ni ::= ...
Node *parse_simple_statement_suffix_ni(FILE *fp, char *name) {
    Node *target = new_var_node(name);

    if (token == TK_INPUT) {
        getNextToken(fp);
        Node *node = new_node(ND_INPUT);
        node->lhs = target;
        return node;
    } 
    
    // "入力する" ではない場合、値が来るはず
    Node *val = parse_value(fp);

    if (token == TK_ASSIGN) {
        getNextToken(fp);
        return new_binary(ND_ASSIGN, target, val);
    } else if (token == TK_ADD) {
        getNextToken(fp);
        return new_binary(ND_ADD, target, val);
    } else if (token == TK_MUL) {
        getNextToken(fp);
        return new_binary(ND_MUL, target, val);
    } else {
        error(ERR_SYNTAX, "「入力する」「を代入する」「をたす」「をかける」が期待されています");
    }
    return NULL;
}

// simple_statement_suffix_kara ::= (TOKEN_LITERAL | TOKEN_VARIABLE) "をひく"
Node *parse_simple_statement_suffix_kara(FILE *fp, char *name) {
    Node *target = new_var_node(name);
    Node *val = parse_value(fp);
    expect(TK_SUB, fp); // 「をひく」
    return new_binary(ND_SUB, target, val);
}

// --- 条件式 (calc.c と同じ階層構造) ---

// condition_expression ::= {ws_or_ln} condition_term { ws_or_ln "または" ws_or_ln condition_term } {ws_or_ln}
Node *parse_condition_expression(FILE *fp) {
    consumeWsOrLn(fp);
    Node *node = parse_condition_term(fp);
    consumeWsOrLn(fp);

    while (token == TK_OR) {
        getNextToken(fp);
        expectWsOrLn(fp); // 「または」の後は空白必須
        node = new_binary(ND_OR, node, parse_condition_term(fp));
        consumeWsOrLn(fp);
    }
    return node;
}

// condition_term ::= condition_factor { ws_or_ln "かつ" ws_or_ln condition_factor }
Node *parse_condition_term(FILE *fp) {
    Node *node = parse_condition_factor(fp);
    consumeWsOrLn(fp);

    while (token == TK_AND) {
        getNextToken(fp);
        expectWsOrLn(fp); // 「かつ」の後は空白必須
        node = new_binary(ND_AND, node, parse_condition_factor(fp));
        consumeWsOrLn(fp);
    }
    return node;
}

// condition_factor ::= simple_condition | "（" condition_expression "）"
Node *parse_condition_factor(FILE *fp) {
    Node *node;
    if (token == TK_LPAR) {
        getNextToken(fp);
        node = parse_condition_expression(fp);
        expect(TK_RPAR, fp);
    } 
    // simple_condition の開始トークンは TK_LITERAL か TK_VARIABLE
    else if (token == TK_LITERAL || token == TK_VARIABLE) {
        node = parse_simple_condition(fp);
    } 
    else {
        error(ERR_SYNTAX, "条件式または「（」が期待されています");
    }
    return node;
}

// simple_condition ::= (TOKEN_LITERAL | TOKEN_VARIABLE) "が" (TOKEN_LITERAL | TOKEN_VARIABLE) comparison_op
Node *parse_simple_condition(FILE *fp) {
    Node *lhs = parse_value(fp);
    expect(TK_GA, fp);
    Node *rhs = parse_value(fp);
    return parse_comparison_op(fp, lhs, rhs);
}

// comparison_op ::= "以上か" | ...
Node *parse_comparison_op(FILE *fp, Node *lhs, Node *rhs) {
    if (token == TK_OP_GE) { getNextToken(fp); return new_binary(ND_GE, lhs, rhs); }
    if (token == TK_OP_LE) { getNextToken(fp); return new_binary(ND_LE, lhs, rhs); }
    if (token == TK_OP_GT) { getNextToken(fp); return new_binary(ND_GT, lhs, rhs); }
    if (token == TK_OP_LT) { getNextToken(fp); return new_binary(ND_LT, lhs, rhs); }
    if (token == TK_OP_EQ) { getNextToken(fp); return new_binary(ND_EQ, lhs, rhs); }
    if (token == TK_OP_NE) { getNextToken(fp); return new_binary(ND_NE, lhs, rhs); }
    
    error(ERR_SYNTAX, "比較演算子（「以上か」など）が期待されています");
    return NULL;
}

// parse_value ::= TOKEN_LITERAL | TOKEN_VARIABLE
Node *parse_value(FILE *fp) {
    if (token == TK_LITERAL) {
        Node *node = new_num(atof(tokenStr));
        getNextToken(fp);
        return node;
    } else if (token == TK_VARIABLE) {
        Node *node = new_var_node(tokenStr);
        getNextToken(fp);
        return node;
    } else {
        error(ERR_SYNTAX, "数値または変数が期待されています");
    }
    return NULL;
}