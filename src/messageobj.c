//
//  messageobj.c
//  tidy
//
//  Created by Jim Derry on 2/20/17.
//  Copyright © 2017 balthisar.com. All rights reserved.
//

#include "messageobj.h"
#include "message.h"
#include "tidy-int.h"
#if !defined(NDEBUG) && defined(_MSC_VER)
#include "sprtf.h"
#endif


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
 **  This version serves as the designated initializer and as such
 **  requires every known parameter.
 */
static TidyMessageImpl *tidyMessageCreateInit( TidyDocImpl *doc,
                                              Node *node,
                                              uint code,
                                              int line,
                                              int column,
                                              TidyReportLevel level,
                                              va_list args )
{
    TidyMessageImpl *result = TidyDocAlloc(doc, sizeof(TidyMessageImpl));
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

    /* (here we will do something with args) */

    /* Things we create... */

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


    return result;
}


/** Creates a TidyMessageImpl, but without line numbers.
 */
TidyMessageImpl *tidyMessageCreate( TidyDocImpl *doc,
                                          uint code,
                                          TidyReportLevel level,
                                          va_list args )
{
    TidyMessageImpl *result;
    va_list args_copy;

    va_copy(args_copy, args);
    result = tidyMessageCreateInit(doc, NULL, code, 0, 0, level, args_copy);
    va_end(args_copy);

    return result;
}


/** Creates a TidyMessageImpl, using the line and column from the provided
 **  Node as the message position source.
 */
TidyMessageImpl *tidyMessageCreateWithNode( TidyDocImpl *doc,
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
    result = tidyMessageCreateInit(doc, node, code, line, col, level, args_copy);
    va_end(args_copy);

    return result;
}


/** Creates a TidyMessageImpl, using the line and column from the provided
 **  document's Lexcer as the message position source.
 */
TidyMessageImpl *tidyMessageCreateWithLexer( TidyDocImpl *doc,
                                                   uint code,
                                                   TidyReportLevel level,
                                                   va_list args )
{
    TidyMessageImpl *result;
    va_list args_copy;
    int line = ( doc->lexer ? doc->lexer->lines : 0 );
    int col  = ( doc->lexer ? doc->lexer->columns : 0 );

    va_copy(args_copy, args);
    result = tidyMessageCreateInit(doc, NULL, code, line, col, level, args_copy);
    va_end(args_copy);

    return result;
}


/** Because instances of TidyMessage retain memory, they must be released
 **  when we're done with them.
 */
void tidyMessageRelease( TidyMessageImpl message )
{
    TidyDocFree( tidyDocToImpl(message.tidyDoc), message.messageDefault );
    TidyDocFree( tidyDocToImpl(message.tidyDoc), message.message );
    TidyDocFree( tidyDocToImpl(message.tidyDoc), message.messagePosDefault );
    TidyDocFree( tidyDocToImpl(message.tidyDoc), message.messagePos );
    TidyDocFree( tidyDocToImpl(message.tidyDoc), message.messageOutputDefault );
    TidyDocFree( tidyDocToImpl(message.tidyDoc), message.messageOutput );
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

