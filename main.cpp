#include <cstdlib>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>


#define _CONCAT1(X,Y) X##Y
#define CONCAT(X,Y) _CONCAT1(X,Y)

#define assert(COND, MSG) if (!(COND)) { printf("ERROR: " MSG "\n"); exit(1); }

#define REQUIRED(NAME, OPTIONAL_EXPR) \
    auto CONCAT(_opt, __LINE__) = OPTIONAL_EXPR; \
    CONCAT(_opt, __LINE__).die_on_error(); \
    NAME = CONCAT(_opt, __LINE__).value;

template<typename T>
struct optional {
    const char* error;
    T value;

    void die_on_error(const char* msg=0) {
        if (error != 0) {
            if (msg == 0) {
                printf("ERROR: %s\n", error);
            } else {
                printf("ERROR: %s\n", msg);
            }
            exit(1);
        }
    }
};

template<typename T>
optional<T> ok(T val) { return {0, val}; }
#define error(msg) { msg }

template<typename T>
struct bag {
    T* items;
    int len;
    int capacity;


    optional<T> operator [] (int index) {
        if (index < 0 || index >= len) { return error("Out of bag bounds"); }
        return ok(items[index]);
    }
};

template<typename T>
bag<T> create_bag(int cap) {
    bag<T> b;
    b.capacity = cap;
    b.len = 0;
    b.items = (T*)malloc(sizeof(T) * cap);
    return b;
}

template<typename T>
void bag_add(bag<T>* b, T item) {
    if (b->len == b->capacity) {
        b->capacity *= 2;
        if (b->capacity < 8) b->capacity = 8;
        b->items = (T*)realloc(b->items, sizeof(T)*b->capacity);
    }
    b->items[b->len] = item;
    b->len++;
}

template<typename T>
optional<T> bag_remove(bag<T>* b, int index) {
    if (index < 0 || index >= b->len) { return error("Out of bag bounds"); }
    T target = b->items[index];
    if (index!=b->len-1) {
        b->items[index] = b->items[b->len-1];
    }
    b->len--;
    return ok(target);
}

template<typename T>
optional<T> bag_pop(bag<T>* b) {
    return bag_remove(b, b->len-1);
}

template<typename T>
optional<T> bag_at(bag<T>* b, int index) {
    if (index < 0 || index >= b->len) { return error("Out of bag bounds"); }
    return ok(b->items[index]);
}

struct string {
    const char* text;
    int len;
};

void print(string* str) {
    printf("%.*s", str->len, str->text);
}

template<size_t N>
constexpr string const_string(char const(&chars)[N]) {
    return string{chars, N};
}

const string to_string(const char* ch) {
    string out;
    out.text = ch;
    out.len = 0;

    if (ch == 0) { return out; }
    while (*ch != 0) { ch++; out.len++; }
    return out;
}

bool starts(const string* str1, const string* str2) {
    if (str1 == 0 || str2 == 0) return false;
    if (str1->len < str2->len) return false;
    for (int i=0; i<str2->len; i++) {
        if (str1->text[i] != str2->text[i]) return false;
    }
    return true;
}

bool starts(const string* str1, const char* str2) {
    for (int i=0; i<str1->len; i++) {
        auto ch = str2[i];
        if (ch == 0) return false;
        if (ch != str1->text[i]) return false;
    }
    return true;
}

bool equal(const string* str1, const string* str2) {
    if (str1 != 0 && str2 == 0) return false;
    if (str1 == 0 && str2 != 0) return false;
    if (str1->len != str2->len) return false;
    for (int i=0; i<str1->len; i++) {
        if (str1->text[i] != str2->text[i]) return false;
    }
    return true;
}

bool equal(const string* str1, const char* str2) {
    for (int i=0; i<str1->len; i++) {
        if (str1->text[i] != str2[i]) return false;
    }
    return true;
}

string copy(string* str) {
    auto data = (char*)malloc(str->len);
    for (int i=0; i<str->len; i++) {
        data[i] = str->text[i];
    }
    return {.text=data, .len=str->len};
}

string slice(string str, int start, int end) {
    return {
        .text=str.text+start,
        .len=end-start,
    };
}

string slice(string* str, int start, int end) {
    return {
        .text=str->text+start,
        .len=end-start,
    };
}

string trim(string str) {
    char ch;
    while (ch = str.text[0], ch == ' ' || ch == '\n' || ch == '\t') {
        str.text++;
        str.len--;
    }

    while (ch = str.text[str.len-1], ch == ' ' || ch == '\n' || ch == '\t') {
        str.len--;
    }

    return str;
}

void fill_charp(string str, char* charp) {
    for (int i=0; i<str.len; i++) {
        charp[i] = str.text[i];
    }
    charp[str.len] = 0;
}

