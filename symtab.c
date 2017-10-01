#include <stdio.h>
#include <alloc.h>
#include <string.h>
#include "symtab.h"
#include "parse.h"

static SYMTABPTR symtabnow=NULL, rootsymtab=NULL;
SYMENTPTR symbol;

void *Malloc(char *fn, int nbytes);
void _Free(char *fn, void *p);
extern int linenum;

#define TRUE	1
#define FALSE	0
#define _free(p)	if (p!=NULL)	{free(p); p=NULL;}

static UniqSymID	= 0;

void Sym_EnterBlock(SYMENTPTR record)
{
	if (rootsymtab==NULL)
		{
		symtabnow=rootsymtab=Malloc("Sym_EnterBlock",SYMTABSIZE);
		rootsymtab->parent = NULL;
		}
	else	{
		symtabnow=rootsymtab;
		while (symtabnow->child) symtabnow=symtabnow->child;
		if (record) symtabnow->child = record->aux.fieldtable;
		else symtabnow->child=Malloc("Sym_EnterBlock",SYMTABSIZE);
		symtabnow->child->parent = symtabnow;
		symtabnow = symtabnow->child;
		}
	if (record==NULL)
		{
		symtabnow->child=NULL;
		symtabnow->first=NULL;
		}
}


void Sym_FreeTables(SYMTABPTR from)
{
	symbol = from->first;
	printf("\n");
	while (symbol)
		{
		SYMENTPTR tmp = symbol;
		symbol = symbol->next;
		if (tmp->type == TYP_CONSTANT)
			printf("#undef %s\n",tmp->cname);
		if (from!=rootsymtab)
		_Free("Sym_FreeTables",tmp->name);
		_Free("Sym_FreeTables",tmp->cname);
		if (tmp->type==TYP_RECORD)
			if ((tmp->type & (TYP_SHAREFIELDS|TYP_GENERATED))==0)
				{
				Sym_FreeTables(tmp->aux.fieldtable);
				_Free("Sym_FreeTables",tmp->aux.fieldtable);
				}
		if ((tmp->type & (TYP_ARRAY|TYP_SHAREFIELDS))==TYP_ARRAY)
			_Free("Sym_FreeTables",tmp->aux.indexptr);
		_Free("Sym_FreeTables",tmp);
		}
}


char *Sym_TypName(unsigned short typ)
{
	static typname[100];
	typname[0]=0;
	if (typ & TYP_TYPEDEF) strcat(typname,"Typedef ");
	if (typ & TYP_FUNCTION) strcat(typname,"function returning ");
	if (typ & TYP_ARRAY) strcat(typname,"array of ");
	if (typ & TYP_POINTER) strcat(typname,"pointer to ");
	if (typ & TYP_GENERATED) strcat(typname,"generated ");
	if (typ & TYP_SHAREFIELDS) strcat(typname,"shared fieldlist ");
	if (typ & TYP_FIELD) strcat(typname,"field ");
	if (typ & TYP_UNSIGNED) strcat(typname,"unsigned ");
	if (typ & TYP_CONSTANT) strcat(typname,"constant ");
	switch (typ & 31)
		{
		case 1: strcat(typname,"UNIT");		break;
		case 2: strcat(typname,"PROCEDURE"); 	break;
		case 4: strcat(typname,"LABEL");	break;
		case 5: strcat(typname,"FILE");		break;
		case 6: strcat(typname,"STRING");	break;
		case 7: strcat(typname,"BYTE");		break;
		case 8: strcat(typname,"SHORT");	break;
		case 9: strcat(typname,"LONG");		break;
		case 10: strcat(typname,"FLOAT");	break;
		case 11: strcat(typname,"DOUBLE");	break;
		case 12: strcat(typname,"BOOLEAN");	break;
		case 13: strcat(typname,"ENUM");	break;
		case 14: strcat(typname,"SUBRANGE");	break;
		case 15: strcat(typname,"RECORD");	break;
		case 16: strcat(typname,"SET");		break;
		}
	return typname;
}

