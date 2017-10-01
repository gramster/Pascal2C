#include <stdio.h>
#include <alloc.h>
#include <string.h>

/****************************************************************

	PASCAL2C - (c) 1989 G. Wheeler

	Compile using the compact model if large programs
		must be processed.

****************************************************************/

extern int withsearchlevel;

#include "tokens.h"
#include "symtab.h"
#include "parse.h"

#define TRUE	1
#define FALSE	0

#define _free(p)	if (p!=NULL) free(p)

struct WithScope Withs[16];
extern char c;
int withlevel = 0;

int is_stdout = FALSE;

/* Indentation level */

int indentlevel = 0;

unsigned short mode=0;

/* Output Files */

FILE *errfile;
FILE *cfile;
FILE *hfile;
FILE *infile;
FILE *fp_now;
FILE *tempin;
FILE *tempout;
FILE *null_fp;

struct Lex_State *lexstate;

/* Constant expression value, and expression type */

long exprval;
unsigned short exprtype;

int min, max;

#define TYP(t)		((mode & (IN_RECORD|IN_TYPEDEF))==IN_TYPEDEF ?(t|TYP_TYPEDEF):(t&TYP_NOTYPE))

char modulenow[80];
char func_rtn_type[80];


void *Malloc(char *fn,int nbytes)
{
	void *rtn = malloc(nbytes);
/*	fprintf(errfile,"%04X : malloc(%d) called from %s\n",rtn,nbytes,fn);
	fflush(errfile); */
	return rtn;
}

void _Free(char *fn, void *p)
{
/*	fprintf(errfile,"%04X : _free called from %s\n",p,fn);
	fflush(errfile); */
	_free(p);
}

void Open_Files(char *basename)
{
	char fname[13];
	char *tmp = fname;
	strcpy(fname,basename);
	while (*++tmp);
	strcpy(tmp,".c");
	if (is_stdout)
		{
		putchar(7);
		cfile = stdout;
		hfile = stdout;
		}
	else	{
		if ((cfile = fopen(fname,"wt"))==NULL)
			{
			fprintf(stderr,"Cannot create program file : %s\n",fname);
			exit(0);
			}
		strcpy(tmp,".h");
		if ((hfile = fopen(fname,"wt"))==NULL)
			{
			fprintf(stderr,"Cannot create header file : %s\n",fname);
			exit(0);
			}
		}
	strcpy(tmp,".err");
	if ((errfile = fopen(fname,"wt"))==NULL)
		{
		fprintf(stderr,"Cannot create error file : %s\n",fname);
		exit(0);
		}
	strcpy(tmp,".TPU");
	if (access(fname,0)!=0)
		{
		strcpy(tmp,".PAS");
		if (access(fname,0)!=0)
			{
			*tmp=0;
			fprintf(stderr,"Cannot open file %s.tpu or %s.pas\n",fname);
			exit(0);
			}
		}
	if ((infile=fopen(fname,"rt"))==NULL)
		{
		fprintf(stderr,"Cannot open file : %s\n",fname);
		exit(0);
		}
	null_fp = fopen("NUL","w");
	fp_now = cfile;
}


void Close_Files(void)
{
	fclose(hfile);
	fclose(cfile);
	fclose(errfile);
	fclose(infile);
}

void Indent(void)
{
	int i=8*indentlevel;
	fputc('\n',fp_now);
	while (i--) fputc(' ',fp_now);
}

void Dump_IDs(char *format, unsigned short type)
{
	char *scratch;
	while ((scratch=Sym_DeQueueID())!=NULL)
		{
		if (Sym_Insert(scratch,type))
			fprintf(fp_now,format,symbol->cname);
		}
}


char *append(char *dest, int free_dest,char *source, int free_source)
{
	char *rtn;
	int l1, l2;
	l1 = dest ? strlen(dest) : 0;
	l2 = source ? strlen(source) : 0;
	rtn = Malloc("append",l1+l2+1);
	*rtn = 0;
	if (dest) strcpy(rtn,dest);
	if (source) strcat(rtn,source);
	if (free_dest) _Free("append",dest);
	if (free_source) _Free("append",source);
	return rtn;
}

/**************************************************************************/

void field_width(void)
{
	if (is(COLON))
		{
		Lex_Accept(COLON);
		fprintf(errfile,"Fieldwidth specifier on line %d ignored\n",linenum);
		_free(expression());
		if (is(COLON))
			{
			Lex_Accept(COLON);
			_free(expression());
			}
		}
}

void ident_list(void)
{
	Sym_QueueID(token);
	Lex_Accept(IDENT);
	while (is(COMMA))
		{
		Lex_Accept(COMMA);
		Sym_QueueID(token);
		Lex_Accept(IDENT);
		}
}


void actual_param(void)
{
	char *tmp;
	fprintf(fp_now,tmp=expression());
	_Free("actual_param",tmp);
	if (is(COLON)) field_width();
}

void actual_param_list(void)
{
	Lex_Accept(LEFTPAR);
	actual_param();
	while (is(COMMA))
		{
		Lex_Accept(COMMA);
		fprintf(fp_now,",");
		actual_param();
		}
	Lex_Accept(RIGHTPAR);
}

void program_heading(void)
{
	Lex_Accept(PROGRAM);
	fprintf(fp_now,"/* %s */\n\n",Sym_CName(token));
	Lex_Accept(IDENT);
	if (is(LEFTPAR))
		{
		Lex_Accept(LEFTPAR);
		ident_list();
		Lex_Accept(RIGHTPAR);
		Dump_IDs("\nFILE *%s;",TYP_FILE);
		}
	fprintf(fp_now,"\n\n");
}

void uses_clause(void)
{
	char *scratch;
	extern char c;
	FILE *temph, *tempc;
	Lex_Accept(USES);
	ident_list();
	Lex_Accept(SEMICOLON);
	temph = hfile;
	tempc = cfile;
	while ((scratch=Sym_DeQueueID())!=NULL)
		{
		char usefl[16];
		FILE *tempout,*tempin;
		struct Lex_State  *lstate;
		if (Sym_Insert(scratch,TYP_UNIT))
			fprintf(tempc,"\n#include \"%s.h\"",symbol->cname);
		strcpy(usefl,symbol->cname);
		strcat(usefl,".pas");
		if (access(usefl,0)==0)
			{
			tempout = fp_now;
			tempin = infile;
			fp_now = null_fp;
			fprintf(errfile,"Processing use file %s\n",symbol->cname);
			lstate = Lex_MarkPos();
			if ((infile = fopen(usefl,"rt"))==NULL)
				fprintf(stderr,"Cannot open file : %s\n",usefl);
			else	{
				c = ' ';
				Lex_GetToken();
				if (is(UNIT)) unit();
				else program();
				}
			fp_now = tempout;
			infile = tempin;
			Lex_ResumePos(lstate);
			_Free("uses_clause",lstate);
			}
		else fprintf(stderr,"No access to file: %s\n",usefl);
		}
	cfile = cfile;
	hfile = hfile;
	fprintf(fp_now,"\n\n");
}

