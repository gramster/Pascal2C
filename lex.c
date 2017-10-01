#include <stdio.h>
#include <ctype.h>
#include <alloc.h>
#include <string.h>
#include "tokens.h"
#include "symtab.h"

void *Malloc(char *fn, int nbytes);
extern void _Free(char *fn, void *p);

#define EOF_CH		26
#define FALSE		0
#define TRUE		!FALSE

int tokentype=UNDEF;
int in_comment=FALSE;
int in_quotes=FALSE;
static int tokenpos;
char token[80];
extern FILE *infile;
char c=' ';
static char tmp[80];
int linenum=1;
int tokenval;

char *TokenNames[]= /* Reserved words plus predefined types */
	{
	"",		
	"ABSOLUTE",
	"AND",
	"ARRAY",
	"BEGIN",
	"CASE",
	"CONST",
	"DIV",
	"DO",
	"DOWNTO",
	"ELSE",
	"END",
	"EXTERNAL",
	"FILE",
	"FOR",
	"FORWARD",
	"FUNCTION",
	"GOTO",
	"IF",
	"IMPLEMENTATION",
	"IN",
	"INLINE",
	"INTERFACE",
	"INTERRUPT",
	"LABEL",
	"MOD",
	"NIL",
	"NOT",
	"OF",
	"OR",
	"PACKED",
	"PROCEDURE",
	"PROGRAM",
	"RECORD",
	"REPEAT",
	"SET",
	"SHL",
	"SHR",
	"STRING",
	"THEN",
	"TO",
	"TYPE",
	"UNIT",
	"UNTIL",
	"USES",
	"VAR",
	"WHILE",
	"WITH",
	"XOR",
	">=",
	"<=",
	":=",
	"..",
	"{",
	"}",
	"+",
	"-",
	"*",
	"/",
	"=",
	"(",
	")",
	"<",
	">",
	"[",
	"]",
	".",
	",",
	";",
	":",
	"^",
	"@",
	"# or '",
	"","","","","",
	"Identifier",
	"Qualified identifier",
	"Number"

};

char *CTokenNames[]= /* punctuation only */
{
	">=", "<=", "=","RANGE","/*","*/","+","-","*","/","==","(",")",
	"<", ">","[","]",".",",",";",":","*","&","'"};

static char lookahead=' ';

struct Lex_State *Lex_MarkPos(void)
{
	struct Lex_State *savestate = Malloc("Lex_MarkPos",sizeof(struct Lex_State));
	savestate->pos = ftell(infile);
	savestate->c = c;
	savestate->lookahead = lookahead;
	savestate->tokentype = tokentype;
	strcpy(savestate->token,token);
	savestate->linenum = linenum;
	savestate->tokenval = tokenval;
	return savestate;
}

void Lex_ResumePos(struct Lex_State *savestate)
{
	c = savestate->c;
	lookahead = savestate->lookahead;
	tokentype = savestate->tokentype;
	strcpy(token, savestate->token);
	linenum = savestate->linenum;
	tokenval = savestate->tokenval;
	fseek(infile,savestate->pos,SEEK_SET);
}



static void Lex_NextChar(void)
{
	c = lookahead;
	lookahead=getc(infile);
	if (c=='\n') linenum++;
	if (feof(infile)) lookahead=EOF_CH;
}

static int Lex_IsReservedWord(char *token)
{
	int i;
	for (i=FIRST_RESERVED;i<=LAST_RESERVED;i++)
		if (strcmp(token, TokenNames[i])==0) return i;
	return UNDEF;
}

static void Lex_GetEscapeChar(void)
{
	int val=0;
	int alldigits;
	Lex_NextChar();
	while (isdigit(c))
		{
		val = val*10 + c -'0';
		Lex_NextChar();
		}
	token[tokenpos++]='\\';
	switch (val)
		{
		case 0: token[tokenpos++]='0'; break;
		case 8: token[tokenpos++]='b'; break;
		case 9: token[tokenpos++]='t'; break;
		case 10:token[tokenpos++]='n'; break;
		case 12:token[tokenpos++]='f'; break;
		case 13:token[tokenpos++]='r';break;
		default: token[tokenpos++]='0';
			alldigits=FALSE;
			if (val/64)
				{
				token[tokenpos++]=val/64+'0';
				val %= 64;
				alldigits=TRUE;
				}
			if (val/8 || alldigits)
				{
				token[tokenpos++]=val/8+'0';
				val %= 8;
				}
			token[tokenpos++] = val+'0';
			break;
		}
}


void Lex_SkipComment()
{
	extern FILE *fp_now;
	fprintf(fp_now,"/*");
	while (TRUE)
		{
		fputc(c,fp_now);
		if (c=='}') break;
		else if (c=='*')
			{
			Lex_NextChar();
			if (c==')') break;
			}
		else Lex_NextChar();
		}
	Lex_NextChar();
	fprintf(fp_now,"*/\n");
}

