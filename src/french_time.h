#pragma once

#include "pebble.h"
#include "mini_printf.h"
#include "time.h"

#define LINE_BUFFER_SIZE 50

void fuzzy_time(struct tm* t, char* str_line1, char* str_line2, char* str_line3);

void info_lines(struct tm* t, char* str_line1, char* str_line2);