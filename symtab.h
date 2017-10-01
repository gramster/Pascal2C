/* SYMTAB.H */

/*********/
/* Types */
/*********/

#define TYP_UNIT	1
#define TYP_PROCEDURE	2
#define TYP_LABEL	4
#define TYP_FILE	5
#define TYP_STRING	6
#define TYP_BYTE	7
#define TYP_SHORT	8
#define TYP_LONG	9
#define TYP_FLOAT 	10
#define TYP_DOUBLE	11
#define TYP_BOOLEAN	12
#define TYP_ENUM	13
#define TYP_SUBRANGE	14
#define TYP_RECORD	15
#define TYP_SET		16

/******************/
/* Type Modifiers */
/******************/

#define TYP_TYPEDEF	32
#define TYP_NOTYPE	(0xFFFF ^ TYP_TYPEDEF) /* Mask to clear typedef field*/
#define TYP_UNSIGNED	64
#define TYP_ARRAY	128
#define TYP_POINTER	256
#define TYP_CONSTANT	512
#define TYP_FUNCTION	1024
#define TYP_NOFUNCTION	(0xFFFF ^ TYP_FUNCTION)
#define TYP_FIELD	2048
#define TYP_SHAREFIELDS	4096	/* Shares a field symbol table */
#define TYP_GENERATED	8192

/**********************/
/* Symbol table Entry */
/**********************/

struct indexes
{
	int dims;
	int min[10],max[10];
};

struct syment
{
	char *name;		/* pointer to string table	*/
	char len;		/* length of name		*/
	char *cname;		/* C equivalent			*/
	unsigned short type;	/* Type & modifier		*/
	union 	{
		struct ordinal
			{
			int min,max;
			} o;
		struct indexes *indexptr;	/* array indices	*/
		int constval;			/* constant value	*/
		struct syment *fieldtable;	/* record fields	*/
		struct syment *basetype;	/* pointer base type	*/
		} aux;
	struct syment *next;	/* linked list */
};


/****************/
/* Symbol Table */
/****************/

struct symtab
{
	struct syment *first;
	char mustfree;   /* Must be freed on exit from block; false for
				record field symbol tables that are linked
				in during a 'WITH' statement. */
	struct symtab *child, *parent;
};

/******************************/
/* Type definitions and sizes */
/******************************/

typedef struct syment *SYMENTPTR;
typedef struct symtab *SYMTABPTR;

#define SYMTABSIZE	sizeof(struct symtab)
#define SYMENTSIZE	sizeof(struct syment)

extern SYMENTPTR symbol;

/**************/
/* Prototypes */
/**************/

void Sym_EnterBlock(SYMENTPTR record);
void Sym_FreeTables(SYMTABPTR from);
char *Sym_TypName(unsigned short typ);
void Sym_ShowTable(SYMTABPTR root);
void Sym_DumpStab(void);
void Sym_ExitBlock(SYMENTPTR record);

SYMENTPTR Sym_FindSymbol(SYMTABPTR root,char *name);
SYMENTPTR Sym_LookUp(char *name);
SYMENTPTR Sym_LookUpField(SYMENTPTR record, char *fldname);
SYMENTPTR Sym_LookUpVar(char *name);

SYMENTPTR Sym_Insert(char *name, unsigned short type);
SYMENTPTR Sym_InsertConst(char *name, unsigned short type, int val);
SYMENTPTR Sym_InsertOrdinal(char *name,unsigned short type, int min, int max);
SYMENTPTR Sym_InsertArray(char *name,unsigned short type, int Min[], int Max[], int dims);

void Sym_EnterRecordLevel(char *name);
SYMENTPTR Sym_MarkAsField(char *name,int is_generated);
void Sym_ExitRecordLevel(char *name,int is_generated);

void Sym_SetUpElt(char *name, char *cname, unsigned short type);
void Sym_InitTable(void);

void Sym_QueueID(char *name);
char *Sym_DeQueueID(void);

char *Sym_CName(char *name);

