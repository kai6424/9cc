#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// トークンの型を表す値
enum {
  TK_NUM = 256, // 整数トークン
  TK_EOF,       // 入力の終わりを表すトークン
};

// ノードの型を表す値
enum {
  ND_NUM = 256, // 整数のノードの型
};

// トークンの型
typedef struct {
  int ty;       // トークンの型
  int val;      // tyがTK_NUMの場合、その数値
  char *input;  // トークン文字列（エラーメッセージ用）
} Token;

// ノードの型
typedef struct Node {
  int ty;           // 演算子かND_NUM
  struct Node *lhs; // 左辺
  struct Node *rhs; // 右辺
  int val;          // tyがND_NUMの場合のみ使う
} Node;

// ベクタの型
typedef struct {
  void **data;  // データの本体
  int capacity; // バッファの大きさ
  int len;      // 追加済み要素の数
} Vector;

// トークナイズした結果のトークン列はこの配列に保存する
// 100個以上のトークンは来ないものとする
Token tokens[100];

// トークン列の位置
int pos = 0;

void tokenize(char *);
Node *new_node(int, Node *, Node *);
Node *new_node_num(int);
Vector *new_vector();
Vector vec_push(Vector *, void *);
int consume(int);
Node *add();
Node *mul();
Node *term();
void gen(Node *);
void error(int);
void error_with_message(char *, char *);
int expect(int, int, int);
void runtest();

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  // testオプションの場合、テストコードを実行
  if (strcmp(argv[1], "-test") == 0) {
    runtest();
    return 0;
  }

  // トークナイズしてパースする
  tokenize(argv[1]);
  Node *node = add();

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  // 抽象構文木を下りながらコード生成する
  gen(node);

  // スタックトップに式全体が残っているはずなので
  // それをRAXにロードして関数からの返り値とする
  printf("  pop rax\n");

  printf("  ret\n");
  return 0;
}

// pが指している文字列をトークンに分割してtokensに保存する
void tokenize(char *p) {
  int i = 0;
  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')') {
      tokens[i].ty = *p;
      tokens[i].input = p;
      i++;
      p++;
      continue;
    }

    if (isdigit(*p)) {
      tokens[i].ty = TK_NUM;
      tokens[i].input = p;
      tokens[i].val = strtol(p, &p, 10);
      i++;
      continue;
    }

    fprintf(stderr, "トークナイズできません: %s\n", p);
    exit(1);
  }

  tokens[i].ty = TK_EOF;
  tokens[i].input = p;
}

// 2項演算子ノードを生成する
Node *new_node(int ty, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));
  node->ty = ty;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

// 数値ノードを生成する
Node *new_node_num(int val) {
  Node *node = malloc(sizeof(Node));
  node->ty = ND_NUM;
  node->val = val;
  return node;
}

// 新しいベクタを生成する
Vector *new_vector() {
  Vector *vec = malloc(sizeof(Vector));
  vec->data = malloc(sizeof(void *) * 16);
  vec->capacity = 16;
  vec->len = 0;
  return vec;
}

// ベクタに要素を追加する
Vector vec_push(Vector *vec, void *elem) {
  if (vec->capacity == vec->len) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, sizeof(void *) * vec->capacity);
  }
  vec->data[vec->len++] = elem;
}

// トークンを1つ読み込む
int consume(int ty) {
  if (tokens[pos].ty != ty)
    return 0;
  pos++;
  return 1;
}

// 加減算ノードを生成する
Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume('+'))
      node = new_node('+', node, mul());
    else if (consume('-'))
      node = new_node('-', node, mul());
    else
      return node;
  }
}

// 乗除算ノードを生成する
Node *mul() {
  Node *node = term();

  for (;;) {
    if (consume('*'))
      node = new_node('*', node, term());
    else if (consume('/'))
      node = new_node('/', node, term());
    else
      return node;
  }
}

// ノード項を生成する
Node *term() {
  if (consume('(')) {
    Node *node = add();
    if (!consume(')'))
      error_with_message("開きカッコに対応する閉じカッコがありません: %s", tokens[pos].input);
      return node;
  }

  if (tokens[pos].ty == TK_NUM)
    return new_node_num(tokens[pos++].val);

  error_with_message("数値でも開きカッコでもないトークンです: %s", tokens[pos].input);
}

// コードを生成する
void gen(Node *node) {
  if (node->ty == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->ty) {
    case '+':
      printf("  add rax, rdi\n");
      break;
    case '-':
      printf("  sub rax, rdi\n");
      break;
    case '*':
      printf("  mul rdi\n");
      break;
    case '/':
      printf("  mov rdx, 0\n");
      printf("  div rdi\n");
  }

  printf("  push rax\n");
}

// エラーを報告するための関数
void error(int i) {
  fprintf(stderr, "予期せぬトークンです: %s\n", tokens[i].input);
  exit(1);
}

// メッセージ付きエラーを報告するための関数
void error_with_message(char *str1, char *str2) {
  fprintf(stderr, str1, str2);
  exit(1);
}

// 値を比較するための関数
int expect(int line, int expected, int actual) {
  if (expected == actual)
    return 0;
  fprintf(stderr, "%d: %d expected, but got %d\n", line, expected, actual);
  exit(1);
}

// ベクタ関数をテストするための関数
void runtest() {
  Vector *vec = new_vector();
  expect(__LINE__, 0, vec->len);

  for (int i = 0; i < 100; i++)
    vec_push(vec, (void *)i);

  expect(__LINE__, 100, vec->len);
  expect(__LINE__, 0, (int)vec->data[0]);
  expect(__LINE__, 50, (int)vec->data[50]);
  expect(__LINE__, 99, (int)vec->data[99]);

  printf("OK\n");
}
