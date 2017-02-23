/* messageobj.c -- Provides an external API for message reporting; 
                   Abstract internals

  (c) 2017 HTACG
  See tidy.h for the copyright notice.

*/

#include "messageobj.h"
#include "message.h"
#include "tidy-int.h"
#if !defined(NDEBUG) && defined(_MSC_VER)
#include "sprtf.h"
#endif


/*********************************************************************
 * BuildArgArray Support - forward declarations
 *********************************************************************/


/** A record of a single argument and its type. An array these
**  represents the arguments supplied to a format string, ordered
**  in the same position as they occur in the format string. Because
**  Windows doesn't support modern positional arguments, Tidy doesn't
**  either.
*/
struct printfArg;


/** Returns a pointer to an allocated array of `printfArg` given a format
 ** string and a va_list, or NULL if not successful or no parameters were
 ** given. Parameter `rv` will return with the count of zero or more
 ** parameters if successful, else -1.
 **
 */
static struct printfArg *BuildArgArray( TidyDocImpl *doc, ctmbstr fmt, va_list ap, int *rv );


/*********************************************************************
 * Tidy Message Object Support
 *********************************************************************/


/** Create an internal representation of a Tidy message with all of
 **  the information that that we know about the message.
 **
 **  The function signature doesn't have to stay static and is a good
 **  place to add instantiation if expanding the API.
 **
 **  We currently know the doc, node, code, line, column, level, and
 **  args, will pre-calculate all of the other members upon creation.
 **  This ensures that we can use members directly, immediately,
 **  without having to use accessors internally.
 **
 **  If any message callback filters are setup by API clients, they 
 **  will be called here.
 **
 **  This version serves as the designated initializer and as such
 **  requires every known parameter.
 */
