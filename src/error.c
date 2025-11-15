#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "error.h"

// 行番号の初期値
int current_line = 1;

// エラー種別ごとのラベルを取得
static const char *get_error_label(ErrorType type) {
    switch (type) {
        case ERR_LEXER:    return "Lexer Error";
        case ERR_SYNTAX:   return "Syntax Error";
        case ERR_SEMANTIC: return "Semantic Error";
        case ERR_CODEGEN:  return "Codegen Error";
        case ERR_SYSTEM:   return "System Error";
        default:           return "Error";
    }
}

void error(ErrorType type, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    const char *label = get_error_label(type);

    // 標準エラー出力へ
    // \033[1;31m ... \033[0m は赤字ボールドで表示するためのコード
    fprintf(stderr, "\033[1;31m[%s]\033[0m at line %d: ", label, current_line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
    exit(1);
}