void label(void)
{
	Sym_Insert(token,TYP_LABEL);
	if (is(IDENT)) Lex_Accept(IDENT);
	else Lex_Accept(NUMBER);
}

void label_decl_part(void)
{
	Lex_Accept(LABEL);
	label();
	while (!is(SEMICOLON))
		{
		Lex_Accept(COMMA);
		label();
		}
	Lex_Accept(SEMICOLON);
}

/********************************************************************/



void typed_constant(void)
{
	if (is(NIL))
		{
		fprintf(fp_now,"NULL");
		Lex_Accept(NIL);
		}
	else if (is(LEFTBRAK))
		{
		/* Set constant */
		fprintf(fp_now,"SetCons(");
		Lex_Accept(LEFTBRAK);
		while (!is(RIGHTBRAK))
			{
			constant();
			if (is(SUBRANGE))
				{
				Lex_Accept(SUBRANGE);
				fprintf(fp_now,",RANGE,");
				constant();
				}
			if (is(COMMA))
				{
				fputc(',',fp_now);
				Lex_Accept(COMMA);
				}
			}
		Lex_Accept(RIGHTBRAK);
		}
	else if (is(LEFTPAR))
		{
		Lex_Accept(LEFTPAR);
		fputc('{',fp_now);
		while (!is(RIGHTPAR))
			{
			if (is(IDENT))
				{
				lexstate = Lex_MarkPos();
				Lex_Accept(IDENT);
				if (is(COLON))
					Lex_Accept(COLON);
				else Lex_ResumePos(lexstate);
				_Free("typed_constant",lexstate);
				}
			typed_constant();
			if (is(COMMA) || is(SEMICOLON))
				{
				Lex_Accept(tokentype);
				fputc(',',fp_now);
				}
			}
		Lex_Accept(RIGHTPAR);
		fputc('}',fp_now);
		}
	else if (is(_STRING))
		{
		fprintf(fp_now,token);
		fprintf(errfile,"String: initialiser of typed constant on line %d\n",linenum);
		Lex_Accept(_STRING);
		}
	else constant();
}


void const_decl_part(void)
{
	char tmp[64],*e;
	fprintf(fp_now,"\n\n");
 	Lex_Accept(CONST);
	do	{
		strncpy(tmp,token,63);
		strupr(tmp);
		Lex_Accept(IDENT);
		if (is(COLON))
			{
			Lex_Accept(COLON);
			type(tmp);
			Lex_Accept(EQUAL);
			fprintf(fp_now," = ");
			typed_constant();
			fprintf(fp_now,";");
			}
		else	{
			int val;
			fprintf(fp_now,"\n#define %s\t",tmp);
			Lex_Accept(EQUAL);
			val=constant();
			Sym_InsertConst(tmp,exprtype,val);
			}
		Lex_Accept(SEMICOLON);
		} while (is(IDENT));
	fprintf(fp_now,"\n\n");
}


/********************************/
/* Type definitions		*/
/********************************/


unsigned short ordinal_type(char *typname)
{
	char typtmp[64];
	SYMENTPTR short rtn;
	unsigned short typ;
	int close_comment=FALSE;
	if (!InMode(IN_ARRAY))
		{
		strncpy(typtmp,typname,63);
		typname = typtmp;
		}
	else typname = NULL;
	min=max=0;
	if (tokentype==LEFTPAR)
		{
		/* Enumeration type */
		int enumval=1;
		Lex_Accept(LEFTPAR);
		if (!InMode(IN_ARRAY))
			{
			strupr(token);
			fprintf(fp_now,"enum { %s",token);
			Sym_InsertConst(token,TYP_CONSTANT|TYP_SHORT,0);
			}
		Lex_Accept(IDENT);
		while (is(COMMA))
			{
			Lex_Accept(COMMA);
			if (!InMode(IN_ARRAY))
				{
				strupr(token);
				fprintf(fp_now,", %s",token);
				Sym_InsertConst(token,TYP_CONSTANT|TYP_SHORT,enumval++);
				}
			Lex_Accept(IDENT);
			max++;
			}
		if (!InMode(IN_ARRAY)) fprintf(fp_now," } ");
		typ = TYP_ENUM;
		Lex_Accept(RIGHTPAR);
		}
	else if (Sym_LookUp(token), symbol && (symbol->type & TYP_TYPEDEF))
		{
		typ = symbol->type;
		if (!InMode(IN_ARRAY))
			fprintf(fp_now,"%s\t",symbol->cname);
		min = symbol->aux.o.min;
		max = symbol->aux.o.max;
		Lex_Accept(tokentype);
		}
	else if (is(IDENT) || is(NUMBER) || is(MINUS) || is(PLUS))
		{
		/* Subrange type */
		if (!InMode(IN_ARRAY))
			{
			fprintf(fp_now,"/* Subrange ");
			close_comment=TRUE;
			}
		min = constant();
		Lex_Accept(SUBRANGE);
		if (close_comment) fprintf(fp_now,"..");
		max = constant();
		typ = TYP_SUBRANGE;
		if (close_comment) fprintf(fp_now," */");
		if (!InMode(IN_ARRAY))
			{
			Indent();
			fprintf(fp_now,"int\t");
			}
		}
	typ = TYP(typ);
	if (!InMode(IN_ARRAY))
		{
		if (Sym_InsertOrdinal(typname,TYP(typ),min,max))
			if (!InMode(IN_ARRAY))
				fprintf(fp_now,symbol->cname);
		}
	else if (!InMode(IN_ARRAY))
		{
		strlwr(typname);
		fprintf(fp_now,typname);
		}
	return typ;
}


/*****************************************************************

unsigned short real_type(char *typname)
{
	Sym_LookUp(token);
	Lex_Accept(tokentype);
	fprintf(fp_now,"%s\t",symbol->cname);
	if (!InMode(IN_ARRAY))
		{
		if (Sym_Insert(typname,TYP(symbol->type)))
			fprintf(fp_now,symbol->cname);
		}
	else	{
		strlwr(typname);
		fprintf(fp_now,typname);
		}
	return TYP(symbol->type);
}

******************************************************************/

unsigned short string_type(char *typname)
{
	Lex_Accept(STRING);
	if (!InMode(IN_ARRAY)) Sym_Insert(typname,TYP(TYP_STRING));
	else	{
		symbol=NULL;
		strlwr(typname);
		}
	if (is(LEFTBRAK))
		{
		Lex_Accept(LEFTBRAK);
		fprintf(fp_now,"char\t%s[%d]",symbol?symbol->cname:typname,(int)tokenval+1);
		Lex_Accept(tokentype);
		Lex_Accept(RIGHTBRAK);
		}
	else fprintf(fp_now,"char\t%s[257]",symbol?symbol->cname:typname);
	return TYP(TYP_STRING);
}

