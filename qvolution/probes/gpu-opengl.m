#import <AppKit/AppKit.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static GLuint shader(GLenum kind, const char *source) {
    GLuint value=glCreateShader(kind); glShaderSource(value,1,&source,NULL); glCompileShader(value);
    GLint ok=0; glGetShaderiv(value,GL_COMPILE_STATUS,&ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(value,sizeof(log),NULL,log); fprintf(stderr,"FAIL: shader: %s\n",log); return 0; }
    return value;
}
int main(int argc,char **argv) {
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    [NSApplication sharedApplication];
    CGLPixelFormatAttribute attributes[]={kCGLPFAAccelerated,kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,0};
    CGLPixelFormatObj format=NULL; CGLContextObj context=NULL; GLint count=0;
    CGLError error=CGLChoosePixelFormat(attributes,&format,&count);
    if (error || !format || !count) { fprintf(stderr,"FAIL: CGLChoosePixelFormat error=%d\n",error); return 1; }
    error=CGLCreateContext(format,NULL,&context);
    if (error || !context || (error=CGLSetCurrentContext(context))) { fprintf(stderr,"FAIL: CGL context error=%d\n",error); return 1; }
    const char *renderer=(const char *)glGetString(GL_RENDERER), *vendor=(const char *)glGetString(GL_VENDOR), *version=(const char *)glGetString(GL_VERSION);
    if (!renderer || !vendor || !version) { fputs("FAIL: no current GL renderer\n",stderr); return 1; }
    printf("renderer=%s\nvendor=%s\nversion=%s\n",renderer,vendor,version); fflush(stdout);
    NSString *name=[NSString stringWithUTF8String:renderer];
    for (NSString *software in @[@"llvmpipe",@"softpipe",@"swrast",@"swiftshader",@"software"])
        if ([name rangeOfString:software options:NSCaseInsensitiveSearch].location!=NSNotFound) { fputs("FAIL: software renderer does not qualify as hardware acceleration\n",stderr); return 1; }
    const char *vertex="#version 150\nvoid main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],0,1);}";
    const char *fragment="#version 150\nout vec4 result;void main(){result=vec4(0.25,0.5,0.75,1.0);}";
    GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment); if (!vs || !fs) return 1;
    GLuint program=glCreateProgram(); glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked){fputs("FAIL: shader program link\n",stderr);return 1;}
    GLuint texture,fbo,vao;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){fputs("FAIL: framebuffer incomplete\n",stderr);return 1;}
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glUseProgram(program);glViewport(0,0,16,16);glDrawArrays(GL_TRIANGLES,0,3);glFinish();
    unsigned char pixel[4]={0};glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);GLenum status=glGetError();
    printf("readback=%u,%u,%u,%u gl_error=%u\n",pixel[0],pixel[1],pixel[2],pixel[3],status);
    int expected=(argc==2 && strcmp(argv[1],"--negative-control")==0)?0:64;
    BOOL correct=status==GL_NO_ERROR && abs((int)pixel[0]-expected)<=2 && abs((int)pixel[1]-128)<=2 && abs((int)pixel[2]-191)<=2 && pixel[3]==255;
    glDeleteVertexArrays(1,&vao);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);CGLSetCurrentContext(NULL);CGLDestroyContext(context);CGLDestroyPixelFormat(format);[pool drain];
    if(!correct){fputs("FAIL: GPU triangle readback\n",stderr);return 1;}
    puts("PASS: guest CGL hardware renderer and shader triangle readback");return 0;
}
