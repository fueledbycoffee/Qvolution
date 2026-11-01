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

@class CTCarrier;
@protocol CTTelephonyNetworkInfoDelegate;

@interface CTTelephonyNetworkInfo : NSObject {
    id<CTTelephonyNetworkInfoDelegate> _delegate;
    void (^_serviceSubscriberCellularProvidersDidUpdateNotifier)(NSString *);
    void (^_subscriberCellularProviderDidUpdateNotifier)(CTCarrier *);
}

@property (nonatomic, readonly, retain) NSString *dataServiceIdentifier;
@property (nonatomic, assign) id<CTTelephonyNetworkInfoDelegate> delegate;
@property (nonatomic, readonly, retain) NSDictionary<NSString *, NSString *> *serviceCurrentRadioAccessTechnology;
@property (nonatomic, readonly, retain) NSString *currentRadioAccessTechnology;
@property (nonatomic, readonly, retain) NSDictionary<NSString *, CTCarrier *> *serviceSubscriberCellularProviders;
@property (nonatomic, readonly, retain) CTCarrier *subscriberCellularProvider;
@property (nonatomic, copy) void (^serviceSubscriberCellularProvidersDidUpdateNotifier)(NSString *);
@property (nonatomic, copy) void (^subscriberCellularProviderDidUpdateNotifier)(CTCarrier *);

@end

@protocol CTTelephonyNetworkInfoDelegate <NSObject>
@optional
- (void)dataServiceIdentifierDidChange:(NSString *)identifier;
@end
