#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ./jpc <filename.jpc>\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        return 1;
    }

    // 1. 字句解析の初期化
    getNextToken(fp);

    // 2. 構文解析 (ASTの構築)
    Node *root = parse_program(fp);
    fclose(fp);

    // 3. コード生成 (標準出力へCコードを出力)
    codegen(root);

    return 0;
}