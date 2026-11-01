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

#import <CoreTelephony/CTCarrier.h>

/* Darling has no cellular service, so a carrier reports no values: the
 * documentation's value when no carrier information is available. */
@implementation CTCarrier

- (NSString *)carrierName
{
    return nil;
}

- (NSString *)mobileCountryCode
{
    return nil;
}

- (NSString *)mobileNetworkCode
{
    return nil;
}

- (NSString *)isoCountryCode
{
    return nil;
}

- (BOOL)allowsVOIP
{
    return NO;
}

@end