int index_of(string str, char ch, int nth) {
    if (nth == 0) return -1;

    int step = 1;
    int start = 0;
    int end = str.len;

    if (nth < 0) {
        nth = -nth;
        step = -1;
        end = -1;
        start = str.len-1;
    }

    int current = 0;
    for (int i=start; i!=end; i+=step) {
        if (str.text[i] == ch) {
            current++;
            if (current == nth) {
                return i;
            }
        }
    }

    return -1;
}

optional<string> cwd_str() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == 0) { return error("Fuck you"); }
    auto str = to_string(cwd);
    return ok(copy(&str));
}

string path_frag(
    string path,
    int start,
    int end
) {
    int start_index, end_index;

    if (start == 0) { start_index = path.len; }
    else { start_index = index_of(path, '/', start); }

    if (end == 0) { end_index = path.len; }
    else { end_index = index_of(path, '/', end); }

    if (start_index == -1) { start_index = end_index; }
    if (end_index == -1) { end_index = start_index; }
    if (start_index == -1) { return {0, 0}; }

    return {path.text+start_index, end_index-start_index};
}

bool dir_exists(char* path) {
    DIR* dir = opendir(path);
    if (dir != 0) {
        closedir(dir);
        return true;
    }
    return false;
}

void charp_set(char* charp, const char* text, int at) {
    int idx = 0;
    while (text[idx] != 0) {
        charp[at+idx] = text[idx];
        idx++;
    }
}

optional<string> read_file(char* path) {
    FILE* file = fopen(path, "r");
    if (file == 0) return error("Failed to open file");
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* mem = (char*)malloc(size);
    auto res = fread(mem, size+1, 1, file);
    return ok(string{mem, size});
}

optional<string> git_root(string root) {
    int nth = 0;
    char path[PATH_MAX] = {0};
    fill_charp(root, path);
    int idx = root.len;

    while (true) {
        charp_set(path, "/.git", idx);
        if (dir_exists(path)) {
            return ok(slice(&root, 0, idx));
        }

        idx = index_of(root, '/', nth);
        if (idx == -1) break;
        nth--;
    }
    return error("Not inside of git repo");
}

optional<string> git_branch_name(string root) {
    char path[PATH_MAX];
    fill_charp(root, path);
    charp_set(path, "/.git/HEAD", root.len);
    path[root.len + sizeof("/.git/HEAD") - 1] = 0;

    auto str = read_file(path);
    if (str.error) { return str; }

    auto idx = index_of(str.value, '/', -1);
    if (idx == -1) { return error("Failed to find ref in .git/HEAD"); }

    auto res = trim(slice(&str.value, idx+1, str.value.len));
    return ok(res);
}

optional<string> env_var(const char* name) {
    auto val = getenv(name);
    if (val == 0) return error("Env var not present");
    return ok(to_string(val));
}

/*
    lcap-round(background, red)
    bg-red text-white {
        git(segment=-1)
        "(" user ")"
    }
    rcap-tri(red, blue)
    bg-blue text-black {
        strip-prefix(cwd, git)
    }
*/

bool is_whitespace(char ch) {
    return ch==' ' || ch=='\n' || ch=='\t' || ch=='\r';
}

bool is_ident_char(char ch, bool first) {
    if (first) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
    } else {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '-' || ch == '_';
    }
}

bool is_num_char(char ch, bool first, bool post_dot) {
    if (first) {
        return ch == '-' || (ch >= '0' && ch <= '9');
    } else if (post_dot) {
        return ch >= '0' && ch <= '9';
    } else {
        return ch == '.' || (ch >= '0' && ch <= '9');
    }
}

struct Subline_Tokenizer;

enum TOKEN {
    TK_ERROR=0,
    TK_IDENT,
    TK_NUMBER,
    TK_STRING,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_LANGLE,
    TK_RANGLE,
    TK_COMMA,
    TK_EQUALS,
};

const char* token_type_str(TOKEN t) {
    switch (t) {
    case TK_ERROR: return "error";
    case TK_IDENT: return "ident";
    case TK_NUMBER: return "number";
    case TK_STRING: return "string";
    case TK_LPAREN: return "lpar";
    case TK_RPAREN: return "rpar";
    case TK_LBRACE: return "lbra";
    case TK_RBRACE: return "rbra";
    case TK_LANGLE: return "lang";
    case TK_RANGLE: return "rang";
    case TK_COMMA: return "comma";
    case TK_EQUALS: return "equals";
    default: return "unknown";
    }
}

struct Token {
    string* source;
    TOKEN type;
    int start;
    int end;
};

const string token_text(const Token t) {
    return string{t.source->text+t.start, t.end-t.start};
}

char* token_str(Token t) {
    auto type = token_type_str(t.type);
    char* buf = (char*)malloc(80);
    const char* charp = t.source->text + t.start;
    snprintf(buf, 80, "%s(%.*s)", type, t.end-t.start, charp);
    return buf;
}

struct Subline_Tokenizer {
    string text;
    int index;

    Subline_Tokenizer(string text) {
        this->text = text;
        this->index = 0;
    }

    char ch(int offset=0) {
        int target = this->index + offset;
        if (target < 0 || target >= this->text.len) return 0;
        return this->text.text[target];
    }

