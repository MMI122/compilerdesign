/*
 * NatureLang Runtime Library
 * Copyright (c) 2026
 * 
 * Runtime support functions for compiled NatureLang programs.
 */

#define _POSIX_C_SOURCE 200809L
#include "naturelang_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

/* ============================================================================
 * List Support
 * ============================================================================ */

NLList *nl_list_new(void) {
    /* NLList struct-এর জন্য heap memory allocate করি। */
    NLList *list = malloc(sizeof(NLList));
    /* allocation fail হলে NULL ফেরত দিয়ে caller-কে failure signal দিই। */
    if (!list) return NULL;
    /* প্রাথমিক list capacity ছোট fixed value (8) দিয়ে শুরু। */
    list->capacity = 8;
    /* নতুন list-এ শুরুতে কোনো item নেই। */
    list->length = 0;
    /* item_type default 0 (number/unspecified baseline) হিসেবে initialize। */
    list->item_type = 0;
    /* item pointers রাখার backing array allocate (capacity সংখ্যক slot)। */
    list->items = malloc(sizeof(void*) * list->capacity);
    /* fully initialized list pointer caller-কে return। */
    return list;
}

NLList *nl_list_create(int count, ...) {
    /* প্রথমে একটি empty list object তৈরি করি। */
    NLList *list = nl_list_new();
    /* allocation/create fail হলে NULL ফেরত। */
    if (!list) return NULL;
    
    /* variadic argument iteration-এর জন্য va_list setup। */
    va_list args;
    va_start(args, count);
    
    /* count সংখ্যক variadic value পড়ে list-এ append করি। */
    for (int i = 0; i < count; i++) {
        /* প্রতিটি argument long long হিসেবে read। */
        long long val = va_arg(args, long long);
        /* read করা সংখ্যাটি numeric list item হিসেবে append। */
        nl_list_append_num(list, val);
    }
    
    /* variadic reading context close। */
    va_end(args);
    /* populated list caller-কে ফেরত। */
    return list;
}

void nl_list_free(NLList *list) {
    /* NULL guard: list না থাকলে free করার কিছু নেই। */
    if (list) {
        /* Free string items if this is a string list */
        /*0 → number list (nl_list_append_num এ সেট করা)
    1 → decimal list (nl_list_append_dec এ সেট করা)
    2 → string list (nl_list_append_str এ সেট করা)*/
        /* item_type==2 হলে items গুলো strdup-allocated string pointer। */
        if (list->item_type == 2) {
            /* প্রতিটি string element আলাদাভাবে মুক্ত করি। */
            for (int i = 0; i < list->length; i++) {
                free(list->items[i]);
            }
        }
        /* item pointer array buffer মুক্ত করি। */
        free(list->items);
        /* list struct নিজেকেও মুক্ত করি। */
        free(list);
    }
}

int nl_list_length(NLList *list) {
    /* valid list হলে length ফেরত, নাহলে safe fallback 0। */
    return list ? list->length : 0;
}

