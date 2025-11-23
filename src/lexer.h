#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>
#include <stdbool.h> // bool型を使用

typedef enum {
    TK_EOF,         // ファイル終端
    TK_MAIN,        // メイン
    TK_VARIABLE,    // ”...”
    TK_LITERAL,     // 「...」 (数値)
    TK_PRINT_LIT,   // 「...」 (出力用文字列)
    TK_LPAR,        // （
    TK_RPAR,        // ）
    TK_LBRACE,      // ｛
    TK_RBRACE,      // ｝
    TK_PERIOD,      // 。
    TK_WO,          // を
    TK_NI,          // に
    TK_KARA,        // から
    TK_GA,          // が
    TK_DECLARE,     // で宣言する
    TK_DIV,         // でわる
    TK_ASSIGN,      // を代入する
    TK_ADD,         // をたす
    TK_MUL,         // をかける
    TK_SUB,         // をひく
    TK_INPUT,       // 入力する
    TK_OUTPUT,      // と出力する
    TK_LOOP,        // ループ
    TK_IF,          // もし
    TK_ELSEIF,      // ではなく
    TK_ELSE,        // ではない
    TK_OP_GE,       // 以上か
    TK_OP_LE,       // 以下か
    TK_OP_GT,       // より大きいか
    TK_OP_LT,       // より小さいか
    TK_OP_EQ,       // と一緒か
    TK_OP_NE,       // と違うか
    TK_AND,         // かつ
    TK_OR           // または
} TokenType;

typedef struct {
    TokenType type;
    char str[1024];
    int line;
    bool has_space_before;
    bool has_newline_before;
} Token;

extern Token current_token;

const char* getTokenName(TokenType type);
void getNextToken(FILE *fp);

#endif