void Sym_ShowTable(SYMTABPTR root)
{
	SYMTABPTR stabnow=root;
	SYMENTPTR seltnow;
	while (stabnow)
		{
		seltnow = stabnow->first;
		while (seltnow)
			{
			fprintf(errfile,"Symbol: %s\nCname: %s\nType: %s\n\n",seltnow->name,seltnow->cname,Sym_TypName(seltnow->type));
			if ((seltnow->type & 31) == TYP_RECORD)
				if ((seltnow->type & (TYP_SHAREFIELDS|TYP_GENERATED))==0)
					Sym_ShowTable(seltnow->aux.fieldtable);
			seltnow = seltnow->next;
			}
		stabnow = stabnow->child;
		fprintf(errfile,"Entering child symbol table\n");
		}
}

void Sym_DumpStab(void)
{
	Sym_ShowTable(rootsymtab);
}

void Sym_ExitBlock(SYMENTPTR record)
{
	symtabnow=rootsymtab;
	if (record)
		{
		while (symtabnow->child != record->aux.fieldtable)
	      		symtabnow=symtabnow->child;
		symtabnow->child->parent = record;
		symtabnow->child = NULL;
		}
	else if (symtabnow)
		{
		while (symtabnow->child)
			symtabnow=symtabnow->child;
		Sym_FreeTables(symtabnow);
		if (symtabnow->parent) symtabnow->parent->child=NULL;
		_Free("Sym_ExitBlock",symtabnow);
		if (symtabnow==rootsymtab) rootsymtab=NULL;
		}
}

int level;
int maxlevel;

SYMENTPTR Sym_FindSymbol(SYMTABPTR root,char *name)
{
	char tmpname[64];
	int len = strlen(name);
	level=0;
	strncpy(tmpname,name,63);
	strupr(tmpname);
	symtabnow=root;
	while (symtabnow->child) {symtabnow=symtabnow->child; level++;}
	maxlevel=level;
	while (TRUE)
		{
		symbol = symtabnow->first;
		while (symbol)
			{
			if (len == symbol->len)
				if (strcmp(symbol->name, tmpname)==0)
				       	return symbol;
			symbol=symbol->next;
			}
		if (symtabnow == root) return NULL;
		symtabnow = symtabnow -> parent;
		level--;
		}
}


SYMENTPTR Sym_LookUp(char *name)
{
	return Sym_FindSymbol(rootsymtab,name);
}

SYMENTPTR Sym_LookUpField(SYMENTPTR record, char *fldname)
{
	return Sym_FindSymbol(record->aux.fieldtable,fldname);
}


int withsearchlevel;

SYMENTPTR Sym_LookUpVar(char *name)
{
	SYMENTPTR rtn=NULL;
	if (withlevel)
		{
		withsearchlevel = withlevel;
		while (rtn==NULL && withsearchlevel>0)
			rtn = Sym_FindSymbol(Withs[--withsearchlevel].fieldtable,name);
		}
	if (rtn==NULL)
		{
		withsearchlevel=(-1);
		rtn = Sym_LookUp(name);
		}
	return rtn;
}

char *Sym_CName(char *name)
{
	if (Sym_LookUp(name))
		return symbol->cname;
	else return name;
}



SYMENTPTR Sym_Insert(char *name, unsigned short type)
{
	char tmpname[64];
	int must_prefix=FALSE;
	int len = strlen(name);
	if (name==NULL || *name==0) return symbol=NULL;
	strncpy(tmpname,name,63);
	strupr(tmpname);
	if (Sym_LookUp(name))
		{
		SYMENTPTR tmp;
		fprintf(errfile,"Warning: redefinition of %s on line %d\n",name,linenum);
		if (level==maxlevel)
			{
			fprintf(errfile,"Warning : redeclaration of %s on line %d\n",name,linenum);
			symbol->type = type;
			return tmp;
			}
		else if ((mode & IN_RECORD)==0) must_prefix=TRUE;
		}
	symtabnow=rootsymtab;
	if ((type & TYP_FUNCTION)==0 && (type & TYP_PROCEDURE)!=type)
		while (symtabnow->child) symtabnow=symtabnow->child;
	symbol = symtabnow->first;
	if (symbol)
		{
		while (symbol->next)
			symbol = symbol->next;
		symbol->next = Malloc("Sym_Insert",SYMENTSIZE);
		symbol = symbol->next;
		}
	else	symbol = symtabnow->first = Malloc("Sym_Insert",SYMENTSIZE);
	symbol->next = NULL;
	symbol->len = (char)len;
	symbol->name = Malloc("Sym_Insert",strlen(name)+1);
	strcpy(symbol->name, tmpname);
	if (must_prefix)
		{
		symbol->cname = Malloc("Sym_Insert",strlen(name)+7);
		sprintf(symbol->cname,"u%04d_%s",UniqSymID++,tmpname);
		}
	else	{
		symbol->cname= Malloc("Sym_Insert",strlen(name)+1);
		strcpy(symbol->cname, tmpname);
		}
	if (type&TYP_CONSTANT)	strupr(symbol->cname);
	else strlwr(symbol->cname);
	symbol->type = type;
	return symbol;
}

