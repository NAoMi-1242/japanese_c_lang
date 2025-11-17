# コンパイラ設定
CC = gcc
CFLAGS = -Wall -Wextra

# ターゲット名（実行ファイル名）
TARGET = jpc
LEXER_TEST = lexer-test
PARSER_TEST = parser-test

# ソースコードとヘッダファイル
SRCS = src/jpc.c src/lexer.c src/parser.c src/codegen.c src/error.c
HEADERS = src/lexer.h src/parser.h src/codegen.h src/error.h

# オブジェクトファイル
OBJS = $(SRCS:.c=.o)

# テスト用オブジェクトファイル
LEXER_TEST_OBJS = src/lexer-test.o src/lexer.o src/error.o
PARSER_TEST_OBJS = src/parser-test.o src/parser.o src/lexer.o src/error.o

# --- ルール定義 ---

all: $(TARGET)

lexer: $(LEXER_TEST)

parser: $(PARSER_TEST)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(LEXER_TEST): $(LEXER_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(PARSER_TEST): $(PARSER_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# 各ファイルのコンパイルルールと依存関係

src/jpc.o: src/jpc.c $(HEADERS)
	$(CC) $(CFLAGS) -c src/jpc.c -o src/jpc.o

# lexerはlexer.hとerror.hに依存
src/lexer.o: src/lexer.c src/lexer.h src/error.h
	$(CC) $(CFLAGS) -c src/lexer.c -o src/lexer.o

# parserはparser.h, lexer.h, error.hに依存
src/parser.o: src/parser.c src/parser.h src/lexer.h src/error.h
	$(CC) $(CFLAGS) -c src/parser.c -o src/parser.o

# codegenはcodegen.h, parser.hに依存
src/codegen.o: src/codegen.c src/codegen.h src/parser.h
	$(CC) $(CFLAGS) -c src/codegen.c -o src/codegen.o

# 【新規】error.c のコンパイルルール
src/error.o: src/error.c src/error.h
	$(CC) $(CFLAGS) -c src/error.c -o src/error.o

# テストファイルのコンパイルルール
src/lexer-test.o: src/lexer-test.c src/lexer.h src/error.h
	$(CC) $(CFLAGS) -c src/lexer-test.c -o src/lexer-test.o

src/parser-test.o: src/parser-test.c src/parser.h src/lexer.h src/error.h
	$(CC) $(CFLAGS) -c src/parser-test.c -o src/parser-test.o

clean:
	rm -f $(OBJS) $(LEXER_TEST_OBJS) $(PARSER_TEST_OBJS) $(TARGET) $(LEXER_TEST) $(PARSER_TEST)

.PHONY: all clean test lexer parser