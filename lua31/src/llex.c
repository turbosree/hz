/*
** $Id: llex.c,v 1.1 2000/07/20 06:57:22 jeske Exp $
** Lexical Analizer
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <string.h>

#include "lauxlib.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "lstx.h"
#include "luadebug.h"
#include "lzio.h"



int lua_debug=0;


#define next(LS) (LS->current = zgetc(LS->lex_z))


static struct {
  char *name;
  int token;
} reserved [] = {
    {"and", AND}, {"do", DO}, {"else", ELSE}, {"elseif", ELSEIF},
    {"end", END}, {"function", FUNCTION}, {"if", IF}, {"local", LOCAL},
    {"nil", NIL}, {"not", NOT}, {"or", OR}, {"repeat", REPEAT},
    {"return", RETURN}, {"then", THEN}, {"until", UNTIL}, {"while", WHILE}
};

void luaX_init (void)
{
  int i;
  for (i=0; i<(sizeof(reserved)/sizeof(reserved[0])); i++) {
    TaggedString *ts = luaS_new(reserved[i].name);
    ts->head.marked = reserved[i].token;  /* reserved word  (always > 255) */
  }
}


static void firstline (LexState *LS)
{
  int c = zgetc(LS->lex_z);
  if (c == '#') {
    LS->linenumber++;
    while ((c=zgetc(LS->lex_z)) != '\n' && c != EOZ) /* skip first line */;
  }
  zungetc(LS->lex_z);
}


void luaX_setinput (ZIO *z)
{
  LexState *LS = L->lexstate;
  LS->current = '\n';
  LS->linelasttoken = 0;
  LS->linenumber = 0;
  LS->iflevel = 0;
  LS->ifstate[0].skip = 0;
  LS->ifstate[0].elsepart = 1;  /* to avoid a free $else */
  LS->lex_z = z;
  firstline(LS);
  luaL_resetbuffer();
}



/*
** =======================================================
** PRAGMAS
** =======================================================
*/

#define PRAGMASIZE	20

static void skipspace (LexState *LS)
{
  while (LS->current == ' ' || LS->current == '\t' || LS->current == '\r')
    next(LS);
}


static int checkcond (char *buff)
{
  static char *opts[] = {"nil", "1", NULL};
  int i = luaO_findstring(buff, opts);
  if (i >= 0) return i;
  else if (isalpha((unsigned char)buff[0]) || buff[0] == '_')
    return luaS_globaldefined(buff);
  else {
    luaY_syntaxerror("invalid $if condition", buff);
    return 0;  /* to avoid warnings */
  }
}


static void readname (LexState *LS, char *buff)
{
  int i = 0;
  skipspace(LS);
  while (isalnum(LS->current) || LS->current == '_') {
    if (i >= PRAGMASIZE) {
      buff[PRAGMASIZE] = 0;
      luaY_syntaxerror("pragma too long", buff);
    }
    buff[i++] = LS->current;
    next(LS);
  }
  buff[i] = 0;
}


static void inclinenumber (LexState *LS);


static void ifskip (LexState *LS)
{
  while (LS->ifstate[LS->iflevel].skip) {
    if (LS->current == '\n')
      inclinenumber(LS);
    else if (LS->current == EOZ)
      luaY_syntaxerror("input ends inside a $if", "");
    else next(LS);
  }
}