static TidyMessageImpl *tidyMessageCreateInitV( TidyDocImpl *doc,
                                               Node *node,
                                               uint code,
                                               int line,
                                               int column,
                                               TidyReportLevel level,
                                               va_list args )
{
    TidyMessageImpl *result = TidyDocAlloc(doc, sizeof(TidyMessageImpl));
    TidyDoc tdoc = tidyImplToDoc(doc);
    va_list args_copy;
    enum { sizeMessageBuf=2048 };
    ctmbstr pattern;

    /* Things we know... */

    result->tidyDoc = doc;
    result->tidyNode = node;
    result->code = code;
    result->line = line;
    result->column = column;
    result->level = level;

    /* Things we create... */

    va_copy(args_copy, args);
    result->arguments = BuildArgArray(doc, tidyDefaultString(code), args_copy, &result->argcount);
    va_end(args_copy);

    result->messageKey = TY_(tidyErrorCodeAsKey)(code);

    result->messageFormatDefault = tidyDefaultString(code);
    result->messageFormat = tidyLocalizedString(code);

    result->messageDefault = TidyDocAlloc(doc, sizeMessageBuf);
    va_copy(args_copy, args);
    TY_(tmbvsnprintf)(result->messageDefault, sizeMessageBuf, result->messageFormatDefault, args_copy);
    va_end(args_copy);

    result->message = TidyDocAlloc(doc, sizeMessageBuf);
    va_copy(args_copy, args);
    TY_(tmbvsnprintf)(result->message, sizeMessageBuf, result->messageFormat, args_copy);
    va_end(args_copy);

    result->messagePosDefault = TidyDocAlloc(doc, sizeMessageBuf);
    result->messagePos = TidyDocAlloc(doc, sizeMessageBuf);

    if ( cfgBool(doc, TidyEmacs) && cfgStr(doc, TidyEmacsFile) )
    {
        /* Change formatting to be parsable by GNU Emacs */
        TY_(tmbsnprintf)(result->messagePosDefault, sizeMessageBuf, "%s:%d:%d: ", cfgStr(doc, TidyEmacsFile), line, column);
        TY_(tmbsnprintf)(result->messagePos, sizeMessageBuf, "%s:%d:%d: ", cfgStr(doc, TidyEmacsFile), line, column);
    }
    else
    {
        /* traditional format */
        TY_(tmbsnprintf)(result->messagePosDefault, sizeMessageBuf, tidyDefaultString(LINE_COLUMN_STRING), line, column);
        TY_(tmbsnprintf)(result->messagePos, sizeMessageBuf, tidyLocalizedString(LINE_COLUMN_STRING), line, column);
    }

    result->messagePrefixDefault = tidyDefaultString(level);

    result->messagePrefix = tidyLocalizedString(level);

    if ( line > 0 && column > 0 )
        pattern = "%s%s%s";
    else
        pattern = "%.0s%s%s";

    result->messageOutputDefault = TidyDocAlloc(doc, sizeMessageBuf);
    TY_(tmbsnprintf)(result->messageOutputDefault, sizeMessageBuf, pattern,
                     result->messagePosDefault, result->messagePrefixDefault,
                     result->messageDefault);

    result->messageOutput = TidyDocAlloc(doc, sizeMessageBuf);
    TY_(tmbsnprintf)(result->messageOutput, sizeMessageBuf, pattern,
                     result->messagePos, result->messagePrefix,
                     result->message);


    result->allowMessage = yes;

    /* mssgFilt is a simple error filter that provides minimal information
       to callback functions, and includes the message buffer in LibTidy's
       configured localization.
     */
    if ( doc->mssgFilt )
    {
        result->allowMessage = result->allowMessage & doc->mssgFilt( tdoc, result->level, result->line, result->column, result->messageOutput );
    }

    /* mssgCallback is intended to allow LibTidy users to localize messages
       via their own means by providing a key and the parameters to fill it. */
    if ( doc->mssgCallback )
    {
        TidyDoc tdoc = tidyImplToDoc( doc );
        va_copy(args_copy, args);
        result->allowMessage = result->allowMessage & doc->mssgCallback( tdoc, result->level, result->line, result->column, result->messageKey, args_copy );
        va_end(args_copy);
    }

    /* mssgMessageCallback is the newest interface to interrogate Tidy's
       emitted messages. */
    if ( doc->mssgMessageCallback )
    {
        result->allowMessage = result->allowMessage & doc->mssgMessageCallback( tidyImplToMessage(result) );
    }

    return result;
}


/** Creates a TidyMessageImpl, but without line numbers, such as used for
 ** information report output.
 */
/* TEMPORARY TRANSITION FUNCTION */
TidyMessageImpl *TY_(tidyMessageCreateV)( TidyDocImpl *doc,
                                          uint code,
                                          TidyReportLevel level,
                                          va_list args )
{
    TidyMessageImpl *result;
    va_list args_copy;

    va_copy(args_copy, args);
    result = tidyMessageCreateInitV(doc, NULL, code, 0, 0, level, args_copy);
    va_end(args_copy);

    return result;
}

TidyMessageImpl *TY_(tidyMessageCreate)( TidyDocImpl *doc,
                                         uint code,
                                         TidyReportLevel level,
                                         ... )
{
    TidyMessageImpl *result;
    va_list args;
    va_start(args, level);
    result = tidyMessageCreateInitV(doc, NULL, code, 0, 0, level, args);
    va_end(args);
    
    return result;
}

/** Creates a TidyMessageImpl, using the line and column from the provided
 ** Node as the message position source.
 */
/* TEMPORARY TRANSITION FUNCTION */
TidyMessageImpl *TY_(tidyMessageCreateWithNodeV)( TidyDocImpl *doc,
                                                  Node *node,
                                                  uint code,
                                                  TidyReportLevel level,
                                                  va_list args )
{
    TidyMessageImpl *result;
    va_list args_copy;
    int line = ( node ? node->line :
                ( doc->lexer ? doc->lexer->lines : 0 ) );
    int col  = ( node ? node->column :
                ( doc->lexer ? doc->lexer->columns : 0 ) );

    va_copy(args_copy, args);
    result = tidyMessageCreateInitV(doc, node, code, line, col, level, args_copy);
    va_end(args_copy);

    return result;
}

