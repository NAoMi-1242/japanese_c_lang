#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

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

    printf("=== Lexer Test Start: Reading %s ===\n", argv[1]);

    // ★最初のトークンを先読み
    getNextToken(fp);

    int count = 0;
    // ★ループ条件：getNextTokenの戻り値ではなく、グローバル変数をチェック
    while (current_token.type != TK_EOF) {
        printf("[%03d] Token: %-30s", ++count, getTokenName(current_token.type));
        
        if (current_token.type == TK_VARIABLE || current_token.type == TK_LITERAL || current_token.type == TK_PRINT_LIT) {
            printf(" Value: %s", current_token.str);
        }
        else if (current_token.type == TK_WS) {
            printf(" (U+3000)");
        }
        else if (current_token.type == TK_LN) {
            printf(" (\\n)");
        }
        printf("\n");

        // ★次へ進む
        getNextToken(fp);
    }

    printf("=== Lexer Test End ===\n");
    fclose(fp);
    return 0;
}