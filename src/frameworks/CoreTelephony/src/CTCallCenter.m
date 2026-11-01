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

#import <CoreTelephony/CTCallCenter.h>

/* Darling has no cellular service: no call is ever in progress, which the
 * documentation reports as nil currentCalls, and the event handler is kept but
 * never called. */
@implementation CTCallCenter

@synthesize callEventHandler = _callEventHandler;

- (void)dealloc
{
    [_callEventHandler release];
    [super dealloc];
}

- (NSSet<CTCall *> *)currentCalls
{
    return nil;
}

@end