unsigned short array_type(char *typname)
{
	char tmp[64];
	int Min[10],Max[10];
	SYMENTPTR elts[10];
	int dim_now=0, i;
	unsigned short rtn;
	strncpy(tmp,typname,63);
	strlwr(tmp);
	EnterMode(IN_ARRAY);
	while (is(ARRAY))
		{
		Lex_Accept(ARRAY);
		Lex_Accept(LEFTBRAK);
		ordinal_type("");
		Min[dim_now]=min;
		Max[dim_now++] = max;
		while (is(COMMA))
			{
			Lex_Accept(COMMA);
			ordinal_type("");
			Min[dim_now] = min;
			Max[dim_now++] = max;
			}
		Lex_Accept(RIGHTBRAK);
		Lex_Accept(OF);
		}
	ExitMode(IN_ARRAY);
	rtn = type("");
	Sym_InsertArray(tmp,rtn,Min,Max,dim_now);
	if ((rtn & TYP_POINTER)==0) fputc('\t',fp_now);
	fprintf(fp_now,symbol->cname);
	for (i=0;i<dim_now;i++)
		fprintf(fp_now,"[%d]",Max[i]-Min[i]+1);
	return symbol->type;
}


unsigned short pointer_type(char *typname)
{
	unsigned short rtn;
	char *tmp;
	Lex_Accept(POINTER);
	if (Sym_LookUp(token))
		{
		rtn = TYP_POINTER | symbol->type;
		tmp = symbol->cname;
		}
	else 	{
		rtn = TYP_POINTER | TYP_RECORD;
		tmp = token;
		strlwr(token);
		}
	rtn = TYP(rtn);
	if (Sym_Insert(typname,rtn))
		fprintf(fp_now,"%s\t*%s",tmp,symbol->cname);
	else fprintf(fp_now,"%s\t*",tmp);
	Lex_Accept(IDENT);
	return rtn;
}

unsigned short file_type(char *typname)
{
	char *tmp;
	Lex_Accept(_FILE);
	if (Sym_Insert(typname,TYP(TYP_FILE)))
		tmp = symbol->cname;
	else tmp="";
	fprintf(fp_now,"/* %s : file of ",tmp);
	if (is(OF))
		{
		Lex_Accept(OF);
		type("");
		}
	fprintf(fp_now," */");
	Indent();
	fprintf(fp_now,"FILE\t*%s",tmp);
	return TYP(TYP_FILE);
}

unsigned short set_type(char *typname)
{
	char *tmp;
	Lex_Accept(SET);
	Lex_Accept(OF);
	if (Sym_Insert(typname,TYP(TYP_SET)))
		tmp = symbol->cname;
	else tmp="";
	fprintf(fp_now,"/* %s : set of ",tmp);
	ordinal_type("");
	fprintf(fp_now," */");
	Indent();
	fprintf(fp_now,"SET\t%s",tmp);
	return TYP(TYP_SET);
}

int variant_level=0;

void fixed_part(int is_generated)
{
	char tmp[64], *id;
	unsigned short rtn;
	do	{
		ident_list();
		Lex_Accept(COLON);
		lexstate = Lex_MarkPos();
		while ((id = Sym_DeQueueID())!=NULL)
			{
			Indent();
			Lex_ResumePos(lexstate);
			strncpy(tmp,id,63);
			rtn = type(tmp);
			Sym_MarkAsField(tmp,is_generated);
			fprintf(fp_now,";");
			}
		_Free("fixed_part",lexstate);
		if (!is(SEMICOLON)) break;
		Lex_Accept(SEMICOLON);
		if (!is(IDENT)) break;
		} while (1);
}

void variant(char *uname)
{
	char *vc;
	Indent();
	fprintf(fp_now,"/* ");
	vc=var_constant();
	while (is(COMMA))
		{
		Lex_Accept(COMMA);
		fprintf(fp_now,",");
		constant();
		}
	fprintf(fp_now,"*/");
	Lex_Accept(COLON);
	Lex_Accept(LEFTPAR);
	if (!is(RIGHTPAR)) field_list(uname,vc,TRUE);
	_Free("variant",vc);
	Lex_Accept(RIGHTPAR);
}

void variant_part(void)
{
	char tmp[64];
	char uname[10];
	Lex_Accept(CASE);
	strncpy(tmp,token,63);
	Lex_Accept(IDENT);
	if (is(COLON))
		{
		Lex_Accept(COLON);
		Indent();
		fprintf(fp_now,"%s %s;",token,tmp);
		strcpy(uname,"V_");
		strncat(uname,tmp,7);
		uname[9]=0;
		Lex_Accept(IDENT);
		}
	else sprintf(uname,"V%d",variant_level++);
	Lex_Accept(OF);
	Indent();
	fprintf(fp_now,"union {");
	indentlevel++;
	Indent();

	variant(uname);
	while (is(SEMICOLON))
		{
		Lex_Accept(SEMICOLON);
		if (is(END)) break;
		variant(uname);
		}
	Indent();
	fprintf(fp_now,"} %s;",uname);
	indentlevel--;
}


void field_list(char *prefix, char *Case, int is_generated)
{
	char tmp1[80],tmp2[80];
	unsigned short typ = TYP_RECORD;
	if (is_generated) typ |= TYP_GENERATED;
	strcpy(tmp1,prefix);
	strcpy(tmp2,prefix);
	if (Case != NULL)
		{
		strcat(tmp1,"*");  strcat(tmp2,"_");
		strcat(tmp1,Case); strcat(tmp2,Case);
		}
	Sym_Insert(tmp1,TYP(typ));
	EnterMode(IN_RECORD);
	Sym_EnterRecordLevel(tmp1);
	Indent();
	fprintf(fp_now,"struct {");
	indentlevel++;
	if (!is(CASE))
		{
		fixed_part(is_generated);
		if (is(SEMICOLON)) Lex_Accept(SEMICOLON);
		}
	if (is(CASE)) variant_part();
	if (is(SEMICOLON)) Lex_Accept(SEMICOLON);
	Indent();
	fprintf(fp_now,"} %s%c",tmp2,is_generated?';':' ');
	indentlevel--;
	Sym_ExitRecordLevel(tmp1,is_generated);
}

unsigned short record_type(char *typname)
{
	char tmp[64];
	strncpy(tmp,typname,63);
	Lex_Accept(RECORD);
	if (!is(END)) field_list(tmp,NULL,FALSE);
	Lex_Accept(END);
	ExitMode(IN_RECORD);
	return TYP(TYP_RECORD);
}


