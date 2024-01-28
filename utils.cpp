#ifndef subline_utils
#define subline_utils

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Standard concatenation macros
#define _CONCAT1(X,Y) X##Y
#define CONCAT(X,Y) _CONCAT1(X,Y)

// Output macros
#define print(...) fprintf(stdout, __VA_ARGS__)
#define warn(...) fprintf(stderr, __VA_ARGS__)

// Will print MSG to stderr and exit the program with status code 1.
#define assert(COND, ...) if (!(COND)) { warn(__VA_ARGS__); exit(1); }

template<typename T>
struct optional {
    const char* error;
    T value;

    void die_on_error(const char* msg=0) {
        assert(error == 0, "%s\n", msg==0?error:msg);
    }
};

// Unwraps an optional expression into a variable called
// NAME (which has to be declared previously). Uses
// optional::die_on_error to exit if the optional errored.
// Used to transform the following code:
//      auto optional_variable = fn_that_returns_optional();
//      optional_variable.die_on_error();
//      auto variable = optional_variable.value;
// into:
//      optional<type> variable;
//      REQUIRED(variable, fn_that_returns_optional());
#define REQUIRED(NAME, OPTIONAL_EXPR) \
    auto CONCAT(_opt, __LINE__) = OPTIONAL_EXPR; \
    CONCAT(_opt, __LINE__).die_on_error(); \
    NAME = CONCAT(_opt, __LINE__).value;

/// Shorthand for returning a successful optional.
template<typename T>
optional<T> ok(T val) { return {0, val}; }

/// Shorthand for returning an unsuccessful optional.
#define error(msg) { msg }

/// Array-like structure. Fast push, remove and lookup,
/// at the expense of no guarantee of order.
/// In practice, order is guaranteed, as long as items
/// are not removed from the middle of the array.
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

/// Initializes a bag and allocates
/// it's backing memory.
template<typename T>
bag<T> create_bag(int cap) {
    bag<T> b;
    b.capacity = cap;
    b.len = 0;
    b.items = (T*)malloc(sizeof(T) * cap);
    return b;
}

/// Appends an item to the end of a bag.
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

/// Removes an item from a bag.
/// If the item is not in the last position
/// in the bag, the item in the last position
/// is moved to overwrite it.
/// Then, the length is decreased.
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

/// Shorthand for removing the last
/// item from a bag.
/// This can be used safely, because it
/// preserves the order of the items in the bag.
template<typename T>
optional<T> bag_pop(bag<T>* b) {
    return bag_remove(b, b->len-1);
}

/// strings do not include the 0-terminator.
/// They can also be mere views into a different
/// string or char*, so there is no guarantee that
/// a 0-terminator even exists.
struct string {
    const char* text;
    int len;
};

// Macros to simplify using strings in printf-style
// functions. Used like:
//      printf("This is the string: " FSTR "\n", FARG(some_string))
#define FSTR "%.*s"
#define FARG(STR) STR.len, STR.text

/// Used to create strings at compile time.
template<size_t N>
constexpr string const_string(char const(&chars)[N]) {
    return string{chars, N};
}

/// Used to create strings with a printf api.
string stringf(const char* fmt, ...) __attribute__ ((format (printf, 1, 2)));
string stringf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* buf;
    int len = vasprintf(&buf, fmt, args);
    va_end(args);
    return {buf, len};
}

/// Converts char* into a string at runtime.
/// The char* must be 0-terminated.
const string to_string(const char* ch) {
    string out;
    out.text = ch;
    out.len = 0;
    if (ch == 0) { return out; }
    while (ch[out.len] != 0) { out.len++; }
    return out;
}

/// Returns the entire line that the character at the given
/// offset is a part of.
const string line_at_offset(const string* str, int offset) {
    int line_start = offset;
    int line_end = offset;
    while (line_start >= 0 && str->text[line_start] != '\n') {
        line_start--;
    }

    while (line_end < str->len && str->text[line_end] != '\n') {
        line_end++;
    }

    int len = line_end - line_start;
    return string{str->text + line_start + 1, len-1};
}

/// Allocates memory and copies a string to it.
/// len+1 bytes are allocated, with the last byte
/// set to 0. The last byte is NOT considered to
/// be part of the resulting string, but is put
/// there for convenience.
string copy(const string* str) {
    auto data = (char*)malloc(str->len+1);
    for (int i=0; i<str->len; i++) {
        data[i] = str->text[i];
    }
    data[str->len] = 0;
    return {.text=data, .len=str->len};
}

