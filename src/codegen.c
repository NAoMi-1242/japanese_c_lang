#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"

// --- ヘルパー関数 ---

// インデント出力
void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("\t");
    }
}

// --- コード生成メイン ---

void gen(Node *node, int depth);

void gen_block(Node *node, int depth) {
    for (; node; node = node->next) {
        gen(node, depth);
    }
}

void gen(Node *node, int depth) {
    if (!node) return;

    switch (node->kind) {
    case ND_PROGRAM:
        printf("#include <stdio.h>\n");
        printf("int main() {\n");
        gen_block(node->next, 1);
        print_indent(1);
        printf("return 0;\n");
        printf("}\n");
        return;

    case ND_BLOCK:
        // codegen側ではスコープ管理（巻き戻し）は不要
        gen_block(node->next, depth);
        return;

    // --- 制御構文 ---
    case ND_IF:
    case ND_ELSEIF:
        if (node->kind == ND_IF) {
            print_indent(depth);
            printf("if (");
        } else {
            printf(" else if (");
        }

        gen(node->cond, 0);
        printf(") {\n");
        gen_block(node->then, depth + 1);
        print_indent(depth);
        printf("}");

        if (node->els) {
            if (node->els->kind == ND_ELSEIF) {
                gen(node->els, depth);
            } else {
                printf(" else {\n");
                gen_block(node->els, depth + 1);
                print_indent(depth);
                printf("}\n");
            }
        } else {
            printf("\n");
        }
        return;

    case ND_LOOP:
        print_indent(depth);
        printf("while (");
        gen(node->cond, 0);
        printf(") {\n");
        gen_block(node->then, depth + 1);
        print_indent(depth);
        printf("}\n");
        return;

    // --- 文 ---
    
    case ND_DECLARE:
        print_indent(depth);
        // Parserが割り当てたIDをそのまま使う
        printf("double jpc_var_%d = ", node->lhs->var_id);
        gen(node->rhs, 0);
        printf(";\n");
        return;

    case ND_ASSIGN:
        print_indent(depth);
        printf("jpc_var_%d = ", node->lhs->var_id);
        gen(node->rhs, 0);
        printf(";\n");
        return;

    case ND_INPUT:
        print_indent(depth);
        printf("scanf(\"%%lf\", &jpc_var_%d);\n", node->lhs->var_id);
        return;

    case ND_OUTPUT:
        print_indent(depth);
        if (node->lhs->kind == ND_STR_LIT) {
            // 文字列リテラル: Parserが生成したfmtとargsを使う
            printf("printf(\"%s\"", node->lhs->strVal);
            // 埋め込まれた変数のIDリストを出力
            for (int i = 0; i < node->lhs->argc; i++) {
                printf(", jpc_var_%d", node->lhs->args[i]);
            }
            printf(");\n");
        } else {
            // 通常の数値出力
            printf("printf(\"%%g\\n\", ");
            gen(node->lhs, 0);
            printf(");\n");
        }
        return;

    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
        print_indent(depth);
        gen(node->lhs, 0); 
        switch (node->kind) {
            case ND_ADD: printf(" += "); break;
            case ND_SUB: printf(" -= "); break;
            case ND_MUL: printf(" *= "); break;
            case ND_DIV: printf(" /= "); break;
            default: break;
        }
        gen(node->rhs, 0);
        printf(";\n");
        return;

    // --- 式 ---
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_GT:
    case ND_GE:
    case ND_AND:
    case ND_OR:
        printf("(");
        gen(node->lhs, 0);
        switch (node->kind) {
            case ND_EQ:  printf(" == "); break;
            case ND_NE:  printf(" != "); break;
            case ND_LT:  printf(" < "); break;
            case ND_LE:  printf(" <= "); break;
            case ND_GT:  printf(" > "); break;
            case ND_GE:  printf(" >= "); break;
            case ND_AND: printf(" && "); break;
            case ND_OR:  printf(" || "); break;
            default: break;
        }
        gen(node->rhs, 0);
        printf(")");
        return;

    case ND_LITERAL:
        printf("%f", node->val);
        return;

    case ND_VAR:
        printf("jpc_var_%d", node->var_id);
        return;

    default:
        fprintf(stderr, "Codegen Error: Unknown Node Kind %d\n", node->kind);
        exit(1);
    }
}

void codegen(Node *node) {
    gen(node, 0);
}