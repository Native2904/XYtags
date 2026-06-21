/**
 * xytags_stub32.cpp  -  32-Bit Stub fuer xytags.wdx
 *
 * Dieser Stub registriert die drei Felder als String-Typ bei TC.
 * Die eigentliche Arbeit macht xytags.wdx64 (64-Bit).
 * Beide Dateien muessen im selben Verzeichnis liegen.
 *
 * Build (MinGW-w64 mit Win32+Win64 Support):
 *   g++ -m32 -shared -O2 -o xytags.wdx xytags_stub32.cpp
 *       -static -static-libgcc -static-libstdc++ -lkernel32 -Wl,--kill-at -s
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CONTENT_FIELD_TYPE_STRING        8   // ft_string (war faelschlich 7 = ft_multiplechoice)
#define CONTENT_NO_FIELD                -1
#define CONTENT_FILE_NOT_SUPPORTED      -2
#define CONTENT_FIELD_VALUE_NOT_SUPPORTED -2

typedef struct {
    int   size;
    DWORD PluginInterfaceVersionLow;
    DWORD PluginInterfaceVersionHi;
    char  DefaultIniName[MAX_PATH];
} ContentDefaultParamStruct;

static const char* g_fields[] = { "XY_Label", "XY_Tags", "XY_Comment" };

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) { (void)h;(void)r; return TRUE; }

extern "C" {

BOOL __declspec(dllexport) __stdcall
ContentGetDetectString(char* DetectString, int maxlen)
{
    lstrcpynA(DetectString, "", maxlen);
    return TRUE;
}

int __declspec(dllexport) __stdcall
ContentGetSupportedField(int FieldIndex, char* FieldName, char* Units, int maxlen)
{
    if (FieldIndex < 0 || FieldIndex > 2) return CONTENT_NO_FIELD;
    lstrcpynA(FieldName, g_fields[FieldIndex], maxlen);
    Units[0] = '\0';
    return CONTENT_FIELD_TYPE_STRING;
}

int __declspec(dllexport) __stdcall
ContentGetSupportedFieldFlags(int FieldIndex) { (void)FieldIndex; return 0; }

int __declspec(dllexport) __stdcall
ContentGetValue(char* FileName, int FieldIndex, int UnitIndex,
                void* FieldValue, int maxlen, int flags)
{
    (void)FileName;(void)FieldIndex;(void)UnitIndex;
    (void)FieldValue;(void)maxlen;(void)flags;
    return CONTENT_FILE_NOT_SUPPORTED;
}

void __declspec(dllexport) __stdcall
ContentSetDefaultParams(ContentDefaultParamStruct* dps) { (void)dps; }

void __declspec(dllexport) __stdcall
ContentPluginUnloading(void) {}

} // extern "C"