TidyMessageImpl *TY_(tidyMessageCreateWithNode)( TidyDocImpl *doc,
                                                 Node *node,
                                                 uint code,
                                                 TidyReportLevel level,
                                                 ... )
{
    TidyMessageImpl *result;
    va_list args_copy;
    int line = ( node ? node->line :
                ( doc->lexer ? doc->lexer->lines : 0 ) );
    int col  = ( node ? node->column :
                ( doc->lexer ? doc->lexer->columns : 0 ) );
    
    va_start(args_copy, level);
    result = tidyMessageCreateInitV(doc, node, code, line, col, level, args_copy);
    va_end(args_copy);
    
    return result;
}


/** Creates a TidyMessageImpl, using the line and column from the provided
 ** document's Lexer as the message position source.
 */
/* TEMPORARY TRANSITION FUNCTION */
TidyMessageImpl *TY_(tidyMessageCreateWithLexerV)( TidyDocImpl *doc,
                                                   uint code,
                                                   TidyReportLevel level,
                                                   va_list args )
{
    TidyMessageImpl *result;
    va_list args_copy;
    int line = ( doc->lexer ? doc->lexer->lines : 0 );
    int col  = ( doc->lexer ? doc->lexer->columns : 0 );

    va_copy(args_copy, args);
    result = tidyMessageCreateInitV(doc, NULL, code, line, col, level, args_copy);
    va_end(args_copy);

    return result;
}

TidyMessageImpl *TY_(tidyMessageCreateWithLexer)( TidyDocImpl *doc,
                                                  uint code,
                                                  TidyReportLevel level,
                                                  ... )
{
    TidyMessageImpl *result;
    va_list args_copy;
    int line = ( doc->lexer ? doc->lexer->lines : 0 );
    int col  = ( doc->lexer ? doc->lexer->columns : 0 );
    
    va_start(args_copy, level);
    result = tidyMessageCreateInitV(doc, NULL, code, line, col, level, args_copy);
    va_end(args_copy);
    
    return result;
}


/** Because instances of TidyMessage retain memory, they must be released
 **  when we're done with them.
 */
void TY_(tidyMessageRelease)( TidyMessageImpl *message )
{
    TidyDocFree( tidyDocToImpl(message->tidyDoc), message->arguments );
    TidyDocFree( tidyDocToImpl(message->tidyDoc), message->messageDefault );
    TidyDocFree( tidyDocToImpl(message->tidyDoc), message->message );
    TidyDocFree( tidyDocToImpl(message->tidyDoc), message->messagePosDefault );
    TidyDocFree( tidyDocToImpl(message->tidyDoc), message->messagePos );
    TidyDocFree( tidyDocToImpl(message->tidyDoc), message->messageOutputDefault );
    TidyDocFree( tidyDocToImpl(message->tidyDoc), message->messageOutput );
}


/*********************************************************************
 * Modern Message Callback Functions
 * In addition to being exposed to the API, they are used internally
 * to produce the required strings as needed.
 *********************************************************************/


ctmbstr TY_(getMessageKey)( TidyMessageImpl message )
{
    return message.messageKey;
}

int TY_(getMessageLine)( TidyMessageImpl message )
{
    return message.line;
}

int TY_(getMessageColumn)( TidyMessageImpl message )
{
    return message.column;
}

TidyReportLevel TY_(getMessageLevel)( TidyMessageImpl message )
{
    return message.level;
}

ctmbstr TY_(getMessageFormatDefault)( TidyMessageImpl message )
{
    return message.messageFormatDefault;
}

