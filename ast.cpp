#ifndef subline_ast
#define subline_ast

#include "utils.cpp"
#include "tokenizer.cpp"

enum AT_TYPE {
    AT_ERROR = 0,
    AT_IDENT,
    AT_STRING,
    AT_COLOR,
    AT_ENV,
    AT_NUMBER,
    AT_PARAMS,
    AT_CALL,
    AT_PARAM_NAMED,
    AT_BLOCK,
    AT_IF,
};

const char* ast_type_str(AT_TYPE t) {
    switch (t) {
    case AT_ERROR: return "error";
    case AT_IDENT: return "ident";
    case AT_STRING: return "string";
    case AT_NUMBER: return "number";
    case AT_COLOR: return "color";
    case AT_ENV: return "env";
    case AT_PARAMS: return "params";
    case AT_CALL: return "call";
    case AT_PARAM_NAMED: return "named param";
    case AT_BLOCK: return "block";
    case AT_IF: return "if";
    default: return "unknown";
    }
}

struct AST_Node {
    AT_TYPE kind;
};

#define AST_SPOOFER(name, type) \
    struct type;\
    type* to_##name(AST_Node* self); \
    AST_Node* downcast(type* self); \
    string to_string(type* self); \
    string to_error(type* self);

AST_SPOOFER(value, AST_Value);
AST_SPOOFER(params, AST_Params);
AST_SPOOFER(call, AST_Call);
AST_SPOOFER(block, AST_Block);
AST_SPOOFER(param_named, AST_Param_Named);
AST_SPOOFER(if, AST_If);

AST_Node* create_node(Arena* a, AT_TYPE k) {
    auto n = arena_alloc<AST_Node>(a);
    n->kind = k;
    return n;
}

string to_string(AST_Node* node) {
    switch (node->kind) {
    case AT_BLOCK: return to_string(to_block(node));
    case AT_CALL: return to_string(to_call(node));
    case AT_PARAMS: return to_string(to_params(node));
    case AT_IDENT:
    case AT_NUMBER:
    case AT_COLOR:
    case AT_ENV:
    case AT_STRING: return to_string(to_value(node));
    case AT_IF: return to_string(to_if(node));
    default: return stringf("[NODE TYPE: %d]", node->kind);
    }
}