static void inclinenumber (LexState *LS)
{
  static char *pragmas [] =
    {"debug", "nodebug", "endinput", "end", "ifnot", "if", "else", NULL};
  next(LS);  /* skip '\n' */
  ++LS->linenumber;
  if (LS->current == '$') {  /* is a pragma? */
    char buff[PRAGMASIZE+1];
    int ifnot = 0;
    int skip = LS->ifstate[LS->iflevel].skip;
    next(LS);  /* skip $ */
    readname(LS, buff);
    switch (luaO_findstring(buff, pragmas)) {
      case 0:  /* debug */
        if (!skip) lua_debug = 1;
        break;
      case 1:  /* nodebug */
        if (!skip) lua_debug = 0;
        break;
      case 2:  /* endinput */
        if (!skip) {
          LS->current = EOZ;
          LS->iflevel = 0;  /* to allow $endinput inside a $if */
        }
        break;
      case 3:  /* end */
        if (LS->iflevel-- == 0)
          luaY_syntaxerror("unmatched $end", "$end");
        break;
      case 4:  /* ifnot */
        ifnot = 1;
        /* go through */
      case 5:  /* if */
        if (LS->iflevel == MAX_IFS-1)
          luaY_syntaxerror("too many nested $ifs", "$if");
        readname(LS, buff);
        LS->iflevel++;
        LS->ifstate[LS->iflevel].elsepart = 0;
        LS->ifstate[LS->iflevel].condition = checkcond(buff) ? !ifnot : ifnot;
        LS->ifstate[LS->iflevel].skip = skip || !LS->ifstate[LS->iflevel].condition;
        break;
      case 6:  /* else */
        if (LS->ifstate[LS->iflevel].elsepart)
          luaY_syntaxerror("unmatched $else", "$else");
        LS->ifstate[LS->iflevel].elsepart = 1;
        LS->ifstate[LS->iflevel].skip = LS->ifstate[LS->iflevel-1].skip ||
                                      LS->ifstate[LS->iflevel].condition;
        break;
      default:
        luaY_syntaxerror("unknown pragma", buff);
    }
    skipspace(LS);
    if (LS->current == '\n')  /* pragma must end with a '\n' ... */
      inclinenumber(LS);
    else if (LS->current != EOZ)  /* or eof */
      luaY_syntaxerror("invalid pragma format", buff);
    ifskip(LS);
  }
}


/*
** =======================================================
** LEXICAL ANALIZER
** =======================================================
*/



#define save(c)	luaL_addchar(c)
#define save_and_next(LS)  (save(LS->current), next(LS))


char *luaX_lasttoken (void)
{
  save(0);
  return luaL_buffer();
}


static int read_long_string (LexState *LS, YYSTYPE *l)
{
  int cont = 0;
  while (1) {
    switch (LS->current) {
      case EOZ:
        save(0);
        return WRONGTOKEN;
      case '[':
        save_and_next(LS);
        if (LS->current == '[') {
          cont++;
          save_and_next(LS);
        }
        continue;
      case ']':
        save_and_next(LS);
        if (LS->current == ']') {
          if (cont == 0) goto endloop;
          cont--;
          save_and_next(LS);
        }
        continue;
      case '\n':
        save('\n');
        inclinenumber(LS);
        continue;
      default:
        save_and_next(LS);
    }
  } endloop:
  save_and_next(LS);  /* pass the second ']' */
  L->Mbuffer[L->Mbuffnext-2] = 0;  /* erases ']]' */
  l->pTStr = luaS_new(L->Mbuffbase+2);
  L->Mbuffer[L->Mbuffnext-2] = ']';  /* restores ']]' */
  return STRING;
}