static void nl_list_ensure_capacity(NLList *list) {
    /* বর্তমান length capacity-তে পৌঁছালে grow প্রয়োজন। */
    if (list->length >= list->capacity) {
        /* growth policy: capacity দ্বিগুণ করে amortized append খরচ কমাই। */
        list->capacity *= 2;
        /* expanded capacity অনুযায়ী items array realloc করি। */
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
}

void nl_list_append(NLList *list, void *item) {
    /* NULL list হলে append করার কিছু নেই, early return। */
    if (!list) return;
    /* নতুন element বসানোর আগে capacity যথেষ্ট কিনা নিশ্চিত করি। */
    nl_list_ensure_capacity(list);
    /* item pointer next slot-এ রেখে length post-increment করি। */
    list->items[list->length++] = item;
}

void nl_list_append_num(NLList *list, long long value) {
    /* defensive guard: list না থাকলে কাজ বন্ধ। */
    if (!list) return;
    /* numeric item যোগের আগে array grow প্রয়োজন কিনা দেখি। */
    nl_list_ensure_capacity(list);
    /* Store number directly as pointer (works for 64-bit) */
    /* integer value intptr_t হয়ে void* slot-এ store করি (tagless storage)। */
    list->items[list->length++] = (void*)(intptr_t)value;
    /* item_type 0 মানে number-list mode চিহ্নিত করি। */
    list->item_type = 0;
}

void nl_list_append_dec(NLList *list, double value) {
    /* invalid list pointer হলে append করা যাবে না। */
    if (!list) return;
    /* append-এর আগে প্রয়োজন হলে items array বড় করি। */
    nl_list_ensure_capacity(list);
    /* double সরাসরি pointer-cast safe নয়, তাই heap-এ value রাখি। */
    double *ptr = malloc(sizeof(double));
    /* allocated slot-এ decimal value লিখি। */
    *ptr = value;
    /* decimal pointer list slot-এ append। */
    list->items[list->length++] = ptr;
    /* item_type 1 দিয়ে decimal-list হিসেবে mark করি। */
    list->item_type = 1;
}

void nl_list_append_str(NLList *list, const char *value) {
    /* list না থাকলে append operation skip। */
    if (!list) return;
    /* নতুন string item-এর জন্য capacity ensure। */
    nl_list_ensure_capacity(list);
    /* input string-এর owned copy (strdup) list-এ সংরক্ষণ করি। */
    list->items[list->length++] = strdup(value);
    /* item_type 2 = string-list mode। */
    list->item_type = 2;
}

void *nl_list_get(NLList *list, int index) {
    /* invalid list বা out-of-range index হলে safe NULL ফেরত। */
    if (!list || index < 0 || index >= list->length) {
        return NULL;
    }
    /* valid index হলে raw item pointer ফেরত। */
    return list->items[index];
}

long long nl_list_get_num(NLList *list, int index) {
    /* invalid access হলে numeric fallback 0। */
    if (!list || index < 0 || index >= list->length) {
        return 0;
    }
    /* stored pointer-slot থেকে intptr_t হয়ে long long-এ back-cast। */
    /*list->items[index] হলো void*
    (intptr_t) দিয়ে সেই pointer value-টাকে integer-এ রূপান্তর করছে
    তারপর (long long) করে function-এর return type-এর সাথে মিলিয়ে দিচ্ছে*/
    return (long long)(intptr_t)list->items[index];
}

double nl_list_get_dec(NLList *list, int index) {
    /* list invalid বা index range-এর বাইরে হলে safe fallback 0.0। */
    if (!list || index < 0 || index >= list->length) {
        return 0.0;
    }
    /* stored item-কে double pointer ধরে dereference করে decimal value ফেরত। */
    return *(double*)list->items[index];
}

char *nl_list_get_str(NLList *list, int index) {
    /* invalid access হলে empty string fallback দিই (NULL নয়)। */
    if (!list || index < 0 || index >= list->length) {
        return "";
    }
    /* valid slot-এর stored pointer-কে string pointer হিসেবে return। */
    return (char*)list->items[index];
}

void nl_list_set(NLList *list, int index, void *item) {
    /* invalid list/index হলে set operation skip। */
    if (!list || index < 0 || index >= list->length) return;
    /* target index-এ নতুন raw item pointer বসাই। */
    list->items[index] = item;
}

void nl_list_set_num(NLList *list, int index, long long value) {
    /* list বা index invalid হলে update না করে return। */
    if (!list || index < 0 || index >= list->length) return;
    /* numeric value intptr_t হয়ে pointer-slot-এ encode করে রাখি। */
    list->items[index] = (void*)(intptr_t)value;
}

void nl_list_remove(NLList *list, int index) {
    /* remove request invalid হলে কিছু না করে বের হয়ে যাই। */
    if (!list || index < 0 || index >= list->length) return;
    
    /* Free string item if applicable */
    /* string-list mode হলে remove হওয়া slot-এর string memory আগে free। */
    if (list->item_type == 2) {
        free(list->items[index]);
    }
    
    /* Shift items */
    /* removed slot পূরণে ডানদিকের item গুলো এক ধাপ বামে shift। */
    for (int i = index; i < list->length - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    /* logical list length এক কমাই। */
    list->length--;
}

int nl_list_contains_num(NLList *list, long long value) {
    /* NULL list হলে contain check false। */
    if (!list) return 0;
    /* linear scan করে target number পাওয়া যায় কিনা দেখি। */
    for (int i = 0; i < list->length; i++) {
        if (nl_list_get_num(list, i) == value) {
            /* match মিললেই true return। */
            return 1;
        }
    }
    /* scan শেষে না পেলে false। */
    return 0;
}

int nl_list_contains_str(NLList *list, const char *value) {
    /* list বা query string invalid হলে false। */
    if (!list || !value) return 0;
    /* সব item string compare করে containment check। */
    for (int i = 0; i < list->length; i++) {
        char *item = nl_list_get_str(list, i);
        if (item && strcmp(item, value) == 0) {
            /* exact string match পেলে true। */
            return 1;
        }
    }
    /* কোনো match না থাকলে false। */
    return 0;
}

/* ============================================================================
 * String Support
 * ============================================================================ */

char *nl_concat(const char *a, const char *b) {
    /* NULL input এ safe behavior: missing string-কে empty string ধরি। */
    if (!a) a = "";
    if (!b) b = "";
    
    /* দুই input string-এর length আলাদা করে মাপি। */
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    /* concat result + trailing NUL এর জন্য heap buffer allocate। */
    char *result = malloc(len_a + len_b + 1);
    
    /* allocation successful হলে দুই অংশ copy করে final string তৈরি। */
    if (result) {
        /* প্রথম অংশ a buffer শুরুতে বসাই। */
        memcpy(result, a, len_a);
        /* দ্বিতীয় অংশ b, a-এর পরেই কপি করি (NUL সহ)। */
        memcpy(result + len_a, b, len_b + 1);
    }
    
    /* caller-owned result pointer return (বা allocation fail হলে NULL)। */
    return result;
}

char *nl_num_to_string(long long value) {
    /* long long number stringে রূপান্তরের জন্য fixed-size buffer allocate। */
    char *result = malloc(32);
    /* allocation success হলে decimal text format-এ লিখি। */
    if (result) {
        snprintf(result, 32, "%lld", value);
    }
    /* caller এই string পরে free করবে। */
    return result;
}

char *nl_dec_to_string(double value) {
    /* floating value string render-এর জন্য তুলনামূলক বড় buffer allocate। */
    char *result = malloc(64);
    /* allocation success হলে %g format-এ compact decimal string লিখি। */
    if (result) {
        snprintf(result, 64, "%g", value);
    }
    /* allocated string pointer return। */
    return result;
}

char *nl_bool_to_string(int value) {
    /* boolean truth value-কে human-readable yes/no stringে map করি। */
    return strdup(value ? "yes" : "no");
}

char *nl_to_string(long long value) {
    /* generic numeric-to-string helper, বর্তমানে number path-এ delegate করে। */
    return nl_num_to_string(value);
}

int nl_string_equals(const char *a, const char *b) {
    /* কোনো একটি NULL হলে pointer identity rule apply (দুটোই NULL হলে equal)। */
    /*If either string pointer is NULL, the function does not do content comparison.
It checks whether the two pointers are exactly the same address/value using a == b.*/
/*Both are NULL: returns true (1), because they are identical pointer values.
One NULL and one non-NULL: returns false (0), because pointer values differ.
Both non-NULL: then it goes to strcmp(a, b) == 0 for actual text comparison.*/
    if (!a || !b) return a == b;
    /* non-NULL হলে lexical equality check strcmp দিয়ে। */
    return strcmp(a, b) == 0;
}

int nl_string_length(const char *s) {
    /* NULL string-এর length safe fallback 0, নাহলে strlen cast করে int। */
    return s ? (int)strlen(s) : 0;
}

char *nl_substring(const char *s, int start, int end) {
    /* source string NULL হলে empty string return। */
    if (!s) return strdup("");
    
    /* source length বের করে bounds normalize করার প্রস্তুতি। */
    int len = (int)strlen(s);
    /* negative start index হলে 0-তে clamp করি। */
    if (start < 0) start = 0;
    /* end index source length ছাড়ালে শেষ পর্যন্ত clamp। */
    /*s = "hello" → len = 5
call: nl_substring(s, 1, 100)
clamp-এর পরে end = 5
result হবে "ello" (1 থেকে শেষ পর্যন্ত)*/
    if (end > len) end = len;
    /* invalid/empty range হলে empty string return। */
    if (start >= end) return strdup("");
    
    /* substring effective length গণনা। */
    int sub_len = end - start;
    /* substring + NUL এর জন্য নতুন buffer allocate। */
    char *result = malloc(sub_len + 1);
    /* allocation success হলে range copy করে terminator বসাই। */
    if (result) {
        memcpy(result, s + start, sub_len);
        result[sub_len] = '\0';
    }
    /* caller-owned substring pointer return। */
    return result;
}
/*
if (!haystack || !needle) return 0;

যদি কোনো input NULL হয়, function সরাসরি 0 (false) ফেরত দেয়।
মানে invalid input হলে “contains” ধরা হবে না।
return strstr(haystack, needle) != NULL;

strstr haystack-এর ভিতরে needle-এর প্রথম occurrence খোঁজে।
পেলে pointer ফেরত দেয় (non-NULL), না পেলে NULL দেয়।
তাই != NULL expression true/false এ convert হয়ে return হয়।
Quick example:

nl_string_contains("banana", "nan") → 1
nl_string_contains("banana", "xyz") → 0
nl_string_contains(NULL, "a") → 0
*/

int nl_string_contains(const char *haystack, const char *needle) {
    /* NULL input থাকলে containment false। */
    if (!haystack || !needle) return 0;
    /* strstr non-NULL হলে needle haystack-এর মধ্যে আছে। */
    return strstr(haystack, needle) != NULL;
}

char *nl_string_upper(const char *s) {
    /* source NULL হলে empty string fallback। */
    if (!s) return strdup("");
    
    /* mutable copy বানাই যাতে inplace uppercase করা যায়। */
    char *result = strdup(s);
    /* copy success হলে character-by-character uppercase transform। */
    if (result) {
        for (char *p = result; *p; p++) {
            /* locale-safe cast করে toupper প্রয়োগ। */
            *p = toupper((unsigned char)*p);
        }
    }
    /* transformed (বা NULL) pointer return। */
    return result;
}

char *nl_string_lower(const char *s) {
    /* source NULL হলে empty string return। */
    if (!s) return strdup("");
    
    /* writable duplicate তৈরি করি। */
    char *result = strdup(s);
    /* duplicate success হলে lowercase conversion loop চালাই। */
    if (result) {
        for (char *p = result; *p; p++) {
            /* প্রতিটি byte-কে tolower দিয়ে normalize। */
            *p = tolower((unsigned char)*p);
        }
    }
    /* caller-owned lowercase string return। */
    return result;
}

char *nl_string_trim(const char *s) {
    /* NULL source হলে empty result। */
    if (!s) return strdup("");
    
    /* Skip leading whitespace */
    /* শুরু থেকে whitespace skip করে first non-space-এ যাই। */
    while (*s && isspace((unsigned char)*s)) s++;
    
    /* পুরো string whitespace হলে empty string return। */
    if (*s == '\0') return strdup("");
    
    /* Find end of string */
    /* শেষ character থেকে reverse scan করে trailing whitespace trim point খুঁজি। */
    /*naturelang_runtime.c:421 এ comment বলছে এখন string-এর শেষ দিক থেকে whitespace খুঁজব।

naturelang_runtime.c:423 এ
const char *end = s + strlen(s) - 1;
মানে end pointer প্রথমে string-এর শেষ valid character-এ দাঁড়ায়।

naturelang_runtime.c:424 এ while loop:
while (end > s && isspace((unsigned char)*end)) end--;
যতক্ষণ:

end শুরু pointer s-এর আগে না যায় (end > s), এবং
current character whitespace (isspace(...))
ততক্ষণ end এক ধাপ করে বামে সরে।

ফলাফল:

loop শেষ হলে end এমন character-এ থাকবে যা trailing whitespace নয়।
এরপর নিচের লাইনে length বের করে কপি করার সময় শেষের space/tab/newline বাদ পড়ে যায়।
Quick example:

input: " hello "
leading skip-এর পর s → "hello "
end শুরুতে শেষ ' ' এ
loop শেষে end গিয়ে 'o' তে দাঁড়ায়
*/
    const char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    
    /* trimmed segment length নির্ণয়। */
    int len = end - s + 1;
    /* trimmed text + NUL buffer allocate। */
    char *result = malloc(len + 1);
    /* allocation success হলে trimmed অংশ copy ও terminate। */
    if (result) {
        memcpy(result, s, len);
        result[len] = '\0';
    }
    /* trimmed string caller-কে return। */
    return result;
}

/* ============================================================================
 * Math Support
 * ============================================================================ */

long long nl_pow_int(long long base, long long exp) {
    if (exp < 0) return 0;
    if (exp == 0) return 1;
    
    long long result = 1;
    while (exp > 0) {
        if (exp & 1) {
            result *= base;
        }
        base *= base;
        exp >>= 1;
    }
    return result;
}
/*nl_abs
Signature: long long nl_abs(long long value)
Integer (long long) এর absolute value দেয়
নিজেই ternary দিয়ে করে: value < 0 ? -value : value
nl_fabs
Signature: double nl_fabs(double value)
Floating-point (double) এর absolute value দেয়
libc fabs() call করে return করে
Quick example:

nl_abs(-7) → 7 (integer)
nl_fabs(-7.25) → 7.25 (double)
কেন দুইটা লাগে:

Integer আর floating-point arithmetic আলাদা
return type/precision ঠিক রাখতে আলাদা helper রাখা হয়েছে*/

long long nl_abs(long long value) {
    return value < 0 ? -value : value;
}

double nl_fabs(double value) {
    return fabs(value);
}

long long nl_min(long long a, long long b) {
    return a < b ? a : b;
}

long long nl_max(long long a, long long b) {
    return a > b ? a : b;
}

double nl_fmin(double a, double b) {
    return a < b ? a : b;
}

double nl_fmax(double a, double b) {
    return a > b ? a : b;
}

static int random_initialized = 0;

long long nl_random(long long min, long long max) {
    /* RNG seed এখনো initialize না হলে একবার seed set করি। */
    if (!random_initialized) {
        /* current wall-clock time দিয়ে srand করে pseudo-random sequence শুরু। */
        /*time(NULL) বর্তমান সময় (সেকেন্ডে, Unix epoch থেকে) রিটার্ন করে।
(unsigned int) কাস্ট দিয়ে সেটাকে srand-এর উপযোগী টাইপে নেয়।
srand(...) pseudo-random generator-এর seed সেট করে।
এরপর rand() যে সংখ্যা দেবে, সেটা এই seed-এর ওপর নির্ভর করে sequence তৈরি করবে।
কেন দরকার:

seed না দিলে (বা সবসময় একই seed দিলে) rand() প্রতিবার program run-এ একই sequence দিতে পারে।
current time দিয়ে seed দিলে run ভেদে sequence বদলায়।*/
/*ধরা যাক এখন সময় 1711622400 (epoch seconds)
তাহলে কার্যত এটা হবে:
srand(1711622400u);
এরপর rand() থেকে একটি sequence আসবে, যেমন (ধারণাগত উদাহরণ):

rand() → 83452341
rand() → 1923345
rand() → 99812377
আর nl_random(10, 15) করলে ফর্মুলা:
range = 15 - 10 + 1 = 6
result = 10 + (rand() % 6)
উদাহরণ:

যদি rand()%6 = 4 হয়, result = 14
যদি rand()%6 = 0 হয়, result = 10
যদি rand()%6 = 5 হয়, result = 15
মানে output সবসময় 10..15 এর মধ্যে থাকবে (inclusive)।

কেন time(NULL):

প্রতিবার program run-এ seed বদলায়
তাই random sequence-ও বদলায়
কেন একবারই srand:

বারবার srand দিলে (বিশেষ করে দ্রুত loop-এ) sequence খারাপ হতে পারে
তাই random_initialized flag দিয়ে একবার seed করাই সঠিক approach।*/
        srand((unsigned int)time(NULL));
        /* seed initialization সম্পন্ন হয়েছে বলে flag raise। */
        random_initialized = 1;
    }
    
    /* caller min/max উল্টো দিলে swap করে valid range বানাই। */
    if (min > max) {
        /* temporary variable দিয়ে মান অদলবদল। */
        long long tmp = min;
        min = max;
        max = tmp;
    }
    
    /* inclusive range length: [min, max] তাই +1। */
    long long range = max - min + 1;
    /* rand()%range offset-কে min যোগ করে final bounded random ফেরত। */
    return min + (rand() % range);
}

/* ============================================================================
 * I/O Support
 * ============================================================================ */

void nl_display(const char *message) {
    if (message) {
        printf("%s\n", message);
    }
}

void nl_display_num(long long value) {
    printf("%lld\n", value);
}

void nl_display_dec(double value) {
    printf("%g\n", value);
}

void nl_display_bool(int value) {
    printf("%s\n", value ? "yes" : "no");
}

static char nl_input_buffer[4096];

char *nl_input(const char *prompt) {
    /* prompt text দেওয়া থাকলে আগে সেটি print করি। */
    if (prompt) {
        /* prompt newline ছাড়া দেখাই যাতে user একই লাইনে input দিতে পারে। */
        printf("%s", prompt);
        /* stdout buffer flush করে prompt সাথে সাথে দৃশ্যমান করি। */
        fflush(stdout);
    }
    
    /* stdin থেকে একটি লাইন পড়ার চেষ্টা করি shared buffer-এ। */
    if (fgets(nl_input_buffer, sizeof(nl_input_buffer), stdin)) {
        /* Remove trailing newline */
        /* input-এর শেষে থাকা '\n' থাকলে সেটিকে NUL দিয়ে trim করি। */
        nl_input_buffer[strcspn(nl_input_buffer, "\n")] = '\0';
        /* caller-owned copy ফেরত দিতে buffer content strdup করি। */
        return strdup(nl_input_buffer);
    }
    
    /* EOF/error হলে empty string return করে safe fallback দিই। */
    return strdup("");
}

long long nl_input_num(const char *prompt) {
    /* raw text input সংগ্রহ করি prompt সহ। */
    char *input = nl_input(prompt);
    /* string-কে long long number-এ parse করি। */
    long long result = atoll(input);
    /* temporary input buffer copy মুক্ত করি। */
    free(input);
    /* parsed integer result caller-কে ফেরত। */
    return result;
}

double nl_input_dec(const char *prompt) {
    /* prompt দেখিয়ে text input নেই। */
    char *input = nl_input(prompt);
    /* string input-কে double decimal-এ convert করি। */
    double result = atof(input);
    /* temporary copied string memory free। */
    free(input);
    /* parsed decimal value return। */
    return result;
}

/* ============================================================================
 * Type Conversion
 * ============================================================================ */

long long nl_to_number(const char *s) {
    return s ? atoll(s) : 0;
}

double nl_to_decimal(const char *s) {
    return s ? atof(s) : 0.0;
}

int nl_to_flag(long long value) {
    return value != 0;
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

char *nl_strdup(const char *s) {
    /* input string valid হলে strdup copy, নাহলে NULL ফেরত। */
    return s ? strdup(s) : NULL;
}

void *nl_alloc(size_t size) {
    /* calloc দিয়ে requested size-এর zero-initialized memory allocate। */
    void *ptr = calloc(1, size);
    /* allocation fail হলে runtime error throw/exit path trigger। */
    if (!ptr) {
        nl_error("Memory allocation failed");
    }
    /* সফল allocation pointer caller-কে ফেরত। */
    return ptr;
}

void nl_free(void *ptr) {
    /* generic free wrapper: caller দেওয়া pointer মুক্ত করি। */
    free(ptr);
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

void nl_error(const char *message) {
    /* runtime error message stderr-এ নির্দিষ্ট format-এ print করি। */
    /* message NULL হলে fallback হিসেবে "Unknown error" ব্যবহার করি। */
    fprintf(stderr, "Runtime Error: %s\n", message ? message : "Unknown error");
    /* fatal runtime error হওয়ায় non-zero status (1) দিয়ে program terminate করি। */
    exit(1);
}

void nl_assert(int condition, const char *message) {
    /* assertion condition false হলে এটাকে runtime error হিসেবে handle করি। */
    if (!condition) {
        /* custom message থাকলে সেটি, নাহলে default "Assertion failed" পাঠাই। */
        nl_error(message ? message : "Assertion failed");
    }
}

/* ============================================================================
 * Initialization
 * ============================================================================ */
/*Program startup-এ random generator seed set করে (srand(time(NULL)))।
random_initialized = 1 করে দেয়, যাতে nl_random পরে গিয়ে আবার seed না করে।
Initialization concern এক জায়গায় রাখে, ফলে runtime startup behavior predictable হয়।
Future expansion point দেয়: পরে global cache, locale, IO state ইত্যাদি init দরকার হলে এখানেই যোগ করা যাবে।
Important detail:

এই codebase-এ nl_random নিজেও lazy seed করে (flag check করে), তাই strictly mandatory না।
কিন্তু nl_runtime_init থাকলে startup contract পরিষ্কার হয়: “runtime ready before use”।*/
void nl_runtime_init(void) {
    /* runtime startup-এ pseudo-random generator seed initialize করি। */
    /* time(NULL) ভিত্তিক seed দিলে প্রতিটি run-এ sequence সাধারণত আলাদা হয়। */
    srand((unsigned int)time(NULL));
    /* nl_random যেন আবার seed না করে, তাই initialized flag on করি। */
    random_initialized = 1;
}

void nl_runtime_cleanup(void) {
    /* বর্তমানে runtime-level global resource cleanup প্রয়োজন নেই। */
    /* ভবিষ্যতে shared resource যোগ হলে cleanup logic এখানে বসবে। */
}
