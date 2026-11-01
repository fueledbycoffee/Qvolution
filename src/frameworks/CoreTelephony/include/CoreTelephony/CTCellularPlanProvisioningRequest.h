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

#include <Foundation/Foundation.h>

@interface CTCellularPlanProvisioningRequest : NSObject <NSSecureCoding> {
    NSString *_address;
    NSString *_matchingID;
    NSString *_OID;
    NSString *_confirmationCode;
    NSString *_ICCID;
    NSString *_EID;
}

@property (nonatomic, retain) NSString *address;
@property (nonatomic, retain) NSString *matchingID;
@property (nonatomic, retain) NSString *OID;
@property (nonatomic, retain) NSString *confirmationCode;
@property (nonatomic, retain) NSString *ICCID;
@property (nonatomic, retain) NSString *EID;

@end
