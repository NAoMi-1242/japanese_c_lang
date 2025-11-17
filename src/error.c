#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "error.h"
#include "lexer.h" // token_line のために必要

// 行番号の初期値
int current_line = 1;

// エラー種別ごとのラベルを取得
static const char *get_error_label(ErrorType type) {
    switch (type) {
        case ERR_LEXER:    return "字句解析エラー";
        case ERR_SYNTAX:   return "構文解析エラー";
        case ERR_SEMANTIC: return "意味解析エラー";
        case ERR_CODEGEN:  return "コード生成エラー";
        case ERR_SYSTEM:   return "システムエラー";
        default:           return "エラー";
    }
}

void error(ErrorType type, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    const char *label = get_error_label(type);

    // 標準エラー出力へ
    fprintf(stderr, "\033[1;31m[%s]\033[0m ", label); // 赤字ボールド

    // ERR_SYSTEM の場合は行番号を表示しない
    if (type != ERR_SYSTEM) {
        // Lexer, Syntax, Semantic エラーは行番号を表示
        fprintf(stderr, "%d行目: ", token_line);
    }

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
    exit(1);
}