    bool move(int amt=1) {
        int target = this->index + amt;
        if (target < 0 || target > this->text.len) return false;
        index = target;
        return true;
    }

    Token token(TOKEN type, int start) {
        return token(type, start, index);
    }

    Token token(TOKEN type, int start, int end) {
        return Token{&text, type, start, end};
    }

    void eat_whitespace() {
        while (is_whitespace(ch())) {
            move();
        }
    }

    optional<Token> parse_ident() {
        eat_whitespace();
        int start = index;
        while (is_ident_char(ch(), start==index)) {
            move();
        }
        if (index == start) { return error("Not an identifier"); }
        return ok(token(TK_IDENT, start));
    }

    optional<Token> parse_string() {
        eat_whitespace();
        int start = index;
        char first = ch();
        if (first != '"') { return error("Not a string"); }
        move();

        while (ch() != first) {
            if (ch() == '\\') { move(); }
            move();
        }
        move();

        return ok(token(TK_STRING, start));
    }

    optional<Token> parse_number() {
        eat_whitespace();
        bool dot = false;
        int start = index;
        while(is_num_char(ch(), start==index, dot)) {
            if (ch() == '.') dot = true;
            move();
        }
        if (index == start) { return error("Not a number"); }
        return ok(token(TK_NUMBER, start));
    }

    optional<Token> parse_symbol() {
        eat_whitespace();

        switch (ch()) {
            #define SYM(TK) move(); return ok(token(TK, index-1))
            case '{': SYM(TK_LBRACE);
            case '}': SYM(TK_RBRACE);
            case '(': SYM(TK_LPAREN);
            case ')': SYM(TK_RPAREN);
            case '[': SYM(TK_LANGLE);
            case ']': SYM(TK_RANGLE);
            case ',': SYM(TK_COMMA);
            case '=': SYM(TK_EQUALS);
            #undef SYM
            default:
                auto msg = (char*)malloc(sizeof("Not a symbol: ") + 1);
                sprintf(msg, "Not a symbol: %c", ch());
                return error(msg);
        }
    }

    bag<Token> tokenize() {
        auto tokens = create_bag<Token>(text.len);
        Token tok;
        while(true) {
            eat_whitespace();
            if (is_ident_char(ch(), true)) {
                REQUIRED(tok, parse_ident());
                bag_add(&tokens, tok);
            } else if (is_num_char(ch(), true, false)) {
                REQUIRED(tok, parse_number());
                bag_add(&tokens, tok);
            } else if (ch() == '"') {
                REQUIRED(tok, parse_string());
                bag_add(&tokens, tok);
            } else if (ch() == 0) {
                break;
            } else {
                REQUIRED(tok, parse_symbol());
                bag_add(&tokens, tok);
            }
        }
        return tokens;
    }
};

enum AT_TYPE {
    AT_ERROR = 0,
    AT_IDENT,
    AT_STRING,
    AT_NUMBER,
    AT_PARAMS,
    AT_CALL,
    AT_PARAM_NAMED,
    AT_BLOCK,
};

template<typename T>
void* spoof(T* ptr) { return (void*)ptr; }

struct Arena {
    void* items;
    int capacity;
    int current;
};

Arena create_arena(int cap) {
    Arena a;
    a.capacity = cap;
    a.current = 0;
    a.items = malloc(cap);
    return a;
}

template<typename T>
T* arena_alloc(Arena* a) {
    auto size = sizeof(T);
    auto old = a->current;
    auto next = a->current+size;
    assert(next <= a->capacity, "Arena space used up");
    a->current = next;
    return (T*)((char*)a->items+old);
}

void arena_clear(Arena* a) {
    a->current = 0;
}

struct AST_Node {
    AT_TYPE kind;
};

#define AST_SPOOFER(name, type) \
    struct type;\
    type* to_##name(AST_Node* self); \
    AST_Node* downcast(type* self); \
    void print(type* self);

AST_SPOOFER(value, AST_Value);
AST_SPOOFER(params, AST_Params);
AST_SPOOFER(call, AST_Call);
AST_SPOOFER(block, AST_Block);
AST_SPOOFER(param_named, AST_Param_Named);

AST_Node* create_node(Arena* a, AT_TYPE k) {
    auto n = arena_alloc<AST_Node>(a);
    n->kind = k;
    return n;
}

void print(AST_Node* node) {
    switch (node->kind) {
    case AT_BLOCK: print(to_block(node)); break;
    case AT_CALL: print(to_call(node)); break;
    case AT_PARAMS: print(to_params(node)); break;
    case AT_IDENT:
    case AT_NUMBER:
    case AT_STRING: print(to_value(node)); break;
    default: printf("UNKNOWN NODE TYPE"); break;
    }
}

struct AST_Value {
    AT_TYPE kind;
    Token token;
};

AST_Value* create_value(Arena* a, AT_TYPE k, Token t) {
    auto n = arena_alloc<AST_Value>(a);
    n->kind = k;
    n->token = t;
    return n;
}

