/* Original strict typed configuration control. Never creates a HAL tap or stream. */
#import <Foundation/Foundation.h>
#import <CoreAudio/CATapDescription.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void verifyTap(BOOL value,const char* message) {
    if (!value) { ++failures; fprintf(stderr,"FAIL CATapDescription: %s\n",message); }
}
static void initializers(void) {
    for (unsigned mode=0;mode<6;++mode) {
        NSMutableArray *input=[NSMutableArray arrayWithObjects:@11,@22,nil];
        CATapDescription *tap=nil;
        switch (mode) {
            case 0: tap=[[CATapDescription alloc] initMonoMixdownOfProcesses:input]; break;
            case 1: tap=[[CATapDescription alloc] initStereoMixdownOfProcesses:input]; break;
            case 2: tap=[[CATapDescription alloc] initMonoGlobalTapButExcludeProcesses:input]; break;
            case 3: tap=[[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:input]; break;
            case 4: tap=[[CATapDescription alloc] initWithProcesses:input andDeviceUID:@"own-device" withStream:3]; break;
            case 5: tap=[[CATapDescription alloc] initExcludingProcesses:input andDeviceUID:@"own-device" withStream:3]; break;
        }
        [input removeAllObjects];
        verifyTap(tap!=nil,"initializer returned nil");
        verifyTap([tap.processes isEqualToArray:@[@11,@22]],"initializer failed process-list copy");
        verifyTap(tap.isExclusive==(mode==2 || mode==3 || mode==5),"initializer inclusion/exclusion flags");
        verifyTap(tap.isMixdown==(mode<4),"initializer mixdown flag");
        verifyTap(tap.isMono==(mode==0 || mode==2),"initializer mono/stereo flag");
        if (mode<4) verifyTap(tap.deviceUID==nil && tap.stream==nil,"mixdown unexpectedly selects hardware stream");
        else verifyTap([tap.deviceUID isEqualToString:@"own-device"] && [tap.stream integerValue]==3,
            "device initializer lost UID/stream selection");
        verifyTap(tap.isMuted==CATapUnmuted,"documented unmuted default");
        [tap release];
    }
}
static void properties(void) {
    CATapDescription *tap=[[CATapDescription alloc] init];
    verifyTap(tap.isMuted==CATapUnmuted,"default mute behavior");
    NSMutableArray *processes=[NSMutableArray arrayWithObjects:@31,@32,nil];
    tap.processes=processes; [processes addObject:@33];
    verifyTap([tap.processes isEqualToArray:@[@31,@32]],"process setter borrowed mutable array");
    NSArray *old=[tap.processes retain];
    tap.processes=@[@99];
    verifyTap([old isEqualToArray:@[@31,@32]] && [tap.processes isEqualToArray:@[@99]],"process replacement lost stored snapshot");
    [old release];
    NSMutableArray *bundles=[NSMutableArray arrayWithObject:@"org.original.one"];
    tap.bundleIDs=bundles; [bundles removeAllObjects];
    verifyTap([tap.bundleIDs isEqualToArray:@[@"org.original.one"]],"bundle-ID setter failed copy");
    NSMutableString *name=[NSMutableString stringWithString:@"Original tap"];
    tap.name=name; [name appendString:@" changed"];
    verifyTap([tap.name isEqualToString:@"Original tap"],"name setter failed copy");
    NSMutableString *device=[NSMutableString stringWithString:@"device-123"];
    tap.deviceUID=device; [device appendString:@" changed"];
    verifyTap([tap.deviceUID isEqualToString:@"device-123"],"device UID setter failed copy");
    NSUUID *uuid=[[NSUUID alloc] initWithUUIDString:@"00000000-0000-4000-8000-000000000123"];
    tap.UUID=uuid; [uuid release];
    verifyTap([[tap.UUID UUIDString] isEqualToString:@"00000000-0000-4000-8000-000000000123"],"UUID setter failed ownership/value");
    NSNumber *stream=[[NSNumber alloc] initWithInteger:7];
    tap.stream=stream; [stream release];
    verifyTap([tap.stream integerValue]==7,"stream setter failed ownership/value");
    tap.deviceUID=nil; tap.stream=nil;
    verifyTap(tap.deviceUID==nil && tap.stream==nil,"nullable hardware selection cannot clear");
    tap.exclusive=YES; tap.mixdown=YES; tap.mono=YES;
    tap.privateTap=YES; tap.processRestoreEnabled=YES;
    verifyTap(tap.isExclusive && tap.isMixdown && tap.isMono && tap.isPrivate && tap.isProcessRestoreEnabled,
        "boolean setters/getters did not retain true");
    tap.exclusive=NO; tap.mixdown=NO; tap.mono=NO;
    tap.privateTap=NO; tap.processRestoreEnabled=NO;
    verifyTap(!tap.isExclusive && !tap.isMixdown && !tap.isMono && !tap.isPrivate && !tap.isProcessRestoreEnabled,
        "boolean setters/getters did not retain false");
    for (CATapMuteBehavior mute=CATapUnmuted;mute<=CATapMutedWhenTapped;++mute) {
        tap.muteBehavior=mute; verifyTap(tap.isMuted==mute,"numeric mute setter/getter mismatch");
    }
    [tap release];
}
static void projectDefaults(void) {
    CATapDescription *a=[[CATapDescription alloc] init],*b=[[CATapDescription alloc] init];
    verifyTap(a.processes!=nil && a.processes.count==0,"project empty processes default");
    verifyTap(a.bundleIDs!=nil && a.bundleIDs.count==0,"project empty bundle IDs default");
    verifyTap(a.name!=nil && a.name.length==0,"project empty name default");
    verifyTap(a.UUID!=nil && b.UUID!=nil && ![[a.UUID UUIDString] isEqualToString:[b.UUID UUIDString]],
        "project fresh UUID default");
    verifyTap(a.deviceUID==nil && a.stream==nil,"project unspecified hardware default");
    verifyTap(!a.isPrivate && !a.isExclusive && !a.isMixdown && !a.isMono && !a.isProcessRestoreEnabled,
        "project false flag defaults");
    verifyTap(a.isMuted==CATapUnmuted,"documented unmuted default");
    [a release]; [b release];
}
int main(int argc,char** argv) {
    if (argc!=2) return 2;
    @autoreleasepool {
        if (!strcmp(argv[1],"model")) { initializers(); properties(); }
        else if (!strcmp(argv[1],"project-defaults")) projectDefaults();
        else return 2;
    }
    if (failures) return 1;
    printf("PASS_CATAPDESCRIPTION mode=%s\n",argv[1]);
    return 0;
}
