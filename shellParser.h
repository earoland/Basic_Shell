/*
 * shellParser.h
 *
 * Written by Dr. William Kreahling
 *            Department of Mathematics and Computer Science
 *            Western Carolina University
 *
 * This file contains constants and function prototypes shared
 * by the shellParser and shell files.
 */
#ifndef SHELL_PARSER_H
#define SHELL_PARSER_H

#define MAX_ARGS           256
#define MAX_STRING_LENGTH 1024

/* Function prototypes */
char** getArgList(void);

#endif
