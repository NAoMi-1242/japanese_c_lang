#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "error.h"

// グローバル変数の実体定義
extern int current_line; // error.hでextern宣言されたものを使う

char tokenStr[1024];
TokenType token;
int token_line; // トークンが出現した行番号

// バッファリング用 (その他省略)
static int pushback_buf[20]; 
static int pushback_count = 0;

// --- 基本読み込み関数 ---
int getCh(FILE *fp) {
    int c;
    if (pushback_count > 0) {
        c = pushback_buf[--pushback_count];
    } else {
        if (fp == NULL) return EOF;
        do {
            c = fgetc(fp);
        } while (c == '\r');
    }
    
    if (c == '\n') {
        current_line++;
    }
    return c;
}

void ungetCh(int c) {
    if (c == '\n') {
        current_line--;
    }

    if (c != EOF && pushback_count < 20) {
        pushback_buf[pushback_count++] = c;
    } else if (pushback_count >= 20) {
        error(ERR_SYSTEM, "プッシュバックバッファがオーバーフローしました");
    }
}

// --- コメント処理関数 ---
int checkCommentOut(FILE *fp, int c) {
    // UTF-8: ＃ = EF BC 83, ＄ = EF BC 84
    if (c == 0xEF) {
        int next1 = getCh(fp);
        if (next1 == 0xBC) {
            int next2 = getCh(fp);
            
            // --- 行コメント: ＃ (EF BC 83) ---
            if (next2 == 0x83) { 
                // 改行まで読んで完全に消費（改行も含めてコメント扱い）
                while ((c = getCh(fp)) != '\n' && c != EOF);
                return 1; // スキップ完了
            }
            
            // --- ブロックコメント: ＄ (EF BC 84) ---
            else if (next2 == 0x84) { 
                while (1) {
                    c = getCh(fp);
                    if (c == EOF) break;
                    // 終了記号「＄」チェック
                    if (c == 0xEF) {
                        int n1 = getCh(fp);
                        if (n1 == 0xBC) {
                            int n2 = getCh(fp);
                            if (n2 == 0x84) {
                                // 終了＄の直後の改行も消費
                                c = getCh(fp);
                                if (c != '\n' && c != EOF) {
                                    ungetCh(c); // 改行以外なら戻す
                                }
                                break;
                            }
                            ungetCh(n2); ungetCh(n1);
                        } else { ungetCh(n1); }
                    }
                }
                return 1; // スキップ完了
            }
            
            // 違った場合
            ungetCh(next2); ungetCh(next1);
            return 0; 
        }
        ungetCh(next1);
        return 0; 
    }
    return 0; 
}

// --- 文字読み込み関数 ---
// skip_space: 0=すべて返す, 1=半角空白/タブのみスキップ
int readUTF8Char(FILE *fp, char *buf, int skip_space) {
    int c;

    while (1) {
        c = getCh(fp);
        if (c == EOF) return 0;

        // 半角空白・タブのスキップ（skip_space >= 1の場合）
        if (skip_space >= 1) {
            if (c == ' ' || c == '\t') {
                continue;
            }
        }

        break;
    }

    unsigned char uc = (unsigned char)c;
    int len = 1;
    if ((uc & 0xE0) == 0xC0) len = 2;
    else if ((uc & 0xF0) == 0xE0) len = 3;
    else if ((uc & 0xF8) == 0xF0) len = 4;

    buf[0] = (char)c;
    for (int i = 1; i < len; i++) {
        int next = getCh(fp);
        if (next == EOF) break; 
        buf[i] = (char)next;
    }
    buf[len] = '\0';
    return 1;
}

