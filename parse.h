/* Parse.h */


/* Modes */

#define IN_RECORD	1
#define IN_ARRAY	2
#define IN_TYPEDEF	4
#define IN_FUNCTION	8
#define IS_CONSTANT	16
#define NO_ECHO		32
#define IN_FUNCTYPE	64

extern unsigned short mode;

#define	EnterMode(m)	mode |= m
#define ExitMode(m)	mode &= (0xFFFF^(m))
#define InMode(m)	(mode & (m))

extern FILE *errfile;
extern FILE *cfile;
extern FILE *hfile;
extern int withlevel;

struct WithScope
	{
	char *prefix;
	SYMENTPTR *fieldtable;
	};

extern struct WithScope Withs[];

/* Prototypes */

void *Malloc(char *fn,int nbytes);
void _Free(char *fn, void *p);
void Open_Files(char *basename);
void Close_Files(void);
void Indent(void);
void Dump_IDs(char *format, unsigned short type);
char *append(char *dest, int free_dest,char *source, int free_source);
void field_width(void);
void ident_list(void);
void actual_param(void);
void actual_param_list(void);
void program_heading(void);
void uses_clause(void);
void label(void);
void label_decl_part(void);
void typed_constant(void);
void const_decl_part(void);
unsigned short ordinal_type(char *typname);
unsigned short real_type(char *typname);
unsigned short string_type(char *typname);
unsigned short array_type(char *typname);
unsigned short pointer_type(char *typname);
unsigned short file_type(char *typname);
unsigned short set_type(char *typname);
void fixed_part(int is_generated);
void variant(char *uname);
void variant_part(void);
void field_list(char *prefix, char *Case, int is_generated);
unsigned short record_type(char *typname);
unsigned short type(char *typname);
void type_decl(void);
void type_decl_part(void);
void absolute_clause(void);
void variable_decl(void);
void variable_decl_part(void);
char *var_reference(void);
char *function_call(void);
char *factor(void);
char *term(void);
char *simple_expr(void);
char *expression(void);
long constant(void);
char *var_constant(void);
void with_stmt(void);
void if_stmt(void);
void case_stmt(void);
void repeat_stmt(void);
void while_stmt(void);
void for_stmt(void);
void goto_stmt(void);
void procedure_stmt(void);
void assignment_stmt(void);
void statement();
void compound_stmt(int is_function);
void inline_directive(char *heading);
char *parameter_decl(char *line);
char *formal_parameter_list(char *line);
char *procedure_heading(void);
void procedure_body(char *heading);
void procedure_decl(int is_interface);
char *function_heading(void);
void function_body(char *heading);
void function_decl(int is_interface);
void decl_part(void);
void block(char *name,int is_function);
void program(void);
void interface_part(void);
void implementation_part(void);
void initialization_part(char *unitname);
void unit(void);


