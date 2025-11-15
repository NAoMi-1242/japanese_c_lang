#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"

// ASTを再帰的に表示するデバッグ関数
void print_ast(Node *node, int depth) {
    if (node == NULL) return;

    for (int i = 0; i < depth; i++) printf("  ");

    switch (node->kind) {
        case ND_PROGRAM: printf("PROGRAM\n"); break;
        case ND_BLOCK:   printf("BLOCK\n"); break;
        case ND_IF:      printf("IF\n"); break;
        case ND_ELSEIF:  printf("ELSE IF\n"); break;
        case ND_LOOP:    printf("LOOP\n"); break;
        case ND_DECLARE: printf("DECLARE\n"); break;
        case ND_ASSIGN:  printf("ASSIGN\n"); break;
        case ND_ADD:     printf("ADD\n"); break;
        case ND_SUB:     printf("SUB\n"); break;
        case ND_MUL:     printf("MUL\n"); break;
        case ND_DIV:     printf("DIV\n"); break;
        case ND_INPUT:   printf("INPUT\n"); break;
        case ND_OUTPUT:  printf("OUTPUT\n"); break;
        case ND_VAR:     printf("VAR: %s\n", node->name); break;
        case ND_LITERAL: printf("NUM: %f\n", node->val); break;
        case ND_STR_LIT: printf("STR: %s\n", node->strVal); break;
        case ND_EQ:      printf("EQ (==)\n"); break;
        case ND_NE:      printf("NE (!=)\n"); break;
        case ND_LT:      printf("LT (<)\n"); break;
        case ND_LE:      printf("LE (<=)\n"); break;
        case ND_GT:      printf("GT (>)\n"); break;
        case ND_GE:      printf("GE (>=)\n"); break;
        case ND_AND:     printf("AND\n"); break;
        case ND_OR:      printf("OR\n"); break;
        default:         printf("UNKNOWN NODE (%d)\n", node->kind); break;
    }

    // 子ノードや次の文を表示
    if (node->cond) {
        for (int i = 0; i < depth+1; i++) printf("  ");
        printf("[cond]:\n");
        print_ast(node->cond, depth + 2);
    }
    if (node->then) {
        for (int i = 0; i < depth+1; i++) printf("  ");
        printf("[then]:\n");
        print_ast(node->then, depth + 2);
    }
    if (node->els) {
        for (int i = 0; i < depth+1; i++) printf("  ");
        printf("[else]:\n");
        print_ast(node->els, depth + 2);
    }
    
    if (node->lhs) print_ast(node->lhs, depth + 1);
    if (node->rhs) print_ast(node->rhs, depth + 1);
    
    if (node->next) print_ast(node->next, depth); // 兄弟ノードは同じインデントで
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

    // 1. Lexer初期化 (先読み)
    getNextToken(fp);

    // 2. Parser実行 (AST構築)
    printf("=== Parser Start ===\n");
    Node *root = parse_program(fp);
    printf("=== Parser End ===\n");

    // 3. AST表示 (デバッグ)
    printf("=== AST Dump ===\n");
    print_ast(root, 0);

    fclose(fp);
    return 0;
}