void print(AST_Value* val) {
    char* ch = token_str(val->token);
    printf("%s", ch);
    free(ch);
}

struct AST_Param_Named {
    AT_TYPE kind;
    Token name;
    AST_Node* value;
};

AST_Param_Named* create_param_named(Arena* a, Token name, AST_Node* value) {
    auto n = arena_alloc<AST_Param_Named>(a);
    n->kind = AT_PARAM_NAMED;
    n->name = name;
    assert(value != 0, "Null param value");
    n->value = value;
    return n;
}

void print(AST_Param_Named* val) {
    string name = token_text(val->name);
    print(&name);
    printf("=");
    print(val->value);
}

struct AST_Params {
    AT_TYPE kind;
    bag<AST_Node*> values;
};

AST_Params* create_params(Arena* a, bag<AST_Node*>* values) {
    auto n = arena_alloc<AST_Params>(a);
    n->kind = AT_PARAMS;
    n->values = *values;
    return n;
}

void print(AST_Params* self) {
    for (int i=0; i<self->values.len; i++) {
        if ((void*)self->values.items[i] == (void*)self) continue;
        print(self->values.items[i]);
        printf(", ");
    }
}

struct AST_Call {
    AT_TYPE kind;
    Token ident;
    AST_Params* params;
};

AST_Call* create_call(Arena* a, Token ident, AST_Params* params) {
    auto n = arena_alloc<AST_Call>(a);
    n->kind = AT_CALL;
    n->ident = ident;
    n->params = params;
    assert(params!=0, "Null call params");
    return n;
}

void print(AST_Call* self) {
    auto name = token_text(self->ident);
    print(&name);
    printf("(");
    print(self->params);
    printf(")");
}

struct AST_Block {
    AT_TYPE kind;
    AST_Params* params;
    bag<AST_Node*> statements;
};

AST_Block* create_block(Arena* a, AST_Params* params, bag<AST_Node*>* statements) {
    auto n = arena_alloc<AST_Block>(a);
    n->kind = AT_BLOCK;
    n->params = params;
    n->statements = *statements;
    return n;
}

void print(AST_Block* self) {
    printf("[");
    print(self->params);
    printf("] {");
    for (int i=0; i<self->statements.len; i++) {
        if ((void*)self->statements.items[i] == (void*)self) continue;
        print(self->statements.items[i]);
    }
    printf("}\n");
}

#undef AST_SPOOFER
#define AST_SPOOFER(NAME, TYPE, KIND) \
    TYPE* to_##NAME(AST_Node* self) { \
        assert(self->kind == KIND, "Wrong kind"); \
        return (TYPE*)spoof(self); \
    } \
    \
    AST_Node* downcast(TYPE* self) { return (AST_Node*)spoof(self); }

AST_Value* to_value(AST_Node* self) {
    assert(self->kind==AT_IDENT || self->kind==AT_STRING || self->kind==AT_NUMBER, "Wrong kind");
    return (AST_Value*)spoof(self);
}
AST_Node* downcast(AST_Value* self) { return (AST_Node*)spoof(self); }

AST_SPOOFER(params, AST_Params, AT_PARAMS);
AST_SPOOFER(call, AST_Call, AT_CALL);
AST_SPOOFER(block, AST_Block, AT_BLOCK);
AST_SPOOFER(param_named, AST_Param_Named, AT_PARAM_NAMED);

struct Subline_Parser {
    bag<Token> tokens;
    Arena arena;
    int index;

    static Subline_Parser create(bag<Token>* t) {
        Arena a = create_arena(sizeof(AST_Block) * t->len * 2);
        Subline_Parser s = {*t, a, 0};
        return s;
    }

    Token at(int offset=0) {
        int target = this->index + offset;
        auto tok = tokens[target];
        if (tok.error) { return Token{0, TK_ERROR}; }
        return tok.value;
    }

    Token expect(TOKEN type, int offset=0) {
        Token tok = at(offset);
        if (tok.type != type) {
            auto ch = token_str(tok);
            printf("Expected %s, got %s\n", token_type_str(type), ch);
            free(ch);
            exit(1);
        }
        return tok;
    }

    bool move(int amt=1) {
        int target = this->index + amt;
        if (target < 0 || target > tokens.len) return false;
        index = target;
        return true;
    }

    optional<AST_Value*> parse_ident() {
        auto tok = at();
        if (tok.type != TK_IDENT) return error("Not an ident");
        move();
        return ok(create_value(&arena, AT_IDENT, tok));
    }

    optional<AST_Value*> parse_string() {
        auto tok = at();
        if (tok.type != TK_STRING) return error("Not a string");
        move();
        return ok(create_value(&arena, AT_STRING, tok));
    }

    optional<AST_Value*> parse_number() {
        auto tok = at();
        if (tok.type != TK_NUMBER) return error("Not a number");
        move();
        return ok(create_value(&arena, AT_NUMBER, tok));
    }

