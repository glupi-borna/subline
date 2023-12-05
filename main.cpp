#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

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

bool is_git_dir(string root) {
    int nth = 0;
    char path[PATH_MAX];
    int idx = root.len;

    while (true) {
        auto s = slice(root, 0, idx);
        fill_charp(s, path);
        charp_set(path, "/.git", s.len);
        if (dir_exists(path)) {
            return true;
        }

        idx = index_of(root, '/', nth);
        if (idx == -1) break;
        nth--;
    }
    return false;
}

int main() {
    optional<string> opt_cwd = cwd_str();
    if (!opt_cwd.ok) return 0;
    auto cwd = opt_cwd.value;
    auto frag = path_frag(cwd, -1, 0);

    printf("%.*s\n", (int)frag.len, frag.text);
    printf("%d\n", is_git_dir(cwd));

    DIR *dp;
    dirent *ep;
    dp = opendir("./");

    if (dp == NULL) return 0;
}
