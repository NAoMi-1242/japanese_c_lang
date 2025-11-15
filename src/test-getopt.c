#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getoptを使うために必要

int main(int argc, char *argv[]) {
    int opt;
    char *output_file = "a.out"; // デフォルト値
    char *keep_file = NULL;      // NULLなら削除、指定があればその名前で保存

    // --- 1. オプション解析 (-o, -k) ---
    // "o:k:" の意味:
    //   o: -> -o は引数をとる (例: -o main)
    //   k: -> -k は引数をとる (例: -k temp.c)
    while ((opt = getopt(argc, argv, "o:k:")) != -1) {
        switch (opt) {
            case 'o':
                output_file = optarg; // optargに引数("main"など)が入っている
                break;
            case 'k':
                keep_file = optarg;
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-o output] [-k keep_c_file] <input_file>\n", argv[0]);
                return 1;
        }
    }

    // --- 2. 入力ファイルの取得 ---
    // optind は「オプションではない最初の引数のインデックス」
    if (optind >= argc) {
        fprintf(stderr, "Error: Input file not specified.\n");
        return 1;
    }

    char *input_file = argv[optind];

    // --- 3. 確認用出力 (実装時は削除) ---
    printf("Input File:  %s\n", input_file);
    printf("Output File: %s\n", output_file);
    if (keep_file) {
        printf("Keep C File: %s\n", keep_file);
    } else {
        printf("Keep C File: (Disabled)\n");
    }

    // ここからコンパイル処理を開始...
    // compile(input_file, output_file, keep_file);

    return 0;
}