#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>

typedef enum {
    TK_EOF,         // ファイル終端
    
    // プログラム構造
    TK_MAIN,        // メイン

    // 値・変数
    TK_VARIABLE,    // ”...”
    TK_LITERAL,     // 「...」 (数値)
    TK_PRINT_LIT,   // 「...」 (出力用文字列)

    // 空白・改行
    TK_WS,          // 全角空白 (U+3000)
    TK_LN,          // 改行 (\n)

    // 記号
    TK_LPAR,        // （
    TK_RPAR,        // ）
    TK_LBRACE,      // ｛
    TK_RBRACE,      // ｝
    TK_PERIOD,      // 。

    // 助詞
    TK_WO,          // を
    TK_NI,          // に
    TK_KARA,        // から
    TK_GA,          // が

    // キーワード
    TK_DECLARE,     // で宣言する
    TK_DIV,         // でわる
    TK_ASSIGN,      // を代入する
    TK_ADD,         // をたす
    TK_MUL,         // をかける
    TK_SUB,         // をひく
    TK_INPUT,       // 入力する
    TK_OUTPUT,      // と出力する

    // 制御構文
    TK_LOOP,        // ループ
    TK_IF,          // もし
    TK_ELSEIF,      // ではなく
    TK_ELSE,        // ではない
    
    // 比較・論理
    TK_OP_GE,       // 以上か
    TK_OP_LE,       // 以下か
    TK_OP_GT,       // より大きいか
    TK_OP_LT,       // より小さいか
    TK_OP_EQ,       // と一緒か
    TK_OP_NE,       // と違うか
    TK_AND,         // かつ
    TK_OR           // または

} TokenType;

// グローバル変数（先読みトークン）
extern char tokenStr[1024];
extern TokenType token;

// トークン名文字列を返すヘルパー
const char* getTokenName(TokenType type);

// 初期化とトークン取得
void getNextToken(FILE *fp);

#endif