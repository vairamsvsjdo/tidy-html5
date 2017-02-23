#ifndef messageobj_h
#define messageobj_h

/*********************************************************************
 * Provides an external API for message reporting; 
 * Abstract internals
 *
 * This module implements the `TidyMessageImpl` structure (declared
 * in `tidy-int.h`) in order to abstract the reporting of reports
 * and dialogue from the rest of Tidy, and to enable a robust and
 * expandable API for message interrogation by LibTidy users.
 *
 *
 * (c) 2017 HTACG
 * See tidy.h for the copyright notice.
 *********************************************************************/

#include "forward.h"


/** @name Message Creation and Releasing */
/** @{ */


TidyMessageImpl *TY_(tidyMessageCreate)( TidyDocImpl *doc,
                                         uint code,
                                         TidyReportLevel level,
                                         ... );

TidyMessageImpl *TY_(tidyMessageCreateV)( TidyDocImpl *doc,
                                          uint code,
                                          TidyReportLevel level,
                                          va_list args );


TidyMessageImpl *TY_(tidyMessageCreateWithNode)( TidyDocImpl *doc,
                                                 Node *node,
                                                 uint code,
                                                 TidyReportLevel level,
                                                 ... );

TidyMessageImpl *TY_(tidyMessageCreateWithNodeV)( TidyDocImpl *doc,
                                                  Node *node,
                                                  uint code,
                                                  TidyReportLevel level,
                                                  va_list args );

TidyMessageImpl *TY_(tidyMessageCreateWithLexer)( TidyDocImpl *doc,
                                                  uint code,
                                                  TidyReportLevel level,
                                                  ... );

TidyMessageImpl *TY_(tidyMessageCreateWithLexerV)( TidyDocImpl *doc,
                                                   uint code,
                                                   TidyReportLevel level,
                                                   va_list args );


void TY_(tidyMessageRelease)( TidyMessageImpl *message );


/** @} */
/** @name Report and Dialogue API */
/** @{ */


/** get the message key string. */
ctmbstr TY_(getMessageKey)( TidyMessageImpl message );

/** get the line number the message applies to. */
int TY_(getMessageLine)( TidyMessageImpl message );

/** get the column the message applies to. */
int TY_(getMessageColumn)( TidyMessageImpl message );

/** get the TidyReportLevel of the message. */
TidyReportLevel TY_(getMessageLevel)( TidyMessageImpl message );

/** the built-in format string */
ctmbstr TY_(getMessageFormatDefault)( TidyMessageImpl message );

/** the localized format string */
ctmbstr TY_(getMessageFormat)( TidyMessageImpl message );

/** the message, formatted, default language */
ctmbstr TY_(getMessageDefault)( TidyMessageImpl message );

/** the message, formatted, localized */
ctmbstr TY_(getMessage)( TidyMessageImpl message );

/** the position part, default language */
ctmbstr TY_(getMessagePosDefault)( TidyMessageImpl message );

/** the position part, localized */
ctmbstr TY_(getMessagePos)( TidyMessageImpl message );

/** the prefix part, default language */
ctmbstr TY_(getMessagePrefixDefault)( TidyMessageImpl message );

/** the prefix part, localized */
ctmbstr TY_(getMessagePrefix)( TidyMessageImpl message );

/** the complete message, as would be output in the CLI */
ctmbstr TY_(getMessageOutputDefault)( TidyMessageImpl message );

/* the complete message, as would be output in the CLI, localized */
ctmbstr TY_(getMessageOutput)( TidyMessageImpl message );


/** @} */


/** @} */
#endif /* messageobj_h */
