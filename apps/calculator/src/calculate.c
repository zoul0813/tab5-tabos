#include <calculator/calculate.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Two precedence levels; no recursion or unbounded expression stack. */
bool calculator_evaluate(const char* expression, char* output, size_t capacity)
{
    const char* cursor = expression;
    double sum = 0.0, term = 0.0;
    char operation = '+';
    for (size_t count = 0U; count < 128U; ++count) {
        while (isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        char* end;
        const double value = strtod(cursor, &end);
        if (end == cursor || !isfinite(value)) {
            return false;
        }
        if (operation == '+') {
            sum  += term;
            term  = value;
        } else if (operation == '-') {
            sum  += term;
            term  = -value;
        } else if (operation == '*') {
            term *= value;
        } else if (operation == '/') {
            if (value == 0.0) {
                return false;
            }
            term /= value;
        }
        if (!isfinite(sum) || !isfinite(term)) {
            return false;
        }
        cursor = end;
        while (isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            const double result = sum + term;
            if (!isfinite(result)) {
                return false;
            }
            const int length = snprintf(output, capacity, "%.12g", result);
            return length >= 0 && (size_t) length < capacity;
        }
        operation = *cursor++;
        if (operation != '+' && operation != '-' && operation != '*' && operation != '/') {
            return false;
        }
    }
    return false;
}