    optional<AST_Node*> parse_param() {
        auto ident = at(0);
        if (at(0).type != TK_IDENT) return parse_expr();
        if (at(1).type != TK_EQUALS) return parse_expr();
        move(2);
        AST_Node* value;
        REQUIRED(value, parse_expr());
        return ok(downcast(create_param_named(&arena, ident, value)));
    }

    optional<AST_Params*> parse_params() {
        expect(TK_LPAREN);
        move();

        bool first = true;
        auto values = create_bag<AST_Node*>(8);
        while (at(0).type != TK_RPAREN) {
            if (at(0).type == TK_ERROR) return error("Unexpected token in parameter list");
            if (!first) {
                expect(TK_COMMA);
                move();
                if (at(0).type == TK_RPAREN) { break; }
            } else { first = false; }
            AST_Node* param;
            REQUIRED(param, parse_param());
            bag_add(&values, param);
        }
        move();

        auto p = create_params(&arena, &values);
        return ok(p);
    }

    optional<AST_Call*> parse_call() {
        auto ident = at(0);
        if (ident.type != TK_IDENT) { return error("Not a call"); }
        if (at(1).type != TK_LPAREN) { return error("Not a call"); }
        move();
        AST_Params* params;
        REQUIRED(params, parse_params());
        return ok(create_call(&arena, ident, params));
    }

    optional<AST_Node*> parse_expr() {
        switch(at().type) {
        case TK_STRING: {
            auto p = parse_string();
            if (p.error) return error(p.error);
            return ok(downcast(p.value));
        }

        case TK_NUMBER: {
            auto p = parse_number();
            if (p.error) return error(p.error);
            return ok(downcast(p.value));
        }

        case TK_IDENT: {
            if (at(1).type == TK_LPAREN) {
                auto p = parse_call();
                if (p.error) return error(p.error);
                return ok(downcast(p.value));
            } else {
                auto p = parse_ident();
                if (p.error) return error(p.error);
                return ok(downcast(p.value));
            }
        }

        default:
            printf("%s: %d\n", token_type_str(at(0).type), at(0).start);
            return error("Expected an expression");
        }
    }

    optional<AST_Node*> parse_statement() {
        auto block = parse_block();
        if (block.error == 0) {
            return ok(downcast(block.value));
        }
        return parse_expr();
    }

    optional<AST_Params*> parse_block_params() {
        expect(TK_LANGLE);
        move();

        auto values = create_bag<AST_Node*>(8);
        while (at(0).type != TK_RANGLE) {
            if (at(0).type == TK_ERROR) return error("Unexpected token in block parameter list");
            AST_Node* param;
            REQUIRED(param, parse_param());
            bag_add(&values, param);
        }
        move();

        auto params = create_params(&arena, &values);
        return ok(params);
    }

    optional<AST_Block*> parse_block() {
        if (at(0).type != TK_LANGLE) return error("Not a block");

        AST_Params* params;
        REQUIRED(params, parse_block_params());

        expect(TK_LBRACE);
        move();

        auto statements = create_bag<AST_Node*>(16);
        while (at(0).type != TK_RBRACE) {
            if (at(0).type == TK_ERROR) {
                printf("Unexpected token");
                exit(1);
            }
            AST_Node* stmt;
            REQUIRED(stmt, parse_statement());
            bag_add(&statements, stmt);
        }
        move();

        auto block = create_block(&arena, params, &statements);
        block->params = params;
        block->statements = statements;
        return ok(block);
    }

    optional<bag<AST_Node*>> parse() {
        auto statements = create_bag<AST_Node*>(32);
        while(at(0).type != TK_ERROR) {
            auto stmt = parse_statement();
            if (stmt.error) { printf("ERROR: %s\n", stmt.error); exit(1); }
            bag_add(&statements, stmt.value);
        }
        return ok(statements);
    }
};

struct SGR_Tuple {
    const char* name;
    int code;
};

SGR_Tuple COLOR[] = {
    {"black",   0},
    {"red",     1},
    {"green",   2},
    {"yellow",  3},
    {"blue",    4},
    {"magenta", 5},
    {"cyan",    6},
    {"white",   7},
    {"bright-black",   60},
    {"bright-red",     61},
    {"bright-green",   62},
    {"bright-yellow",  63},
    {"bright-blue",    64},
    {"bright-magenta", 65},
    {"bright-cyan",    66},
    {"bright-white",   67},
};
auto COLORS = sizeof(COLOR) / sizeof(SGR_Tuple);

int color_code(const string* color) {
    for (int i=0; i<COLORS; i++) {
        if (equal(color, COLOR[i].name)) {
            return COLOR[i].code;
        }
    }
    return -1;
}

enum COLOR_TYPE {
    CT_SGR, CT_HEX,
};

struct Color {
    COLOR_TYPE type;
    int value;
};

int hex_digit_to_int(const char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'A' && digit <= 'F') return (digit - 'A') + 10;
    if (digit >= 'a' && digit <= 'f') return (digit - 'a') + 10;
    return -1;
}