SYMENTPTR Sym_InsertOrdinal(char *name, unsigned short typ, int Min, int Max)
{
	if (Sym_Insert(name,typ))
		{
		symbol->aux.o.min = Min;
		symbol->aux.o.max = Max;
		}
	return symbol;
}

SYMENTPTR Sym_InsertConst(char *name, unsigned short type, int val)
{
	if (Sym_Insert(name,type))
		symbol->aux.constval = val;
	return symbol;
}

SYMENTPTR Sym_InsertArray(char *name,unsigned short type, int Min[], int Max[], int dims)
{
	if (Sym_Insert(name,type))
		{
		symbol->aux.indexptr = Malloc("Sym_InsertArray",sizeof(struct indexes));
		symbol->aux.indexptr->dims = dims;
		if (dims>10) fprintf(stderr,"Array %s exceeds ten dimensions!",name);
		while (dims--)
			{
			symbol->aux.indexptr->min[dims]=Min[dims];
			symbol->aux.indexptr->max[dims]=Max[dims];
			}
		}
	return symbol;
}


void Sym_EnterRecordLevel(char *name)
{
	Sym_EnterBlock(NULL);
}

SYMENTPTR Sym_MarkAsField(char *name, int is_generated)
{
	if (Sym_LookUp(name)) symbol->type |= TYP_FIELD;
	else fprintf(stderr,"No stab entry for field: %s\n",name);
	return symbol;
}

void Sym_ExitRecordLevel(char *name, int is_generated)
{
	SYMTABPTR fldlist;
	fldlist = rootsymtab;
	while (fldlist->child) fldlist = fldlist->child;
	Sym_LookUp(name);
	if (symtabnow->child != fldlist)
		fprintf(errfile,"More than one link level above field list (%s)\n",name);
	if ((symbol->type & TYP_RECORD) != TYP_RECORD)
		fprintf(errfile,"Warning - linking field list into non-record (%s)!!\n",name);
	if (is_generated)
		{
		SYMENTPTR sym;
		int len;
		char *ctmp;
		char *prefix, *Case, *Cprefix;
		sym = symbol;
		Case = prefix = name;
		while (*Case != '*') Case++;
		*Case = '\0'; /* terminate prefix */
		Case++; /* Point at case */
		len = 2*strlen(prefix)+strlen(Case)+4;
		Cprefix = Malloc("Sym_ExitRecordLevel",len);
		strcpy(Cprefix,prefix);
		strcat(Cprefix,".");
		strcat(Cprefix,prefix);
		strcat(Cprefix,"_");
		strcat(Cprefix,Case);
		strcat(Cprefix,".");
		while (sym->next) sym = sym->next; /* Move to last entry */
		sym->next = fldlist->first;
		sym = sym->next;
		while (sym) /* Loop through each field entry */
			{
			ctmp = Malloc("Sym_exitRecordLevel",strlen(sym->cname)+len);
			strcpy(ctmp,Cprefix);
			strcat(ctmp,sym->cname);
			_Free("Sym_ExitRecordLevel",sym->cname);
			sym->cname = ctmp;
			sym = sym->next;
			}
		_Free("Sym_ExitRecordLevel",Cprefix);
		fldlist->parent->child = NULL;
		_Free("Sym_ExitRecordLevel",fldlist);
		}
	else	{
		symbol->aux.fieldtable = fldlist;
		fldlist->parent->child = NULL;
		}
}

void Sym_SetUpElt(char *name, char *cname, unsigned short type)
{
	Sym_Insert(name,type);
	_Free("Sym_SetUpElt",symbol->cname);
	symbol->cname = Malloc("Sym_SetUpElt",strlen(cname)+1);
	strcpy(symbol->cname,cname);
}

