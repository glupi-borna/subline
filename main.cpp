#include "utils.cpp"
#include "tokenizer.cpp"
#include "ast.cpp"

#include <cstdio>
#include <initializer_list>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>


optional<string> cwd_str() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == 0) { return error("Failed to get cwd"); }
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
    else { start_index = index_of(&path, '/', start); }

    if (end == 0) { end_index = path.len; }
    else { end_index = index_of(&path, '/', end); }

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

#define CHUNK_SIZE 1024

string read_pipe(FILE* pipe) {
    size_t buffer_size = CHUNK_SIZE;
    char* pipe_text = (char*)malloc(buffer_size);

    size_t total_size = 0;
    ssize_t bytes_read;

    while ((bytes_read = fread(pipe_text+total_size, 1, CHUNK_SIZE, pipe))) {
        total_size += bytes_read;
        if (total_size+CHUNK_SIZE > buffer_size) {
            buffer_size += CHUNK_SIZE;
            pipe_text = (char*)realloc(pipe_text, buffer_size);
        }
    }
    assert(bytes_read != -1, "Failed to read pipe!");

    pipe_text[total_size] = 0;
    return to_string(pipe_text);
}

string read_pipe(int pipe) {
    size_t buffer_size = CHUNK_SIZE;
    char* pipe_text = (char*)malloc(buffer_size);

    size_t total_size = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipe, pipe_text+total_size, CHUNK_SIZE))) {
        total_size += bytes_read;
        if (total_size+CHUNK_SIZE > buffer_size) {
            buffer_size += CHUNK_SIZE;
            pipe_text = (char*)realloc(pipe_text, buffer_size);
        }
    }
    assert(bytes_read != -1, "Failed to read pipe!");

    pipe_text[total_size] = 0;
    return to_string(pipe_text);
}

string read_file(FILE* file) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* mem = (char*)malloc(size);
    auto res = fread(mem, size+1, 1, file);
    return string{mem, size};
}

optional<string> read_file(const char* path) {
    auto file = fopen(path, "r");
    if (file == 0) return error("Failed to open file");
    string out = read_file(file);
    fclose(file);
    return ok(out);
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
        idx = index_of(&root, '/', -1-nth);
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

    // @TODO: str needs to be freed
    auto str = read_file(path);
    if (str.error) { return str; }

    auto idx = index_of(&str.value, '/', -1);
    if (idx == -1) { return error("Failed to find ref in .git/HEAD"); }

    auto branch = slice(&str.value, idx+1, str.value.len);
    branch = trim(&branch);
    return ok(copy(&branch));
}

optional<string> env_var(const char* name) {
    auto val = getenv(name);
    if (val == 0) return error("Env var not present");
    return ok(to_string(val));
}

struct SGR_Tuple {
    const char* name;
    int code;
};

SGR_Tuple COLOR[] = {
    {"default", -1},
    {"black",    0},
    {"red",      1},
    {"green",    2},
    {"yellow",   3},
    {"blue",     4},
    {"magenta",  5},
    {"cyan",     6},
    {"white",    7},
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

s32 color_code(const string* color) {
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
    s64 value;
};

u8 hex_digit_to_int(const char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'A' && digit <= 'F') return (digit - 'A') + 10;
    if (digit >= 'a' && digit <= 'f') return (digit - 'a') + 10;
    warn("Invalid hex digit: %c\n", digit);
    exit(1);
}

