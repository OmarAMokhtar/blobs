#ifndef TEXTURE_H
#define TEXTURE_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

GLuint loadBMP(const char* buffer);
GLuint loadDDS(const char* buffer);
GLuint loadTGA(const char* buffer);

#endif