unsigned short type(char *typname)
{
	unsigned short typ;
	switch(tokentype)
		{
		case FUNCTION:  EnterMode(IN_FUNCTYPE);
				function_heading();
				ExitMode(IN_FUNCTYPE);
				typ = TYP_POINTER|TYP_FUNCTION;
				break;
		case PROCEDURE: EnterMode(IN_FUNCTYPE);
				procedure_heading();
				ExitMode(IN_FUNCTYPE);
				typ = TYP_POINTER|TYP_PROCEDURE;
				break;
		case LEFTPAR: 	typ = ordinal_type(typname);	break;
		case ARRAY:	typ = array_type(typname); 	break;
		case RECORD:	typ = record_type(typname);	break;
		case STRING:	typ = string_type(typname);	break;
		case SET:	typ = set_type(typname);  	break;
		case _FILE:	typ = file_type(typname); 	break;
		case POINTER:	typ = pointer_type(typname);	break;
		case MINUS:
		case PLUS:
		case NUMBER:	typ = ordinal_type(typname);	break;
		case IDENT:     Sym_LookUp(token);
				if (symbol->type & TYP_TYPEDEF)
					{
					char *tmp = symbol->cname;
					SYMTABPTR *tabtmp=NULL;
					if ((symbol->type & TYP_RECORD)==TYP_RECORD)
						tabtmp = symbol->aux.fieldtable;
					if (Sym_Insert(typname,typ=TYP(symbol->type)))
						{
						fprintf(fp_now,"%s\t%s",tmp,symbol->cname);
						if (tabtmp)
							{
							symbol->type |= TYP_SHAREFIELDS;
							symbol->aux.fieldtable = tabtmp;
							}
						}
					else fprintf(fp_now,"%s\t",tmp);
					Lex_Accept(IDENT);
					}
				else typ=ordinal_type(typname);
				break;
		}
	return typ;
}

void type_decl(void)
{
	Sym_QueueID(token);
	Lex_Accept(IDENT);
	Lex_Accept(EQUAL);
	Indent();
	fprintf(fp_now,"typedef\t");
	(void)type(Sym_DeQueueID());
	Lex_Accept(SEMICOLON);
}


void type_decl_part(void)
{
	fprintf(fp_now,"\n/*********************/\n/* Type Declarations */\n/*********************/\n");
	EnterMode(IN_TYPEDEF);
	Lex_Accept(TYPE);
	type_decl();
	while (is(IDENT))
		{
		fprintf(fp_now,";");
		type_decl();
		}
	fprintf(fp_now,";\n\n");
	ExitMode(IN_TYPEDEF);
}

/*************************/
/* Variable declarations */
/*************************/

void absolute_clause(void)
{
	Lex_Accept(ABSOLUTE);
	fprintf(errfile,"Absolute variable declared on line %d\n",linenum);
	if (is(IDENT)) Lex_Accept(IDENT);
	else	{
		Lex_Accept(NUMBER);
		Lex_Accept(COLON);
		Lex_Accept(NUMBER);
		}
}

void variable_decl(void)
{
	char tmp[80],*id;
	ident_list();
	Lex_Accept(COLON);
	lexstate = Lex_MarkPos();
	while ((id = Sym_DeQueueID())!=NULL)
		{
		Indent();
		if (fp_now==hfile) fprintf(fp_now,"extern ");
		Lex_ResumePos(lexstate);
		strcpy(tmp,id);
		type(tmp);
		if (is(ABSOLUTE)) absolute_clause();
		fprintf(fp_now,";");
		}
	_Free("variable_decl",lexstate);
	Lex_Accept(SEMICOLON);
}

void variable_decl_part(void)
{
	Lex_Accept(VAR);
	while (is(IDENT)) variable_decl();
}

/***************/
/* Expressions */
/***************/

SYMENTPTR var_symbol;

char *var_reference(void)
{
	char var[64];
	char *line=NULL;
	char scratch[8];
	SYMENTPTR sym;
	strncpy(var,token,63);
	sym = Sym_LookUpVar(token);
	if (sym->type & TYP_FUNCTION)
		{
		if (strcmp(token,modulenow))
			{
			line = append(line,TRUE,"*(",FALSE);
			line = append(line,TRUE,function_call(),TRUE);
			line = append(line,TRUE,")",FALSE);
			Lex_Accept(POINTER);
			}
		else 	{
			line = append(line,TRUE,token,FALSE);
			line = append(line,TRUE,"_rtn",FALSE);
			Lex_Accept(IDENT);
			}
		}
	else if (sym->type & TYP_TYPEDEF)
		{
		line = append(line,TRUE,"(",FALSE);
		line = append(line,TRUE,sym->cname,FALSE);
		line = append(line,TRUE,")",FALSE);
		Lex_Accept(IDENT);
		Lex_Accept(LEFTPAR);
		line = append(line,TRUE,var_reference(),TRUE);
		Lex_Accept(RIGHTPAR);
		}
	else 	{
		if (withsearchlevel>=0)
			{
			line = append(line,TRUE,Withs[withsearchlevel].prefix,FALSE);
			line = append(line,TRUE,".",FALSE);
			line = append(line,TRUE,sym->cname,FALSE);
			}
		else line = append(line,TRUE,sym->cname,FALSE);
		Lex_Accept(IDENT);
		}
	while (is(LEFTBRAK) || is(POINTER) || is(PERIOD))
		{
		switch(tokentype)
			{
			case LEFTBRAK:  fprintf(errfile,"Array reference %s not normalised on line %d\n",sym->cname,linenum);
					Lex_Accept(LEFTBRAK);
					line = append(line,TRUE,"[",FALSE);
					line = append(line,TRUE,expression(),TRUE);
					while (is(COMMA))
						{
						line = append(line,TRUE,"][",FALSE);
						Lex_Accept(COMMA);
						line = append(line,TRUE,expression(),TRUE);
						}
					Lex_Accept(RIGHTBRAK);
					line = append(line,TRUE,"]",FALSE);
					break;
			case POINTER:	line = append("(*(",FALSE,line,TRUE);
					line = append(line,TRUE,"))",FALSE);
					Lex_Accept(POINTER);
					break;
			case PERIOD:	Lex_Accept(PERIOD);
					line = append(line,TRUE,".",FALSE);
/****					if ((sym->type & 31)==TYP_RECORD)
						sym=Sym_LookUpField(sym,token);
					else sym = NULL;
					if (sym) line = append(line,TRUE,sym->cname,FALSE);
					else 	{***/
						strlwr(token);
						line = append(line,TRUE,token,FALSE);
						fprintf(errfile,"Field %s not found in symbol table on line %d\n",token,linenum);
					/***	}***/
					Lex_Accept(IDENT);
					break;
			}
		}
	var_symbol = sym;
	return line;
}



char *function_call(void) /* NB must set exprtyp to the function return typ */
{
	char *line=NULL;
	Sym_LookUp(token);
	exprtype = symbol->type & TYP_NOFUNCTION;
	line = append(line,TRUE,symbol->cname,FALSE);
	line = append(line,TRUE,"(",FALSE);
	Lex_Accept(IDENT);
	if (is(LEFTPAR))
		{
		Lex_Accept(LEFTPAR);
		line = append(line,TRUE,expression(),TRUE);
		while (is(COMMA))
			{
			Lex_Accept(COMMA);
			line = append(line,TRUE,",",FALSE);
			line = append(line,TRUE,expression(),TRUE);
			}
		Lex_Accept(RIGHTPAR);
		}
	line = append(line,TRUE,")",FALSE);
	return line;
}