u32 hex_to_int(const string* hex) {
    u32 r, g, b;
    switch (hex->len) {
    case 4: {
        r = hex_digit_to_int(hex->text[1]);
        g = hex_digit_to_int(hex->text[2]);
        b = hex_digit_to_int(hex->text[3]);
        r += (r*16);
        g += (g*16);
        b += (b*16);
    } break;
    case 7: {
        u8 r1 = hex_digit_to_int(hex->text[1]);
        u8 r2 = hex_digit_to_int(hex->text[2]);
        u8 g1 = hex_digit_to_int(hex->text[3]);
        u8 g2 = hex_digit_to_int(hex->text[4]);
        u8 b1 = hex_digit_to_int(hex->text[5]);
        u8 b2 = hex_digit_to_int(hex->text[6]);
        r = r1*16 + r2;
        g = g1*16 + g2;
        b = b1*16 + b2;
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
#define SGR1(arg) printf(ESCAPE "%ldm", (s64)arg)

Display_Style default_style() {
    Display_Style style;
    style.bg = Color{CT_SGR, -1};
    style.text = Color{CT_SGR, -1};
    style.intensity = INT_NORMAL;
    style.italic = false;
    style.underline = false;
    style.strike = false;
    return style;
}

void reset(Subline_State* s) {
    SGR1(0);
    s->style = default_style();
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

void style_apply(Subline_State* s, Display_Style old_style, Display_Style new_style);

void text_apply(Subline_State* s, Color col) {
    if (col.type == CT_SGR) {
        if (col.value == -1) {
            SGR1(0);
            auto text_only = default_style();
            text_only.text = col;
            s->style.text = col;
            style_apply(s, text_only, s->style);
            return;
        }

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
        if (col.value == -1) {
            SGR1(0);
            auto bg_only = default_style();
            bg_only.bg = col;
            s->style.bg = col;
            style_apply(s, bg_only, s->style);
            return;
        }

        SGR1(40+col.value);
    } else if (col.type == CT_HEX) {
        printf(ESCAPE "48;2;%d;%d;%dm", red(col), green(col), blue(col));
    } else {
        fprintf(stderr, "Unknown color type: %d\n", col.type);
        exit(1);
    }
    s->style.bg = col;
}

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

#define ARGUMENT_TXT(count) ((count)==1 ? "argument" : "arguments")

#define FN_ERROR(FN, FMT, ...) \
    GENERIC_ERROR(FN, FSTR "() " FMT, FARG(token_text(FN)), __VA_ARGS__); \
    exit(1);

void assert_arg_count(Token* fn_name, bag<AST_Node*>* args, int count) {
    if (args == 0 && count != 0) {
        FN_ERROR(fn_name, "expects %d %s", count, ARGUMENT_TXT(count));
    }

    if (args == 0) return;

    if (args->len != count) {
        FN_ERROR(fn_name, "expects %d %s", count, ARGUMENT_TXT(count));
    }
}

void assert_arg_count_min(Token* fn_name, bag<AST_Node*>* args, int count) {
    if (args == 0 || args->len < count) {
        FN_ERROR(fn_name, "expects at least %d %s", count, ARGUMENT_TXT(count));
    }
}

string type_string(
    std::initializer_list<AT_TYPE> types
) {
    char type_str[1028];
    int offset = 0;
    for (auto t : types) {
        if (offset != 0) {
            type_str[offset++] = ' ';
            type_str[offset++] = '|';
            type_str[offset++] = ' ';
        }

        auto type_name = ast_type_str(t);
        while (*type_name != 0) {
            type_str[offset] = *type_name;
            type_name++;
            offset++;
        }
    }

    auto str = to_string(type_str);
    return copy(&str);
}

bool arg_type_matches(
    AST_Node* arg,
    std::initializer_list<AT_TYPE> types
) {
    for (auto t : types) {
        if (arg->kind == t) return true;
    }
    return false;
}

AST_Node* arg_type_raw(
    Token* fn_name,
    bag<AST_Node*>* args,
    int idx,
    std::initializer_list<AT_TYPE> types
) {
    auto arg = args->items[idx];
    if (!arg_type_matches(arg, types)) {
        GENERIC_ERROR(
            arg, FSTR "() expects argument %d to be of type " FSTR "!\n",
            FARG(to_string(fn_name)), idx+1, FARG(type_string(types))
        );
    }
    return arg;
}

string arg_type(
    Token* fn_name,
    bag<AST_Node*>* args,
    int idx,
    std::initializer_list<AT_TYPE> types
) {
    auto arg = arg_type_raw(fn_name, args, idx, types);
    return unquote(token_text(&to_value(arg)->token));
}

string arg_type_named(
    Token* fn_name,
    bag<AST_Node*>* args,
    const char* arg_name,
    std::initializer_list<AT_TYPE> types
) {
    auto idx = named_param_idx(args, arg_name);
    if (idx == -1) {
        GENERIC_ERROR(
            fn_name, FSTR "() expects a named argument '%s' of type " FSTR "!\n",
            FARG(token_text(fn_name)), arg_name, FARG(type_string(types))
        );
    }

    auto named = to_param_named(args->items[idx]);
    auto arg = named->value;
    if (!arg_type_matches(named->value, types)) {
        GENERIC_ERROR(
            &named->name, FSTR "() expects '%s' to be of type " FSTR "!\n",
            FARG(token_text(fn_name)), arg_name, FARG(type_string(types))
        );
    };

    return unquote(token_text(&to_value(arg)->token));
}

struct Command_Result {
    string out;
    string err;
    int code;
};

Command_Result run_command(string* cmd_args, int len) {
    int offset = 0;
    int idx = 0;
    char cmd_text[1024];
    char* arr[len+1];

    for (int i=0; i<len; i++) {
        auto arg = cmd_args[i];
        arr[idx] = cmd_text + offset;

        for (int i=0; i<arg.len; i++) {
            cmd_text[offset] = arg.text[i];
            offset++;
        }

        cmd_text[offset] = 0;
        offset++;
        idx++;
    }
    arr[idx] = 0;

    int pipe_stdout[2];
    int pipe_stderr[2];
    assert(pipe(pipe_stdout) == 0, "Failed to create output pipe!");
    assert(pipe(pipe_stderr) == 0, "Failed to create error pipe!");

    pid_t pid = fork();
    assert(pid != -1, "Fork failed! %s", strerror(errno));

    if (pid == 0) {
        close(pipe_stdout[0]);
        close(pipe_stderr[0]);

        dup2(pipe_stdout[1], STDOUT_FILENO);
        dup2(pipe_stderr[1], STDERR_FILENO);

        close(pipe_stdout[1]);
        close(pipe_stderr[1]);

        execvp(arr[0], arr);
        assert(false, "Failed to run %s: %s", arr[0], strerror(errno));
    }

    close(pipe_stdout[1]);
    close(pipe_stderr[1]);

    siginfo_t siginfo;
    auto wait_res = waitid(P_PID, pid, &siginfo, WEXITED);
    assert(wait_res >= 0, "waitid() failed: %s", strerror(errno));

    auto out = read_pipe(pipe_stdout[0]);
    auto err = read_pipe(pipe_stderr[0]);

    return {
        .out=out,
        .err=err,
        .code=WEXITSTATUS(siginfo.si_status),
    };
}

string do_call(Subline_State* s, Token* fn_name, bag<AST_Node*>* args) {
    auto fn_name_str = token_text(fn_name);

    #define ARG_COUNT(count) assert_arg_count(fn_name, args, count)
    #define ARG_COUNT_MIN(count) assert_arg_count_min(fn_name, args, count)
    #define ARG_TYPE(idx, ...) arg_type(fn_name, args, idx, {__VA_ARGS__})
    #define ARG_TYPE_RAW(idx, ...) arg_type_raw(fn_name, args, idx, {__VA_ARGS__})
    #define ARG_NAMED(name, ...) arg_type_named(fn_name, args, name, {__VA_ARGS__})

    if (equal(&fn_name_str, "text")) {
        ARG_COUNT(1);
        auto value = ARG_TYPE(0, AT_STRING, AT_COLOR, AT_IDENT);
        text_apply(s, string_to_color(value));

        return {0};

    } else if (equal(&fn_name_str, "bg")) {
        ARG_COUNT(1);
        auto value = ARG_TYPE(0, AT_STRING, AT_COLOR, AT_IDENT);
        bg_apply(s, string_to_color(value));
        return {0};

    } else if (equal(&fn_name_str, "cap")) {
        ARG_COUNT(3);
        auto cap_arg = eval(ARG_TYPE_RAW(0, AT_STRING, AT_IDENT));
        auto text = ARG_NAMED("text", AT_IDENT, AT_STRING, AT_COLOR);
        auto bg = ARG_NAMED("bg", AT_IDENT, AT_STRING, AT_COLOR);

        auto text_col = string_to_color(text);
        auto bg_col = string_to_color(bg);

        text_apply(s, bg_col);
        print(FSTR, FARG(cap_arg));
        text_apply(s, text_col);
        bg_apply(s, bg_col);

        return {0};

    } else if (equal(&fn_name_str, "arrow")) {
        ARG_COUNT(3);
        auto arrow_arg = eval(ARG_TYPE_RAW(0, AT_STRING, AT_IDENT));
        auto text = ARG_NAMED("text", AT_IDENT, AT_STRING, AT_COLOR);
        auto bg = ARG_NAMED("bg", AT_IDENT, AT_STRING, AT_COLOR);

        auto text_col = string_to_color(text);
        auto bg_col = string_to_color(bg);

        text_apply(s, s->style.bg);
        bg_apply(s, bg_col);

        print(FSTR, FARG(arrow_arg));

        text_apply(s, text_col);
        bg_apply(s, bg_col);

        return {0};

    } else if (equal(&fn_name_str, "env")) {
        ARG_COUNT(1);
        auto value = ARG_TYPE(0, AT_IDENT, AT_STRING);
        char envname[255];
        fill_charp(value, envname);
        auto envvar = getenv(envname);
        if (envvar == 0) return {0};
        auto str = to_string(envvar);
        return copy(&str);

    } else if (equal(&fn_name_str, "stdout")) {
        ARG_COUNT_MIN(1);
        string strs[args->len];
        for (int i=0; i<args->len; i++) {
            strs[i] = eval(args->items[i]);
        }
        auto res = run_command(strs, args->len);
        return trim(&res.out);

    } else if (equal(&fn_name_str, "_")) {
        return to_string(" ");

    } else if (equal(&fn_name_str, "bold")) {
        ARG_COUNT(0);
        bold_enable(s);
        return {0};

    } else if (equal(&fn_name_str, "regular")) {
        ARG_COUNT(0);
        bold_dim_disable(s);
        return {0};

    } else if (equal(&fn_name_str, "dim")) {
        ARG_COUNT(0);
        dim_enable(s);
        return {0};

    } else if (equal(&fn_name_str, "italic")) {
        ARG_COUNT(0);
        italic_enable(s);
        return {0};

    } else if (equal(&fn_name_str, "normal")) {
        ARG_COUNT(0);
        italic_disable(s);
        return {0};

    } else if (equal(&fn_name_str, "underline")) {
        ARG_COUNT(0);
        underline_enable(s);
        return {0};

    } else if (equal(&fn_name_str, "no-underline")) {
        ARG_COUNT(0);
        underline_disable(s);
        return {0};

    } else if (equal(&fn_name_str, "strike")) {
        ARG_COUNT(0);
        strike_disable(s);
        return {0};

    } else if (equal(&fn_name_str, "no-strike")) {
        ARG_COUNT(0);
        strike_enable(s);
        return {0};

    } else if (equal(&fn_name_str, "dir")) {
        ARG_COUNT(0);
        auto home_charp = getenv("HOME");
        if (home_charp == 0) return s->cwd;

        auto home = to_string(home_charp);
        if (starts(&s->cwd, &home)) {
            return stringf("~" FSTR, FARG(strip_prefix(&s->cwd, &home)));
        }

        return s->cwd;

    } else if (equal(&fn_name_str, "in-git-repo")) {
        ARG_COUNT(0);
        if (s->git.error == 0) {
            return SBLN_TRUE;
        } else {
            return SBLN_FALSE;
        }

    } else if (equal(&fn_name_str, "git-branch")) {
        ARG_COUNT(0);
        if (s->git.error == 0) {
            return s->git.value.branch;
        } else {
            return {0};
        }

    } else if (equal(&fn_name_str, "git-root")) {
        ARG_COUNT(0);
        if (s->git.error == 0) {
            return s->git.value.dir;
        } else {
            return {0};
        }

    } else if (equal(&fn_name_str, "git-dir")) {
        ARG_COUNT(0);
        auto cwd = &s->cwd;
        if (s->git.error != 0) return {0};

        auto gitdir = &s->git.value.dir;
        if (equal(cwd, gitdir)) {
            return const_string("/");
        } else {
            return strip_prefix(cwd, gitdir);
        }

    } else if (equal(&fn_name_str, "not")) {
        ARG_COUNT(1);
        auto arg = args->items[0];
        auto val = eval(arg);
        return equal(&val, &SBLN_TRUE) ? SBLN_FALSE : SBLN_TRUE;

    } else if (equal(&fn_name_str, "eq")) {
        ARG_COUNT(2);
        auto arg1 = eval(args->items[0]);
        auto arg2 = eval(args->items[1]);
        return equal(&arg1, &arg2) ? SBLN_TRUE : SBLN_FALSE;

    } else if (equal(&fn_name_str, "starts")) {
        ARG_COUNT(2);
        auto arg1 = eval(args->items[0]);
        auto arg2 = eval(args->items[1]);

        return starts(&arg1, &arg2) ? SBLN_TRUE : SBLN_FALSE;

    } else if (equal(&fn_name_str, "strip-prefix")) {
        ARG_COUNT(2);
        auto arg1 = eval(args->items[0]);
        auto arg2 = eval(args->items[1]);

        if (starts(&arg1, &arg2)) {
            return string{arg1.text+arg2.len, arg1.len-arg2.len};
        } else {
            return arg1;
        }
    }

    GENERIC_ERROR(fn_name, "Unhandled call: " FSTR, FARG(fn_name_str));
    exit(0);
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

#include <bitset>
#include <iostream>
void print_binary(s64 num) {
    std::bitset<64> x(num);
    std::cout << x << '\n';
}

string replace_escapes(Token* tok) {
    auto str = unquote(token_text(tok));
    char* new_text = (char*)malloc(str.len+1);
    int ni = 0;
    for (int i=0; i<str.len; i++, ni++) {
        if (str.text[i] != '\\') {
            new_text[ni] = str.text[i];
            continue;
        }
        switch (str.text[i+1]) {
            case 'a': new_text[ni] = '\a'; i++; break;
            case 'b': new_text[ni] = '\b'; i++; break;
            case 'e': new_text[ni] = '\e'; i++; break;
            case 'f': new_text[ni] = '\f'; i++; break;
            case 'n': new_text[ni] = '\n'; i++; break;
            case 'r': new_text[ni] = '\r'; i++; break;
            case 't': new_text[ni] = '\t'; i++; break;
            case 'v': new_text[ni] = '\v'; i++; break;
            case '\\': new_text[ni] = '\\'; i++; break;
            case '\'': new_text[ni] = '\''; i++; break;
            case '\"': new_text[ni] = '"'; i++; break;
            case '\?': new_text[ni] = '?'; i++; break;
            case 'u': {
                #define CHECK_HEX(OFFSET) \
                if (!is_hex_char(str.text[i+OFFSET])) { \
                    auto err = error_at(tok->source, (str.text+i) - tok->source->text); \
                    warn(FSTR "Invalid escape sequence.", FARG(err)); \
                    exit(1); \
                }
                CHECK_HEX(2);
                CHECK_HEX(3);
                CHECK_HEX(4);
                CHECK_HEX(5);
                auto val = strtol(str.text+i+2, 0, 16);

                if (val <= 0x7f) {
                    new_text[ni] = (char)val;
                } else if (val <= 0x7ff) {
                    new_text[ni++] = 0xC0 | (char)((val >> 6) & 0x1F);
                    new_text[ni]   = 0x80 | (char)(val & 0x3f);
                } else if (val <= 0xffff) {
                    new_text[ni++] = 0xE0 | (char)((val >> 12) & 0x0F);
                    new_text[ni++] = 0x80 | (char)((val >> 6) & 0x3F);
                    new_text[ni]   = 0x80 | (char)(val & 0x3F);
                }

                i+=5;
            } break;

            case 'U': {
                #define CHECK_HEX(OFFSET) \
                if (!is_hex_char(str.text[i+OFFSET])) { \
                    auto err = error_at(tok->source, (str.text+i) - tok->source->text); \
                    warn(FSTR "Invalid escape sequence.", FARG(err)); \
                    exit(1); \
                }
                CHECK_HEX(2);
                CHECK_HEX(3);
                CHECK_HEX(4);
                CHECK_HEX(5);
                CHECK_HEX(6);
                CHECK_HEX(7);
                CHECK_HEX(8);
                CHECK_HEX(9);
                auto val = strtol(str.text+i+2, 0, 16);

                if (val <= 0x7f) {
                    new_text[ni] = (char)val;
                } else if (val <= 0x7ff) {
                    new_text[ni++] = 0xC0 | (char)((val >> 6) & 0x1F);
                    new_text[ni]   = 0x80 | (char)((val >> 0) & 0x3f);
                } else if (val <= 0xffff) {
                    new_text[ni++] = 0xE0 | (char)((val >> 12) & 0x0F);
                    new_text[ni++] = 0x80 | (char)((val >>  6) & 0x3F);
                    new_text[ni]   = 0x80 | (char)((val >>  0) & 0x3F);
                } else if (val <= 0x10FFF) {
                    new_text[ni++] = 0xF0 | (char)((val >> 18) & 0x07);
                    new_text[ni++] = 0x80 | (char)((val >> 12) & 0x3F);
                    new_text[ni++] = 0x80 | (char)((val >>  6) & 0x3F);
                    new_text[ni]   = 0x80 | (char)((val >>  0) & 0x3F);
                } else {
                    new_text[ni++] = (char)0xEF;
                    new_text[ni++] = (char)0xBF;
                    new_text[ni]   = (char)0xBD;
                }

                i += 9;
            } break;
        }
    }
    new_text[ni] = 0;

    return string{new_text, ni};
}

string eval(AST_Node* node) {
    switch (node->kind) {
    case AT_IDENT: {
        auto val = to_value(node);
        return do_call(&state, &val->token, 0);
    } break;

    case AT_STRING: {
        auto val = to_value(node);
        return unquote(replace_escapes(&val->token));
    } break;

    case AT_COLOR:
    case AT_NUMBER: {
        return token_text(&to_value(node)->token);
    } break;

    case AT_ENV: {
        auto val = token_text(&to_value(node)->token);
        val.text++; val.len--;
        char envname[val.len+1];
        fill_charp(val, envname);
        auto envvar = getenv(envname);
        if (envvar == 0) return {0};
        auto str = to_string(envvar);
        return copy(&str);
    } break;

    case AT_CALL: {
        auto val = to_call(node);
        auto params = val->params->values;
        return do_call(&state, &val->ident, &params);
    } break;

    case AT_BLOCK: {
        auto block = to_block(node);

        auto old_style = state.style;
        if (block->params.error == 0) {
            auto params = block->params.value;
            for (int i=0; i<params->values.len; i++) {
                eval(params->values.items[i]);
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

    case AT_IF: {
        auto if_stmt = to_if(node);
        auto val = eval(if_stmt->condition);
        if (equal(&val, &SBLN_TRUE)) {
            auto v = eval(if_stmt->body);
            display(v);
            return v;
        } else if (if_stmt->else_body.error == 0) {
            auto v = eval(if_stmt->else_body.value);
            display(v);
            return v;
        }
        return {0};
    } break;

    default: {
        printf("[%d]", node->kind);
        return {0};
    } break;
    }
}

int main() {
    state.style = default_style();
    string subline = read_pipe(stdin);

    REQUIRED(state.cwd, cwd_str());
    // string frag = path_frag(copy(&cwd), -1, 0);
    // auto venv = env_var("VIRTUAL_ENV");
    // auto last = env_var("?");


    optional<string> git_dir = git_root(copy(&state.cwd));
    state.git.error = git_dir.error;
    if (git_dir.error == 0) {
        state.git.value.dir = git_dir.value;
        // @TODO: This copy() needs to be freed
        auto branch = git_branch_name(copy(&git_dir.value));
        if (branch.error) {
            state.git.value.branch = {0};
        } else {
            state.git.value.branch = branch.value;
        }
    }

    auto st = Subline_Tokenizer(subline);
    /*auto st = Subline_Tokenizer(to_string(R"END(
        _
        cap("P((", bg=#ff0000, text=#ffffff)

        [bold] {
            _
            if in-git-repo {
                git-branch _ "(" $USER ")"
            } else {
                $USER
            }
            _
        }

        arrow("P>>", bg=#ffff00, text=#000000)

        [bold] {
            _
            if in-git-repo {
                git-dir
            } else {
                dir
            }
            _
        }

        arrow("P>>", text=default, bg=default)
    )END"));*/

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
