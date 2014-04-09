

#include "cropper.h"

#ifdef WIN
#include <Windows.h>
#endif

#ifdef OSX
#include <stdlib.h>
#endif

#include <stdio.h>


void toChunks( CropArea* areas, AreaChunk* chunks, unsigned int len, size_t width, size_t height ) {
	CropArea* area;
	AreaChunk* chunk;
	for( int i = 0; i<len; i++ ) {
		chunk = &chunks[i];
		area = &areas[i];

		chunk->face = area->face;
		chunk->index = area->index;
		chunk->x = (float)area->x / (float)width;
		chunk->y = (float)area->y / (float)height;
		chunk->width = (float)area->width / (float)width;
		chunk->height = (float)area->height / (float)height;
	}
}

bool
refineV( unsigned char* face, int faceIndex,CropArea* areas, size_t width, size_t height, unsigned int& numAreas, int threshold, int athreshold ){
	//printf( "inlen : %i \n", numAreas );
	int nareas = numAreas;
	int newlen = nareas;

	int x, y, w, h, aw, ah, right, bottom;
	int _in = -1;

	int XM = 4;
	int YM = width*4;

	int minx = height;
	int maxx = 0;
	int lminx = 0;
	int lmaxx = 0;

	int column = 0;
	int cy = 0;

	bool blank = false;
	bool replaced = false;
	int repArea = 0;

	for( int i = 0; i< nareas; i++ ){
		replaced = false;
		_in = -1;

		x = areas[i].x;
		y = areas[i].y;
		w = areas[i].width;
		h = areas[i].height;
		right = x + w;
		bottom = y + h;

		
		minx = height;
		maxx = 0;

		column = x-1;

		

		while( column < right+1 ) {

			cy = y;

			lminx = -1;
			lmaxx = -1;
			blank = true;

			while( cy < bottom ) {
				if( face[column*XM + cy*YM + 3] > athreshold ) {
					blank = false;
					if(lminx==-1) lminx = cy;
					lmaxx = cy;
				}

				cy++;
			}
			if( !blank ) {
				if( minx > lminx ) minx = lminx;
				if( maxx < lmaxx ) maxx = lmaxx;
			}


			if( !blank && _in == -1 ) {
				_in = column;
			} else if( blank && _in > -1) {

				aw = column-_in;
				ah = maxx-minx+1;
				if( aw*ah > threshold ) {
					if(replaced){
						repArea = newlen;
						newlen ++;
					} else {
						repArea = i;
						replaced = true;
					}
					areas[repArea].face = faceIndex;
					areas[repArea].x = _in;
					areas[repArea].y = minx;
					areas[repArea].height = ah;
					areas[repArea].width = aw;
					
				}

				_in = -1;
				minx = height;
				maxx = 0;
			}
			column++;
		}

		if( _in > -1 ) {

			aw = column-1-_in;
			ah = maxx-minx+1;
			if( aw*ah > threshold ) {
				if(replaced){
					repArea = newlen;
					newlen ++;
				} else {
					repArea = i;
					replaced = true;
				}
				areas[repArea].face = faceIndex;
				areas[repArea].x = _in;
				//printf( "minx b : %i, %i \n", minx, maxx );
				areas[repArea].y = minx;
				areas[repArea].height = ah;
				areas[repArea].width = aw;
			}

			_in = -1;
		}


	}
	//printf( "oldlen : %i, new : %i \n", numAreas, newlen );
	numAreas = newlen;
	return (newlen != numAreas);
}

CropArea* processFace( unsigned char* face, int faceIndex, size_t width, size_t height, unsigned int& numAreas, int threshold, int athreshold ) {

	CropArea* areas = (CropArea*) malloc( height*sizeof(CropArea) );
	
	unsigned char* ptr = face;
	
	int currentBound = 0;
	int currentline = 0;
	int currentPx = 0;
	int minx = width;
	int maxx = 0;
	int lminx = 0;
	int lmaxx = 0;
	int loffset = 0;

	int w = 0;
	int h = 0;

	bool blank = false;

	int _in = -1;

	while( currentline < height ) {

		currentPx = 0;
		blank = true;

		loffset = currentline*width*4;

		lminx = -1;
		lmaxx = -1;

		while( currentPx < width ) {
			if( face[loffset+currentPx*4+3] > athreshold ) {
				blank = false;
				if(lminx==-1) lminx = currentPx;
				lmaxx = currentPx;
			}
			currentPx++;
		}
		if( !blank ) {
			if( minx > lminx ) minx = lminx;
			if( maxx < lmaxx ) maxx = lmaxx;
		}

		if( !blank && _in == -1 ) {
			_in = currentline;
		} else if( blank && _in > -1) {
			w = maxx-minx+1;
			h = currentline-_in;
			if( w*h > threshold ) {
				areas[currentBound].face = faceIndex;
				areas[currentBound].y = _in;
				areas[currentBound].x = minx;
				areas[currentBound].width = w;
				areas[currentBound].height = h;
				currentBound ++;
			}
			_in = -1;

			minx = width;
			maxx = 0;
		}

		currentline++;
	}
	if( _in > -1 ) {
		w = maxx-minx+1;
		h = currentline-_in;
		if( w*h > threshold ) {
			areas[currentBound].face = faceIndex;
			areas[currentBound].y = _in;
			areas[currentBound].x = minx;
			areas[currentBound].width =w;
			areas[currentBound].height = h;

			currentBound ++;
		}
	}
	if(currentBound==0)
		numAreas = 0;
	else
		numAreas = currentBound;

	refineV( face, faceIndex, areas, width, height, numAreas, threshold, athreshold );

	for( int i = 0; i< numAreas; i++ ){
		areas[i].index = i;
	}

	return areas;
}