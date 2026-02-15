/*
 * NatureLang Runtime Library
 * Copyright (c) 2026
 * 
 * Runtime support functions for compiled NatureLang programs.
 */

#ifndef NATURELANG_RUNTIME_H
#define NATURELANG_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * List Support
 * ============================================================================ */

/* Dynamic list structure */
typedef struct NLList {
    void **items;
    int length;
    int capacity;
    int item_type;  /* 0=number, 1=decimal, 2=text, 3=flag */
} NLList;

/* Create a new list with initial elements (variadic) */
NLList *nl_list_create(int count, ...);

/* Create an empty list */
NLList *nl_list_new(void);

/* Free a list */
void nl_list_free(NLList *list);

/* Get list length */
int nl_list_length(NLList *list);

/* Append to list */
void nl_list_append(NLList *list, void *item);
void nl_list_append_num(NLList *list, long long value);
void nl_list_append_dec(NLList *list, double value);
void nl_list_append_str(NLList *list, const char *value);

/* Get from list */
void *nl_list_get(NLList *list, int index);
long long nl_list_get_num(NLList *list, int index);
double nl_list_get_dec(NLList *list, int index);
char *nl_list_get_str(NLList *list, int index);

/* Set in list */
void nl_list_set(NLList *list, int index, void *item);
void nl_list_set_num(NLList *list, int index, long long value);

/* Remove from list */
void nl_list_remove(NLList *list, int index);

/* Check if list contains item */
int nl_list_contains_num(NLList *list, long long value);
int nl_list_contains_str(NLList *list, const char *value);

/* ============================================================================
 * String Support
 * ============================================================================ */

/* Concatenate two strings (returns newly allocated string) */
char *nl_concat(const char *a, const char *b);

/* Convert number to string */
char *nl_num_to_string(long long value);

/* Convert decimal to string */
char *nl_dec_to_string(double value);

/* Convert boolean to string */
char *nl_bool_to_string(int value);

/* Generic to_string for expressions */
char *nl_to_string(long long value);

/* String comparison */
int nl_string_equals(const char *a, const char *b);

/* String length */
int nl_string_length(const char *s);

/* Substring */
char *nl_substring(const char *s, int start, int end);

/* String contains */
int nl_string_contains(const char *haystack, const char *needle);

/* String to upper/lower */
char *nl_string_upper(const char *s);
char *nl_string_lower(const char *s);

/* String trim */
char *nl_string_trim(const char *s);

/* ============================================================================
 * Math Support
 * ============================================================================ */

/* Power function for integers */
long long nl_pow_int(long long base, long long exp);

/* Absolute value */
long long nl_abs(long long value);
double nl_fabs(double value);

/* Min/Max */
long long nl_min(long long a, long long b);
long long nl_max(long long a, long long b);
double nl_fmin(double a, double b);
double nl_fmax(double a, double b);

/* Random number */
long long nl_random(long long min, long long max);

/* ============================================================================
 * I/O Support
 * ============================================================================ */

/* Display functions */
void nl_display(const char *message);
void nl_display_num(long long value);
void nl_display_dec(double value);
void nl_display_bool(int value);

/* Input functions */
char *nl_input(const char *prompt);
long long nl_input_num(const char *prompt);
double nl_input_dec(const char *prompt);

/* ============================================================================
 * Type Conversion
 * ============================================================================ */

/* String to number */
long long nl_to_number(const char *s);

/* String to decimal */
double nl_to_decimal(const char *s);

/* Number to boolean */
int nl_to_flag(long long value);

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/* Safe string duplication */
char *nl_strdup(const char *s);

/* Allocate and zero memory */
void *nl_alloc(size_t size);

/* Free memory */
void nl_free(void *ptr);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/* Runtime error */
void nl_error(const char *message);

/* Assertion */
void nl_assert(int condition, const char *message);

/* ============================================================================
 * Initialization
 * ============================================================================ */

/* Initialize runtime (called automatically) */
void nl_runtime_init(void);

/* Cleanup runtime */
void nl_runtime_cleanup(void);

#endif /* NATURELANG_RUNTIME_H */
