/*
 This file is part of Darling.

 Copyright (C) 2019 Lubos Dolezel

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#import <CoreTelephony/CTSubscriberInfo.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GUESS: empty/nil are conservative unsupported-device results. */
@implementation CTSubscriberInfo

+ (NSArray<CTSubscriber *> *)subscribers
{
    return [NSArray array];
}

+ (CTSubscriber *)subscriber
{
    const char *diagnostics = getenv("CATALYST_DIAG");
    if (diagnostics && *diagnostics && strcmp(diagnostics, "0") != 0) {
        static dispatch_once_t once;
        dispatch_once(&once, ^{
            fprintf(stderr, "CATALYST STUB: +[CTSubscriberInfo subscriber]\n");
        });
    }
    return nil;
}

@end
