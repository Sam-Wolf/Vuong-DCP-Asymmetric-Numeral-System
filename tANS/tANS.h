#include <Vfw.h>
#define DllExport   __declspec( dllexport )  


// Encode Function
extern "C" DllExport  unsigned int Encode(const unsigned char * __restrict in, unsigned char * __restrict out, const unsigned int length);

// Decode Funktion
extern "C" DllExport void Decode(const unsigned char * __restrict in, unsigned char * __restrict out, const unsigned int length);

// Compression Method ID
extern "C" DllExport unsigned long  getCompressID() {
	return MAKEFOURCC('T', 'A', 'N', 'S');
};

// Compression Method Name 
extern "C"  DllExport const char * getCompressName() {
	return "Asymmetric Numeral System(tANS)";
};