string to_string(int num) { return stringf("%d", num); }
string to_string(double num) { return stringf("%f", num); }

/// Concatenates all strings from an array into a single
/// string. Like with "copy", len+1 bytes will be allocated,
/// with the last byte set to 0, but not being part of the
/// resulting string.
string concat(const string* strings, int count) {
    string out;
    out.len = 0;
    for (int i=0; i<count; i++) {
        auto str = strings[i];
        out.len += str.len;
    }

    auto text = (char*)malloc(out.len+1);
    int idx = 0;

    for (int i=0; i<count; i++) {
        auto str = strings[i];
        for (int j=0; j<str.len; j++) {
            text[idx] = str.text[j];
            idx++;
        }
    }
    text[out.len] = 0;

    out.text = text;
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
        if (ch == 0) return true;
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

/// Returns a substring of the original string.
/// No bounds checking is performed.
const string slice(const string* str, int start, int end) {
    return {
        .text=str->text+start,
        .len=end-start,
    };
}

/// Returns a slice of the string with any leading
/// and trailing whitespace not included. This is
/// merely a view into the original string and not
/// a copy.
const string trim(const string* str) {
    string out = *str;
    char ch;
    while (ch = out.text[0], ch == ' ' || ch == '\n' || ch == '\t') {
        out.text++;
        out.len--;
    }

    while (ch = out.text[out.len-1], ch == ' ' || ch == '\n' || ch == '\t') {
        out.len--;
    }

    return out;
}

/// Returns a slice of the string with the prefix removed,
/// if it was there originally. Returns the original string
/// if it does not start with the prefix.
/// The returned value is merely a view into the original string
/// and not a copy.
const string strip_prefix(const string* str, const string* prefix) {
    if (starts(str, prefix)) {
        return string{
            str->text + prefix->len,
            str->len - prefix->len
        };
    } else {
        return *str;
    }
}

string error_at(string* s, int offset) {
    auto line = line_at_offset(s, offset);
    int line_start = line.text - s->text;
    int line_offset = offset - line_start;
    return stringf(FSTR "\n%*c^\n", FARG(line), line_offset, ' ');
}

/// Writes a string to a character pointer, and
/// zero-terminates it.
void fill_charp(string str, char* charp) {
    for (int i=0; i<str.len; i++) {
        charp[i] = str.text[i];
    }
    charp[str.len] = 0;
}

/// Writes "text" to the charp at offset "at", and
/// inserts a zero-terminator at the end.
void charp_set(char* charp, const char* text, int at) {
    int idx = 0;
    while (text[idx] != 0) {
        charp[at+idx] = text[idx];
        idx++;
    }
    charp[at+idx] = 0;
}

/// Finds the nth index of character "ch" within the
/// string. If the character is not present, returns -1.
int index_of(const string* str, char ch, int nth) {
    if (nth == 0) return -1;

    int step = 1;
    int start = 0;
    int end = str->len;

    if (nth < 0) {
        nth = -nth;
        step = -1;
        end = -1;
        start = str->len-1;
    }

    int current = 0;
    for (int i=start; i!=end; i+=step) {
        if (str->text[i] == ch) {
            current++;
            if (current == nth) {
                return i;
            }
        }
    }

    return -1;
}

/// Very barebones arena.
struct Arena {
    void* items;
    int capacity;
    int current;
};

/// Creates an arena with a specific capacity.
Arena create_arena(int cap) {
    Arena a;
    a.capacity = cap;
    a.current = 0;
    a.items = malloc(cap);
    return a;
}

/// Attempts to allocate space for T in an arena.
/// Terminates the program if there is not enough space.
template<typename T>
T* arena_alloc(Arena* a) {
    auto size = sizeof(T);
    auto old = a->current;
    auto next = a->current+size;
    assert(next <= a->capacity, "Arena space used up");
    a->current = next;
    return (T*)((char*)a->items+old);
}

/// Clears the arena.
void arena_clear(Arena* a) {
    a->current = 0;
}

/// Turns any pointer into a void*.
template<typename T>
void* spoof(T* ptr) { return (void*)ptr; }


#endif