ctmbstr TY_(getMessageFormat)( TidyMessageImpl message )
{
    return message.messageFormat;
}

ctmbstr TY_(getMessageDefault)( TidyMessageImpl message )
{
    return message.messageDefault;
}

ctmbstr TY_(getMessage)( TidyMessageImpl message )
{
    return message.message;
}

ctmbstr TY_(getMessagePosDefault)( TidyMessageImpl message )
{
    return message.messagePosDefault;
}

ctmbstr TY_(getMessagePos)( TidyMessageImpl message )
{
    return message.messagePos;
}

ctmbstr TY_(getMessagePrefixDefault)( TidyMessageImpl message )
{
    return message.messagePrefixDefault;
}

ctmbstr TY_(getMessagePrefix)( TidyMessageImpl message )
{
    return message.messagePrefix;
}


ctmbstr TY_(getMessageOutputDefault)( TidyMessageImpl message )
{
    return message.messageOutputDefault;
}

ctmbstr TY_(getMessageOutput)( TidyMessageImpl message )
{
    return message.messageOutput;
}


/*********************************************************************
 * BuildArgArray support
 * Adapted loosely from Mozilla `prprf.c`, Mozilla Public License:
 *   - https://www.mozilla.org/en-US/MPL/2.0/
 *********************************************************************/


struct printfArg {
    TidyFormatParameterType type;  /* type of the argument    */
    union {                        /* the argument            */
        int i;
        unsigned int ui;
        int32_t i32;
        uint32_t ui32;
        int64_t ll;
        uint64_t ull;
        double d;
        const char *s;
        size_t *ip;
#ifdef WIN32
        const WCHAR *ws;
#endif
    } u;
};

static struct printfArg* BuildArgArray( TidyDocImpl *doc, const char *fmt, va_list ap, int* rv )
{
    int number = 0;
    int cn = -1;
    int i = 0;
    const char* p;
    char  c;
    struct printfArg* nas;
    
    /* first pass: determine number of valid % to allocate space. */
    
    p = fmt;
    *rv = 0;
    
    while( ( c = *p++ ) != 0 )
    {
        if( c != '%' )
            continue;
        
        if( ( c = *p++ ) == '%' )	/* skip %% case */
            continue;
        else
            number++;
    }
        

    if( number == 0 )
        return NULL;

    
    nas = (struct printfArg*)TidyDocAlloc( doc, number * sizeof( struct printfArg ) );
    if( !nas )
    {
        *rv = -1;
        return NULL;
    }


    for( i = 0; i < number; i++ )
    {
        nas[i].type = tidyFormatType_UNKNOWN;
    }
    
    
    /* second pass: set nas[].type. */
    
