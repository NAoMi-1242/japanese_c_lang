#ifndef ERROR_H
#define ERROR_H

// エラー種別の定義
typedef enum {
    ERR_LEXER,    // 字句解析エラー (不正な文字、閉じ忘れなど)
    ERR_SYNTAX,   // 構文エラー (文法違反、予期しないトークン)
    ERR_SEMANTIC, // 意味解析エラー (未定義変数、二重定義など)
    ERR_CODEGEN,  // コード生成エラー (実装されていないノードなど)
    ERR_SYSTEM    // システムエラー (メモリ不足など)
} ErrorType;

// グローバルな行番号カウンタ
extern int current_line;

// エラー報告関数
// type: エラーの種類
// fmt: フォーマット文字列
void error(ErrorType type, const char *fmt, ...);

#endif