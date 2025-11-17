#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "error.h"

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
    // \033[1;31m ... \033[0m は赤字ボールドで表示するためのコード
    fprintf(stderr, "\033[1;31m[%s]\033[0m %d行目: ", label, current_line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
    exit(1);
}