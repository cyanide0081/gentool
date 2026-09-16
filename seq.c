// win32 api ;D
#if defined(_WIN32)
#    define _CRT_SECURE_NO_WARNINGS
#    include <windows.h>
#else
#    error "No windows? o_o"
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

// the data we need to build the text
typedef struct {
    Date date;
    size_t seq;
} Data;

#define data_create(d, s) ((Data){.date = (d), .seq = (s)})

// a string to replace stinky c-strings
typedef struct {
   char *data;
   size_t len;
} String;

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

#define string_lit(l) ((String){.data = (l), .len = sizeof(l) - 1})
#define string_create(s, l) ((String){.data = (s), .len = (size_t)(l)})
#define string_from_cstring(s) ((String){.data = (s), .len = strlen(s)})
#define string_is_empty(s) ((s).len == 0)

#define RADIX 10
#define GBUF_SIZE 0xFF

// global data
static char DATE_BUF[GBUF_SIZE];
static char FILE_IN_BUF[GBUF_SIZE];
static char FILE_OUT_BUF[GBUF_SIZE];

static inline size_t max_decimal(size_t digits)
{
    size_t result = 1;
    while (digits-- > 0) {
        result *= RADIX;
    }

    return result - 1;
}

static inline size_t buf_fill_fmt(
    char *dst, char *src, char c,
    size_t value, bool saturate
) {
    // get our length first
    size_t len = 0;
    while (*src++ == c) {
        len += 1;
    }

    if (saturate && value > max_decimal(len)) {
        memset(dst, '9', len);
    } else {
        // dumb zero-padding
        memset(dst, '0', len);

        // fill out our number
        for (size_t i = 0; i < len; i++) {
            size_t digit = value % RADIX;
            value /= RADIX;
            dst[len - i - 1] = digit + '0';
        }
    }

    return len;
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
                width = buf_fill_fmt(dst, src, c, data.date.year, false);
                break;
            case 'M':
                width = buf_fill_fmt(dst, src, c, data.date.month, false);
                break;
            case 'd':
                width = buf_fill_fmt(dst, src, c, data.date.day, false);
                break;
            case 'x':
                width = buf_fill_fmt(dst, src, c, data.seq, true);
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

// NOTE: platform specific section (only windows for now)
static bool clipboard_set_string(String str)
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
    data[str.len] = '\0';

    GlobalUnlock(handle);
    SetClipboardData(CF_TEXT, handle);
    CloseClipboard();
    return true;
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

    FILE *fp = fopen(filename.data, "rb");
    if (fp != NULL) {
        len = fread(buf, 1, GBUF_SIZE - 1, fp);
        fclose(fp);
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
    FILE *fp = fopen(filename.data, "wb");
    if (fp != NULL) {
        fwrite(str.data, 1, str.len, fp);
        fclose(fp);
    }
}

#define DATA_FILE_NAME ".seq"

/**
 * stores an auto-incrementing sequence after the formatted date
 * and restores the value daily, copying the result to the clipboard
 * on each run, such as 2026091301
 *
 * usage: seq [fmt]
 * fmt: a sequence of symbols (amount of letters sets the padding/truncation):
 *
 * symbol    meaning                   example
 * ------    -------                   -------
 * y         year of era               2004
 * M         month of year             06
 * d         day of month              09
 * x         incrementing sequence     (auto-incrementing)
 *
 * example: seq yyyyMMddxxx
 * ->           20260913012
 *
 * TODO: on windows the data file isn't hidden because we need to create it
 *       through CreateFile with the FILE_ATTRIBUTE_HIDDEN flag set (portability...)
 */
int main(int argc, char *argv[])
{
    // validate the input
    if (argc != 2 || strlen(argv[1]) >= GBUF_SIZE) {
        fprintf(stderr, "usage: seq [fmt] (example: seq yyyyMMddxxx)\n");
        return 1;
    }

    // get the current date (year, month and day)
    Date now = date_now();

    // do our file and sequence bookkeeping
    String contents = string_from_file_contents(string_lit(DATA_FILE_NAME));
    size_t seq = get_and_increment_sequence(contents, now);
    Data data = data_create(now, seq);
    file_write_string(string_lit(DATA_FILE_NAME), string_from_data(data));

    // parse our format
    String fmt = string_from_cstring(argv[1]);
    String result = string_from_fmt(fmt, data);
    printf("%.*s\n", (int)result.len, result.data);

    // copy the result to the clipboard
    return (int)clipboard_set_string(result);
}
