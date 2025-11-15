#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

// トークン種別を文字列に変換するデバッグ用関数
const char* getTokenName(TokenType type) {
    switch (type) {
        case TK_EOF:         return "TK_EOF";
        case TK_MAIN:        return "TK_MAIN (メイン)";
        
        // 値・変数
        case TK_VARIABLE:    return "TK_VARIABLE (変数)";
        case TK_LITERAL:     return "TK_LITERAL (数値)";
        case TK_PRINT_LIT:   return "TK_PRINT_LIT (出力文字)";
        
        // 記号
        case TK_LPAR:        return "TK_LPAR (";
        case TK_RPAR:        return "TK_RPAR )";
        case TK_LBRACE:      return "TK_LBRACE {";
        case TK_RBRACE:      return "TK_RBRACE }";
        case TK_PERIOD:      return "TK_PERIOD 。";
        
        // 助詞
        case TK_WO:          return "TK_WO (を)";
        case TK_NI:          return "TK_NI (に)";
        case TK_KARA:        return "TK_KARA (から)";
        case TK_GA:          return "TK_GA (が)";
        
        // キーワード (命令)
        case TK_DECLARE:     return "TK_DECLARE (で宣言する)";
        case TK_DIV:         return "TK_DIV (でわる)";
        case TK_ASSIGN:      return "TK_ASSIGN (を代入する)";
        case TK_ADD:         return "TK_ADD (をたす)";
        case TK_MUL:         return "TK_MUL (をかける)";
        case TK_SUB:         return "TK_SUB (をひく)";
        case TK_INPUT:       return "TK_INPUT (入力する)";
        case TK_OUTPUT:      return "TK_OUTPUT (と出力する)";
        
        // 制御構文
        case TK_LOOP:        return "TK_LOOP (ループ)";
        case TK_IF:          return "TK_IF (もし)";
        case TK_ELSEIF:      return "TK_ELSEIF (ではなく)";
        case TK_ELSE:        return "TK_ELSE (ではない)";
        
        // 比較・論理
        case TK_OP_GE:       return "TK_OP_GE (以上か)";
        case TK_OP_LE:       return "TK_OP_LE (以下か)";
        case TK_OP_GT:       return "TK_OP_GT (より大きいか)";
        case TK_OP_LT:       return "TK_OP_LT (より小さいか)";
        case TK_OP_EQ:       return "TK_OP_EQ (と一緒か)";
        case TK_OP_NE:       return "TK_OP_NE (と違うか)";
        case TK_AND:         return "TK_AND (かつ)";
        case TK_OR:          return "TK_OR (または)";
        
        default:             return "UNKNOWN_TOKEN";
    }
}

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
    while (token != TK_EOF) {
        printf("[%03d] Token: %-30s", ++count, getTokenName(token));
        
        if (token == TK_VARIABLE || token == TK_LITERAL || token == TK_PRINT_LIT) {
            printf(" Value: %s", tokenStr);
        }
        printf("\n");

        // ★次へ進む
        getNextToken(fp);
    }

    printf("=== Lexer Test End ===\n");
    fclose(fp);
    return 0;
}