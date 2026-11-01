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

#import <CoreTelephony/CTCellularPlanProvisioningRequest.h>

/* A plain value object: the eSIM download it describes is never started, since
 * Darling has no cellular plan support. */
@implementation CTCellularPlanProvisioningRequest

@synthesize address = _address;
@synthesize matchingID = _matchingID;
@synthesize OID = _OID;
@synthesize confirmationCode = _confirmationCode;
@synthesize ICCID = _ICCID;
@synthesize EID = _EID;

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super init];
    if (self) {
        _address = [[coder decodeObjectOfClass:[NSString class] forKey:@"address"] retain];
        _matchingID = [[coder decodeObjectOfClass:[NSString class] forKey:@"matchingID"] retain];
        _OID = [[coder decodeObjectOfClass:[NSString class] forKey:@"OID"] retain];
        _confirmationCode = [[coder decodeObjectOfClass:[NSString class] forKey:@"confirmationCode"] retain];
        _ICCID = [[coder decodeObjectOfClass:[NSString class] forKey:@"ICCID"] retain];
        _EID = [[coder decodeObjectOfClass:[NSString class] forKey:@"EID"] retain];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:_address forKey:@"address"];
    [coder encodeObject:_matchingID forKey:@"matchingID"];
    [coder encodeObject:_OID forKey:@"OID"];
    [coder encodeObject:_confirmationCode forKey:@"confirmationCode"];
    [coder encodeObject:_ICCID forKey:@"ICCID"];
    [coder encodeObject:_EID forKey:@"EID"];
}

- (void)dealloc
{
    [_address release];
    [_matchingID release];
    [_OID release];
    [_confirmationCode release];
    [_ICCID release];
    [_EID release];
    [super dealloc];
}

@end
