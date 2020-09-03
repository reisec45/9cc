#include "9cc.h"

char *user_input;
Token *token;
static VarList *locals;

//Reports an error and exit
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// Reports an error location and exit.
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s",pos,"");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}


// Check if next token is expected symbol 
bool consume(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    return false;
  token = token->next;
  return true;
}

Token *consume_ident() {
  if (token->kind != TK_IDENT) 
    return NULL;
  
  Token *t = token;
  token = token->next;
  return t;
}

// Check if next token is expected symbol 
void expect(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    error_at(token->str,"expected '%c'",op);
  token = token->next;
}

// Check if next token is num 
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str,"expected a number");
  int val = token->val;
  token = token->next;
  return val;
}

bool at_eof() {
  return token->kind == TK_EOF;
}

// Create new token 
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

bool startswith(char *p, char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

bool is_alpha(char c ) {
  return ('a' <= c && c <='z') || ('A' <= c && c <= 'Z') || c == '_';
}

bool is_alnum(char c) {
  return is_alpha(c) || ('0' <= c && c <= '9');
}
static char *starts_with_reserved(char *p) {
  static char *kw[] = {"return", "if", "while", "for", "int"};

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    int len = strlen(kw[i]);
    if (startswith(p, kw[i]) && !is_alnum(p[len]))
      return kw[i];
  }
  
  static char *ops[] = {"==", "!=", "<=", ">="};

  for (int i = 0; i < sizeof(ops) / sizeof(*ops); i++) {
    int len = strlen(ops[i]);
    if (startswith(p, ops[i]))
      return ops[i];
  }
  return NULL;
}


// Tokenize Input
Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p){
    // skip space
    if (isspace(*p)) {
      p++;
      continue;
    }
    char *kw = starts_with_reserved(p);
    if (kw) {
      int len = strlen(kw);
      cur = new_token(TK_RESERVED, cur, p, len);
      p += len;
      continue;
    }
    
//    if (startswith(p,"if") && !is_alnum(p[2])) {
//      cur = new_token(TK_RESERVED, cur, p, 2);
//      p += 2;
//      continue;
//    }
//    if (startswith(p,"else") && !is_alnum(p[4])) {
//      cur = new_token(TK_RESERVED, cur, p, 4);
//      p += 4;
//      continue;
//    }
//
//
//   if (startswith(p,"for") && !is_alnum(p[3])) {
//      cur = new_token(TK_RESERVED, cur, p, 3);
//      p += 3;
//      continue;
//    }
//
//   if (startswith(p,"while") && !is_alnum(p[5])) {
//      cur = new_token(TK_RESERVED, cur, p, 5);
//      p += 5;
//      continue;
//    }
//
//    if (startswith(p, "return") && !is_alnum(p[6])) {
//      cur = new_token(TK_RESERVED, cur, p, 6);
//      p += 6;
//      continue;
//    }
//
//
//    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">=")) {
//      cur = new_token(TK_RESERVED, cur, p, 2);
//      p += 2;
//      continue;
//    }
    if (strchr("+-*/()<>;={},&", *p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }
  
    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    if (is_alpha(*p)) {
      char *q = p++;
      while (is_alnum(*p))
        p++;
      cur = new_token(TK_IDENT, cur, q, p - q);
      continue;
    }

    error_at(p,"Invalid token");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}

//
// Parser
//

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node *node = calloc(1,sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}


LVar *find_lvar(Token *tok) {
  for (VarList *vl = locals; vl; vl = vl->next) {
    LVar *var = vl->var;
    if (strlen(var->name) == tok->len && !strncmp(tok->str, var->name, tok->len))
      return var;
  }
  return NULL;
}

Node *new_branch(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node *new_unary(NodeKind kind, Node *expr) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = expr;
  return node;
}

char *duplicate(char *str, int len) {

    char *buffer = malloc(len + 1);
    memcpy(buffer, str, len);
    buffer[len] = '\0';

    return buffer;
}

char *expect_ident() { 
  if (token->kind != TK_IDENT)
    error_at(token->str, "expected an identifier");
  char *s = duplicate(token->str, token->len);
  token = token->next;
  return s;
}

static LVar *new_lvar(char *name) {
  LVar *var = calloc(1, sizeof(LVar));
  var->name = name;

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = var;
  vl->next = locals;
  locals = vl;
  return var;
}

static Function *function();
static Node *stmt();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *primary();


// program = function*
Function *program() {
  Function head = {};
  Function *cur = &head;

  while (!at_eof()){
    cur->next = function();
    cur = cur->next;
  }
  return head.next;
}

static VarList *read_func_params(void) {
  if (consume(")"))
    return NULL;

  VarList *head = calloc(1, sizeof(VarList));
  head->var = new_lvar(expect_ident());
  VarList *cur = head;

  while (!consume(")")) {
    expect(",");
    cur->next = calloc(1, sizeof(VarList));
    cur->next->var = new_lvar(expect_ident());
    cur = cur->next;
  }
  return head;
}

// function = basetype ident "(" params? ")" "{" stmt* "}"
// params = ident ( "," ident)*
static Function *function() {
  locals = NULL;
  Function *fn = calloc(1, sizeof(Function));
  fn->name = expect_ident();

  expect("(");
  fn->params = read_func_params();
  expect("{");
  Node head = {};
  Node *cur = &head;

  while (!consume("}")){
    cur->next = stmt();
    cur = cur->next;
  }
  fn->node = head.next;
  fn->locals = locals;
  
  return fn;
  
}

// stmt = expr ";" | "return" expr ";" | "if" "(" expr ")" stmt | "for" "(" expr? ";" expr? ";" expr? ")" stmt | "while" "(" expr ")" stmt | "{" stmt "}"
static Node *stmt() {
  if (consume("return")) {
    Node *node = new_unary(ND_RETURN, expr());
    expect(";");
    return node;
  }
  if (consume("if")) {
    Node *node = new_branch(ND_IF);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();

    if (consume("else")) 
      node->els = stmt();
    return node;
  }
  if (consume("while")) {
    Node *node = new_branch(ND_WHILE);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();

    return node;
  }
  if (consume("for")) {
    Node *node = new_branch(ND_FOR);
    expect("(");
    if (!consume(";")) {
      node->init = expr();
      expect(";");
    }
    if (!consume(";")) {
      node->cond = expr();
      expect(";");
    }
    if (!consume(")")) {
      node->inc = expr();
      expect(")");
    }
    node->then = stmt();
    return node;
  }
  if (consume("{")) {
    Node head = {};
    Node *cur = &head;
    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }
    Node *node = new_branch(ND_BLOCK);
    node->body = head.next;
    return node;
  }
  else {
    Node *node = expr();
    expect(";");
    return node;
  }
}