string to_error(AST_Node* node) {
    switch (node->kind) {
    case AT_BLOCK: return to_error(to_block(node));
    case AT_CALL: return to_error(to_call(node));
    case AT_PARAMS: return to_error(to_params(node));
    case AT_IDENT:
    case AT_NUMBER:
    case AT_COLOR:
    case AT_ENV:
    case AT_STRING: return to_error(to_value(node));
    case AT_IF: return to_error(to_if(node));
    default: assert(false, "Unhandled AST node type (to_error): %d\n", node->kind);
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

string to_string(AST_Value* val) {
    return to_string(&val->token);
}

string to_error(AST_Value* val) {
    return to_error(&val->token);
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

string to_string(AST_Param_Named* val) {
    string name = token_text(&val->name);
    string valstr = to_string(val->value);
    return stringf("%.*s=%.*s", name.len, name.text, valstr.len, valstr.text);
}

string to_error(AST_Param_Named* val) {
    return to_error(&val->name);
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

int named_param_idx(bag<AST_Node*>* params, const char* name) {
    for (int i=0; i<params->len; i++) {
        auto val = params->items[i];
        if (val->kind != AT_PARAM_NAMED) continue;
        auto named = to_param_named(val);
        auto param_name = token_text(&named->name);
        if (equal(&param_name, name)) return i;
    }
    return -1;
}

optional<AST_Param_Named*> named_param_find(bag<AST_Node*>* params, const char* name) {
    int idx = named_param_idx(params, name);
    if (idx == -1) return error("Missing named parameter");
    return ok(to_param_named(params->items[idx]));
}

string to_string(AST_Params* self) {
    string strs[self->values.len];
    for (int i=0; i<self->values.len; i++) {
        strs[i] = to_string(self->values.items[i]);
    }
    return concat(strs, self->values.len);
}

string to_error(AST_Params* val) {
    if (val->values.len == 0) return {0};
    return to_error(val->values.items[0]);
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

string to_string(AST_Call* self) {
    auto name = token_text(&self->ident);
    auto params = to_string(self->params);
    return stringf("%.*s(%.*s)", name.len, name.text, params.len, params.text);
}

string to_error(AST_Call* val) {
    return to_error(&val->ident);
}

struct AST_Block {
    AT_TYPE kind;
    optional<AST_Params*> params;
    bag<AST_Node*> statements;
};

AST_Block* create_block(Arena* a, optional<AST_Params*> params, bag<AST_Node*>* statements) {
    auto n = arena_alloc<AST_Block>(a);
    n->kind = AT_BLOCK;
    n->params = params;
    n->statements = *statements;
    return n;
}

string to_string(AST_Block* self) {
    string params = {0, 0};
    if (self->params.error == 0) {
        params = stringf("[%.*s] ", params.len, params.text);
    }

    string stmts[self->statements.len];
    for (int i=0; i<self->statements.len; i++) {
        stmts[i] = to_string(self->statements.items[i]);
    }
    string stmts_str = concat(stmts, self->statements.len);

    return stringf("%.*s{%.*s}", params.len, params.text, stmts_str.len, stmts_str.text);
}

string to_error(AST_Block* val) {
    if (val->params.error != 0 && val->params.value->values.len > 0) {
        return to_error(val->params.value);
    }

    if (val->statements.len > 0) {
        return to_error(val->statements.items[0]);
    }

    return {0};
}

struct AST_If {
    AT_TYPE kind;
    AST_Node* condition;
    AST_Node* body;
    optional<AST_Node*> else_body;
};

AST_If* create_if(Arena* a, AST_Node* condition, AST_Node* body, optional<AST_Node*> else_body) {
    auto n = arena_alloc<AST_If>(a);
    n->kind = AT_IF;
    n->condition = condition;
    n->body = body;
    n->else_body = else_body;
    return n;
}

string to_string(AST_If* self) {
    auto cond = to_string(self->condition);
    auto body = to_string(self->body);
    auto else_body = string{0, 0};
    if (self->else_body.error == 0) else_body = to_string(self->else_body.value);

    if (self->else_body.error == 0) {
        return stringf("if " FSTR " " FSTR " else " FSTR, FARG(cond), FARG(body), FARG(else_body));
    } else {
        return stringf("if " FSTR " " FSTR, FARG(cond), FARG(body));
    }
}

string to_error(AST_If* val) {
    return to_error(val->condition);
}

#undef AST_SPOOFER
#define AST_SPOOFER(NAME, TYPE, KIND) \
    TYPE* to_##NAME(AST_Node* self) { \
        if (self->kind != KIND) { \
            GENERIC_ERROR(self, \
                "Expected %s, got %s\n", \
                ast_type_str(KIND), \
                ast_type_str(self->kind) \
            );\
        }\
        return (TYPE*)spoof(self); \
    } \
    AST_Node* downcast(TYPE* self) { return (AST_Node*)spoof(self); }

AST_Value* to_value(AST_Node* self) {
    if (
        self->kind!=AT_IDENT && self->kind!=AT_STRING &&
        self->kind!=AT_NUMBER && self->kind!=AT_COLOR &&
        self->kind!=AT_ENV
    ) {
        GENERIC_ERROR(self, "Expected a value, got %s", ast_type_str(self->kind));
    }
    return (AST_Value*)spoof(self);
}
AST_Node* downcast(AST_Value* self) { return (AST_Node*)spoof(self); }

AST_SPOOFER(params, AST_Params, AT_PARAMS);
AST_SPOOFER(call, AST_Call, AT_CALL);
AST_SPOOFER(block, AST_Block, AT_BLOCK);
AST_SPOOFER(param_named, AST_Param_Named, AT_PARAM_NAMED);
AST_SPOOFER(if, AST_If, AT_IF);

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

    Token expect(TOKEN type, int offset=0, Token* additional=0) {
        Token tok = at(offset);
        if (tok.type != type) {
            if (additional != 0) {
                warn(FSTR, FARG(to_error(additional)));
            }
            GENERIC_ERROR(&tok, "Expected %s, got " FSTR, token_type_str(type), FARG(token_text(&tok)));
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

    optional<AST_Value*> parse_color() {
        auto tok = at();
        if (tok.type != TK_COLOR) return error("Not a color");
        move();
        return ok(create_value(&arena, AT_COLOR, tok));
    }

    optional<AST_Value*> parse_env() {
        auto tok = at();
        if (tok.type != TK_ENV) return error("Not an env");
        move();
        return ok(create_value(&arena, AT_ENV, tok));
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
        bool in_named_params = false;

        while (at(0).type != TK_RPAREN) {
            if (at(0).type == TK_ERROR) return error("Unexpected token in parameter list");

            if (!first) {
                expect(TK_COMMA);
                move();
                if (at(0).type == TK_RPAREN) { break; }
            } else {
                first = false;
            }

            auto param_token = at(0);
            AST_Node* param;
            REQUIRED(param, parse_param());
            if (in_named_params && param->kind != AT_PARAM_NAMED) {
                GENERIC_ERROR(
                    &param_token,
                    "Named parameters must come after positional parameters!"
                );
            }

            if (param->kind == AT_PARAM_NAMED) in_named_params = true;
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

        case TK_COLOR: {
            auto p = parse_color();
            if (p.error) return error(p.error);
            return ok(downcast(p.value));
        }

        case TK_ENV: {
            auto p = parse_env();
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
            auto tok = at(0);
            auto msg = stringf(FSTR "\nExpected an expression", FARG(to_error(&tok)));
            return error(msg.text);
        }
    }

    optional<AST_If*> parse_if() {
        if (at(0).type != TK_KWD_IF) {
            return error("Expected the 'if' keyword");
        }
        move();

        AST_Node* condition;
        REQUIRED(condition, parse_expr());

        AST_Node* body;
        REQUIRED(body, parse_statement());

        if (at(0).type != TK_KWD_ELSE) {
            return ok(create_if(&arena, condition, body, error("No else block")));
        }

        move();

        AST_Node* else_body;
        REQUIRED(else_body, parse_statement());

        return ok(create_if(&arena, condition, body, ok(else_body)));
    }

    optional<AST_Node*> parse_statement() {
        auto block = parse_block();
        if (block.error == 0) {
            return ok(downcast(block.value));
        }

        auto if_stmt = parse_if();
        if (if_stmt.error == 0) {
            return ok(downcast(if_stmt.value));
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
        optional<AST_Params*> params;
        if (at(0).type == TK_LANGLE) {
            params = parse_block_params();
            params.die_on_error("Invalid block params");
        } else {
            params = error("No params");
        }

        if (params.error == 0) {
            auto prev = at(-1);
            expect(TK_LBRACE, 0, &prev);
        } else {
            if (at(0).type != TK_LBRACE) return error("No opening brace");
        }

        auto opener = at(0);
        move();

        auto statements = create_bag<AST_Node*>(16);
        while (at(0).type != TK_RBRACE) {
            if (at(0).type == TK_ERROR) {
                GENERIC_ERROR(&opener, "Unclosed block!");
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
            if (stmt.error) { warn("%s\n", stmt.error); exit(1); }
            bag_add(&statements, stmt.value);
        }
        return ok(statements);
    }
};

#endif
