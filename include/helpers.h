#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>

void _itoa(uint32_t num, char *buffer);
int16_t _atoi(const char *str);
char* _strtok(char* str, char delim);

#endif