char *factor(void)
{
	char *line=NULL;
	long val;
	unsigned short typ;
	int op;

	switch(tokentype)
		{
		case LEFTPAR:	Lex_Accept(LEFTPAR);
				line = append(line,TRUE,"(",FALSE);
				line = append(line,TRUE,expression(),TRUE);
				val = exprval;
				typ = exprtype;
				line = append(line,TRUE,")",FALSE);
				Lex_Accept(RIGHTPAR);
				break;
		case NOT:
		case PLUS:
		case MINUS:     if (tokentype==NOT) line = append(line,TRUE,"!",FALSE);
				else line = append(line, TRUE, TokenNames[tokentype],FALSE);
				Lex_Accept(op=tokentype);
				line = append(line,TRUE,factor(),TRUE);
				typ = exprtype;
				val = exprval;
				if (op==NOT) val = !val; else if (op==MINUS) val = -val;
				break;
		case NIL:	line = append(line,TRUE,"NULL",FALSE);
				val = 0l;
				typ = TYP_POINTER;
				Lex_Accept(NIL);
				break;
		case NUMBER:	line = append(line,TRUE,token,FALSE);
				val = tokenval;
				typ = TYP_SHORT;
				Lex_Accept(NUMBER);
				break;
		case _STRING:   line = append(line,TRUE,token,FALSE);
				Lex_Accept(tokentype);
				typ = TYP_STRING;
				if (InMode(IS_CONSTANT))
					fprintf(errfile,"String constant - cannot evaluate (line %d)\n",linenum);
				break;
		case CHAR:	line = append(line,TRUE,token,FALSE);
				val = token[1];
				Lex_Accept(tokentype);
				typ = TYP_BYTE;
				break;
		case LEFTBRAK:	Lex_Accept(LEFTBRAK);
				line = append("SetCons(",FALSE,line,TRUE);
				typ = TYP_SET;
				while (!is(RIGHTBRAK))
					{
					line = append(line,TRUE,expression(),TRUE);
					if (is(SUBRANGE))
						{
						Lex_Accept(SUBRANGE);
						line = append(line,TRUE,",RANGE,",FALSE);
						line = append(line,TRUE,expression(),TRUE);
						}
					if (is(COMMA))
						{
						Lex_Accept(COMMA);
						line = append(line,TRUE,",",FALSE);
						}
					}
				line = append(line,TRUE,")",FALSE);
				Lex_Accept(RIGHTBRAK);
				if (InMode(IS_CONSTANT))
					fprintf(errfile,"Set constructor used in constant on line %d\n",linenum);
				break;
		case ADDRESSOF:	Lex_Accept(ADDRESSOF);
				Sym_LookUp(token);
				if (is(IDENT) && ((symbol->type&TYP_FUNCTION)||(symbol->type==TYP_PROCEDURE)))
					{
					line = append(line,TRUE,symbol->cname,FALSE);
					Lex_Accept(IDENT);
					}
				else 	{
					line = append(line,TRUE,"&",FALSE);
					line = append(line,TRUE,var_reference(),TRUE);
					}
				typ = TYP_SHORT;
				if (InMode(IS_CONSTANT))
					fprintf(errfile,"Addressof operator used in constant on line %d\n",linenum);
				break;
		case IDENT:	Sym_LookUpVar(token);
				if (symbol->type & TYP_FUNCTION)
					if (InMode(IN_FUNCTION) && strcmp(token,modulenow)==0 && c!='(')
						line = append(line,TRUE,var_reference(),TRUE);
					else line = append(line,TRUE,function_call(),TRUE);
				else if (symbol->type & TYP_TYPEDEF)
					{
					line = append(line,TRUE,"(",FALSE);
					line = append(line,TRUE,symbol->cname,FALSE);
					line = append(line,TRUE,")",FALSE);
					Lex_Accept(IDENT);
					Lex_Accept(LEFTPAR);
					line = append(line,TRUE,expression(),TRUE);
					Lex_Accept(RIGHTPAR);
					}
				else line = append(line,TRUE,var_reference(),TRUE);
				typ = exprtype;
				val = exprval;
				break;
		}
	exprtype = typ;
	exprval = val;
	return line;
}



char *term(void)
{
	char *line=NULL;
	long val;
	unsigned short typ;
	int op;
	int is_set=0;
	line = append(line,TRUE,factor(),TRUE);
	typ = exprtype;
	val = exprval;
	while (is(MULT) | is (DIV) | is(DIVIDE) | is(MOD) | is(AND) |
		is(SHL) | is(SHR))
			{
			op=tokentype;
			if (is(MULT) && typ==TYP_SET)
				{
				if (!is_set) line = append("SetInstersect(",FALSE,line,TRUE);
				is_set=1;
				}
			else if (op==DIV) line = append(line,TRUE,"/",FALSE);
			else if (op==MOD) line = append(line,TRUE,"%",FALSE);
			else if (op==AND)
				{
				line = append(line,TRUE,"&",FALSE);
				fprintf(errfile,"Operator & (and) on line %d has higher precedence in Pascal than C\n",linenum);
				}
			else if (op==SHL)
				{
				line = append(line,TRUE,"<<",FALSE);
				fprintf(errfile,"Operator << (shl) on line %d has higher precedence in Pascal than C\n",linenum);
				}
			else if (op==SHR)
				{
				line = append(line,TRUE,">>",FALSE);
				fprintf(errfile,"Operator >> (shr) on line %d has higher precedence in Pascal than C\n",linenum);
				}
			else line = append(line,TRUE,TokenNames[op],FALSE);
			Lex_Accept(op);
			if (is_set) line = append(line,TRUE,",",FALSE);
			line = append(line,TRUE,factor(),TRUE);
			switch(op)
				{
				case MULT:	val *= exprval; break;
				case DIV:	val /= exprval; break;
				case DIVIDE:val /= exprval; break;
				case MOD:	val %= exprval; break;
				case AND:	val &= exprval; break;
				case SHL:	val <<=exprval; break;
				case SHR:	val >>=exprval; break;
				}
			}
	if (is_set) line = append(line,TRUE,")",FALSE);
	exprtype = typ;
	exprval = val;
	return line;
}

char *simple_expr(void)
{
	char *line=NULL;
	unsigned short typ;
	int op;
	int mustclose=0;
	long val;
	line = append(line,TRUE,term(),TRUE);
	val = exprval;
	typ = exprtype;
	while (is(PLUS) | is(MINUS) | is (OR) | is(XOR))
		{
		op=tokentype;
		if (op==PLUS && typ==TYP_STRING)
			{
			if (!mustclose)
				if (!InMode(IS_CONSTANT))
					{
					line = append("StrCat(",FALSE,line,TRUE);
					mustclose=1;
					}
			}
		else if (op==PLUS && typ==TYP_SET)
			{
			if (!mustclose) line = append("SetUnion(",FALSE,line,TRUE);
			mustclose=1;
			}
		else if (op==MINUS && typ==TYP_SET)
			{
			if (!mustclose) line = append("SetDiff(",FALSE,line,TRUE);
			mustclose=1;
			}
		else if (op==OR) line = append(line,TRUE,"|",FALSE);
		else if (op==XOR) line = append(line,TRUE,"^",FALSE);
		else line = append(line,TRUE, TokenNames[op],FALSE);
		Lex_Accept(op);
		if (mustclose) line = append(line,TRUE,",",FALSE);
		line = append(line,TRUE,term(),TRUE);
		if (typ!=TYP_STRING && typ!=TYP_SET)
			switch(op)
				{
				case PLUS:	val += exprval;	break;
				case MINUS:	val -= exprval;	break;
				case OR:	val |= exprval;	break;
				case XOR:	val ^= exprval;	break;
				}
		}
	if (mustclose) line = append(line,TRUE,")",FALSE);
	exprval = val;
	exprtype = typ;
	return line;
}


