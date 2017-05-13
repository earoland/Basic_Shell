/*
 * shellParser.l
 *
 * Written by Dr. William Kreahling and Dr. Andrew Dalton
 *            Department of Mathematics and Computer Science
 *            Western Carolina University
 *
 * This file contains a flex specification for a scanner that reads
 * tokens for a simple UNIX-like shell.  This file is parsed by the
 * 'flex' application which generates the corresponding scanner.
 *
 * This is a *simple* example of what this tool can do.
 */
%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shellParser.h"

/* Prototype one of the functions that gets generated automatically */
char* yyget_text(void);


/* An array of pointers to strings */
static char* arguments[MAX_ARGS + 1] = { NULL };

/* Used as an index into the array above. */
static int   argumentCount           = 0;


/*
 * consumeToken
 *
 * Consume a token from the scanner.  The space for the token will
 * be dynamically allocated and store in 'arguments' at'
 * 'argumentCount'.  'argumentCount' will be incremented by one.
 *
 * This dynamically allocated buffer will be freed in
 * 'getArgList()'
 */
static void consumeToken(void) {
    if (argumentCount < MAX_ARGS) {
        /*
         * strdup returns a dynamically allocated buffer
         * containing a copy of the provided string.  We'll
         * need to free this memory later
         */
        arguments[argumentCount++] = (char*) strdup(yyget_text());
        arguments[argumentCount]   = NULL;
    }
}

/*
 * allocStringBuffer
 *
 * Allocates a buffer of length MAX_STRING_LENGTH for storing
 * a string.
 *
 * This dynamically allocated buffer will be freed in
 * 'getArgList()'
 */
static char* allocStringBuffer(void) {
    char* buffer = (char*) malloc(MAX_STRING_LENGTH * sizeof(char));
    buffer[0] = '\0';
    return buffer;
}

%}

WORD         [a-zA-Z0-9\/\._-]+
REDIRECTION  >>|2>|&>|[><]
PIPE         [|]

%x DOUBLE_QUOTE
%x SINGLE_QUOTE
%%

{WORD}|{REDIRECTION}|{PIPE} {
    consumeToken();
}

\n {
    /*
     * Cause the scanner to return.  'arguments' will contain
     * the tokens corresponding to this line of input.
     */
    return 0;
}

[ \t]+ {
    /* Ignore white space */
}

\" {
    /* Get ready to build up a double-quoted string */
    arguments[argumentCount] = allocStringBuffer();

    /* Go to the DOUBLE_QUOTE state */
    BEGIN DOUBLE_QUOTE;
}

\' {
    /* Get ready to build up a single-quoted string */
    arguments[argumentCount] = allocStringBuffer();

    /* Go to the SINGLE_QUOTE state */
    BEGIN SINGLE_QUOTE;
}

. {
    /* Catch-all for unsupported characters */
    printf("Unknown char: %s\n", yyget_text());
}

<DOUBLE_QUOTE,SINGLE_QUOTE>{WORD}|{REDIRECTION}|{PIPE} {
    /*
     * In either the DOUBLE_QUOTE or SINGLE_QUOTE states,
     * append a WORD, a REDIRECTION operator, or a 
     * PIPE operator the the line
     */
    strcat(arguments[argumentCount], yyget_text());
}

<DOUBLE_QUOTE,SINGLE_QUOTE>[ \t]+ {
    strcat(arguments[argumentCount], yyget_text());
}

<DOUBLE_QUOTE,SINGLE_QUOTE>\n {
    strcat(arguments[argumentCount], yyget_text());
}

<DOUBLE_QUOTE>\" {
    /*
     * An end double quote in the DOUBLE_QUOTE state brings
     * us back to the normal state (0)
     */
    arguments[++argumentCount] = NULL;
    BEGIN 0;
}

<SINGLE_QUOTE>\' {
    /*
     * An end single quote in the SINGLE_QUOTE state brings
     * us back to the normal state (0)
     */
    arguments[++argumentCount] = NULL;
    BEGIN 0;
}

%%

/*
 * getArgList
 *
 * Returns an array of pointers to strings corresponding to
 * the tokens on the input line.
 */
char** getArgList(void) {
    int i;

    /*
     * Free any dynamically allocated buffers from previous
     * invocations of getArgList()
     */
    for (i = 0; i < argumentCount; ++i) {
        if (arguments[i] != NULL) {
            free(arguments[i]);
        }
    }
    
    /* Reset our state */
    argumentCount = 0;
    arguments[0]  = NULL;

    /* Scan until one of the rules returns a value */
    yylex();

    return arguments;
}

/*
char* yyget_text(void) {
    return yytext;
}
*/