void Sym_InitTable(void)
{
/***	printf("#define odd(x)		(x % 2)\n");
	printf("#define pred(x)		(x-1)\n");
	printf("#define succ(x)		(x+1)\n");
	printf("#define MK_FP(s,o)	(s<<16+o)\n");
	printf("#define round(x)	((int)(x+.5))\n");
	printf("#define swap(x)		(((x&0xFF)<<8) + ((x&0xFF00)>>8))\n");
	printf("typedef unsigned char BOOLEAN;\n");
	printf("#define TRUE		1\n");
	printf("#define FALSE		0\n");
***/

	Sym_Insert("abs",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("chr","(char)",TYP_FUNCTION|TYP_BYTE);
	Sym_SetUpElt("hi","0xFF00 & ",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("length","strlen",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("lo","0xFF & ",TYP_FUNCTION|TYP_SHORT);
	Sym_Insert("odd",TYP_FUNCTION|TYP_BOOLEAN);
	Sym_SetUpElt("ord","(int)",TYP_FUNCTION|TYP_SHORT);
	Sym_Insert("pred",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("ptr","MK_FP",TYP_FUNCTION|TYP_LONG);
	Sym_Insert("round",TYP_FUNCTION|TYP_SHORT);
	Sym_Insert("sizeof",TYP_FUNCTION|TYP_SHORT);
	Sym_Insert("succ",TYP_FUNCTION|TYP_SHORT);
	Sym_Insert("swap",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("trunc","(int)",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("boolean","BOOLEAN",TYP_TYPEDEF|TYP_BOOLEAN);
	Sym_SetUpElt("byte","unsigned char",TYP_TYPEDEF|TYP_BYTE);
	Sym_Insert("char",TYP_TYPEDEF|TYP_BYTE);
	Sym_SetUpElt("comp","double",TYP_TYPEDEF|TYP_DOUBLE);
	Sym_Insert("double",TYP_TYPEDEF|TYP_DOUBLE);
	Sym_SetUpElt("extended","double",TYP_TYPEDEF|TYP_DOUBLE);
	Sym_SetUpElt("integer","int",TYP_TYPEDEF|TYP_SHORT);
	Sym_SetUpElt("longint","long",TYP_TYPEDEF|TYP_LONG);
	Sym_SetUpElt("real","float",TYP_TYPEDEF|TYP_FLOAT);
	Sym_SetUpElt("shortint","char",TYP_TYPEDEF|TYP_BYTE);
	Sym_SetUpElt("single","float",TYP_TYPEDEF|TYP_FLOAT);
	Sym_SetUpElt("word","unsigned short",TYP_TYPEDEF|TYP_SHORT|TYP_UNSIGNED);
	Sym_SetUpElt("true","TRUE",TYP_CONSTANT|TYP_BOOLEAN);
	Sym_SetUpElt("false","FALSE",TYP_CONSTANT|TYP_BOOLEAN);
	Sym_SetUpElt("write","Write",TYP_PROCEDURE);
	Sym_SetUpElt("writeln","WriteLn",TYP_PROCEDURE);
	Sym_SetUpElt("read","Read",TYP_PROCEDURE);
	Sym_SetUpElt("readln","ReadLn",TYP_PROCEDURE);
	Sym_SetUpElt("assign","FOpen",TYP_PROCEDURE);
	Sym_Insert("chdir",TYP_PROCEDURE);
	Sym_SetUpElt("close","fclose",TYP_PROCEDURE);
	Sym_SetUpElt("erase","UnLink",TYP_PROCEDURE);
	Sym_SetUpElt("getdir","getcurdir",TYP_PROCEDURE);
	Sym_Insert("mkdir",TYP_PROCEDURE);
	Sym_SetUpElt("rename","ReName",TYP_PROCEDURE);
	Sym_SetUpElt("reset","ReWind",TYP_PROCEDURE);
	Sym_SetUpElt("rewrite","Creat",TYP_PROCEDURE);
	Sym_Insert("rmdir",TYP_PROCEDURE);
	Sym_SetUpElt("append","FOpenAppend",TYP_PROCEDURE);
	Sym_SetUpElt("flush","fflush",TYP_PROCEDURE);
	Sym_SetUpElt("settextbuf","SetBuf",TYP_PROCEDURE);
	Sym_SetUpElt("blockread","BlockRead",TYP_PROCEDURE);
	Sym_SetUpElt("blockwrite","BlockWrite",TYP_PROCEDURE);
	Sym_SetUpElt("seek","FSeek",TYP_PROCEDURE);
	Sym_SetUpElt("truncate","Trunc",TYP_PROCEDURE);
	Sym_SetUpElt("exit","Exit",TYP_PROCEDURE);
	Sym_SetUpElt("halt","Halt",TYP_PROCEDURE);
	Sym_SetUpElt("runerror","RunError",TYP_PROCEDURE);
	Sym_SetUpElt("dispose","Dispose",TYP_PROCEDURE);
	Sym_SetUpElt("freemem","FreeMem",TYP_PROCEDURE);
	Sym_SetUpElt("getmem","GetMem",TYP_PROCEDURE);
	Sym_SetUpElt("mark","Mark",TYP_PROCEDURE);
	Sym_SetUpElt("new","New",TYP_PROCEDURE);
	Sym_SetUpElt("release","Release",TYP_PROCEDURE);
	Sym_SetUpElt("dec","Dec",TYP_PROCEDURE);
	Sym_SetUpElt("inc","Inc",TYP_PROCEDURE);
	Sym_SetUpElt("delete","StrDel",TYP_PROCEDURE);
	Sym_SetUpElt("insert","StrIns",TYP_PROCEDURE);
	Sym_SetUpElt("str","ItoA",TYP_PROCEDURE);
	Sym_SetUpElt("val","AtoI",TYP_PROCEDURE);
	Sym_SetUpElt("fillchar","MemSet",TYP_PROCEDURE);
	Sym_SetUpElt("move","MemCpy",TYP_PROCEDURE);
	Sym_Insert("randomize",TYP_PROCEDURE);
	Sym_SetUpElt("eof","feof",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("ioresult","FError",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("eoln","Eoln",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("seekeof","SeekEOF",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("seekeoln","SeekEoln",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("filepos","FTell",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("filesize","FileSize",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("maxavail","MaxAvail",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("memavail","MemAvail",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("arctan","atan",TYP_FUNCTION|TYP_SHORT);
	Sym_Insert("cos",TYP_FUNCTION|TYP_SHORT);
	Sym_Insert("exp",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("frac","Frac",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("int","Int",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("ln","log",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("pi","PI",TYP_FUNCTION|TYP_SHORT);
	Sym_Insert("sin",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("sqr","Pow2",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("sqrt","sqrt",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("concat","StrCat",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("copy","StrCpy",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("pos","StrPos",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("addr","Addr",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("cseg","_CS",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("dseg","_DS",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("ofs","OffSet",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("seg","Segment",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("sptr","_SP",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("sseg","_SS",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("paramcount","argc",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("paramstr","argv",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("random","random",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("upcase","toupper",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("input","stdin",TYP_FILE);
	Sym_SetUpElt("output","stdout",TYP_FILE);
	Sym_SetUpElt("text","FILE *",TYP_FILE|TYP_TYPEDEF);

	/* DOS Unit */
		/* Flag constants */
	Sym_InsertConst("FCarry",TYP_SHORT,0x0001);
	Sym_InsertConst("FParity",TYP_SHORT,0x0004);
	Sym_InsertConst("FAuxiliary",TYP_SHORT,0x0010);
	Sym_InsertConst("FZero",TYP_SHORT,0x0040);
	Sym_InsertConst("FSign",TYP_SHORT,0x0080);
	Sym_InsertConst("FOverflow",TYP_SHORT,0x0800);
		/* File mode constants */
	Sym_InsertConst("fmClosed",TYP_SHORT,0xD7B0);
	Sym_InsertConst("fmInput",TYP_SHORT,0xD7B1);
	Sym_InsertConst("fmOutput",TYP_SHORT,0xD7B2);
	Sym_InsertConst("fmInOut",TYP_SHORT,0xD7B3);
		/* File Attributes */
	Sym_InsertConst("ReadOnly",TYP_SHORT,0x1);
	Sym_InsertConst("Hidden",TYP_SHORT,0x2);
	Sym_InsertConst("SysFile",TYP_SHORT,0x4);
	Sym_InsertConst("VolumeID",TYP_SHORT,0x8);
	Sym_InsertConst("Directory",TYP_SHORT,0x10);
	Sym_InsertConst("Archive",TYP_SHORT,0x20);
	Sym_InsertConst("AnyFile",TYP_SHORT,0x3F);

	Sym_SetUpElt("doserror","_doserrno",TYP_SHORT);
	Sym_SetUpElt("getintvec","GetVect",TYP_PROCEDURE);
	Sym_SetUpElt("intr","intr",TYP_PROCEDURE);
	Sym_SetUpElt("msdos","DosInt",TYP_PROCEDURE);
	Sym_SetUpElt("setintvec","SetVect",TYP_PROCEDURE);
	Sym_SetUpElt("getdate","GetDate",TYP_PROCEDURE);
	Sym_SetUpElt("getftime","GetFTime",TYP_PROCEDURE);
	Sym_SetUpElt("gettime","GetTime",TYP_PROCEDURE);
	Sym_SetUpElt("packtime","PackTime",TYP_PROCEDURE);
	Sym_SetUpElt("setdate","SetDate",TYP_PROCEDURE);
	Sym_SetUpElt("setftime","SetFTime",TYP_PROCEDURE);
	Sym_SetUpElt("settime","SetTime",TYP_PROCEDURE);
	Sym_SetUpElt("unpacktime","UnpackTime",TYP_PROCEDURE);
	Sym_SetUpElt("findfirst","FindFirst",TYP_PROCEDURE);
	Sym_SetUpElt("findnext","FindNext",TYP_PROCEDURE);
	Sym_SetUpElt("getfattr","GetFAttr",TYP_PROCEDURE);
	Sym_SetUpElt("setfattr","SetFAttr",TYP_PROCEDURE);
	Sym_SetUpElt("fsplit","FSplit",TYP_PROCEDURE);
	Sym_SetUpElt("execute","Exec",TYP_PROCEDURE);
	Sym_SetUpElt("keep","Keep",TYP_PROCEDURE);
	Sym_SetUpElt("realtowordword","Real_WordWord",TYP_PROCEDURE);
	Sym_SetUpElt("swapvectors","SwapVect",TYP_PROCEDURE);
	Sym_SetUpElt("setcbreak","SetCBreak",TYP_PROCEDURE);
	Sym_SetUpElt("getcbreak","GetCBreak",TYP_PROCEDURE);
	Sym_SetUpElt("setverify","SetVerify",TYP_PROCEDURE);
	Sym_SetUpElt("getverify","GetVerify",TYP_PROCEDURE);
	Sym_SetUpElt("wordwordtoreal","WordWord_Real",TYP_FUNCTION|TYP_FLOAT);

	Sym_SetUpElt("diskfree","DiskFree",TYP_FUNCTION|TYP_LONG);
	Sym_SetUpElt("disksize","DiskSize",TYP_FUNCTION|TYP_LONG);
	Sym_SetUpElt("fexpand","FExpand",TYP_FUNCTION|TYP_STRING);
	Sym_SetUpElt("fsearch","FSearch",TYP_FUNCTION|TYP_STRING);
	Sym_SetUpElt("dosexitcode","DosExitCode",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("envcount","EnvCount",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("envstr","EnvStr",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("getenv","GetEnv",TYP_FUNCTION|TYP_SHORT);
	Sym_SetUpElt("dosversion","_version",TYP_FUNCTION|TYP_SHORT);
}

static struct Sym_IDList
{
	char *name;
	struct Sym_IDList *next;
} *ListRoot, *ListNow;

#define LISTELTSIZE	sizeof(struct Sym_IDList)

void Sym_QueueID(char *name)
{
	if (ListRoot==NULL)
		{
		ListNow = ListRoot=Malloc("Sym_QueueID",LISTELTSIZE);
		}
	else	{
		ListNow=ListRoot;
		while (ListNow->next) ListNow=ListNow->next;
		ListNow->next = Malloc("Sym_QueueID",LISTELTSIZE);
		ListNow=ListNow->next;
		}
	ListNow->next = NULL;
	ListNow->name = Malloc("Sym_QueueID",strlen(name)+1);
	strcpy(ListNow->name,name);
}

static char ID_HoldBuff[80];

char *Sym_DeQueueID(void)
{
	struct Sym_IDList *tmp=ListRoot;
	if (ListRoot==NULL) return NULL;
	strcpy(ID_HoldBuff,ListRoot->name);
	_Free("Sym_DeQueueID",ListRoot->name);
	ListRoot = ListRoot->next;
	_Free("Sym_DeQueueID",tmp);
	return ID_HoldBuff;
}