char *expression(void)
{
	int op;
	long val;
	unsigned short typ;
	int mustclose=FALSE;
	char *line=NULL;
	char *s_op;

	line = append(line,TRUE,simple_expr(),TRUE);
	val = exprval;
	typ = exprtype;
	op=tokentype;
	if (is(LESS) || is(GREATER) || is(LESSEQ) || is(NOTEQUAL) ||
		is(GRTEQ) || is(EQUAL) || is(IN))
			{
			s_op = TokenNames[op];
			if (is(IN))
				{
				line = append("SetElt(",FALSE,line,TRUE);
				line = append(line,TRUE,",",FALSE);
				mustclose=TRUE;
				s_op=NULL;
				}
			else if (typ == TYP_SET)
				{
				line = append("SetCmp(",FALSE,line,TRUE);
				line = append(line,TRUE,",",FALSE);
				mustclose=TRUE;
				}
			else if (typ == TYP_STRING)
				{
				line = append("strcmp(",FALSE,line,TRUE);
				line = append(line,TRUE,",",FALSE);
				mustclose=TRUE;
				fprintf(errfile,"String compare on line %d\n",linenum);
				}
			else if (is(EQUAL)) line = append(line,TRUE,"==",FALSE);
			else if (is(NOTEQUAL)) line = append(line,TRUE,"!=",FALSE);
			else line = append(line,TRUE,s_op,FALSE);

			typ = TYP_BOOLEAN;
			Lex_Accept(op);
			line = append(line,TRUE,simple_expr(),TRUE);
			if (mustclose)
				{
				line = append(line,TRUE,")",FALSE);
				if (s_op)
					{
					line = append("(",FALSE,line,TRUE);
					line = append(line,TRUE,s_op,FALSE);
					line = append(line,TRUE,"0)",FALSE);
					}
				}
			switch (op)
				{
				case LESS:	val=(val<exprval);	break;
				case GREATER:	val=(val>exprval);	break;
				case LESSEQ:	val=(val<=exprval);	break;
				case GRTEQ:	val=(val>=exprval);	break;
				case EQUAL:	val=(val==exprval);	break;
				case NOTEQUAL:	val=(val!=exprval);	break;
				}
			}
	exprval = val;
	exprtype = typ;
	return line;
}

long constant(void)
{
	char *tmp;
	EnterMode(IS_CONSTANT);	/* Flag used by expressions */
	tmp=expression();
	if (!InMode(IN_ARRAY|NO_ECHO)) fprintf(fp_now,tmp);
	_Free("constant",tmp);
	ExitMode(IS_CONSTANT);
	return exprval;
}


char *var_constant(void)	/* used for variants */
{
	char *tmp;
	EnterMode(IS_CONSTANT);	/* Flag used by expressions */
	tmp=expression();
	if (!InMode(IN_ARRAY|NO_ECHO)) fprintf(fp_now,tmp);
	ExitMode(IS_CONSTANT);
	return tmp;
}

/**************/
/* Statements */
/**************/

void with_stmt(void) /* Figure out how to deal with this */
			/* uses var_reference, statement */
{
	SYMENTPTR withsym;
	char *tmp;
	fprintf(errfile,"With statement at line %d\n",linenum);
	if (withlevel>15) fprintf(stderr,"MAX WITHs EXCEEDED!!!!\n");
	Lex_Accept(WITH);
	do	{
		tmp = var_reference();
		withsym = var_symbol;
		Withs[withlevel].prefix = withsym->cname;
		withsym->cname = tmp;
		Withs[withlevel++].fieldtable = withsym->aux.fieldtable;
		if (is(COMMA)) Lex_Accept(COMMA);
		} while (!is(DO));
	Lex_Accept(DO);
	statement();
	/* Restore original prefix */
	_Free("with_stmt",withsym->cname);
	withsym->cname = Withs[--withlevel].prefix;
}


void if_stmt(void) /* uses expression, statement */
{
	char *tmp;
	Lex_Accept(IF);
	Indent();
	fprintf(fp_now,"if (");
	fprintf(fp_now,tmp=expression()); _Free("if_stmt",tmp);
	fprintf(fp_now,") ");
	indentlevel++;
	Lex_Accept(THEN);
	statement();
	if (is(ELSE))
		{
		Lex_Accept(ELSE);
		indentlevel--;
		Indent();
		indentlevel++;
		fprintf(fp_now,"else ");
		statement();
		}
	indentlevel--;
}

void case_stmt(void)	/* uses statement, constant */
{
	char *tmp;
	Lex_Accept(CASE);
	Indent();
	fprintf(fp_now,"switch (%s)",tmp = expression());
	indentlevel++;
	Indent();
	fprintf(fp_now,"{");
	Indent();
	_Free("case_stmt",tmp);
	Lex_Accept(OF);
	do	{
		if (is(SEMICOLON)) Lex_Accept(SEMICOLON);
		if (is(ELSE) || is(END)) break;
		do	{
			long start, end;
			EnterMode(NO_ECHO);
			tmp = expression();
			Indent();
			fprintf(fp_now,"case %s:",tmp);
			_Free("case_stmt",tmp);
			start=end=exprval;
			if (is(SUBRANGE))
				{
				Lex_Accept(SUBRANGE);
				tmp = expression();
				end = exprval;
				Indent();
				fprintf(fp_now,"/* Range .. %s */",tmp);
				fprintf(errfile,"Case range on line %d\n",linenum);
				Indent();
				fprintf(fp_now,"case %s:",tmp);
				_Free("case_stmt",tmp);
				}
			ExitMode(NO_ECHO);
			if (is(COMMA)) Lex_Accept(COMMA);
			} while (!is(COLON));
		Lex_Accept(COLON);
		if (!is(SEMICOLON))
			{
			indentlevel++;
			statement();
			Indent();
			indentlevel--;
			}
		fprintf(fp_now,"break;");
		} while (is(SEMICOLON));
	if (is(ELSE))
		{
		Lex_Accept(ELSE);
		Indent();
		fprintf(fp_now,"default: ");
		indentlevel++;
		statement();
		indentlevel--;
		}
	if (is(SEMICOLON)) Lex_Accept(SEMICOLON);
	Lex_Accept(END);
	Indent();
	fprintf(fp_now,"}");
	indentlevel--;
}

