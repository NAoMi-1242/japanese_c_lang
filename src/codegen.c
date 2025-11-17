#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"
#include "error.h"

// --- プロトタイプ宣言 (内部関数) ---
void gen(Node *node, int depth, FILE *fp);
void gen_block(Node *node, int depth, FILE *fp);
void print_indent(int depth, FILE *fp);

// --- ヘルパー関数 ---

// インデント出力 (出力先 fp を指定)
void print_indent(int depth, FILE *fp) {
    for (int i = 0; i < depth; i++) {
        fprintf(fp, "\t");
    }
}

// --- コード生成メイン ---

// ブロック処理 (出力先 fp を指定)
void gen_block(Node *node, int depth, FILE *fp) {
    for (; node; node = node->next) {
        gen(node, depth, fp);
    }
}

// 再帰的なノード処理 (出力先 fp を指定)
void gen(Node *node, int depth, FILE *fp) {
    if (!node) return;

    switch (node->kind) {
    case ND_PROGRAM:
        fprintf(fp, "#include <stdio.h>\n");
        fprintf(fp, "int main() {\n");
        gen_block(node->next, 1, fp);
        print_indent(1, fp);
        fprintf(fp, "return 0;\n");
        fprintf(fp, "}\n");
        return;

    case ND_BLOCK:
        // スコープ管理は C 側で行われる
        gen_block(node->next, depth, fp);
        return;

    // --- 制御構文 ---
    case ND_IF:
    case ND_ELSEIF:
        if (node->kind == ND_IF) {
            print_indent(depth, fp);
            fprintf(fp, "if (");
        } else {
            fprintf(fp, " else if (");
        }

        gen(node->cond, 0, fp);
        fprintf(fp, ") {\n");
        gen_block(node->then, depth + 1, fp);
        print_indent(depth, fp);
        fprintf(fp, "}");

        if (node->els) {
            if (node->els->kind == ND_ELSEIF) {
                gen(node->els, depth, fp);
            } else {
                fprintf(fp, " else {\n");
                gen_block(node->els, depth + 1, fp);
                print_indent(depth, fp);
                fprintf(fp, "}\n");
            }
        } else {
            fprintf(fp, "\n");
        }
        return;

    case ND_LOOP:
        print_indent(depth, fp);
        fprintf(fp, "while (");
        gen(node->cond, 0, fp);
        fprintf(fp, ") {\n");
        gen_block(node->then, depth + 1, fp);
        print_indent(depth, fp);
        fprintf(fp, "}\n");
        return;

    // --- 文 ---
    
    case ND_DECLARE:
        print_indent(depth, fp);
        fprintf(fp, "double jpc_var_%d = ", node->lhs->var_id);
        gen(node->rhs, 0, fp);
        fprintf(fp, ";\n");
        return;

    case ND_ASSIGN:
        print_indent(depth, fp);
        fprintf(fp, "jpc_var_%d = ", node->lhs->var_id);
        gen(node->rhs, 0, fp);
        fprintf(fp, ";\n");
        return;

    case ND_INPUT:
        print_indent(depth, fp);
        fprintf(fp, "scanf(\"%%lf\", &jpc_var_%d);\n", node->lhs->var_id);
        return;

    case ND_OUTPUT:
        print_indent(depth, fp);
        if (node->lhs->kind == ND_STR_LIT) {
            // 文字列リテラル: Parserが生成したfmtとargsを使う
            fprintf(fp, "printf(\"%s\"", node->lhs->strVal);
            // 埋め込まれた変数のIDリストを出力
            for (int i = 0; i < node->lhs->argc; i++) {
                fprintf(fp, ", jpc_var_%d", node->lhs->args[i]);
            }
            fprintf(fp, ");\n");
        } else {
            // 通常の数値出力
            fprintf(fp, "printf(\"%%g\\n\", ");
            gen(node->lhs, 0, fp);
            fprintf(fp, ");\n");
        }
        return;

    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
        print_indent(depth, fp);
        gen(node->lhs, 0, fp); 
        switch (node->kind) {
            case ND_ADD: fprintf(fp, " += "); break;
            case ND_SUB: fprintf(fp, " -= "); break;
            case ND_MUL: fprintf(fp, " *= "); break;
            case ND_DIV: fprintf(fp, " /= "); break;
            default: break;
        }
        gen(node->rhs, 0, fp);
        fprintf(fp, ";\n");
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
        fprintf(fp, "(");
        gen(node->lhs, 0, fp);
        switch (node->kind) {
            case ND_EQ:  fprintf(fp, " == "); break;
            case ND_NE:  fprintf(fp, " != "); break;
            case ND_LT:  fprintf(fp, " < "); break;
            case ND_LE:  fprintf(fp, " <= "); break;
            case ND_GT:  fprintf(fp, " > "); break;
            case ND_GE:  fprintf(fp, " >= "); break;
            case ND_AND: fprintf(fp, " && "); break;
            case ND_OR:  fprintf(fp, " || "); break;
            default: break;
        }
        gen(node->rhs, 0, fp);
        fprintf(fp, ")");
        return;

    case ND_LITERAL:
        fprintf(fp, "%f", node->val);
        return;

    case ND_VAR:
        fprintf(fp, "jpc_var_%d", node->var_id);
        return;

    default:
        // エラー報告は stderr に行う error() 関数を呼ぶ
        error(ERR_CODEGEN, "Unknown Node Kind %d", node->kind);
    }
}

// --- エントリーポイント ---
// jpc.c から呼び出される
void codegen(Node *node, FILE *fp) {
    gen(node, 0, fp);
}