int Lex_GetToken(void)
{
	tokentype=UNDEF;
	while (tokentype==UNDEF)
	{
	while (c==' ' || c=='\t' || c=='\n') Lex_NextChar();
	switch(c)
		{
		case EOF_CH:	tokentype=EOF;
				break;
		case '>':	Lex_NextChar();
				if (c=='=')
					{
					tokentype=GRTEQ;
					Lex_NextChar();
					}
				else tokentype=GREATER;
				break;
		case '<':	Lex_NextChar();
				if (c=='=')
					{
					tokentype=LESSEQ;
					Lex_NextChar();
					}
				else if (c=='>')
					{
					tokentype = NOTEQUAL;
					Lex_NextChar();
					}
				else tokentype=LESS;
				break;
		case ':':	Lex_NextChar();
				if (c=='=')
					{
					tokentype=ASSIGN;
					Lex_NextChar();
					}
				else tokentype=COLON;
				break;
		case '.':	Lex_NextChar();
				if (c=='.')
					{
					tokentype=SUBRANGE;
					Lex_NextChar();
					}
				else if (c==')')
					{
					tokentype=RIGHTBRAK;
					Lex_NextChar();
					}
				else tokentype=PERIOD;
				break;
		case '(':	Lex_NextChar();
				if (c=='.')
					{
					tokentype=LEFTBRAK;
					Lex_NextChar();
					}
				else if (c=='*')
					{
					Lex_NextChar();
					Lex_SkipComment();
					}
				else tokentype=LEFTPAR;
				break;
		case '{':	Lex_NextChar();
				Lex_SkipComment();
				break;
		case '*':	Lex_NextChar();
				tokentype=MULT;
				break;
		case '+':	tokentype=PLUS;
				Lex_NextChar();
				break;
		case '-':	tokentype=MINUS;
				Lex_NextChar();
				break;
		case '/':	tokentype=DIVIDE;
				Lex_NextChar();
				break;
		case '=':	tokentype=EQUAL;
				Lex_NextChar();
				break;
		case ')':	tokentype=RIGHTPAR;
				Lex_NextChar();
				break;
		case '[':	tokentype=LEFTBRAK;
				Lex_NextChar();
				break;
		case ']':	tokentype=RIGHTBRAK;
				Lex_NextChar();
				break;
		case ',':	tokentype=COMMA;
				Lex_NextChar();
				break;
		case ';':	tokentype=SEMICOLON;
				Lex_NextChar();
				break;
		case '^':	tokentype=CONTENTSOF;
				Lex_NextChar();
				break;
		case '@':	tokentype=ADDRESSOF;
				Lex_NextChar();
				break;
		case '$':	Lex_NextChar();
				token[0]='0';
				token[1]='x';
				tokenpos=2;
				tokentype=NUMBER;
				while (isxdigit(c))
					{
					token[tokenpos++]=c;
					Lex_NextChar();
					}
				token[tokenpos]='\0';
				break;
		case '#':
		case '\'':	tokentype=_STRING;
				token[0]='"';
				tokenpos=1;
				if (c=='#')
					{
					in_quotes=FALSE;
					Lex_GetEscapeChar();
					if (c!='\'' && c!='#')
						{
						token[tokenpos]='\0';
						break;
						}
					}
				else	{
					Lex_NextChar();
					in_quotes=TRUE;
					if (c=='\'')
						{
						Lex_NextChar();
						if (c!='\'')
							{
							strcpy(token,"NULL");
							break;
							}
						token[tokenpos++]='\'';
						Lex_NextChar();
						}
					}
				while (TRUE)
					{
					while (in_quotes || c=='#')
						{
						if (c=='#') Lex_GetEscapeChar();
						else if (c=='\'')
							{
							in_quotes=FALSE;
							Lex_NextChar();
							if (c=='\'')
								{
								in_quotes=TRUE;
								token[tokenpos++]='\'';
								Lex_NextChar();
								}
							}
						else	{
							token[tokenpos++]=c;
							Lex_NextChar();
							}
						}
					if (c=='\'')
						{
						in_quotes=TRUE;
						Lex_NextChar();
						}
					else break;
					}
				token[tokenpos++]='"';
				token[tokenpos]='\0';
				if (tokenpos==3) /* char const */
					{
					token[0]='\'';
					token[2]='\'';
					tokenval = token[1];
					tokentype = CHAR;
					}
				break;
		default:	if (isdigit(c))
					{
					tokentype=NUMBER;
					tokenpos=0;
					while (isdigit(c))
						{
						token[tokenpos++]=c;
						Lex_NextChar();
						}
					if (c=='.' && lookahead!='.')
						{
						token[tokenpos++]=c;
						Lex_NextChar();
						while (isdigit(c))
							{
							token[tokenpos++]=c;
							Lex_NextChar();
							}
						}
					if (c=='e' || c=='E')
						{
						token[tokenpos++]=c;
						Lex_NextChar();
						if (c=='+' || c=='-')
							{
							token[tokenpos++]=c;
							Lex_NextChar();
							}
						while (isdigit(c))
							{
							token[tokenpos++]=c;
							Lex_NextChar();
							}
						}
					token[tokenpos]='\0';
					tokenval=atoi(token);
					}
				else	{
					token[0]=c;
					Lex_NextChar();
					tokenpos=1;
					while (isalnum(c) || c=='_')
						{
						token[tokenpos++]=c;
						Lex_NextChar();
						}
					token[tokenpos]='\0';
					strcpy(tmp,token);
					strupr(tmp);
					if ((tokentype=Lex_IsReservedWord(tmp))==UNDEF)
						{
						tokentype=IDENT;
						if (c=='.')
							{
							Sym_LookUp(token);
							if (symbol->type==TYP_UNIT)
								{
								Lex_NextChar();
								strcat(token,"_");
								tokentype=QUAL_IDENT;
								}
							}
						}
					}
				break;
		}
	}
	return tokentype;
}

void Lex_Accept(int type)
{
	if (type != tokentype)
		{
		fprintf(stderr,"Error on line %d: expected %s (%d), got %s (%d)\n",linenum,TokenNames[type],type,TokenNames[tokentype],tokentype);
		exit(0);
		}
	else Lex_GetToken();
}