    p = fmt;
    while( ( c = *p++ ) != 0 )
    {
        if( c != '%' )
            continue;
        
        if( ( c = *p++ ) == '%' )
            continue; /* skip %% case */
        
        
        /* width -- width via parameter */
        if (c == '*')
        {
            /* not supported feature */
            *rv = -1;
            break;
        }
        
        /* width field -- skip */
        while ((c >= '0') && (c <= '9'))
        {
            c = *p++;
        }
        
        /* precision */
        if (c == '.')
        {
            c = *p++;
            if (c == '*') {
                /* not supported feature */
                *rv = -1;
                break;
            }
            
            while ((c >= '0') && (c <= '9'))
            {
                c = *p++;
            }
        }
        
        
        cn++;
        
        /* size */
        nas[cn].type = tidyFormatType_INTN;
        if (c == 'h')
        {
            nas[cn].type = tidyFormatType_INT16;
            c = *p++;
        } else if (c == 'L')
        {
            nas[cn].type = tidyFormatType_INT64;
            c = *p++;
        } else if (c == 'l')
        {
            nas[cn].type = tidyFormatType_INT32;
            c = *p++;
            if (c == 'l') {
                nas[cn].type = tidyFormatType_INT64;
                c = *p++;
            }
        } else if (c == 'z')
        {
            if (sizeof(size_t) == sizeof(int32_t))
            {
                nas[ cn ].type = tidyFormatType_INT32;
            } else if (sizeof(size_t) == sizeof(int64_t))
            {
                nas[ cn ].type = tidyFormatType_INT64;
            } else
            {
                nas[ cn ].type = tidyFormatType_UNKNOWN;
            }
            c = *p++;
        }
        
        /* format */
        switch (c)
        {
            case 'd':
            case 'c':
            case 'i':
            case 'o':
            case 'u':
            case 'x':
            case 'X':
                break;
                
            case 'e':
            case 'f':
            case 'g':
                nas[ cn ].type = tidyFormatType_DOUBLE;
                break;
                
            case 'p':
                if (sizeof(void *) == sizeof(int32_t))
                {
                    nas[ cn ].type = tidyFormatType_UINT32;
                } else if (sizeof(void *) == sizeof(int64_t))
                {
                    nas[ cn ].type = tidyFormatType_UINT64;
                } else if (sizeof(void *) == sizeof(int))
                {
                    nas[ cn ].type = tidyFormatType_UINTN;
                } else
                {
                    nas[ cn ].type = tidyFormatType_UNKNOWN;
                }
                break;
                
            case 'S':
#ifdef WIN32
                nas[ cn ].type = TYPE_WSTRING;
                break;
#endif
            case 'C':
            case 'E':
            case 'G':
                nas[ cn ].type = tidyFormatType_UNKNOWN;
                break;
                
            case 's':
                nas[ cn ].type = tidyFormatType_STRING;
                break;
                
            case 'n':
                nas[ cn ].type = tidyFormatType_INTSTR;
                break;
                
            default:
                nas[ cn ].type = tidyFormatType_UNKNOWN;
                break;
        }
        
        /* Something's not right. */
        if( nas[ cn ].type == tidyFormatType_UNKNOWN )
        {
            *rv = -1;
            break;
        }
    }
    
    
    /* third pass: fill the nas[cn].ap */
    
    if( *rv < 0 )
    {
        TidyDocFree( doc, nas );;
        return NULL;
    }
    
    cn = 0;
    while( cn < number )
    {
        if( nas[cn].type == tidyFormatType_UNKNOWN )
        {
            cn++;
            continue;
        }
        
        switch( nas[cn].type )
        {
            case tidyFormatType_INT16:
            case tidyFormatType_UINT16:
            case tidyFormatType_INTN:
                nas[cn].u.i = va_arg( ap, int );
                break;
                
            case tidyFormatType_UINTN:
                nas[cn].u.ui = va_arg( ap, unsigned int );
                break;
                
            case tidyFormatType_INT32:
                nas[cn].u.i32 = va_arg( ap, int32_t );
                break;
                
            case tidyFormatType_UINT32:
                nas[cn].u.ui32 = va_arg( ap, uint32_t );
                break;
                
            case tidyFormatType_INT64:
                nas[cn].u.ll = va_arg( ap, int64_t );
                break;
                
            case tidyFormatType_UINT64:
                nas[cn].u.ull = va_arg( ap, uint64_t );
                break;
                
            case tidyFormatType_STRING:
                nas[cn].u.s = va_arg( ap, char* );
                break;
                
#ifdef WIN32
            case tidyFormatType_WSTRING:
                nas[cn].u.ws = va_arg( ap, WCHAR* );
                break;
#endif
                
            case tidyFormatType_INTSTR:
                nas[cn].u.ip = va_arg( ap, size_t* );
                break;
                
            case tidyFormatType_DOUBLE:
                nas[cn].u.d = va_arg( ap, double );
                break;
                
            default:
                TidyDocFree( doc, nas );
                *rv = -1;
                return NULL;
        }
        
        cn++;
    }
    
    *rv = number;
    return nas;
}