void repeat_stmt(void)	/* uses statement, expression */
{
	char *tmp;
	Lex_Accept(REPEAT);
	Indent();
	fprintf(fp_now,"do     {");
	indentlevel++;
	do	{
		statement();
		if (is(SEMICOLON)) Lex_Accept(SEMICOLON);
		} while (!is(UNTIL));
	Lex_Accept(UNTIL);
	Indent();
	fprintf(fp_now,"} while (!%s)",tmp = expression());
	indentlevel--;
	_Free("repeat_stmt",tmp);
}

void while_stmt(void) /* uses expression, statement */
{
	char *tmp;
	Lex_Accept(WHILE);
	Indent();
	fprintf(fp_now,"while (%s)",tmp = expression());
	indentlevel++;
	Indent();
	fprintf(fp_now,"{");
	_Free("while_stmt",tmp);
	Lex_Accept(DO);
	statement();
	Indent();
	fprintf(fp_now,"}");
	indentlevel--;
}

void for_stmt(void)  /* uses expression, statement */
{
	char *control;
	char *op, *cmp, *tmp;
	Lex_Accept(FOR);
	Sym_LookUp(token);
	control=symbol->cname;
	Lex_Accept(IDENT);
	Lex_Accept(ASSIGN);
	Indent();
	fprintf(fp_now,"for (%s=%s; ",control,tmp = expression());
	_Free("for_stmt",tmp);
	if (is(TO)) 	{Lex_Accept(TO); op="++"; cmp="<=";}
	else 		{Lex_Accept(DOWNTO); op="--"; cmp=">=";}
	fprintf(fp_now,"%s%s%s; %s%s)",control,cmp,tmp=expression(),control,op);
	_Free("for_stmt",tmp);
	Lex_Accept(DO);
	indentlevel++;
	statement();
	indentlevel--;
}

void goto_stmt(void)
{
	Lex_Accept(GOTO);
	Sym_LookUp(token);
	Indent();
	fprintf(fp_now,"goto %s",symbol->cname);
	Lex_Accept(tokentype);
}

void procedure_stmt(void)	/* uses actual_param_list */
{
	Sym_LookUp(token);
	Indent();
	fprintf(fp_now,symbol->cname);
	Lex_Accept(IDENT);
	fprintf(fp_now,"(");
	if (is(LEFTPAR))
		actual_param_list();
	fprintf(fp_now,");");
}

void assignment_stmt(void) /* uses var_reference, expression */
{
	char *tmp = var_reference();
	Indent();
	fprintf(fp_now,tmp);
	_Free("assignment",tmp);
	Lex_Accept(ASSIGN);
	fprintf(fp_now," = %s",tmp =expression());
	fprintf(fp_now,";");
	_Free("assignment",tmp);
}

void statement()
{
	Sym_LookUp(token);
	if (is(NUMBER) || (is(IDENT) && symbol->type==TYP_LABEL))
		{
		fprintf(fp_now,"\n%s: ",symbol?symbol->cname:token);
		Lex_Accept(tokentype);
		Lex_Accept(COLON);
		}
	switch(tokentype)
		{
		case BEGIN:	compound_stmt(FALSE);	break;
		case WITH:	with_stmt();		break;
		case IF:	if_stmt();		break;
		case CASE:	case_stmt();		break;
		case REPEAT:	repeat_stmt();		break;
		case WHILE:	while_stmt();		break;
		case FOR:	for_stmt();		break;
		case GOTO:	goto_stmt();		break;
		default:	Sym_LookUp(token);
				if (symbol->type == TYP_PROCEDURE)
					procedure_stmt();
				else assignment_stmt();		break;
		}
}

void compound_stmt(int is_function)
{
	Lex_Accept(BEGIN);
	Indent();
	fprintf(fp_now,"{");
	if (is_function)
		{
		Indent();
		fprintf(fp_now,"%s",func_rtn_type);
		if (func_rtn_type[strlen(func_rtn_type)-1]!='*') fprintf(fp_now," ");
		fprintf(fp_now,"%s_rtn;",modulenow);
		}
	while (!is(END))
		{
		statement();
		if (is(SEMICOLON)) Lex_Accept(SEMICOLON);
		}
	Lex_Accept(END);
	if (is_function)
		{
		Indent();
		fprintf(fp_now,"return %s_rtn;",modulenow);
		}
	Indent();
	fprintf(fp_now,"}");
}

/****************************/
/* Procedures and Functions */
/****************************/

void inline_directive(char *heading)
{
	fprintf(fp_now,heading);
	fprintf(fp_now,"\n{\n");
	fprintf(stderr,"Inline code not yet implemented!!!\n");
	exit(0);
}

char *parameter_decl(char *line)
{
	unsigned short parmtype;
	int ref_param = FALSE;
	char *typname,*idtemp,typtemp[80];
	if (is(VAR))
		{
		Lex_Accept(VAR);
		ref_param=TRUE;
		}
	ident_list();
	if (is(COLON))
		{
		Lex_Accept(COLON);
		if (is(STRING))
			{
			typname="char *";
			parmtype = TYP_STRING;
			}
		else if (is(_FILE))
			{
			typname = "FILE *";
			parmtype=TYP_FILE;
			}
		else 	{
			Sym_LookUp(token);
			typname = strcpy(typtemp,symbol->cname);
			parmtype = symbol->type;
			}
		Lex_Accept(tokentype);
		}
	else 	{
		typname = "";
		fprintf(errfile,"Untyped parameters on line %d\n",linenum);
		}
	idtemp = Sym_DeQueueID();
	line = append(line,TRUE,typname,FALSE);
	if (typname[strlen(typname)-1]!='*') line = append(line,TRUE," ",FALSE);
	Sym_Insert(idtemp,parmtype&TYP_NOTYPE);
	if (ref_param) line = append(line,TRUE,"*",FALSE);
	line = append(line,TRUE,/*idtemp*/symbol->cname,FALSE);
	while (idtemp = Sym_DeQueueID())
		{
		line = append(line,TRUE,",",FALSE);
		line = append(line,TRUE,typname,FALSE);
		line = append(line,TRUE," ",FALSE);
		if (ref_param) line = append(line,TRUE,"*",FALSE);
		Sym_Insert(idtemp,parmtype&TYP_NOTYPE);
		line = append(line,TRUE,/*idtemp*/symbol->cname,FALSE);
		}
	return line;
}

char *formal_parameter_list(char *line)
{
	Lex_Accept(LEFTPAR);
	line = parameter_decl(line);
	while (is(SEMICOLON))
		{
		Lex_Accept(SEMICOLON);
		line = append(line,TRUE,",",FALSE);
		line = parameter_decl(line);
		}
	Lex_Accept(RIGHTPAR);
	return line;
}

char *procedure_heading(void)
{
	char *tmp=NULL;
	Lex_Accept(PROCEDURE);
	if (mode & IN_FUNCTYPE)
		{
		tmp = Malloc("proc_head",10+strlen(token));
		sprintf(tmp,"void (*%s)(",token);
		}
	else	{
		tmp = Malloc("proc_head",7+strlen(token));
		sprintf(tmp,"void %s(",token);
		}
	Sym_Insert(token,TYP(TYP_PROCEDURE));
	Lex_Accept(IDENT);
	if (is(LEFTPAR))
		tmp = formal_parameter_list(tmp);
	else tmp = append(tmp,TRUE,"void",FALSE);
	tmp = append(tmp,TRUE,")",FALSE);
	return tmp;
}