int hex_to_int(const string* hex) {
    int r, g, b;
    switch (hex->len) {
    case 7: {
        r = hex_digit_to_int(hex->text[1]);
        g = hex_digit_to_int(hex->text[2]);
        b = hex_digit_to_int(hex->text[3]);
        r += (r<<4) + r;
        g += (g<<4) + g;
        b += (b<<4) + b;
    } break;
    case 4: {
        int r1 = hex_digit_to_int(hex->text[1]);
        int r2 = hex_digit_to_int(hex->text[2]);
        int g1 = hex_digit_to_int(hex->text[3]);
        int g2 = hex_digit_to_int(hex->text[4]);
        int b1 = hex_digit_to_int(hex->text[5]);
        int b2 = hex_digit_to_int(hex->text[6]);
        r = (r1<<4) + r2;
        g = (g1<<4) + g2;
        b = (b1<<4) + b2;
    } break;
    default: {
        fprintf(stderr, "Invalid color: %.*s", hex->len, hex->text);
        exit(1);
    }
    }
    return (r<<16) + (g<<8) + b;
}

Color string_to_color(string str) {
    if (starts(&str, "#")) {
        return {CT_HEX, hex_to_int(&str)};
    } else {
        return {CT_SGR, color_code(&str)};
    }
}

int red(Color col) { assert(col.type==CT_HEX, "Expected a hex color!"); return (col.value >> 16) & 0xff; }
int green(Color col) { assert(col.type==CT_HEX, "Expected a hex color!"); return (col.value >> 8) & 0xff; }
int blue(Color col) { assert(col.type==CT_HEX, "Expected a hex color!"); return col.value & 0xff; }

struct Git_State {
    string dir;
    string branch;
};

enum INTENSITY {
    INT_DIM=-1,
    INT_NORMAL=0,
    INT_BOLD=1,
};

struct Display_Style {
    Color text;
    Color bg;
    INTENSITY intensity;
    bool italic;
    bool strike;
    bool underline;
};

struct Subline_State {
    string cwd;
    optional<Git_State> git;
    Display_Style style;
    bag<Display_Style> style_stack;
};

#define ESCAPE "\33["
#define SGR1(arg) printf(ESCAPE "%dm", arg)

void reset(Subline_State* s) {
    SGR1(0);
    s->style.bg = Color{CT_SGR, 0};
    s->style.text = Color{CT_SGR, 0};
    s->style.intensity = INT_NORMAL;
    s->style.italic = false;
    s->style.underline = false;
    s->style.strike = false;
}

#define STYLE_FN(NAME, SGR, PROP, VAL) \
    void NAME(Subline_State* s) { \
        SGR1(SGR); \
        s->style.PROP = VAL; \
    }

STYLE_FN(bold_enable, 1, intensity, INT_BOLD);
STYLE_FN(dim_enable, 2, intensity, INT_DIM);
STYLE_FN(bold_dim_disable, 22, intensity, INT_NORMAL);
STYLE_FN(italic_enable, 3, italic, true);
STYLE_FN(italic_disable, 23, italic, true);
STYLE_FN(underline_enable, 4, underline, true);
STYLE_FN(underline_disable, 24, underline, true);
STYLE_FN(strike_enable, 9, strike, true);
STYLE_FN(strike_disable, 29, strike, true);

void text_apply(Subline_State* s, Color col) {
    if (col.type == CT_SGR) {
        SGR1(30+col.value);
    } else if (col.type == CT_HEX) {
        printf(ESCAPE "38;2;%d;%d;%dm", red(col), green(col), blue(col));
    } else {
        fprintf(stderr, "Unknown color type: %d\n", col.type);
        exit(1);
    }
    s->style.text = col;
}

void bg_apply(Subline_State* s, Color col) {
    if (col.type == CT_SGR) {
        SGR1(40+col.value);
        printf(ESCAPE "%dm", 40+col.value);
    } else if (col.type == CT_HEX) {
        printf(ESCAPE "48;2;%d;%d;%dm", red(col), green(col), blue(col));
    } else {
        fprintf(stderr, "Unknown color type: %d\n", col.type);
        exit(1);
    }
    s->style.bg = col;
}

struct CAP {
    const char* name;
    const char* val;
};

CAP CAP_MAP[] = {
    {"P>>", "\ue0b0"},
    {"P>",  "\ue0b1"},
    {"P<<", "\ue0b2"},
    {"P<",  "\ue0b3"},
    {"P))", "\ue0b4"},
    {"P)",  "\ue0b5"},
    {"P((", "\ue0b6"},
    {"P(",  "\ue0b7"},
};
auto CAPS = sizeof(CAP_MAP)/sizeof(CAP);

template<typename T>
T assert_value(bool cond, T value, const char* msg) {
    if (!cond) {
        fprintf(stderr, "%s", msg);
        exit(1);
    }
    return value;
}