// キーワード完全一致チェック関数
int checkKeyword(FILE *fp, const char *expect) {
    char readBuf[100]; 
    int readCount = 0; 
    char charBuf[5];
    const char *p = expect;
    
    while (*p != '\0') {
        if (!readUTF8Char(fp, charBuf, 1)) break;  // 半角空白・タブをスキップ 

        int charLen = strlen(charBuf);
        for(int i=0; i<charLen; i++) {
            readBuf[readCount++] = charBuf[i]; 
        }

        if (strncmp(p, charBuf, strlen(charBuf)) != 0) break; 

        p += strlen(charBuf);
    }

    if (*p == '\0') {
        return 1;
    } else {
        for (int i = readCount - 1; i >= 0; i--) {
            ungetCh((unsigned char)readBuf[i]);
        }
        return 0;
    }
}

// ヘルパー: 全角数字 -> 半角
int convertZenkakuNum(const char *utf8_char, char *out) {
    unsigned char u0 = (unsigned char)utf8_char[0];
    unsigned char u1 = (unsigned char)utf8_char[1];
    unsigned char u2 = (unsigned char)utf8_char[2];

    if (u0 == 0xEF && u1 == 0xBC && (u2 >= 0x90 && u2 <= 0x99)) {
        *out = u2 - 0x90 + '0';
        return 1;
    }
    return 0; 
}

// ヘルパー: 全角ドット -> 半角
int convertZenkakuDot(const char *utf8_char, char *out) {
    unsigned char u0 = (unsigned char)utf8_char[0];
    unsigned char u1 = (unsigned char)utf8_char[1];
    unsigned char u2 = (unsigned char)utf8_char[2];

    if (u0 == 0xEF && u1 == 0xBC && u2 == 0x8E) {
        *out = '.';
        return 1;
    }
    return 0;
}

// トークン種別を文字列に変換するデバッグ用関数
const char* getTokenName(TokenType type) {
    switch (type) {
        case TK_EOF:         return "TK_EOF";
        case TK_MAIN:        return "メイン";
        case TK_VARIABLE:    return "変数（”...”）";
        case TK_LITERAL:     return "数値リテラル";
        case TK_PRINT_LIT:   return "出力リテラル";
        case TK_WS:          return "全角空白（　）";
        case TK_LN:          return "改行（\\n）";
        case TK_LPAR:        return "（";
        case TK_RPAR:        return "）";
        case TK_LBRACE:      return "｛";
        case TK_RBRACE:      return "｝";
        case TK_PERIOD:      return "。";
        case TK_WO:          return "を";
        case TK_NI:          return "に";
        case TK_KARA:        return "から";
        case TK_GA:          return "が";
        case TK_DECLARE:     return "で宣言する";
        case TK_DIV:         return "でわる";
        case TK_ASSIGN:      return "を代入する";
        case TK_ADD:         return "をたす";
        case TK_MUL:         return "をかける";
        case TK_SUB:         return "をひく";
        case TK_INPUT:       return "入力する";
        case TK_OUTPUT:      return "と出力する";
        case TK_LOOP:        return "ループ";
        case TK_IF:          return "もし";
        case TK_ELSEIF:      return "ではなく";
        case TK_ELSE:        return "ではない";
        case TK_OP_GE:       return "以上か";
        case TK_OP_LE:       return "以下か";
        case TK_OP_GT:       return "より大きいか";
        case TK_OP_LT:       return "より小さいか";
        case TK_OP_EQ:       return "と一緒か";
        case TK_OP_NE:       return "と違うか";
        case TK_AND:         return "かつ";
        case TK_OR:          return "または";
        default:             return "UNKNOWN_TOKEN";
    }
}

