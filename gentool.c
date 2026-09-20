// win32 api ;D
#if defined(_WIN64)
#    define PTF_WINDOWS
#    define _CRT_SECURE_NO_WARNINGS
#    include <windows.h>
#    include <bcrypt.h>
#else
#    error "No 64-bit windows? o_o"
#endif

// standard stuff (might delete later haha)
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// a date without a time (wow)
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
} Date;

// the data we need to build the sequence text
typedef struct {
    Date date;
    size_t seq;
} Data;

#define data_create(d, s) ((Data){.date = (d), .seq = (s)})

// a string type to replace stinky c-strings (null-termination is assumed)
typedef struct {
   char *data;
   size_t len;
} String;

// https://www.rfc-editor.org/rfc/rfc9562.html#name-uuid-version-4
typedef struct {
    uint64_t hi;
    uint64_t lo;
} UUIDv4;

// opaque for per-os definition
typedef struct File File;

#define string_lit(l) ((String){.data = (l), .len = sizeof(l) - 1})
#define string_create(s, l) ((String){.data = (s), .len = (size_t)(l)})
#define string_from_cstring(s) ((String){.data = (s), .len = strlen(s)})
#define string_equals(a, b) ((a).len == (b).len && memcmp((a).data, (b).data, (a).len) == 0)
#define string_is_empty(s) ((s).len == 0)
#define string_length(s) ((s).len)

// NOTE: platform specific section (only windows for now)
#if defined(PTF_WINDOWS)
struct File {
    HANDLE handle;
};

#define FILE_ACCESS_READ GENERIC_READ
#define FILE_ACCESS_WRITE GENERIC_WRITE
#define FILE_MODE_OPEN_ALWAYS OPEN_ALWAYS
#define FILE_MODE_CREATE_ALWAYS CREATE_ALWAYS
#define FILE_ATTR_HIDDEN FILE_ATTRIBUTE_HIDDEN

static inline void ptf_get_entropy(void *buf, size_t bytes)
{
    BCryptGenRandom(NULL, buf, bytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

static inline bool ptf_file_is_valid(File file)
{
    return file.handle != INVALID_HANDLE_VALUE;
}

static inline File ptf_file_open(String filename, uint32_t access, uint32_t mode, uint32_t attr)
{
    File result = {
        .handle = CreateFileA(filename.data, access, 0, NULL, mode, attr, NULL),
    };
    return result;
}

static inline void ptf_file_close(File file)
{
    CloseHandle(file.handle);
}

static inline size_t ptf_file_read(File file, void *buf, size_t bytes)
{
    DWORD read = 0;
    ReadFile(file.handle, buf, bytes, &read, NULL);
    return (size_t)read;
}

static inline size_t ptf_file_write(File file, void *buf, size_t bytes)
{
    DWORD written = 0;
    WriteFile(file.handle, buf, bytes, &written, NULL);
    return (size_t)written;
}

static bool ptf_clipboard_set_string(String str)
{
    if (!OpenClipboard(NULL)) {
        fprintf(stderr, "couldn't open dat clipboard\n");
        return false;
    }
    if (!EmptyClipboard()) {
        fprintf(stderr, "couldn't empty dat clipboard\n");
        CloseClipboard();
        return false;
    }

    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, str.len + 1);
    if (handle == NULL) {
        fprintf(stderr, "couldn't allocate dat global object smh\n");
        CloseClipboard();
        return false;
    }

    char *data = GlobalLock(handle);
    if (data == NULL) {
        fprintf(stderr, "couldn't lock dat movable memory smh\n");
        GlobalFree(handle);
        CloseClipboard();
        return false;
    }

    memcpy(data, str.data, str.len);
    data[str.len] = '\0'; // wow so explicit and safe
    GlobalUnlock(handle);

    if (SetClipboardData(CF_TEXT, handle) == NULL) {
        fprintf(stderr, "couldn't set dat clipboard data smh\n");
        GlobalFree(handle);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}
#endif // PTF_WINDOWS

#define GBUF_SIZE 0xFF
#define UUID_BUF_SIZE 37
#define CNPJ_BUF_SIZE 19

// global data
static char DATE_BUF[GBUF_SIZE];
static char FILE_IN_BUF[GBUF_SIZE];
static char FILE_OUT_BUF[GBUF_SIZE];
static char UUID_BUF[UUID_BUF_SIZE];
static char CNPJ_BUF[CNPJ_BUF_SIZE];

static Date date_now(void)
{
    // this code will be platform dependent someday probably idk
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    return (Date){
        .year = lt->tm_year + 1900,
        .month = lt->tm_mon + 1,
        .day = lt->tm_mday,
    };
}

static inline size_t max_num(size_t digits, size_t radix)
{
    size_t result = 1;
    while (digits-- > 0) {
        result *= radix;
    }

    return result - 1;
}


// could be resolved at compile-time but cba with all the macros
static inline bool arch_is_little_endian(void)
{
    size_t val = 1;
    return *((char*)&val) == 1; // if BE the 1-byte will be cut off
}

static inline uint64_t num_to_big_endian(uint64_t value)
{
    if (arch_is_little_endian()) {
        value =
            ((value & 0x00000000000000FFULL) << 56) |
            ((value & 0xFF00000000000000ULL) >> 56) |
            ((value & 0x000000000000FF00ULL) << 40) |
            ((value & 0x00FF000000000000ULL) >> 40) |
            ((value & 0x0000000000FF0000ULL) << 24) |
            ((value & 0x0000FF0000000000ULL) >> 24) |
            ((value & 0x00000000FF000000ULL) <<  8) |
            ((value & 0x000000FF00000000ULL) >>  8);
    }

    return value;
}

#define SYMBOL_TABLE_SIZE 36

static char SYMBOL_FROM_DIGIT_LOWER[SYMBOL_TABLE_SIZE] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z'
};

static char SYMBOL_FROM_DIGIT_UPPER[SYMBOL_TABLE_SIZE] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z'
};

