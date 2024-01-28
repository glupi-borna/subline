#ifndef subline_tokenizer
#define subline_tokenizer

#include "utils.cpp"

#define GENERIC_ERROR(TOKEN, FMT, ...) \
    warn(FSTR FMT "\n", FARG(to_error(TOKEN)),##__VA_ARGS__); \
    exit(1);

#define IN_RANGE(CHAR, START_END) (\
    (CHAR) >= (START_END[0]) && \
    (CHAR) <= (START_END[1])\
)

bool is_whitespace(char ch) {
    return ch==' ' || ch=='\n' || ch=='\t' || ch=='\r';
}

bool is_hex_char(char ch) {
    return IN_RANGE(ch, "af") || IN_RANGE(ch, "AF") || IN_RANGE(ch, "09");
}

bool is_az_char(char ch) {
    return IN_RANGE(ch, "az") || IN_RANGE(ch, "AZ");
}

bool is_ident_char(char ch, bool first) {
    if (first) {
        return is_az_char(ch) || ch == '_';
    } else {
        return is_az_char(ch) || ch == '-' || ch == '_';
    }
}

bool is_num_char(char ch, bool first, bool post_dot) {
    if (first) {
        return ch == '-' || IN_RANGE(ch, "09");
    } else if (post_dot) {
        return IN_RANGE(ch, "09");
    } else {
        return ch == '.' || IN_RANGE(ch, "09");
    }
}

enum TOKEN {
    TK_ERROR=0,
    TK_IDENT,
    TK_NUMBER,
    TK_STRING,
    TK_COLOR,
    TK_ENV,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_LANGLE,
    TK_RANGLE,
    TK_COMMA,
    TK_EQUALS,
    TK_KWD_IF,
    TK_KWD_ELSE,
};

const char* token_type_str(TOKEN t) {
    switch (t) {
    case TK_ERROR: return "error";
    case TK_IDENT: return "ident";
    case TK_NUMBER: return "number";
    case TK_STRING: return "string";
    case TK_COLOR: return "color";
    case TK_ENV: return "env";
    case TK_LPAREN: return "'('";
    case TK_RPAREN: return "')'";
    case TK_LBRACE: return "'{'";
    case TK_RBRACE: return "'}'";
    case TK_LANGLE: return "'['";
    case TK_RANGLE: return "']'";
    case TK_COMMA: return "','";
    case TK_EQUALS: return "'='";
    case TK_KWD_IF: return "'if'";
    case TK_KWD_ELSE: return "'else'";
    default: return "unknown";
    }
}

struct Token {
    string* source;
    TOKEN type;
    int start;
    int end;
};

const string token_text(Token* t) {
    return string{t->source->text+t->start, t->end-t->start};
}

string to_error(Token* t) {
    return error_at(t->source, t->start);
}

string to_string(Token* t) {
    auto type = token_type_str(t->type);
    auto charp = t->source->text + t->start;
    auto len = t->end - t->start;
    return stringf("%s(%.*s)", type, len, charp);
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

    string error_str(int byte) {
        auto line = line_at_offset(&this->text, byte);
        int offset = (this->text.text + byte) - line.text;
        return stringf(FSTR "\n%*c^", FARG(line), offset, ' ');
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
        int start = index;
        while (is_ident_char(ch(), start==index)) {
            move();
        }

        switch (index-start) {
        case 2:
            if (text.text[start]   != 'i') break;
            if (text.text[start+1] != 'f') break;
            return ok(token(TK_KWD_IF, start));
        case 4:
            if (text.text[start]   != 'e') break;
            if (text.text[start+1] != 'l') break;
            if (text.text[start+2] != 's') break;
            if (text.text[start+3] != 'e') break;
            return ok(token(TK_KWD_ELSE, start));
        }

        return ok(token(TK_IDENT, start));
    }

    optional<Token> parse_string() {
        char first = ch();
        if (first != '"') { return error("Not a string"); }

        int start = index;
        move();

        while (ch() != first) {
            if (ch() == '\\') { move(); }
            move();
        }
        move();

        return ok(token(TK_STRING, start));
    }

    optional<Token> parse_color() {
        char first = ch();
        if (first != '#') return error("Not a color");

        int start = index;
        move();

        while (is_hex_char(ch())) {
            move();
        }

        auto tok = token(TK_COLOR, start);
        auto len = index-start;
        auto ttext = token_text(&tok);

        if (len != 4 && len != 7) {
            GENERIC_ERROR(&tok, FSTR " is not a valid hex color!", FARG(token_text(&tok)));
        }

        return ok(tok);
    }

    optional<Token> parse_env() {
        char first = ch();
        if (first != '$') return error("Not an env variable");

        int start = index;
        move();

        while (!is_whitespace(ch()) && ch() != 0) {
            move();
        }

        return ok(token(TK_ENV, start));
    }

    optional<Token> parse_number() {
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
                auto msg = stringf("Not a symbol: %c", ch());
                return error(msg.text);
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
            } else if (ch() == '#') {
                REQUIRED(tok, parse_color());
                bag_add(&tokens, tok);
            } else if (ch() == '$') {
                REQUIRED(tok, parse_env());
                bag_add(&tokens, tok);
            } else if (ch() == 0) {
                break;
            } else {
                auto tok = parse_symbol();
                if (tok.error) {
                    warn("Unexpected character: %c\n", ch());
                    warn(FSTR"\n", FARG(error_str(index)));
                    exit(1);
                }
                bag_add(&tokens, tok.value);
            }
        }
        return tokens;
    }
};


#endif
