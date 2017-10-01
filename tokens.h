/* Tokens.h	*/

#define FIRST_RESERVED	ABSOLUTE
#define LAST_RESERVED	XOR

#define EOF		(-1)
#define UNDEF		0
#define ABSOLUTE	1
#define AND             2
#define ARRAY           3
#define BEGIN           4
#define CASE            5
#define CONST           6
#define DIV             7
#define DO              8
#define DOWNTO          9
#define ELSE            10
#define END             11
#define EXTERNAL        12
#define _FILE           13
#define FOR             14
#define FORWARD         15
#define FUNCTION        16
#define GOTO            17
#define IF              18
#define IMPLEMENTATION  19
#define IN              20
#define INLINE          21
#define INTERFACE       22
#define INTERRUPT       23
#define LABEL           24
#define MOD             25
#define NIL             26
#define NOT             27
#define OF              28
#define OR              29
#define PACKED          30
#define PROCEDURE       31
#define PROGRAM         32
#define RECORD          33
#define REPEAT          34
#define SET             35
#define SHL             36
#define SHR             37
#define STRING          38
#define THEN            39
#define TO              40
#define TYPE            41
#define UNIT            42
#define UNTIL           43
#define USES            44
#define VAR		45
#define WHILE		46
#define WITH		47
#define XOR		48

#define GRTEQ		49	/*	>=	*/
#define LESSEQ          50	/*	<=	*/
#define ASSIGN          51	/*	:=	*/
#define SUBRANGE        52	/*	..	*/
#define STARTCOM        53	/*	(* or {	*/
#define ENDCOM          54	/*	*) or }	*/
#define PLUS            55	/*	+	*/
#define MINUS           56	/*	-	*/
#define MULT            57	/*	*	*/
#define DIVIDE          58	/*	/	*/
#define EQUAL           59	/*	=	*/
#define LEFTPAR		60	/*	(	*/
#define RIGHTPAR	61	/*	)	*/
#define LESS            62	/*	<	*/
#define GREATER         63	/*	>	*/
#define LEFTBRAK        64	/*	[ or (.	*/
#define RIGHTBRAK       65	/*	] or .)	*/
#define PERIOD          66	/*	.	*/
#define COMMA           67	/*	,	*/
#define SEMICOLON	68	/*	;	*/
#define COLON           69	/*	:	*/
#define CONTENTSOF      70	/*	^	*/
#define POINTER		CONTENTSOF
#define ADDRESSOF       71	/*	@	*/
#define _STRING         72	/*	# or '	*/
#define CHAR		73
#define NOTEQUAL	74	/*	<>	*/

#define IDENT		75
#define QUAL_IDENT	76
#define NUMBER		77

struct Lex_State
	{
	long pos;
	char c;
	char lookahead;
	int  tokenval;
	int  linenum;
	int  tokentype;
	char token[80];
	};

extern int tokentype;
extern char token[80];
extern int tokenval;
extern int linenum;
extern char *TokenNames[];
#define is(t)		(tokentype==t)

int Lex_GetToken(void);
void Lex_Accept(int type);
struct Lex_State *Lex_MarkPos(void);
void Lex_ResumePos(struct Lex_State *savestate);
