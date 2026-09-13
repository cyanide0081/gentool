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

// a string to replace stinky c-strings
typedef struct {
   char *data;
   size_t len;
} String;

static Date date_now(void)
{
    // this code will be platform dependent someday probably idk
    time_t t = time(NULL);
    struct tm stm = *localtime(&t);
    return (Date){
        .year = stm.tm_year + 1900,
        .month = stm.tm_mon,
        .day = stm.tm_mday,
    };
}

#define string_lit(l) ((String){.data = (l), .len = sizeof(l) - 1})
#define string_create(s, l) ((String){.data = (s), .len = (size_t)(l)})
#define string_from_cstring(s) ((String){.data = (s), .len = strlen(s)})
#define string_is_empty(s) ((s).len == 0)

#define RADIX 10

static inline size_t count_digits(size_t value)
{
    size_t result = 0;
    while (value > 0) {
        value /= RADIX;
        result += 1;
    }

    return result;
}

static inline size_t buf_fill_fmt(char *dst, char *src, char c, size_t value, bool truncate)
{
    // get our length first
    size_t len = 0;
    while (*src++ == c) {
        len += 1;
    }

    if (truncate && count_digits(value) > len) {
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

#define DATE_BUF_SIZE 0xFF
static char DATE_BUF[DATE_BUF_SIZE];

static String string_from_fmt(String fmt, Data data)
{
    // validate that our global buffer is large enough to fit the result
    assert(fmt.len < DATE_BUF_SIZE);

    char *buf = DATE_BUF;

    // find repeating valid digits and inject their value etc
    for (size_t i = 0; i < fmt.len;) {
        char c = fmt.data[i];
        char *dst = &buf[i];
        char *src = &fmt.data[i];
        size_t width;
        switch (c) {
            case 'y':
                width = buf_fill_fmt(dst, src, 'y', data.date.year, false);
                break;
            case 'M':
                width = buf_fill_fmt(dst, src, 'M', data.date.month, false);
                break;
            case 'd':
                width = buf_fill_fmt(dst, src, 'd', data.date.day, false);
                break;
            case 'x':
                width = buf_fill_fmt(dst, src, 'x', data.seq, true);
                break;
            default:
                // passthru
                buf[i] = fmt.data[i];
                width = 1;
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
        fprintf(stderr, "couldn't lock that global object smh\n");
        GlobalFree(handle);
        CloseClipboard();
        return false;
    }

    memcpy(data, str.data, str.len + 1);
    GlobalUnlock(handle);
    SetClipboardData(CF_TEXT, handle);
    GlobalFree(handle);
    CloseClipboard();
    return true;
}

#define DATA_FMT "%hu-%hhu-%hhu:%zu"

static inline size_t get_and_increment_sequence(String contents, Date now)
{
    Data data = {0};

    // read in data if there is any
    if (!string_is_empty(contents)) {
        sscanf(contents.data, DATA_FMT, &data.date.year, &data.date.month, &data.date.day, &data.seq);
    }

    // reset sequence if stored date isn't today
    if (memcmp(&data.date, &now, sizeof(now)) != 0) {
        data.seq = 0;
    }

    return data.seq + 1;
}

#define FILE_IN_BUF_SIZE 0xFF
static char FILE_IN_BUF[FILE_IN_BUF_SIZE];

static String string_from_file_contents(String filename)
{
    char *buf = FILE_IN_BUF;
    size_t len = 0;

    FILE *fp = fopen(filename.data, "rb");
    if (fp != NULL) {
        len = fread(buf, 1, FILE_IN_BUF_SIZE, fp);
        fclose(fp);
    }

    return string_create(buf, len);
}

#define FILE_OUT_BUF_SIZE 0xFF
static char FILE_OUT_BUF[FILE_OUT_BUF_SIZE];

static String string_from_data(Data data)
{
    char *buf = FILE_OUT_BUF;
    size_t len = sprintf(buf, DATA_FMT, data.date.year, data.date.month, data.date.day, data.seq);
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
 *       through CreateFile with the FILE_ATTRIBUTE_HIDDEN flag set
 */
int main(int argc, char *argv[])
{
    // validate the input
    if (argc != 2) {
        fprintf(stderr, "usage: seq [fmt] (example: seq yyyyMMddxxx)\n");
        return 1;
    }

    // get the current date (year, month and day)
    Date now = date_now();

    // do our file and sequence bookkeeping
    String contents = string_from_file_contents(string_lit(DATA_FILE_NAME));
    size_t seq = get_and_increment_sequence(contents, now);
    Data data = (Data){.date = now, .seq = seq};
    file_write_string(string_lit(DATA_FILE_NAME), string_from_data(data));

    // parse our format
    String fmt = string_from_cstring(argv[1]);
    String result = string_from_fmt(fmt, data);
    printf("sequence: %.*s\n", (int)result.len, result.data);

    // copy the result to the clipboard
    return (int)clipboard_set_string(result);
}
