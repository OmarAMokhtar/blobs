#include <string.h>

#include "texture.h"

#include <GLES2/gl2.h>

#include <android/log.h>

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

GLuint loadBMP(const char *buffer)
{
	unsigned int dataPos;
	unsigned int imageSize;
	unsigned int width, height;
	unsigned char* data;

	if (buffer[0] != 'B' || buffer[1] != 'M') return 0;
	if (*(int*)&(buffer[0x1E])!=0) return 0;
	if (*(int*)&(buffer[0x1C])!=24) return 0;

	dataPos = *(int*)&(buffer[0x0A]);
	imageSize = *(int*)&(buffer[0x22]);
	width = *(int*)&(buffer[0x12]);
	height = *(int*)&(buffer[0x16]);

	if (imageSize == 0) imageSize = width*height*3;
	if (dataPos == 0) dataPos = 54;

	GLuint textureID;
	glGenTextures(1, &textureID);	
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, &buffer[dataPos]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
	glGenerateMipmap(GL_TEXTURE_2D);

	return textureID;
}

#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3

GLuint loadDDS(const char* buffer)
{
	unsigned char header[124];

	/* verify the type of file */ 
	if (strncmp(buffer, "DDS ", 4) != 0) return 0; 

	unsigned int height = *(unsigned int*)&(buffer[12]);
	unsigned int width = *(unsigned int*)&(buffer[16]);
	unsigned int linearSize = *(unsigned int*)&(buffer[20]);
	unsigned int mipMapCount = *(unsigned int*)&(buffer[28]);
	unsigned int fourCC = *(unsigned int*)&(buffer[84]);

	unsigned int bufsize;
	bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize; 
	buffer += 128;

	unsigned int components = (fourCC == FOURCC_DXT1) ? 3 : 4; 
	unsigned int format;
	switch(fourCC) 
	{ 
	case FOURCC_DXT1: 
		format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; 
		break; 
	case FOURCC_DXT3: 
		format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; 
		break; 
	case FOURCC_DXT5: 
		format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; 
		break; 
	default: 
		return 0; 
	}

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);	
	
	unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16; 
	unsigned int offset = 0;

	for (unsigned int level = 0 ; level < mipMapCount && (width || height) ; ++level) 
	{ 
		unsigned int size = ((width+3)/4)*((height+3)/4)*blockSize; 
		glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, size, buffer + offset); 

		offset += size; 
		width  /= 2; 
		height /= 2; 

		if(width < 1) width = 1;
		if(height < 1) height = 1;
	} 

	return textureID;
}

typedef struct
{
	unsigned char imageTypeCode;
	short int imageWidth;
	short int imageHeight;
	unsigned char bitCount;
	unsigned char *imageData;
} TGAFILE;

GLuint loadTGA(const char* buffer)
{
	TGAFILE tgaFile;
	long imageSize;
	int colorMode;
	unsigned char colorSwap;

	buffer += 2;
	
	memcpy(&tgaFile.imageTypeCode, buffer, sizeof(unsigned char));
	buffer += sizeof(unsigned char);

	if (tgaFile.imageTypeCode != 2 && tgaFile.imageTypeCode != 3) return 0;

	buffer += sizeof(short int)*4 + sizeof(unsigned char);

	memcpy(&tgaFile.imageWidth, buffer,sizeof(short int));
	buffer += sizeof(short int);
	memcpy(&tgaFile.imageHeight, buffer, sizeof(short int));
	buffer += sizeof(short int);
	memcpy(&tgaFile.bitCount, buffer, sizeof(unsigned char));
	buffer += sizeof(unsigned char);

	buffer += sizeof(unsigned char);

	colorMode = tgaFile.bitCount / 8;
	imageSize = tgaFile.imageWidth * tgaFile.imageHeight * colorMode;

	tgaFile.imageData = new unsigned char[imageSize];
    memcpy(tgaFile.imageData,buffer,imageSize);

	for (int imageIdx = 0 ; imageIdx < imageSize ; imageIdx += colorMode)
	{
		colorSwap = tgaFile.imageData[imageIdx];
		tgaFile.imageData[imageIdx] = tgaFile.imageData[imageIdx + 2];
		tgaFile.imageData[imageIdx + 2] = colorSwap;
	}

	GLuint textureID;
	glGenTextures(1, &textureID);	
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, tgaFile.imageWidth, tgaFile.imageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, tgaFile.imageData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    delete[] tgaFile.imageData;

	return textureID;
}