/* to avoid warnings; this declaration cannot be public since YYSTYPE
** cannot be visible in llex.h (otherwise there is an error, since
** the parser body redefines it!)
*/
int luaY_lex (YYSTYPE *l);
int luaY_lex (YYSTYPE *l)
{
  LexState *LS = L->lexstate;
  double a;
  luaL_resetbuffer();
  if (lua_debug)
    luaY_codedebugline(LS->linelasttoken);
  LS->linelasttoken = LS->linenumber;
  while (1) {
    switch (LS->current) {

      case ' ': case '\t': case '\r':  /* CR: to avoid problems with DOS */
        next(LS);
        continue;

      case '\n':
        inclinenumber(LS);
        LS->linelasttoken = LS->linenumber;
        continue;

      case '-':
        save_and_next(LS);
        if (LS->current != '-') return '-';
        do { next(LS); } while (LS->current != '\n' && LS->current != EOZ);
        luaL_resetbuffer();
        continue;

      case '[':
        save_and_next(LS);
        if (LS->current != '[') return '[';
        else {
          save_and_next(LS);  /* pass the second '[' */
          return read_long_string(LS, l);
        }

      case '=':
        save_and_next(LS);
        if (LS->current != '=') return '=';
        else { save_and_next(LS); return EQ; }

      case '<':
        save_and_next(LS);
        if (LS->current != '=') return '<';
        else { save_and_next(LS); return LE; }

      case '>':
        save_and_next(LS);
        if (LS->current != '=') return '>';
        else { save_and_next(LS); return GE; }

      case '~':
        save_and_next(LS);
        if (LS->current != '=') return '~';
        else { save_and_next(LS); return NE; }

      case '"':
      case '\'': {
        int del = LS->current;
        save_and_next(LS);
        while (LS->current != del) {
          switch (LS->current) {
            case EOZ:
            case '\n':
              save(0);
              return WRONGTOKEN;
            case '\\':
              next(LS);  /* do not save the '\' */
              switch (LS->current) {
                case 'n': save('\n'); next(LS); break;
                case 't': save('\t'); next(LS); break;
                case 'r': save('\r'); next(LS); break;
                case '\n': save('\n'); inclinenumber(LS); break;
                default : save_and_next(LS); break;
              }
              break;
            default:
              save_and_next(LS);
          }
        }
        next(LS);  /* skip delimiter */
        save(0);
        l->pTStr = luaS_new(L->Mbuffbase+1);
        L->Mbuffer[L->Mbuffnext-1] = del;  /* restore delimiter */
        return STRING;
      }

      case '.':
        save_and_next(LS);
        if (LS->current == '.')
        {
          save_and_next(LS);
          if (LS->current == '.')
          {
            save_and_next(LS);
            return DOTS;   /* ... */
          }
          else return CONC;   /* .. */
        }
        else if (!isdigit(LS->current)) return '.';
        /* LS->current is a digit: goes through to number */
	a=0.0;
        goto fraction;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	a=0.0;
        do {
          a=10.0*a+(LS->current-'0');
          save_and_next(LS);
        } while (isdigit(LS->current));
        if (LS->current == '.') {
          save_and_next(LS);
          if (LS->current == '.') {
            save(0);
            luaY_error(
              "ambiguous syntax (decimal point x string concatenation)");
          }
        }
      fraction:
	{ double da=0.1;
	  while (isdigit(LS->current))
	  {
            a+=(LS->current-'0')*da;
            da/=10.0;
            save_and_next(LS);
          }
          if (toupper(LS->current) == 'E') {
	    int e=0;
	    int neg;
	    double ea;
            save_and_next(LS);
	    neg=(LS->current=='-');
            if (LS->current == '+' || LS->current == '-') save_and_next(LS);
            if (!isdigit(LS->current)) {
              save(0); return WRONGTOKEN; }
            do {
              e=10.0*e+(LS->current-'0');
              save_and_next(LS);
            } while (isdigit(LS->current));
	    for (ea=neg?0.1:10.0; e>0; e>>=1)
	    {
	      if (e & 1) a*=ea;
	      ea*=ea;
	    }
          }
          l->vReal = a;
          return NUMBER;
        }

      case EOZ:
        save(0);
        if (LS->iflevel > 0)
          luaY_syntaxerror("input ends inside a $if", "");
        return 0;

      default:
        if (LS->current != '_' && !isalpha(LS->current)) {
          int c = LS->current;
          save_and_next(LS);
          return c;
        }
        else {  /* identifier or reserved word */
          TaggedString *ts;
          do {
            save_and_next(LS);
          } while (isalnum(LS->current) || LS->current == '_');
          save(0);
          ts = luaS_new(L->Mbuffbase);
          if (ts->head.marked > 255)
            return ts->head.marked;  /* reserved word */
          l->pTStr = ts;
          return NAME;
        }
    }
  }
}

