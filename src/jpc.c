#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // getopt用
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "error.h" // エラー処理用

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [options] <input.jpc>\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o <filename>  コンパイルして実行ファイル <filename> を生成します。\n");
    fprintf(stderr, "                 指定されない場合、Cコードを標準出力に出力します。\n");
    fprintf(stderr, "  -k <filename>  中間Cファイルを <filename> として保存します。\n");
}

int main(int argc, char *argv[]) {
    char *output_exec = NULL;
    char *c_file_name = "_tmp_jpc.c"; // デフォルトCファイル名
    int compile_flag = 0; // -o が指定されたか
    int keep_flag = 0;    // -k が指定されたか
    char *input_file = NULL;
    int opt;

    // 1. オプション解析
    while ((opt = getopt(argc, argv, "o:k:")) != -1) {
        switch (opt) {
            case 'o':
                output_exec = optarg;
                compile_flag = 1; 
                break;
            case 'k':
                c_file_name = optarg; 
                keep_flag = 1;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // ... (入力ファイル取得、fopen は変更なし) ...
    if (optind >= argc) {
        error(ERR_SYSTEM, "入力ファイルが指定されていません。\nUsage: ./jpc [options] <input.jpc>");
    }
    input_file = argv[optind];

    FILE *fp = fopen(input_file, "r");
    if (fp == NULL) {
        error(ERR_SYSTEM, "ファイルを開けません: %s", input_file);
    }

    // 3. 構文解析
    getNextToken(fp);
    Node *root = parse_program(fp);
    fclose(fp);

    // 4. Cコード出力先の決定（デフォルトは標準出力）
    FILE *c_fp = stdout;

    if (compile_flag || keep_flag) {
        c_fp = fopen(c_file_name, "w");
        if (c_fp == NULL) {
            error(ERR_SYSTEM, "Cファイルを作成できません: %s", c_file_name);
        }
    }

    // 5. コード生成
    codegen(root, c_fp);

    // ファイルに出力した場合のみ閉じる
    if (c_fp != stdout) {
        fclose(c_fp);
    }

    // 6. コンパイル実行 (-o が指定された場合のみ)
    if (compile_flag) {
        char compile_cmd[1024];
        snprintf(compile_cmd, sizeof(compile_cmd), "gcc -o %s %s", output_exec, c_file_name);
        
        if (system(compile_cmd) != 0) {
            error(ERR_SYSTEM, "GCCコンパイルに失敗しました。");
        }
    } 

    // 7. 中間ファイルの削除
    if (compile_flag && !keep_flag) {
        remove(c_file_name); // _tmp_jpc.c を削除
    }

    return 0;
}