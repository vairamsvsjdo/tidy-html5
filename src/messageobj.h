//
//  messageobj.h
//  tidy
//
//  Created by Jim Derry on 2/20/17.
//  Copyright © 2017 balthisar.com. All rights reserved.
//

#ifndef messageobj_h
#define messageobj_h


#include "forward.h"


TidyMessageImpl *tidyMessageCreate( TidyDocImpl *doc,
                                          uint code,
                                          TidyReportLevel level,
                                          va_list args );


TidyMessageImpl *tidyMessageCreateWithNode( TidyDocImpl *doc,
                                                  Node *node,
                                                  uint code,
                                                  TidyReportLevel level,
                                           va_list args );

TidyMessageImpl *tidyMessageCreateWithLexer( TidyDocImpl *doc,
                                                   uint code,
                                                   TidyReportLevel level,
                                            va_list args );


void tidyMessageRelease( TidyMessageImpl message );

#endif /* messageobj_h */
