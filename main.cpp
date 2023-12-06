#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>

struct string {
    char* text;
    size_t len;
};

template<typename T>
struct optional {
    bool ok;
    T value;
};

string to_string(char* ch) {
    string out;
    out.text = ch;
    out.len = 0;

    if (ch == 0) {
        return out;
    }

    while (*ch != 0) { ch++; out.len++; }
    return out;
}

string copy(string str) {
    auto data = (char*)malloc(str.len);
    for (int i=0; i<str.len; i++) {
        data[i] = str.text[i];
    }
    return {.text=data, .len=str.len};
}

template<typename T>
optional<T> ok(T val) { return {true, val}; }

template<typename T>
optional<T> not_ok() { return {false}; }

optional<string> cwd_str() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == 0) { return not_ok<string>(); }
    return ok(to_string(cwd));
}

string slice(string str, int start, int end) {
    return {
        .text=str.text+start,
        .len=(size_t)(end-start)
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

    return {path.text + (size_t)start_index, (size_t)(end_index-start_index)};
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
    if (file == 0) return not_ok<string>();
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* mem = (char*)malloc(size);
    auto res = fread(mem, size+1, 1, file);
    return ok<string>({mem, size});
}

optional<string> git_root(string root) {
    int nth = 0;
    char path[PATH_MAX];
    fill_charp(root, path);
    int idx = root.len;

    while (true) {
        charp_set(path, "/.git", idx);
        if (dir_exists(path)) {
            return ok(slice(root, 0, idx));
        }

        idx = index_of(root, '/', nth);
        if (idx == -1) break;
        nth--;
    }
    return not_ok<string>();
}

optional<string> git_branch_name(string root) {
    char path[PATH_MAX];
    fill_charp(root, path);
    charp_set(path, "/.git/HEAD", root.len);
    path[root.len + sizeof("/.git/HEAD") - 1] = 0;

    auto str = read_file(path);
    if (!str.ok) { return str; }

    auto idx = index_of(str.value, '/', -1);
    if (idx == -1) { return not_ok<string>(); }

    auto res = trim(slice(str.value, idx+1, str.value.len));
    return ok(res);
}

optional<string> env_var(const char* name) {
    auto val = getenv(name);
    if (val == 0) return not_ok<string>();
    return ok(to_string(val));
}

int main() {
    optional<string> opt_cwd = cwd_str();
    if (!opt_cwd.ok) return 0;
    auto cwd = opt_cwd.value;
    auto frag = path_frag(copy(cwd), -1, 0);
    auto git_dir = git_root(copy(cwd));
    auto branch = git_dir.ok ? git_branch_name(copy(git_dir.value)) : not_ok<string>();
    auto venv = env_var("VIRTUAL_ENV");
    auto last = env_var("?");

    printf("Dir name: %.*s\n", (int)frag.len, frag.text);
    if (git_dir.ok) {
        printf("GIT: %.*s\n", git_dir.value.len, git_dir.value.text);
    }

    if (branch.ok) {
        printf("BRANCH: %.*s\n", branch.value.len, branch.value.text);
    }

    if (venv.ok) {
        printf("VENV: %.*s\n", venv.value.len, venv.value.text);
    }
}
