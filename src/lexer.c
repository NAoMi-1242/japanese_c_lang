#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lexer.h"
#include "error.h"

extern int current_line;
Token current_token;

static int pushback_buf[20]; 
static int pushback_count = 0;

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
    if (c == '\n') current_line++;
    return c;
}

void ungetCh(int c) {
    if (c == '\n') current_line--;
    if (c != EOF && pushback_count < 20) {
        pushback_buf[pushback_count++] = c;
    } else if (pushback_count >= 20) {
        error(ERR_SYSTEM, "プッシュバックバッファがオーバーフローしました");
    }
}

int checkCommentOut(FILE *fp, int c) {
    if (c == 0xEF) {
        int next1 = getCh(fp);
        if (next1 == 0xBC) {
            int next2 = getCh(fp);
            if (next2 == 0x83) { // 行コメント
                while ((c = getCh(fp)) != '\n' && c != EOF);
                return 1;
            } else if (next2 == 0x84) { // ブロックコメント
                while (1) {
                    c = getCh(fp);
                    if (c == EOF) break;
                    if (c == 0xEF) {
                        int n1 = getCh(fp);
                        if (n1 == 0xBC) {
                            int n2 = getCh(fp);
                            if (n2 == 0x84) {
                                c = getCh(fp);
                                if (c != '\n' && c != EOF) ungetCh(c);
                                break;
                            }
                            ungetCh(n2); ungetCh(n1);
                        } else { ungetCh(n1); }
                    }
                }
                return 1;
            }
            ungetCh(next2); ungetCh(next1);
            return 0;
        }
        ungetCh(next1);
    }
    return 0;
}

