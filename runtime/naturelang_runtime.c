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
    NLList *list = malloc(sizeof(NLList));
    if (!list) return NULL;
    list->capacity = 8;
    list->length = 0;
    list->item_type = 0;
    list->items = malloc(sizeof(void*) * list->capacity);
    return list;
}

NLList *nl_list_create(int count, ...) {
    NLList *list = nl_list_new();
    if (!list) return NULL;
    
    va_list args;
    va_start(args, count);
    
    for (int i = 0; i < count; i++) {
        long long val = va_arg(args, long long);
        nl_list_append_num(list, val);
    }
    
    va_end(args);
    return list;
}

void nl_list_free(NLList *list) {
    if (list) {
        /* Free string items if this is a string list */
        if (list->item_type == 2) {
            for (int i = 0; i < list->length; i++) {
                free(list->items[i]);
            }
        }
        free(list->items);
        free(list);
    }
}

int nl_list_length(NLList *list) {
    return list ? list->length : 0;
}

static void nl_list_ensure_capacity(NLList *list) {
    if (list->length >= list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
}

void nl_list_append(NLList *list, void *item) {
    if (!list) return;
    nl_list_ensure_capacity(list);
    list->items[list->length++] = item;
}

void nl_list_append_num(NLList *list, long long value) {
    if (!list) return;
    nl_list_ensure_capacity(list);
    /* Store number directly as pointer (works for 64-bit) */
    list->items[list->length++] = (void*)(intptr_t)value;
    list->item_type = 0;
}

void nl_list_append_dec(NLList *list, double value) {
    if (!list) return;
    nl_list_ensure_capacity(list);
    double *ptr = malloc(sizeof(double));
    *ptr = value;
    list->items[list->length++] = ptr;
    list->item_type = 1;
}

void nl_list_append_str(NLList *list, const char *value) {
    if (!list) return;
    nl_list_ensure_capacity(list);
    list->items[list->length++] = strdup(value);
    list->item_type = 2;
}

void *nl_list_get(NLList *list, int index) {
    if (!list || index < 0 || index >= list->length) {
        return NULL;
    }
    return list->items[index];
}

long long nl_list_get_num(NLList *list, int index) {
    if (!list || index < 0 || index >= list->length) {
        return 0;
    }
    return (long long)(intptr_t)list->items[index];
}

double nl_list_get_dec(NLList *list, int index) {
    if (!list || index < 0 || index >= list->length) {
        return 0.0;
    }
    return *(double*)list->items[index];
}

char *nl_list_get_str(NLList *list, int index) {
    if (!list || index < 0 || index >= list->length) {
        return "";
    }
    return (char*)list->items[index];
}

void nl_list_set(NLList *list, int index, void *item) {
    if (!list || index < 0 || index >= list->length) return;
    list->items[index] = item;
}

void nl_list_set_num(NLList *list, int index, long long value) {
    if (!list || index < 0 || index >= list->length) return;
    list->items[index] = (void*)(intptr_t)value;
}

void nl_list_remove(NLList *list, int index) {
    if (!list || index < 0 || index >= list->length) return;
    
    /* Free string item if applicable */
    if (list->item_type == 2) {
        free(list->items[index]);
    }
    
    /* Shift items */
    for (int i = index; i < list->length - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    list->length--;
}

int nl_list_contains_num(NLList *list, long long value) {
    if (!list) return 0;
    for (int i = 0; i < list->length; i++) {
        if (nl_list_get_num(list, i) == value) {
            return 1;
        }
    }
    return 0;
}

int nl_list_contains_str(NLList *list, const char *value) {
    if (!list || !value) return 0;
    for (int i = 0; i < list->length; i++) {
        char *item = nl_list_get_str(list, i);
        if (item && strcmp(item, value) == 0) {
            return 1;
        }
    }
    return 0;
}

/* ============================================================================
 * String Support
 * ============================================================================ */

char *nl_concat(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char *result = malloc(len_a + len_b + 1);
    
    if (result) {
        memcpy(result, a, len_a);
        memcpy(result + len_a, b, len_b + 1);
    }
    
    return result;
}

char *nl_num_to_string(long long value) {
    char *result = malloc(32);
    if (result) {
        snprintf(result, 32, "%lld", value);
    }
    return result;
}

char *nl_dec_to_string(double value) {
    char *result = malloc(64);
    if (result) {
        snprintf(result, 64, "%g", value);
    }
    return result;
}

char *nl_bool_to_string(int value) {
    return strdup(value ? "yes" : "no");
}

char *nl_to_string(long long value) {
    return nl_num_to_string(value);
}

int nl_string_equals(const char *a, const char *b) {
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

int nl_string_length(const char *s) {
    return s ? (int)strlen(s) : 0;
}

char *nl_substring(const char *s, int start, int end) {
    if (!s) return strdup("");
    
    int len = (int)strlen(s);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return strdup("");
    
    int sub_len = end - start;
    char *result = malloc(sub_len + 1);
    if (result) {
        memcpy(result, s + start, sub_len);
        result[sub_len] = '\0';
    }
    return result;
}

int nl_string_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    return strstr(haystack, needle) != NULL;
}

char *nl_string_upper(const char *s) {
    if (!s) return strdup("");
    
    char *result = strdup(s);
    if (result) {
        for (char *p = result; *p; p++) {
            *p = toupper((unsigned char)*p);
        }
    }
    return result;
}

char *nl_string_lower(const char *s) {
    if (!s) return strdup("");
    
    char *result = strdup(s);
    if (result) {
        for (char *p = result; *p; p++) {
            *p = tolower((unsigned char)*p);
        }
    }
    return result;
}

char *nl_string_trim(const char *s) {
    if (!s) return strdup("");
    
    /* Skip leading whitespace */
    while (*s && isspace((unsigned char)*s)) s++;
    
    if (*s == '\0') return strdup("");
    
    /* Find end of string */
    const char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    
    int len = end - s + 1;
    char *result = malloc(len + 1);
    if (result) {
        memcpy(result, s, len);
        result[len] = '\0';
    }
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
    if (!random_initialized) {
        srand((unsigned int)time(NULL));
        random_initialized = 1;
    }
    
    if (min > max) {
        long long tmp = min;
        min = max;
        max = tmp;
    }
    
    long long range = max - min + 1;
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
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    
    if (fgets(nl_input_buffer, sizeof(nl_input_buffer), stdin)) {
        /* Remove trailing newline */
        nl_input_buffer[strcspn(nl_input_buffer, "\n")] = '\0';
        return strdup(nl_input_buffer);
    }
    
    return strdup("");
}

long long nl_input_num(const char *prompt) {
    char *input = nl_input(prompt);
    long long result = atoll(input);
    free(input);
    return result;
}

double nl_input_dec(const char *prompt) {
    char *input = nl_input(prompt);
    double result = atof(input);
    free(input);
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
    return s ? strdup(s) : NULL;
}

void *nl_alloc(size_t size) {
    void *ptr = calloc(1, size);
    if (!ptr) {
        nl_error("Memory allocation failed");
    }
    return ptr;
}

void nl_free(void *ptr) {
    free(ptr);
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

void nl_error(const char *message) {
    fprintf(stderr, "Runtime Error: %s\n", message ? message : "Unknown error");
    exit(1);
}

void nl_assert(int condition, const char *message) {
    if (!condition) {
        nl_error(message ? message : "Assertion failed");
    }
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

void nl_runtime_init(void) {
    /* Initialize random number generator */
    srand((unsigned int)time(NULL));
    random_initialized = 1;
}

void nl_runtime_cleanup(void) {
    /* Nothing to clean up currently */
}
