#pragma once
#include <OpenGL/OpenGL.h>
typedef unsigned int CGSConnectionID, CGSWindowID, CGSSurfaceID;
typedef void *CGLWindowRef;
void *_CGSNativeDisplay(CGSConnectionID);
void *_CGSNativeWindowForID(CGSConnectionID, CGSWindowID);
void *_CGSNativeWindowForSurfaceID(CGSConnectionID, CGSWindowID, CGSSurfaceID);
