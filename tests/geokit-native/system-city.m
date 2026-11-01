#import "../../src/private-frameworks/GeoKit/include/GeoKit/GeoKit.h"
@implementation NSObject
@end
@implementation NSString
@end
int main(void)
{
    return [GEOCity systemCity] == nil ? 0 : 1;
}
