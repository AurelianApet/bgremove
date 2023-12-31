// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the BGREMOVE_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// BGREMOVE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef BGREMOVE_EXPORTS
#define BGREMOVE_API __declspec(dllexport)
#else
#define BGREMOVE_API __declspec(dllimport)
#endif

// This class is exported from the bgremove.dll
//class BGREMOVE_API Cbgremove {
//public:
//	Cbgremove(void);
//	// TODO: add your methods here.
//};
//
//extern BGREMOVE_API int nbgremove;
//
//BGREMOVE_API int fnbgremove(void);


extern "C" BGREMOVE_API char*  __stdcall  fnbgRemove(char* base64img, int top, int left, int width, int height);