/**
 * @file parser.y
 * @brief Grammar for HTTP
 * @author Harshad Shirwadkar (2015)
 */

%{
#include <stdio.h>     /* C declarations used in actions */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define SUCCESS 0

/* Define YACCDEBUG to enable debug messages for this lex file */
#ifdef YACCDEBUG
#include <stdio.h>
#define YPRINTF(...) printf(__VA_ARGS__)
#else
#define YPRINTF(...)
#endif


/* yyparse() calls yyerror() on error */
void yyerror (char *s);

/* yyparse() calls yylex() to get tokens */
extern int yylex();

/*
** Global variables required for parsing from buffer
** instead of stdin:
*/

/* Pointer to the buffer that contains input */
char *parsing_buf;

/* Current position in the buffer */
int parsing_offset;

/* Buffer size */
size_t parsing_buf_siz;

%}

/* Various types values that we can get from lex */
%union {
	char str[8192];
	int i;
}

%start request

/*
 * Tokens that yacc expects from lex, essentially these are the tokens
 * declared in declaration section of lex file.
 */
%token t_crlf
%token t_backslash
%token t_digit
%token t_dot
%token t_token_char
%token t_lws
%token t_colon
%token t_separators
%token t_sp
%token t_ws

/* Type of value returned for these tokens */
%type<str> t_crlf
%type<i> t_backslash
%type<i> t_digit
%type<i> t_dot
%type<i> t_token_char
%type<str> t_lws
%type<i> t_colon
%type<i> t_separators
%type<i> t_sp
%type<str> t_ws

/*
 * Followed by this, you should have types defined for all the intermediate
 * rules that you will define. These are some of the intermediate rules:
 */
%type<i> allowed_char_for_token
%type<i> allowed_char_for_text
%type<str> ows
%type<str> token
%type<str> text

%%

/*
** The following 2 rules define a token.
*/

/*
 * Rule 1: Allowed characters in a token
 *
 * An excerpt from RFC 2616:
 * --
 * token          = 1*<any CHAR except CTLs or separators>
 * --
 */
allowed_char_for_token:
t_token_char; |
t_digit {
	$$ = '0' + $1;
}; |
t_dot;

/*
 * Rule 2: A token is a sequence of all allowed token chars.
 */
token:
allowed_char_for_token {
	YPRINTF("token: Matched rule 1.\n");
	snprintf($$, 8192, "%c", $1);
}; |
token allowed_char_for_token {
	YPRINTF("token: Matched rule 2.\n");
	snprintf($$, 8192, "%s%c", $1, $2);
};

/*
** The following 2 rules define text.
*/
/*
 *
 * Rule 3: Allowed characters in text
 *
 * An excerpt from RFC 2616, section 2.2:
 * --
 * The TEXT rule is only used for descriptive field contents and values
 * that are not intended to be interpreted by the message parser. Words
 * of *TEXT MAY contain characters from character sets other than ISO-
 * 8859-1 [22] only when encoded according to the rules of RFC 2047
 * [14].
 *
 * TEXT = <any OCTET except CTLs, but including LWS>
 * --
 *
 */

allowed_char_for_text:
allowed_char_for_token; |
t_separators {
	$$ = $1;
}; |
t_colon {
	$$ = $1;
}; |
t_backslash {
	$$ = $1;
};

/*
 * Rule 4: Text is a sequence of characters allowed in text as per RFC. May
 * 	   also contains spaces.
 */
text: allowed_char_for_text {
	YPRINTF("text: Matched rule 1.\n");
	snprintf($$, 8192, "%c", $1);
}; |
text ows allowed_char_for_text {
	YPRINTF("text: Matched rule 2.\n");
	snprintf($$, 8192, "%s%s%c", $1, $2, $3);
};

/*
 * Rule 5: Optional white spaces
 */
ows: {
	YPRINTF("OWS: Matched rule 1\n");
	snprintf($$, 8192, "");
}; |
t_sp {
	YPRINTF("OWS: Matched rule 2\n");
	snprintf($$, 8192, "%c", $1);
}; |
t_ws {
	YPRINTF("OWS: Matched rule 3\n");
	snprintf($$, 8192, "%s", $1);
};

/*
 * You need to fill this rule, and you are done! You have all the assembly
 * needed. You may wish to define your own rules. Please read RFC 2616
 * and the annotated excerpted text on the course website. All the best!
 *
 * The bogus rule that we have now, matches any text! Remove it and fill it
 * with your own rule.
 */
request: text {
	YPRINTF("Request: Matched rule 1.\n");
	return SUCCESS;
}; |
token {
	YPRINTF("Request: Matched rule 2.\n");
	return SUCCESS;
}

%%

/* C code */

void set_parsing_buf(char *buf, size_t siz)
{
	parsing_buf = buf;
	parsing_offset = 0;
	parsing_buf_siz = siz;
}

int main(int argc, char *argv[])
{
	int fd_in;

	enum {
		STATE_START = 0,
		STATE_CR,
		STATE_CRLF,
		STATE_CRLFCR,
		STATE_CRLFCRLF
	};

	int i, state;
	char buf[8192];
	size_t offset = 0;
	char ch;

	fd_in = open(argv[1], O_RDONLY);

	if(fd_in < 0) {
		printf("Failed to open the file\n");
		return 0;
	}

	state = STATE_START;
	while(state != STATE_CRLFCRLF) {
		char expected = 0;

		if(read(fd_in, &ch, 1) != 1)
			break;

		buf[offset++] = ch;
		switch(state) {
		case STATE_START:
		case STATE_CRLF:
			expected = '\r';
			break;
		case STATE_CR:
		case STATE_CRLFCR:
			expected = '\n';
			break;
		default:
			state = STATE_START;
			continue;
		}

		if(ch == expected)
			state++;
		else
			state = STATE_START;

	}

	if(state == STATE_CRLFCRLF) {
		printf("Found CRLF-CRLF\n");
		set_parsing_buf(buf, offset);
		printf("yyparse returned %d\n", yyparse());
	} else {
		printf("Failed: Could not find CRLF-CRLF\n");
	}

	close(fd_in);

	return 0;
}

void yyerror (char *s) {fprintf (stderr, "%s\n", s);}