void procedure_body(char *heading)
{
	if (is(INLINE)) inline_directive(heading);
	else	{
		if (is(INTERRUPT))
			{
			fprintf(errfile,"Interrupt procedure on line %d\n",linenum);
			Lex_Accept(INTERRUPT);
			Lex_Accept(SEMICOLON);
			}
		if (is(FORWARD) || is(EXTERNAL))
			{
			Lex_Accept(tokentype);
			fprintf(fp_now,"\n%s;",heading);
			}
		else block(heading,FALSE);
		}
}

void procedure_decl(int is_interface)
{
	char *tmp=NULL;
	Sym_EnterBlock(NULL);
	tmp=procedure_heading();
	Lex_Accept(SEMICOLON);
	if (!is_interface)
		{
		procedure_body(tmp);
		Lex_Accept(SEMICOLON);
		}
	else if (is(INLINE))
		{
		inline_directive(tmp);
		Lex_Accept(SEMICOLON);
		}
	_Free("proc_decl",tmp);
	Sym_ExitBlock(NULL);
}

char *function_heading(void)
{
	char *tmp1=NULL,*tmp2=NULL;
	Lex_Accept(FUNCTION);
	if (mode & IN_FUNCTYPE)
		{
		tmp1= Malloc ("func_head",5+strlen(token));
		sprintf(tmp1,"(*%s)(",token);
		}
	else	{
		tmp1 = Malloc("func_head",2+strlen(token));
		sprintf(tmp1,"%s(",token);
		}
	strcpy(modulenow,token);
	Lex_Accept(IDENT);
	if (is(LEFTPAR))
		tmp1 = formal_parameter_list(tmp1);
	else tmp1 = append(tmp1,TRUE,"void",FALSE);
	tmp1 = append(tmp1,TRUE,")",FALSE);
	Lex_Accept(COLON);
	if (is(STRING))
		{
		Sym_Insert(modulenow,TYP(TYP_FUNCTION|TYP_STRING));
		tmp2 = append(tmp2,TRUE,"char *",FALSE);
		}
	else	{
		Sym_LookUp(token);
		tmp2 = append(tmp2,TRUE,symbol->cname,FALSE);
		tmp2 = append(tmp2,TRUE," ",FALSE);
		Sym_Insert(modulenow,TYP(symbol->type | TYP_FUNCTION));
		}
	Lex_Accept(tokentype);
	strcpy(func_rtn_type,tmp2);
	tmp2 = append(tmp2,TRUE,tmp1,TRUE);
	return tmp2;
}


void function_body(char *heading)
{
	EnterMode(IN_FUNCTION);
	if (is(INLINE)) inline_directive(heading);
	else if (is(FORWARD) || is(EXTERNAL))
		{
		Lex_Accept(tokentype);
		fprintf(fp_now,"\n%s;",heading);
		}
	else block(heading,TRUE);
	ExitMode(IN_FUNCTION);
}

void function_decl(int is_interface)
{
	char *tmp=NULL;
	Sym_EnterBlock(NULL);
	tmp = function_heading();
	Lex_Accept(SEMICOLON);
	if (!is_interface)
		{
		function_body(tmp);
		Lex_Accept(SEMICOLON);
		}
	else if (is(INLINE))
		{
		inline_directive(tmp);
		Lex_Accept(SEMICOLON);
		}
	_Free("func_decl",tmp);
	Sym_ExitBlock(NULL);
}



void decl_part(void)
{
	while (tokentype != BEGIN)
		{
		switch(tokentype)
			{
			case LABEL:	label_decl_part();
					break;
			case CONST:	const_decl_part();
					break;
			case TYPE:	type_decl_part();
					break;
			case VAR:	variable_decl_part();
					break;
			case PROCEDURE:
			case FUNCTION:  do	{
						if (is(PROCEDURE))
							procedure_decl(FALSE);
						else function_decl(FALSE);
						} while (is(PROCEDURE) || is(FUNCTION));
					break;

			default: Lex_Accept(UNDEF);	break;
			}
		}
}


void block(char *name,int is_function)
{
	Sym_EnterBlock(NULL);
	decl_part();
	fprintf(fp_now,"\n\n%s",name);
	compound_stmt(is_function);
	Sym_ExitBlock(NULL);
}



void program(void)
{
	if (is(PROGRAM))
		{
		program_heading();
		Lex_Accept(SEMICOLON);
		}
	if (is(USES)) uses_clause();
	block("main()",FALSE);
	Lex_Accept(PERIOD);
}

void interface_part(void)
{
	char *tmp;
	fp_now = hfile;
	Lex_Accept(INTERFACE);
	if (is(USES)) uses_clause();
	while (is(CONST) || is(TYPE) || is(VAR) || is(PROCEDURE) || is(FUNCTION))
		switch (tokentype)
			{
			case CONST:	const_decl_part();	break;
			case TYPE:	type_decl_part();	break;
			case VAR:	variable_decl_part();	break;
			case FUNCTION:	function_decl(TRUE);
					break;
			case PROCEDURE:	procedure_decl(TRUE);
					break;
			}
}


void implementation_part(void)
{
	Lex_Accept(IMPLEMENTATION);
	fp_now = cfile;
	if (is(USES)) uses_clause();
	while (TRUE)
		{
		if (is(LABEL)) label_decl_part();
		else if (is(CONST)) const_decl_part();
		else if (is(TYPE)) type_decl_part();
		else if (is(VAR)) variable_decl_part();
		else if (is(PROCEDURE)) procedure_decl(FALSE);
		else if (is(FUNCTION)) function_decl(FALSE);
		else break;
		}
}


void initialization_part(char *unitname)
{
	if (is(END)) Lex_Accept(END);
	else	{
		fprintf(fp_now,"\n\n\nvoid Init_%s(void)\n",unitname);
		compound_stmt(FALSE);
		}
}

void unit(void)
{
	char unitname[64];
	Lex_Accept(UNIT);
	strncpy(unitname,token,63);
	fprintf(fp_now,"/* %s */\n\n",token);
	Lex_Accept(IDENT);
	Lex_Accept(SEMICOLON);
	interface_part();
	implementation_part();
	initialization_part(unitname);
	Lex_Accept(PERIOD);
}


main(argc,argv)
int argc;
char *argv[];
{
	if (argc!=3)
		{
		fprintf(stderr,"Usage: PASCAL2C <basename> <0|1>\n");
		fprintf(stderr,"     0 - stdout; 1 - <basename>.c\n");
		exit(0);
		}
	is_stdout = (argv[2][0]=='0');
	Open_Files(argv[1]);
	Lex_GetToken();
	Sym_EnterBlock(NULL);
	Sym_InitTable();
	if (is(UNIT)) unit();
	else program();
	Close_Files();
	Sym_ExitBlock(NULL);
}


