#define GL_SILENCE_DEPRECATION 1
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdio.h>
#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION 0x821B
#endif
int main(void) {
 CGLPixelFormatAttribute attrs[]={kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_Legacy,0};
 CGLPixelFormatObj pf=NULL; CGLContextObj ctx=NULL; GLint n=0;
 CGLError e=CGLChoosePixelFormat(attrs,&pf,&n); printf("choose.error: %d\n",e); if(e)return 0;
 e=CGLCreateContext(pf,NULL,&ctx); printf("create.error: %d\n",e); if(e)return 0; CGLSetCurrentContext(ctx);
 GLuint framebuffer, texture;
 glGenFramebuffersEXT(1,&framebuffer); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer);
 glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
 glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,texture,0);
 glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
 printf("fbo.complete: %d\n",glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
 while(glGetError()!=GL_NO_ERROR){}
 const GLubyte *version; glBegin(GL_POINTS);version=glGetString(GL_VERSION);glEnd();
 printf("begin.versionIsNull: %d\n",version==NULL); printf("begin.version.error: %u\n",glGetError());
 GLint value=12345;glBegin(GL_POINTS);glGetIntegerv(GL_MAJOR_VERSION,&value);glEnd();
 printf("begin.major.value: %d\n",value);printf("begin.major.error: %u\n",glGetError());
 value=12345;glGetIntegerv(GL_MAJOR_VERSION,&value);printf("outside.major.value: %d\n",value);printf("outside.major.error: %u\n",glGetError());
 CGLSetCurrentContext(NULL);CGLReleaseContext(ctx);CGLReleasePixelFormat(pf);return 0;
}
