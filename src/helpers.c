#include "../include/helpers.h"
#include <stdint.h>

// Custom Integer to ASCII converter
// I wrote this because standard library functions like sprintf() 
// are way too heavy and eat up too much Flash/RAM on an 8-bit ATmega328P

void _itoa(uint32_t num, char *buffer) {
    // Edge case: if the number is exactly 0, just return "0"
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
    } else {
        uint8_t buff_len = 0;
        
        // extract digits one by one from right to left
        while (num > 0) {
            // (num % 10) gets the rightmost digit
            // adding '0' (ASCII 48) converts the integer into a printable character
            buffer[buff_len] = (num % 10) + '0';
            num = num / 10;     // chop off the last digit
            buff_len++;
        }
        
        // null-terminate
        buffer[buff_len] = '\0';
        
        // since we extracted the digits backwards
        // we need to reverse the array in place to make it correct
        for (uint8_t i = 0; i < (buff_len / 2); i++) {
            char temp;
            temp = buffer[i];
            buffer[i] = buffer[buff_len-1-i];
            buffer[buff_len-1-i] = temp;
        }
    }
}

// Custom string to integer converter (ASCII to Integer)
// Saves Flash memory because we don't need to include <stdlib.h>
int16_t _atoi(const char *str) {
    int16_t res = 0;
    int8_t sign = 1;
    uint8_t i = 0;

    // skip spaces if there are any
    while (str[i] == ' ') {
        i++;
    }

    // check for negative numbers
    if (str[i] == '-' || str[i] == '+') {
        if (str[i] == '-') {
            sign = -1;
        }
        i++;
    }

    // convert characters to numbers one by one
    while (str[i] >= '0' && str[i] <= '9') {
        // multiply current result by 10 to shift digits left, 
        // then add the new digit (str[i] - '0' converts ASCII char to actual number)
        res = res * 10 + (str[i] - '0');
        i++;
    }

    return sign * res;
}

// Custom string tokenizer
// Uses a static pointer to remember the position between calls
char* _strtok(char* str, char delim) {
    static char* next_token = 0; // keeps track of where we are in the string

    // if a new string is passed = reset our pointer to the start
    if (str != 0) {
        next_token = str;
    }

    // if we have nothing left to parse = return NULL
    if (next_token == 0) {
        return 0;
    }

    // skip any leading delimiters
    while (*next_token == delim) {
        next_token++;
    }

    // if we reached the end of the string = stop
    if (*next_token == '\0') {
        return 0;
    }

    // save the start of our actual word
    char* current_token = next_token;

    // walk forward until we hit the next delimiter or the end of the string
    while (*next_token != '\0' && *next_token != delim) {
        next_token++;
    }

    // if we hit the delimiter = replace it with '\0' to terminate the current word,
    // and move the pointer one step forward for the NEXT time the function is called
    if (*next_token != '\0') {
        *next_token = '\0';
        next_token++;
    }

    return current_token;
}