static inline void buf_fill_num(
    char *buf, size_t ofs, size_t len,
    uint64_t value, size_t radix,
    bool upper, bool saturate
) {
    assert(radix <= SYMBOL_TABLE_SIZE);
    
    char *dst = buf + ofs;
    char *table = upper ? SYMBOL_FROM_DIGIT_UPPER : SYMBOL_FROM_DIGIT_LOWER;
    if (saturate && value > max_num(len, radix)) {
        memset(dst, '9', len);
    } else {
        // fill out our number
        for (size_t i = 0; i < len; i++) {
            size_t digit = value % radix;
            value /= radix;
            dst[len - i - 1] = table[digit];
        }
    }
}

static inline void buf_fill_bytes(
    char *dst, size_t dst_ofs,
    uint8_t *src, size_t src_ofs,
    size_t len, bool upper
) {
    char *table = upper ? SYMBOL_FROM_DIGIT_UPPER : SYMBOL_FROM_DIGIT_LOWER;
    for (size_t i = 0; i < len; i++) {
        dst[dst_ofs + i] = table[src[src_ofs + i]];
    }
}

static inline size_t buf_fill_fmt(
    char *dst, char *src, char c,
    size_t value, bool upper, bool saturate
) {
    // get our length first
    size_t len = 0;
    while (*src++ == c) {
        len += 1;
    }

    buf_fill_num(dst, 0, len, value, 10, upper, saturate);
    return len;
}

static inline void buf_trunc_bytes(uint8_t *buf, size_t ofs, size_t len, size_t radix)
{
    for (size_t i = 0; i < len; i++) {
        buf[ofs + i] %= radix;
    }
}

static uint8_t CNPJ_WEIGHTS[13] = {2, 3, 4, 5, 6, 7, 8, 9, 2, 3, 4, 5, 6};

static inline uint8_t cnpj_mod11(uint8_t *digits, bool first)
{
    size_t sum = 0;
    size_t count = first ? 12 : 13;
    for (size_t i = 0; i < count; i++) {
        sum += (digits[count - i - 1] * CNPJ_WEIGHTS[i]);
    }

    // TODO fix
    printf("%zu\n", sum % 11);
    return (uint8_t)sum % 11;
}

