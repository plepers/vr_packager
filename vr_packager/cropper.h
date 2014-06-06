#ifndef CROPPER_H

#define CROPPER_H

#include <stdio.h>

typedef struct CropArea CropArea;
typedef struct AreaChunk AreaChunk;

struct CropArea 
{
	unsigned int face;
	unsigned int index;
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
};

struct AreaChunk 
{
	unsigned int face;
	unsigned int index;
	float x;
	float y;
	float width;
	float height;
};

typedef struct core_bounds_params {
	
    unsigned char* face;
    int faceIndex;
    size_t width;
    size_t height;
    unsigned int numAreas;
    int threshold;
    int athreshold;
    
    CropArea* areasResult;
    
} BOUNDS_PARAM;



void* processFace( void* input ) ;

void toChunks( CropArea* areas, AreaChunk* chunks, unsigned int len, size_t width, size_t height ) ;

#endif