// --- メイン解析関数 ---
void getNextToken(FILE *fp) {
    char charBuf[5];
    
    // トークン開始時の行番号を記録
    token_line = current_line;
    
    // コメントを明示的にスキップ
    while (1) {
        if (!readUTF8Char(fp, charBuf, 1)) {  // 半角空白・タブをスキップ
            token = TK_EOF;
            return;
        }
        
        // コメント判定（最初の1文字を読んだ時点でチェック）
        unsigned char u0 = (unsigned char)charBuf[0];
        if (u0 == 0xEF) {
            // pushback してcheckCommentOutで判定
            for (int i = strlen(charBuf) - 1; i >= 0; i--) {
                ungetCh((unsigned char)charBuf[i]);
            }
            int c = getCh(fp);
            if (checkCommentOut(fp, c)) {
                continue;  // コメントだった場合、次のトークンへ
            }
            ungetCh(c);  // コメントでなければ戻す
            
            // 再度読み直し
            if (!readUTF8Char(fp, charBuf, 1)) {
                token = TK_EOF;
                return;
            }
        }
        
        break;  // コメントでない文字が来たらループ終了
    }
    
    strcpy(tokenStr, charBuf);

    // --- 改行 ---
    if (strcmp(charBuf, "\n") == 0) { token = TK_LN; return; }

    // --- 全角空白 (E3 80 80) ---
    if (strcmp(charBuf, "　") == 0) { token = TK_WS; return; }

    // --- 記号 ---
    if (strcmp(charBuf, "（") == 0) { token = TK_LPAR; return; }
    if (strcmp(charBuf, "）") == 0) { token = TK_RPAR; return; }
    if (strcmp(charBuf, "｛") == 0) { token = TK_LBRACE; return; }
    if (strcmp(charBuf, "｝") == 0) { token = TK_RBRACE; return; }
    if (strcmp(charBuf, "。") == 0) { token = TK_PERIOD; return; }

    // --- 助詞 ---
    if (strcmp(charBuf, "に") == 0) { token = TK_NI; return; }
    if (strcmp(charBuf, "が") == 0) { token = TK_GA; return; }
    
    // --- キーワード分岐 ---
    if (strcmp(charBuf, "メ") == 0) { 
        if (checkKeyword(fp, "イン")) { token = TK_MAIN; return; }
        error(ERR_LEXER, "「メ」で始まる不明なキーワードです -> %s", charBuf);
    }

    if (strcmp(charBuf, "で") == 0) {
        if (checkKeyword(fp, "宣言する")) { token = TK_DECLARE; return; }
        if (checkKeyword(fp, "わる"))     { token = TK_DIV; return; }
        
        if (checkKeyword(fp, "は")) { 
            if (checkKeyword(fp, "なく")) { token = TK_ELSEIF; return; }
            if (checkKeyword(fp, "ない")) { token = TK_ELSE; return; }
            error(ERR_LEXER, "「で」で始まる不明なキーワードです -> %s", charBuf);
        }
        error(ERR_LEXER, "「で」で始まる不明なキーワードです -> %s", charBuf);
    }

    if (strcmp(charBuf, "を") == 0) {
        if (checkKeyword(fp, "代入する")) { token = TK_ASSIGN; return; }
        if (checkKeyword(fp, "たす"))     { token = TK_ADD; return; }
        if (checkKeyword(fp, "かける"))   { token = TK_MUL; return; }
        if (checkKeyword(fp, "ひく"))     { token = TK_SUB; return; }
        token = TK_WO; return; 
    }
    
    if (strcmp(charBuf, "か") == 0) {
        if (checkKeyword(fp, "ら")) { token = TK_KARA; return; }
        if (checkKeyword(fp, "つ")) { token = TK_AND; return; }
        error(ERR_LEXER, "「か」で始まる不明なキーワードです -> %s", charBuf);
    }

    if (strcmp(charBuf, "ル") == 0) { 
        if (checkKeyword(fp, "ープ")) { token = TK_LOOP; return; }
        error(ERR_LEXER, "「ル」で始まる不明なキーワードです -> %s", charBuf);
    }
    
    if (strcmp(charBuf, "も") == 0) { 
        if (checkKeyword(fp, "し")) { token = TK_IF; return; }
        error(ERR_LEXER, "「も」で始まる不明なキーワードです -> %s", charBuf);
    }
    
    if (strcmp(charBuf, "入") == 0) { 
        if (checkKeyword(fp, "力する")) { token = TK_INPUT; return; }
        error(ERR_LEXER, "「入」で始まる不明なキーワードです -> %s", charBuf);
    }
    
    if (strcmp(charBuf, "と") == 0) { 
        if (checkKeyword(fp, "出力する")) { token = TK_OUTPUT; return; }
        if (checkKeyword(fp, "一緒か"))   { token = TK_OP_EQ; return; }
        if (checkKeyword(fp, "違うか"))   { token = TK_OP_NE; return; }
        error(ERR_LEXER, "「と」で始まる不明なキーワードです -> %s", charBuf);
    }
    
    if (strcmp(charBuf, "ま") == 0) { 
        if (checkKeyword(fp, "たは")) { token = TK_OR; return; }
        error(ERR_LEXER, "「ま」で始まる不明なキーワードです -> %s", charBuf);
    }
    
    if (strcmp(charBuf, "以") == 0) {
        if (checkKeyword(fp, "上か")) { token = TK_OP_GE; return; }
        if (checkKeyword(fp, "下か")) { token = TK_OP_LE; return; }
        error(ERR_LEXER, "「以」で始まる不明なキーワードです -> %s", charBuf);
    }
    
    if (strcmp(charBuf, "よ") == 0) { 
        if (checkKeyword(fp, "り")) { 
            if (checkKeyword(fp, "大きいか")) { token = TK_OP_GT; return; }
            if (checkKeyword(fp, "小さいか")) { token = TK_OP_LT; return; }
            error(ERR_LEXER, "「より」で始まる不明なキーワードです -> %s", charBuf);
        }
        error(ERR_LEXER, "「よ」で始まる不明なキーワードです -> %s", charBuf);
    }

    // --- 変数 ("...") ---
    if (strcmp(charBuf, "”") == 0) {
        token = TK_VARIABLE;
        tokenStr[0] = '\0';
        while (1) {
            if (!readUTF8Char(fp, charBuf, 0)) { 
                error(ERR_LEXER, "変数名の途中でファイルが終了しました");
            }
            if (strcmp(charBuf, "\n") == 0 || strcmp(charBuf, "\r") == 0) {
                error(ERR_LEXER, "変数名の引用符（”）が閉じられていません");
            }
            if (strcmp(charBuf, "”") == 0) break;
            strcat(tokenStr, charBuf);
        }
        return;
    }

    // --- リテラル (「...」) ---
    if (strcmp(charBuf, "「") == 0) {
        char rawStr[1024] = ""; 
        char numStr[1024] = ""; 
        int is_pure_number = 1;
        int dot_count = 0; 

        while (1) {
            if (!readUTF8Char(fp, charBuf, 0)) {
                error(ERR_LEXER, "文字列リテラルの途中でファイルが終了しました");
            }
            if (strcmp(charBuf, "\n") == 0 || strcmp(charBuf, "\r") == 0) {
                error(ERR_LEXER, "文字列リテラルの引用符（「）が閉じられていません");
            }
            if (strcmp(charBuf, "」") == 0) break;

            if (strlen(rawStr) >= 1000) {
                error(ERR_LEXER, "文字列リテラルが長すぎます");
            }

            strcat(rawStr, charBuf);

            char converted;
            if (isdigit(charBuf[0])) {
                strncat(numStr, charBuf, 1);
            }
            else if (charBuf[0] == '.') {
                strncat(numStr, charBuf, 1);
                dot_count++;
            }
            else if (convertZenkakuNum(charBuf, &converted)) {
                strncat(numStr, &converted, 1);
            }
            else if (convertZenkakuDot(charBuf, &converted)) {
                strncat(numStr, &converted, 1);
                dot_count++;
            }
            else {
                is_pure_number = 0;
            }
        }
        
        if (is_pure_number && dot_count <= 1) {
            int len = strlen(numStr);
            if (len > 0 && numStr[0] != '.' && numStr[len-1] != '.') {
                token = TK_LITERAL;
                strcpy(tokenStr, numStr);
            } else {
                token = TK_PRINT_LIT;
                strcpy(tokenStr, rawStr);
            }
        } else {
            token = TK_PRINT_LIT;
            strcpy(tokenStr, rawStr);
        }
        return;
    }

    error(ERR_LEXER, "不明なトークンです: %s", charBuf);
}