static String cnpj_random(void)
{
    char *buf = CNPJ_BUF;
    uint8_t digits[14] = {0};

    // fill separators
    buf[2] = '.';
    buf[6] = '.';
    buf[10] = '/';
    buf[15] = '-';

    // get and truncate random digits
    ptf_get_entropy(digits, 12);
    digits[0] = 1;
    digits[1] = 2;
    digits[2] = 10;
    digits[3] = 11;
    digits[4] = 12;
    digits[5] = 3;
    digits[6] = 4;
    digits[7] = 5;
    digits[8] = 0;
    digits[9] = 1;
    digits[10] = 13;
    digits[11] = 14;

    buf_trunc_bytes(digits, 0, 12, 36);

    // calc them check digits
    digits[12] = cnpj_mod11(digits, true);
    digits[13] = cnpj_mod11(digits, false);

    // fill values
    buf_fill_bytes(buf, 0,  digits, 0,  2, true);
    buf_fill_bytes(buf, 3,  digits, 2,  3, true);
    buf_fill_bytes(buf, 7,  digits, 5,  3, true);
    buf_fill_bytes(buf, 11, digits, 8,  4, true);
    buf_fill_bytes(buf, 16, digits, 12, 2, true);

    return string_create(buf, 18);   
}

static UUIDv4 uuidv4_random(void)
{
    UUIDv4 id = {0};
    uint8_t buf[sizeof(id)];

    ptf_get_entropy(buf, sizeof(buf)); // fill it all with random data
    buf[6] = (buf[6] & 0x0F) | 0x40; // ver field (bits 48-51) -> 0100xxxx
    buf[8] = (buf[8] & 0x3F) | 0x80; // var field (bits 64-65) -> 10xxxxxx

    memcpy(&id, buf, sizeof(id));
    return id;
}

static String string_from_uuidv4(UUIDv4 id)
{
    char *buf = UUID_BUF;

    // fill them dashes
    buf[8] = '-';
    buf[13] = '-';
    buf[18] = '-';
    buf[23] = '-';

    // force conversion to BE
    uint64_t hi = num_to_big_endian(id.hi);
    uint64_t lo = num_to_big_endian(id.lo);

    // fill values
    buf_fill_num(buf, 0,   8, (hi & 0xFFFFFFFF00000000ULL) >> 32, 16, false, false);
    buf_fill_num(buf, 9,   4, (hi & 0x00000000FFFF0000ULL) >> 16, 16, false, false);
    buf_fill_num(buf, 14,  4, (hi & 0x000000000000FFFFULL) >> 0,  16, false, false);
    buf_fill_num(buf, 19,  4, (lo & 0xFFFF000000000000ULL) >> 48, 16, false, false);
    buf_fill_num(buf, 24, 12, (lo & 0x0000FFFFFFFFFFFFULL) >> 0,  16, false, false);

    return string_create(buf, 36);
}

static String string_from_fmt(String fmt, Data data)
{
    // validate that our global buffer is large enough to fit the result
    assert(fmt.len < GBUF_SIZE);

    char *buf = DATE_BUF;

    // find repeating valid digits and inject their value etc
    for (size_t i = 0; i < fmt.len;) {
        char c = fmt.data[i];
        char *dst = &buf[i];
        char *src = &fmt.data[i];
        size_t width = 1;
        switch (c) {
            case 'y':
                width = buf_fill_fmt(dst, src, c, data.date.year, false, false);
                break;
            case 'M':
                width = buf_fill_fmt(dst, src, c, data.date.month, false, false);
                break;
            case 'd':
                width = buf_fill_fmt(dst, src, c, data.date.day, false, false);
                break;
            case 'x':
                width = buf_fill_fmt(dst, src, c, data.seq, false, true);
                break;
            default:
                // passthru
                *dst = c;
                break;
        }

        i += width;
    }

    return string_create(buf, fmt.len);
}

#define DATA_FMT "%hu-%hhu-%hhu:%zu"