// expr = assign
static Node *expr() {
  return assign();
}

// assign = equality ("=" assign)?
static Node *assign() {
  Node *node = equality();

  if (consume("=")){
    node = new_node(ND_ASSIGN, node, assign());
  }

  return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("=="))
      node = new_node(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_node(ND_NE, node, relational());
    else
      return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<"))
      node = new_node(ND_LT, node, add());
    else if (consume("<="))
      node = new_node(ND_LE, node, add());
    else if (consume(">"))
      node = new_node(ND_LT, add(), node);
    else if (consume(">="))
      node = new_node(ND_LE, add(), node);
    else
      return node;
  }
}

// add = mul ("+" mul | "-" mul)*
static Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+"))
      node = new_node(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_node(ND_SUB, node, mul());
    else
      return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*"))
      node = new_node(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_node(ND_DIV, node, unary());
    else
      return node;
  }
}

// unary = ("+" | "-" | "*" | "&")? unary | primary
static Node *unary() {
  if (consume("+"))
    return unary();
  if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), unary()); 
  if (consume("&"))
    return new_unary(ND_ADDR, unary());
  if (consume("*"))
    return new_unary(ND_DEREF, unary());
  
  return primary();
}

// func_args = "(" (assign ("," assign)*? ")"
static Node *func_args() {
  if(consume(")"))
    return NULL;

  Node *head = assign();
  Node *cur = head;

  while(consume(",")) {
    cur->next = assign();
    cur = cur->next;
  }
  expect(")");
  return head;
}


// primary = "(" expr ")" | ident func-args? | num
static Node *primary(){
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  } 
  
  Token *tok = consume_ident();
  if (tok) {
    if (consume("(")) {
      Node *node = new_branch(ND_FUNCALL);
      node->funcname = duplicate(tok->str, tok->len);
      node->args = func_args();
      return node;
    }
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;

    LVar *lvar = find_lvar(tok);
    if (lvar) {
      node->var = lvar;
    }
    else {
      lvar = new_lvar(duplicate(tok->str, tok->len));
      node->var = lvar;
    }
    return node;
  }

  return new_node_num(expect_number());
}




