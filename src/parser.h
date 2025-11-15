#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

// ノードの種類
typedef enum {
    ND_PROGRAM,     // プログラム全体
    ND_BLOCK,       // ブロック { ... }
    ND_IF,          // もし
    ND_ELSEIF,      // ではなく
    ND_LOOP,        // ループ
    ND_DECLARE,     // 宣言
    ND_ASSIGN,      // 代入
    ND_ADD,         // +
    ND_SUB,         // -
    ND_MUL,         // *
    ND_DIV,         // /
    ND_INPUT,       // 入力
    ND_OUTPUT,      // 出力
    ND_VAR,         // 変数
    ND_LITERAL,     // 数値
    ND_STR_LIT,     // 文字列
    
    // 比較演算
    ND_EQ,          // と一緒か (==)
    ND_NE,          // (!=)
    ND_LT,          // より小さいか (<)
    ND_LE,          // 以下か (<=)
    ND_GT,          // より大きいか (>)
    ND_GE,          // 以上か (>=)
    
    // 論理演算
    ND_AND,         // かつ
    ND_OR           // または
} NodeKind;

// 構造体の前方宣言 (これにより中で Node* が使える)
typedef struct Node Node;

// ノードの定義
struct Node {
    NodeKind kind;  // ノードの種類
    Node *next;     // 次の文（リスト構造用）

    Node *lhs;      // 左辺 (Left Hand Side)
    Node *rhs;      // 右辺 (Right Hand Side)
    
    // 制御構文用
    Node *cond;     // 条件式
    Node *then;     // 実行ブロック
    Node *els;      // elseブロック

    // 値・名前用
    char *name;     // 変数名 ("A"など)
    char *strVal;   // 文字列リテラルの中身
    double val;     // 数値リテラルの値
};

// 関数プロトタイプ宣言
Node *parse_program(FILE *fp);

#endif