static inline size_t get_and_increment_sequence(String contents, Date now)
{
    Data data = {0};
    size_t parsed = 0;

    // read in data if there is any
    if (!string_is_empty(contents)) {
        parsed = sscanf(
            contents.data, DATA_FMT,
            &data.date.year, &data.date.month, &data.date.day, &data.seq
        );
    }

    // reset sequence if stored date isn't today (or the format's broken)
    if (parsed != 4 || memcmp(&data.date, &now, sizeof(now)) != 0) {
        data.seq = 0;
    }

    return data.seq + 1;
}

static String string_from_file_contents(String filename)
{
    char *buf = FILE_IN_BUF;
    size_t len = 0;

    File file = ptf_file_open(filename, FILE_ACCESS_READ, FILE_MODE_OPEN_ALWAYS, 0);
    if (ptf_file_is_valid(file)) {
        len = ptf_file_read(file, buf, GBUF_SIZE - 1);
        ptf_file_close(file);
    }

    return string_create(buf, len);
}

static String string_from_data(Data data)
{
    char *buf = FILE_OUT_BUF;
    size_t len = snprintf(
        buf, GBUF_SIZE, DATA_FMT,
        data.date.year, data.date.month, data.date.day, data.seq
    );
    return string_create(buf, len);
}

static inline void file_write_string(String filename, String str)
{
    File file = ptf_file_open(filename, FILE_ACCESS_WRITE, FILE_MODE_CREATE_ALWAYS, FILE_ATTR_HIDDEN);
    if (ptf_file_is_valid(file)) {
        ptf_file_write(file, str.data, str.len);
        ptf_file_close(file);
    }
}

typedef enum {
    MODE_NONE,
    MODE_UUIDV4,
    MODE_CNPJ,
    MODE_SEQ,
} Mode;

static inline Mode mode_infer(int argc, char *argv[])
{
    Mode mode = MODE_NONE;
    if (argc > 1) {
        String arg = string_from_cstring(argv[1]);
        if (argc >= 2 && string_equals(arg, string_lit("uuidv4"))) {
            mode = MODE_UUIDV4;
        } else if (argc >= 2 && string_equals(arg, string_lit("cnpj"))) {
            mode = MODE_CNPJ;
        } else if (argc >= 3 && string_equals(arg, string_lit("seq"))) {
            mode = MODE_SEQ;
        }
    }

    return mode;
}

#define DATA_FILE_NAME ".seq"
#define USAGE \
    "generates data and copies it to the system clipboard\n" \
    "usage: gentool mode [args]\n" \
    "    modes:\n" \
    "        uuidv4   generates a pseudorandom UUIDv4\n" \
    "                 (example: gentool uuidv4)\n" \
    "        seq      generates a date-based sequence with daily reset\n" \
    "                 and format specifiers [y(year), M(month), d(day), x(sequence)]\n" \
    "                 (example: gentool seq yyyyMMddxxx)\n" \

int main(int argc, char *argv[])
{
    // we need at least a mode
    Mode mode = mode_infer(argc, argv);
    String result = {0};
    switch (mode) {
        case MODE_NONE:
            fprintf(stderr, USAGE);
            return 1;
        case MODE_UUIDV4:
            // yup
            result = string_from_uuidv4(uuidv4_random());
            break;
        case MODE_CNPJ:
            result = cnpj_random();
            break;
        case MODE_SEQ:
            // making sure it'll fit
            String fmt = string_from_cstring(argv[2]);
            if (string_length(fmt) >= GBUF_SIZE) {
                fprintf(stderr, "format string too large (%zu bytes)\n", fmt.len);
                return 1;
            }

            // do our file and sequence bookkeeping
            Date now = date_now();
            String contents = string_from_file_contents(string_lit(DATA_FILE_NAME));
            size_t seq = get_and_increment_sequence(contents, now);
            Data data = data_create(now, seq);
            file_write_string(string_lit(DATA_FILE_NAME), string_from_data(data));

            // parse our format
            result = string_from_fmt(fmt, data);
            break;
    }

    // print result and copy to clipboard
    printf("%.*s\n", (int)result.len, result.data);
    return ptf_clipboard_set_string(result) ? 0 : 1;
}