string unquote(const string str) {
    string out = str;
    if (out.text[0] == '"') { out.text++; out.len--; }
    if (out.text[out.len-1] == '"') { out.len--; }
    return out;
}

string eval(AST_Node* node);

auto SBLN_FALSE = const_string("~~FALSE~~");
auto SBLN_TRUE = const_string("~~TRUE~~");

string do_call(Subline_State* s, string* fn_name, bag<AST_Node*>* args) {
    #define NO_ARGS(FN_NAME) assert(args == 0 || args->len == 0, FN_NAME "() expects no arguments")
    #define ARG_COUNT(FN_NAME, NUM) assert(args->len == NUM, FN_NAME "() expects exactly " #NUM " argument(s)")

    #define STR_ARG(FN_NAME, IDX) \
        unquote(token_text(assert_value(\
            args->items[IDX]->kind == AT_STRING, \
            to_value(args->items[IDX]), \
            FN_NAME "() expects a string as argument " #IDX \
        )->token))

    #define IDENT_STR_ARG(FN_NAME, IDX) \
        unquote(token_text(assert_value(\
            args->items[IDX]->kind == AT_STRING || args->items[IDX]->kind == AT_IDENT, \
            to_value(args->items[IDX]), \
            FN_NAME "() expects a string or an identifier as argument " #IDX \
        )->token))

    #define IDENT_CALL_ARG(FN_NAME, IDX) \
        assert_value(\
            args->items[IDX]->kind == AT_IDENT || args->items[IDX]->kind == AT_CALL, \
            args->items[IDX], \
            FN_NAME "() expects an identifier or a call as argument " #IDX \
        )

    if (equal(fn_name, "text")) {
        ARG_COUNT("text", 1);
        auto value = IDENT_STR_ARG("text", 0);
        text_apply(s, string_to_color(value));

        return {0};

    } else if (equal(fn_name, "bg")) {
        ARG_COUNT("bg", 1);
        auto value = IDENT_STR_ARG("bg", 0);
        bg_apply(s, string_to_color(value));
        return {0};

    } else if (equal(fn_name, "cap")) {
        ARG_COUNT("cap", 1);
        auto value = STR_ARG("cap", 0);
        for (int i=0; i<CAPS; i++) {
            auto cap = CAP_MAP[i];
            if (equal(&value, cap.name)) {
                return to_string(cap.val);
            }
        }
        return value;

    } else if (equal(fn_name, "env")) {
        ARG_COUNT("env", 1);
        auto value = IDENT_STR_ARG("env", 0);
        char envname[255];
        fill_charp(value, envname);
        auto envvar = getenv(envname);
        if (envvar == 0) return {0};
        auto str = to_string(envvar);
        return copy(&str);

    } else if (equal(fn_name, "_")) {
        return to_string(" ");

    } else if (equal(fn_name, "bold")) {
        NO_ARGS("bold");
        bold_enable(s);
        return {0};

    } else if (equal(fn_name, "regular")) {
        NO_ARGS("regular");
        bold_dim_disable(s);
        return {0};

    } else if (equal(fn_name, "dim")) {
        NO_ARGS("dim");
        dim_enable(s);
        return {0};

    } else if (equal(fn_name, "italic")) {
        NO_ARGS("italic");
        italic_enable(s);
        return {0};

    } else if (equal(fn_name, "normal")) {
        NO_ARGS("normal");
        italic_disable(s);
        return {0};

    } else if (equal(fn_name, "underline")) {
        NO_ARGS("underline");
        underline_enable(s);
        return {0};

    } else if (equal(fn_name, "no-underline")) {
        NO_ARGS("no-underline");
        underline_disable(s);
        return {0};

    } else if (equal(fn_name, "strike")) {
        NO_ARGS("strike");
        strike_disable(s);
        return {0};

    } else if (equal(fn_name, "no-strike")) {
        NO_ARGS("no-strike");
        strike_enable(s);
        return {0};

    } else if (equal(fn_name, "if")) {
        ARG_COUNT("if", 1);
        auto fn = IDENT_CALL_ARG("if", 0);
        return eval(fn);

    } else if (equal(fn_name, "eq")) {
        ARG_COUNT("eq", 2);
        auto arg1 = eval(args->items[0]);
        auto arg2 = eval(args->items[0]);

        return equal(&arg1, &arg2) ? SBLN_TRUE : SBLN_FALSE;

    } else if (starts(fn_name, "starts")) {
        ARG_COUNT("starts", 2);
        auto arg1 = eval(args->items[0]);
        auto arg2 = eval(args->items[0]);

        return starts(&arg1, &arg2) ? SBLN_TRUE : SBLN_FALSE;

    } else if (equal(fn_name, "strip-prefix")) {
        ARG_COUNT("strip-prefix", 2);
        auto arg1 = eval(args->items[0]);
        auto arg2 = eval(args->items[1]);

        if (starts(&arg1, &arg2)) {
            return string{arg1.text+arg2.len, arg1.len-arg2.len};
        } else {
            return arg1;
        }
    }

    printf("Unhandled call: %.*s\n", fn_name->len, fn_name->text);
    return {0};
}

