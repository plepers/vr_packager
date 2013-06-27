// cubicblur.cpp : Defines the entry point for the console application.
//

#include "vr_packager.h"

using namespace TCLAP;
using namespace std;


unsigned char** splitCrossmap( unsigned char* png, size_t width, size_t height );
float* assembleCrossmap( float** faces, int faceSize );
unsigned char* f32toRgbe( float* faces, int w, int h, double base );
float* subscale( float* pixels, int w, int h, int level );
float** subscaleFaces(float** faces, int w, int h, int level );

unsigned char* crop( unsigned char* face, CropArea* area, int facesize );

int numMipForSize( int inSize ) {
	
	int mip = 1;
	int size = inSize;
	while( size > 2 ) {
		size = size >> 1;
		mip++;
	}

	return mip;
		
}

int getMipForSize( int reqSize, int inSize ) {
	int nummips = numMipForSize( inSize );

	for( int m = 0; m<nummips;m++ ) {
		if(( inSize >> m ) <= reqSize ) return m;
	}
	return -1;
}

bool IsPowerOfTwo(unsigned int x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

double getNewBase( char min, char max ) {
	double newbaseMax = pow( pow( 2.0, max ), 1.0/128.0 );
	double newbaseMin = pow( pow( 2.0, min ), -1.0/128.0 );
	return max( newbaseMax, newbaseMin );
}


int main(int argc, char* argv[])
{

	int faceSize;

	CmdLine cmd("diffusefilter - generate blured cube map", ' ', "0.1");

	ValueArg<string> a_input("i","input","input raw cube texture",true,"","string", cmd );
	ValueArg<string> a_output("o","output","output raw file",true,"","string", cmd );
	ValueArg<int> a_threshold("t","threshold","minimum crops area",false, 1,"int", cmd );
	ValueArg<int> a_at("a","alpha","alpha limit",false, 10,"int", cmd );
	
	

	cmd.parse( argc, argv );
	
	const char* input = a_input.getValue().c_str();
	const char* output = a_output.getValue().c_str();
	
	int threshold = a_threshold.getValue();
	int athreshold = a_at.getValue();

	
	
	printf( "package input cube texture \n    input %s \n    output %s \n" ,  input, output );


	//==================================================
	//								       Load png file
	//==================================================

	unsigned char* png;
	size_t width, height;

	unsigned error = lodepng_decode32_file(&png, &width, &height, input);

	if(error!=0) {
		printf("decoder error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}


	faceSize = height/4;

	printf( "png loaded \n   size : %i*%i \n \n", width, height );
	
	

	//==================================================
	//								       extract faces
	//==================================================

	int mlevel = 1;

	printf( "splitCrossmap \n" );
	unsigned char** faces = splitCrossmap( png, width, height );
	free( png );


	//==================================================
	//								       find bounds
	//==================================================

	unsigned int numAreas;
	int i;
	unsigned char * _crop;

	int casize = sizeof(AreaChunk);
	AreaChunk* report = (AreaChunk*) malloc(255* casize);
	AreaChunk* rptr = report;
	int reportsize = 0;

	char nameBuf[255];

	AreaChunk* chunks;

	for( i = 0; i< 6; i++ ) {
		CropArea* areas = processFace( faces[i], i, faceSize, faceSize, numAreas, threshold, athreshold);

		
		chunks = (AreaChunk*) malloc( numAreas * sizeof( AreaChunk ) );
		toChunks( areas, chunks, numAreas,faceSize,faceSize );
		memcpy( rptr, chunks, numAreas*casize);
		free( chunks );

		rptr += numAreas;
		reportsize += numAreas*casize;

		if(numAreas> 0)
		   printf( "Areas in face %i : %i\n", i, numAreas );


		for( int n = 0; n< numAreas; n++ ){
			
			sprintf( nameBuf, "%s%d_%d.png", output, i, n );
			printf( "	[%i, %i, %i, %i] \n", areas[n].x, areas[n].y,areas[n].width, areas[n].height);
			_crop = crop( faces[i], &areas[n], faceSize);
	        error = lodepng_encode32_file( nameBuf, _crop, areas[n].width, areas[n].height );
			if(error!=0) {
				printf("encode error %u: %s\n", error, lodepng_error_text(error));
				return 1;
			}

			
		}

		free( faces[i] );

	}

	sprintf( nameBuf, "%sareas.bin", output );

	FILE* file;
	file = fopen(nameBuf, "wb" );
	if(!file) {
		printf( "error while writing areas report");
		return 1;
	}
	fwrite(report , 1 , reportsize, file);
	fclose(file);

	free(report);

	

	free( faces );

	return 0;
}

float** subscaleFaces(float** faces, int w, int h, int level ) {
	float ** sfaces = (float **) malloc( 6 * sizeof( float* ) );

	for (int j = 0; j < 6; ++j) 
		sfaces[j] = subscale( faces[j], w, h, level );

	return sfaces;
}

float* subscale( float* pixels, int w, int h, int level ) {

	int rw = w >> level;
	int rh = h >> level;

	int sx, sy;

	int w3 = w*3;
	int rw3 = rw*3;

	int p;
	
	int scale = 1<<(level);
	int scale2 = scale*scale;

	int plen = w*h;

	float* res = (float*) malloc( plen * sizeof(float)*3  );
	
	double pbuffR;
	double pbuffG;
	double pbuffB;


	for( int y = 0; y< rh; y++ ) {
		for( int x = 0; x< rw; x++ ) {
			pbuffR = 0.0f;
			pbuffG = 0.0f;
			pbuffB = 0.0f;
			sx = x*scale;
			sy = y*scale;

			for( int i = sy; i< sy+scale; i++ ) {
				for( int j = sx; j< sx+scale; j++ ) {
					p = (i*w3 + j*3);
					pbuffR += pixels[ p ];
					pbuffG += pixels[ p + 1 ];
					pbuffB += pixels[ p + 2 ];
				}
			}
			p =  (y*rw3 + x*3);
			res[ p ] = pbuffR/scale2;
			res[ p + 1 ] = pbuffG/scale2;
			res[ p + 2 ] = pbuffB/scale2;
		}

	}

	return res;

}

unsigned char* f32toRgbe( float* pixels, int w, int h, double base ) {

	//base = 2.0;

	int j;

	int resSize = w*h;

	unsigned char* rgbe = ( unsigned char* ) malloc( resSize*4*sizeof( unsigned char* ) );

	float r, g, b; 
	double e, re;
	int f;

	double logbase = log( base ); 
	

	int c = 0;
	int fc = 0;
	for (j = 0; j < resSize; j++) {
		
		fc = j*3;
		c = j*4;

		r = pixels[fc];
		g = pixels[fc+1];
		b = pixels[fc+2];

		re = max( r, max( g, b ) );

		f = int( ceil( log( re ) / logbase ) );

		if( f < -128.0f ) f = -128.0f;
		if( f > 127.0f ) f = 127.0f;

		e = pow( base, f );
		
		r = r*255.0f / e;
		g = g*255.0f / e;
		b = b*255.0f / e;

		f += 128.0f;

		
		rgbe[c] = char( r );
		rgbe[c+1] = char( g );
		rgbe[c+2] = char( b );
		rgbe[c+3] = char( f );
	}


	return rgbe;
}

float* assembleCrossmap( float** faces, int faceSize) {

	int i;
	int j;
	int p;
	float* face;
	float* startPos;

	

	int fsize = sizeof( float );
	int facesize3 = faceSize*3;

	const int linelen = fsize*3*faceSize;
	
	int twidth = faceSize * 3;
	int theight = faceSize * 4;

	int resSize = faceSize*faceSize*12* 3;

	float* source = (float*) malloc( resSize * sizeof(float)  );

	for (j = 0; j < resSize; ++j) {
		source[j] = 0.0;
	}

	// left face

	face = faces[0];

	for (j = 0; j < faceSize; ++j) {
		startPos = &source[ twidth * (faceSize+j)*3 ];
		memcpy( startPos, face, linelen );
		face += facesize3;
	}

	// right face

	face = faces[1];

	for (j = 0; j < faceSize; ++j) {
		startPos = &source[ (twidth * (faceSize+j) + (2 * faceSize))*3 ];
		memcpy( startPos, face, linelen );
		face += facesize3;
	}

	// top face

	face = faces[2];

	for (j = 0; j < faceSize; ++j) {
		startPos = &source[ (twidth * j + (faceSize))*3 ];
		memcpy( startPos, face, linelen );
		face += facesize3;
	}

	// bottom face

	face = faces[3];

	for (j = 0; j < faceSize; ++j) {
		startPos = &source[ (twidth * (2*faceSize+j) + (faceSize))*3 ];
		memcpy( startPos, face, linelen );
		face += facesize3;
	}

	// front face

	face = faces[4];

	for (j = 0; j < faceSize; ++j) {
		startPos = &source[ (twidth * (faceSize+j) + (faceSize))*3 ];
		memcpy( startPos, face, linelen );
		face += facesize3;
	}

	// back face (vertical revert)

	face = faces[5];
	int c = 0;
	for (j = 0; j < faceSize; ++j) {
		for ( i = 0; i < faceSize; ++i) {
			p = (( twidth * (theight-j-1) )+(2 * faceSize - (i + 1) ))*3;
			source[ p ] = face[c++];
			source[ p+1 ] = face[c++];
			source[ p+2 ] = face[c++];
		}
	}

	return source;

}

unsigned char* crop( unsigned char* face, CropArea* area, int facesize ) {

	int ly = area->y;
	int my = ly + area->height;
	void* startPos;

	int fsize = sizeof( unsigned char );
	int xoff = area->x;
	const int cpylen = area->width*fsize*4;
	int linelen = area->width*4;

	unsigned char * crop = (unsigned char *) malloc( area->width*area->height*4 * sizeof( unsigned char ) );
	unsigned char * ptr = crop;
	for( ;ly < my; ly++){
		startPos = &face[ (facesize * ly + xoff)*4 ];
		memcpy( ptr, startPos, cpylen );
		ptr += linelen;
	}

	return crop;
}

unsigned char** splitCrossmap( unsigned char* png, size_t width, size_t height ){

	int i;
	int j;
	int p;
	int faceSize;
	int faceSize3;
	int lineByteslen;
	void* startPos;
	int fsize = sizeof( unsigned char );
	
	int comps = 4;


	unsigned char* face;
	unsigned char ** faces;


	faceSize = height/4;
	faceSize3 = faceSize*comps;
	const int cpylen = faceSize*fsize*comps;

	faces = (unsigned char **) malloc( 6 * sizeof( unsigned char* ) );

	lineByteslen = width * comps;
	
	for (j = 0; j < 6; ++j) 
		faces[j] = (unsigned char *) malloc( faceSize*faceSize*comps * sizeof( unsigned char ) );
	
	
	
	// left face

	face = faces[1];

	for (j = 0; j < faceSize; ++j) {
		startPos = &png[ (width * (faceSize+j))*comps ];
		memcpy( face, startPos, cpylen );
		face += faceSize3;
	}

	// right face

	face = faces[0];
	
	for (j = 0; j < faceSize; ++j) {
		startPos = &png[ (width * (faceSize+j) + (2 * faceSize))*comps ];
		memcpy( face, startPos, cpylen );
		face += faceSize3;
	}

	// top face

	face = faces[2];

	for (j = 0; j < faceSize; ++j) {
		startPos = &png[ (width * j + (faceSize))*comps ];
		memcpy( face, startPos, cpylen );
		face += faceSize3;
	}

	// bottom face

	face = faces[3];

	for (j = 0; j < faceSize; ++j) {
		startPos = &png[ (width * (2*faceSize+j) + (faceSize))*comps ];
		memcpy( face, startPos, cpylen );
		face += faceSize3;
	}

	// front face

	face = faces[4];

	for (j = 0; j < faceSize; ++j) {
		startPos = &png[ (width * (faceSize+j) + (faceSize))*comps ];
		memcpy( face, startPos, cpylen );
		face += faceSize3;
	}

	// back face (vertical revert)

	face = faces[5];
	int c = 0;
	for (j = 0; j < faceSize; ++j) {
		for ( i = 0; i < faceSize; ++i) {
			p = (( width * (height-j-1) )+(2 * faceSize - (i + 1) ))*comps;
			face[c++] = png[ p ];
			face[c++] = png[ p+1 ];
			face[c++] = png[ p+2 ];
			face[c++] = png[ p+3 ];
		}
	}


	return faces;

}