int readUTF8Char(FILE *fp, char *buf) {
    int c = getCh(fp);
    if (c == EOF) return 0;
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

int checkKeyword(FILE *fp, const char *expect) {
    char readBuf[100]; 
    int readCount = 0; 
    char charBuf[5];
    const char *p = expect;
    while (*p != '\0') {
        if (!readUTF8Char(fp, charBuf)) break; 
        int charLen = strlen(charBuf);
        for(int i=0; i<charLen; i++) readBuf[readCount++] = charBuf[i];
        if (strncmp(p, charBuf, strlen(charBuf)) != 0) break; 
        p += strlen(charBuf);
    }
    if (*p == '\0') return 1;
    else {
        for (int i = readCount - 1; i >= 0; i--) ungetCh((unsigned char)readBuf[i]);
        return 0;
    }
}

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

// 全角マイナス（－）を半角マイナス（-）に変換
int convertZenkakuMinus(const char *utf8_char, char *out) {
    unsigned char u0 = (unsigned char)utf8_char[0];
    unsigned char u1 = (unsigned char)utf8_char[1];
    unsigned char u2 = (unsigned char)utf8_char[2];
    // 「－」(U+FF0D) は UTF-8 で EF BC 8D
    if (u0 == 0xEF && u1 == 0xBC && u2 == 0x8D) {
        *out = '-';
        return 1;
    }
    return 0;
}

const char* getTokenName(TokenType type) {
    switch (type) {
        case TK_EOF:         return "TK_EOF";
        case TK_MAIN:        return "メイン";
        case TK_VARIABLE:    return "変数（”...”）";
        case TK_LITERAL:     return "数値リテラル";
        case TK_PRINT_LIT:   return "出力リテラル";
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

void getNextToken(FILE *fp) {
    char charBuf[5];
    // 独立したboolフラグを使用
    bool has_space = false;
    bool has_newline = false;

    // 1. スキップループ
    while (1) {
        int line_before = current_line;

        if (!readUTF8Char(fp, charBuf)) {
            current_token.type = TK_EOF;
            current_token.has_space_before = has_space;
            current_token.has_newline_before = has_newline;
            return;
        }

        // 全角空白
        if (strcmp(charBuf, "　") == 0) { 
            has_space = true; 
            continue; 
        }
        // 改行
        if (strcmp(charBuf, "\n") == 0) { 
            has_newline = true; 
            continue; 
        }
        // 半角空白・タブ
        if (charBuf[0] == ' ' || charBuf[0] == '\t') { 
            has_space = true; 
            continue; 
        }

        // コメント判定
        unsigned char u0 = (unsigned char)charBuf[0];
        if (u0 == 0xEF) {
            for (int i = strlen(charBuf) - 1; i >= 0; i--) ungetCh((unsigned char)charBuf[i]);
            int c = getCh(fp);
            
            if (checkCommentOut(fp, c)) {
                // コメントは空白扱い
                has_space = true;
                // コメント内で行が進んでいれば改行扱い
                if (current_line > line_before) {
                    has_newline = true;
                }
                continue;
            }
            ungetCh(c); 
            if (!readUTF8Char(fp, charBuf)) {
                current_token.type = TK_EOF;
                current_token.has_space_before = has_space;
                current_token.has_newline_before = has_newline;
                return;
            }
        }
        
        break;
    }

    // 2. トークンの確定
    current_token.line = current_line;
    current_token.has_space_before = has_space;
    current_token.has_newline_before = has_newline;
    strcpy(current_token.str, charBuf);

    // --- 記号・助詞・キーワード判定 ---
    if (strcmp(charBuf, "（") == 0) { current_token.type = TK_LPAR; return; }
    if (strcmp(charBuf, "）") == 0) { current_token.type = TK_RPAR; return; }
    if (strcmp(charBuf, "｛") == 0) { current_token.type = TK_LBRACE; return; }
    if (strcmp(charBuf, "｝") == 0) { current_token.type = TK_RBRACE; return; }
    if (strcmp(charBuf, "。") == 0) { current_token.type = TK_PERIOD; return; }
    if (strcmp(charBuf, "に") == 0) { current_token.type = TK_NI; return; }
    if (strcmp(charBuf, "が") == 0) { current_token.type = TK_GA; return; }
    
    if (strcmp(charBuf, "メ") == 0) { 
        if (checkKeyword(fp, "イン")) { current_token.type = TK_MAIN; return; }
        error(ERR_LEXER, "「メ」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "で") == 0) {
        if (checkKeyword(fp, "宣言する")) { current_token.type = TK_DECLARE; return; }
        if (checkKeyword(fp, "わる"))     { current_token.type = TK_DIV; return; }
        if (checkKeyword(fp, "は")) { 
            if (checkKeyword(fp, "なく")) { current_token.type = TK_ELSEIF; return; }
            if (checkKeyword(fp, "ない")) { current_token.type = TK_ELSE; return; }
            error(ERR_LEXER, "「で」で始まる不明なキーワードです -> %s", charBuf);
        }
        error(ERR_LEXER, "「で」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "を") == 0) {
        if (checkKeyword(fp, "代入する")) { current_token.type = TK_ASSIGN; return; }
        if (checkKeyword(fp, "たす"))     { current_token.type = TK_ADD; return; }
        if (checkKeyword(fp, "かける"))   { current_token.type = TK_MUL; return; }
        if (checkKeyword(fp, "ひく"))     { current_token.type = TK_SUB; return; }
        current_token.type = TK_WO; return; 
    }
    if (strcmp(charBuf, "か") == 0) {
        if (checkKeyword(fp, "ら")) { current_token.type = TK_KARA; return; }
        if (checkKeyword(fp, "つ")) { current_token.type = TK_AND; return; }
        error(ERR_LEXER, "「か」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "ル") == 0) { 
        if (checkKeyword(fp, "ープ")) { current_token.type = TK_LOOP; return; }
        error(ERR_LEXER, "「ル」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "も") == 0) { 
        if (checkKeyword(fp, "し")) { current_token.type = TK_IF; return; }
        error(ERR_LEXER, "「も」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "入") == 0) { 
        if (checkKeyword(fp, "力する")) { current_token.type = TK_INPUT; return; }
        error(ERR_LEXER, "「入」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "と") == 0) { 
        if (checkKeyword(fp, "出力する")) { current_token.type = TK_OUTPUT; return; }
        if (checkKeyword(fp, "一緒か"))   { current_token.type = TK_OP_EQ; return; }
        if (checkKeyword(fp, "違うか"))   { current_token.type = TK_OP_NE; return; }
        error(ERR_LEXER, "「と」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "ま") == 0) { 
        if (checkKeyword(fp, "たは")) { current_token.type = TK_OR; return; }
        error(ERR_LEXER, "「ま」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "以") == 0) {
        if (checkKeyword(fp, "上か")) { current_token.type = TK_OP_GE; return; }
        if (checkKeyword(fp, "下か")) { current_token.type = TK_OP_LE; return; }
        error(ERR_LEXER, "「以」で始まる不明なキーワードです -> %s", charBuf);
    }
    if (strcmp(charBuf, "よ") == 0) { 
        if (checkKeyword(fp, "り")) { 
            if (checkKeyword(fp, "大きいか")) { current_token.type = TK_OP_GT; return; }
            if (checkKeyword(fp, "小さいか")) { current_token.type = TK_OP_LT; return; }
            error(ERR_LEXER, "「より」で始まる不明なキーワードです -> %s", charBuf);
        }
        error(ERR_LEXER, "「よ」で始まる不明なキーワードです -> %s", charBuf);
    }

    // --- 変数 ---
    if (strcmp(charBuf, "”") == 0) {
        current_token.type = TK_VARIABLE;
        current_token.str[0] = '\0';
        while (1) {
            char tmpBuf[5];
            int c = getCh(fp);
            if (c == EOF) error(ERR_LEXER, "変数名の途中でファイルが終了しました");
            ungetCh(c);
            
            // 変数名内部は空白スキップしない (readUTF8Charのロジック)
            int first = getCh(fp);
            char utf8Buf[5];
            int len = 1;
            unsigned char uc = (unsigned char)first;
            if ((uc & 0xE0) == 0xC0) len = 2;
            else if ((uc & 0xF0) == 0xE0) len = 3;
            else if ((uc & 0xF8) == 0xF0) len = 4;
            utf8Buf[0] = (char)first;
            for(int i=1; i<len; i++) utf8Buf[i] = (char)getCh(fp);
            utf8Buf[len] = '\0';
            strcpy(tmpBuf, utf8Buf);

            if (strcmp(tmpBuf, "\n") == 0 || strcmp(tmpBuf, "\r") == 0) {
                error(ERR_LEXER, "変数名の引用符（”）が閉じられていません");
            }
            if (strcmp(tmpBuf, "”") == 0) break;
            strcat(current_token.str, tmpBuf);
        }
        return;
    }

    // --- リテラル ---
    if (strcmp(charBuf, "「") == 0) {
        char rawStr[1024] = ""; 
        char numStr[1024] = ""; 
        int is_pure_number = 1;
        int dot_count = 0; 

        while (1) {
            char tmpBuf[5];
            int first = getCh(fp);
            if (first == EOF) error(ERR_LEXER, "文字列リテラルの途中でEOF");
            
            // リテラル内は空白スキップしない
            char utf8Buf[5];
            int len = 1;
            unsigned char uc = (unsigned char)first;
            if ((uc & 0xE0) == 0xC0) len = 2;
            else if ((uc & 0xF0) == 0xE0) len = 3;
            else if ((uc & 0xF8) == 0xF0) len = 4;
            utf8Buf[0] = (char)first;
            for(int i=1; i<len; i++) utf8Buf[i] = (char)getCh(fp);
            utf8Buf[len] = '\0';
            strcpy(tmpBuf, utf8Buf);

            if (strcmp(tmpBuf, "\n") == 0 || strcmp(tmpBuf, "\r") == 0) {
                error(ERR_LEXER, "文字列リテラルの引用符（「）が閉じられていません");
            }
            if (strcmp(tmpBuf, "」") == 0) break;

            if (strlen(rawStr) >= 1000) error(ERR_LEXER, "文字列リテラルが長すぎます");
            strcat(rawStr, tmpBuf);

            char converted;
            if (isdigit(tmpBuf[0])) strncat(numStr, tmpBuf, 1);
            else if (tmpBuf[0] == '.') { strncat(numStr, tmpBuf, 1); dot_count++; }
            else if (convertZenkakuNum(tmpBuf, &converted)) strncat(numStr, &converted, 1);
            else if (convertZenkakuDot(tmpBuf, &converted)) { strncat(numStr, &converted, 1); dot_count++; }
            // マイナス記号（半角/全角）の処理（先頭のみ許可）
            else if (strlen(numStr) == 0 && (tmpBuf[0] == '-' || convertZenkakuMinus(tmpBuf, &converted))) { strcat(numStr, "-"); }
            else is_pure_number = 0;
        }
        
        if (is_pure_number && dot_count <= 1 && strlen(numStr) > 0 && numStr[0] != '.' && numStr[strlen(numStr)-1] != '.') {
            current_token.type = TK_LITERAL;
            strcpy(current_token.str, numStr);
        } else {
            current_token.type = TK_PRINT_LIT;
            strcpy(current_token.str, rawStr);
        }
        return;
    }

    error(ERR_LEXER, "不明なトークンです: %s", charBuf);
}