void style_apply(Subline_State* s, Display_Style old_style, Display_Style new_style) {
    if (old_style.bg.type != new_style.bg.type) {
        bg_apply(s, new_style.bg);
    } else if (old_style.bg.value != new_style.bg.value) {
        bg_apply(s, new_style.bg);
    }

    if (old_style.text.type != new_style.text.type) {
        text_apply(s, new_style.text);
    } else if (old_style.text.value != new_style.text.value) {
        text_apply(s, new_style.text);
    }

    if (old_style.strike != new_style.strike) {
        if (new_style.strike) {
            strike_enable(s);
        } else {
            strike_disable(s);
        }
    }

    if (old_style.italic != new_style.italic) {
        if (new_style.italic) {
            italic_enable(s);
        } else {
            italic_disable(s);
        }
    }

    if (old_style.underline != new_style.underline) {
        if (new_style.underline) {
            underline_enable(s);
        } else {
            underline_disable(s);
        }
    }

    if (old_style.intensity != new_style.intensity) {
        switch (new_style.intensity) {
        case INT_DIM: dim_enable(s); break;
        case INT_BOLD: bold_enable(s); break;
        case INT_NORMAL: bold_dim_disable(s); break;
        }
    }
}

void style_push(Subline_State* s, Display_Style style) {
    auto old_style = s->style;
    bag_add(&s->style_stack, s->style);
    s->style = style;
    auto new_style = s->style;
    style_apply(s, old_style, new_style);
}

void style_pop(Subline_State* s) {
    auto old_style = s->style;
    REQUIRED(s->style, bag_pop(&s->style_stack));
    auto new_style = s->style;
    style_apply(s, old_style, new_style);
}

Subline_State state;

void display(string str) {
    if (str.text == 0 || str.len == 0) return;
    printf("%.*s", str.len, str.text);
}

string eval(AST_Node* node) {
    switch (node->kind) {
    case AT_IDENT: {
        auto val = to_value(node);
        auto name = token_text(val->token);
        return do_call(&state, &name, 0);
    } break;

    case AT_STRING: {
        auto val = to_value(node);
        auto text = token_text(val->token);
        return unquote(text);
    } break;

    case AT_NUMBER: {
        auto val = to_value(node);
        auto num = token_text(val->token);
        return num;
    } break;

    case AT_CALL: {
        auto val = to_call(node);
        auto name = token_text(val->ident);
        auto params = val->params->values;
        return do_call(&state, &name, &params);
    } break;

    case AT_BLOCK: {
        auto block = to_block(node);

        auto old_style = state.style;
        for (int i=0; i<block->params->values.len; i++) {
            auto val = eval(block->params->values.items[i]);
            if (i == 0 && equal(&val, &SBLN_FALSE)) {
                return {0};
            }
        }

        auto new_style = state.style;
        state.style = old_style;
        style_push(&state, new_style);

        for (int i=0; i<block->statements.len; i++) {
            auto val = eval(block->statements.items[i]);
            display(val);
        }

        style_pop(&state);
        return {0};
    } break;

    default: {
        printf("[%d]", node->kind);
        return {0};
    } break;
    }
}

int main() {
    REQUIRED(state.cwd, cwd_str());
    // string frag = path_frag(copy(&cwd), -1, 0);
    // auto venv = env_var("VIRTUAL_ENV");
    // auto last = env_var("?");

    optional<string> git_dir = git_root(copy(&state.cwd));
    auto branch = git_dir.error ? git_dir : git_branch_name(copy(&git_dir.value));

    //     printf("Dir name: %.*s\n", (int)frag.len, frag.text);
    //     if (!git_dir.error) {
    //         printf("GIT: %.*s\n", git_dir.value.len, git_dir.value.text);
    //     }
    //
    //     if (!branch.error) {
    //         printf("BRANCH: %.*s\n", branch.value.len, branch.value.text);
    //     }
    //
    //     if (!venv.error) {
    //         printf("VENV: %.*s\n", venv.value.len, venv.value.text);
    //     }

    auto st = Subline_Tokenizer(to_string(R"END(
        _ text(red) cap("P((")

        [bg(red) text(white) bold] {
            _
            env("?")
            env(USER)
            _
        }

        text(red) bg(bright-black) cap("P>>")

        [bg(bright-black) text(bright-white) italic] {
            _
            [starts(env(PWD), env(HOME))] {
                "~"
            }
            strip-prefix(env(PWD), env(HOME))
            _
        }

        text(bright-black) bg(black) cap("P>>")
    )END"));
    auto tokens = st.tokenize();
    auto sp = Subline_Parser::create(&tokens);
    bag<AST_Node*> stmts;
    REQUIRED(stmts, sp.parse());
    for (int i=0; i<stmts.len; i++) {
        auto val = eval(stmts.items[i]);
        display(val);
    }
    reset(&state);
}
