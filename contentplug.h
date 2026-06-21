// contentplug.h - TC Content Plugin Interface (subset needed for WDX)
#ifndef CONTENTPLUG_H
#define CONTENTPLUG_H

#define CONTENT_FIELD_TYPE_NUMERIC_32   0
#define CONTENT_FIELD_TYPE_NUMERIC_64   1
#define CONTENT_FIELD_TYPE_FLOAT        2
#define CONTENT_FIELD_TYPE_DATE         3
#define CONTENT_FIELD_TYPE_TIME         4
#define CONTENT_FIELD_TYPE_BOOLEAN      5
#define CONTENT_FIELD_TYPE_MULTIPLECHOICE 6
#define CONTENT_FIELD_TYPE_STRING       7
#define CONTENT_FIELD_TYPE_FULLTEXT     8
#define CONTENT_FIELD_TYPE_DATETIME     9

#define CONTENT_FIELD_FLAG_EDIT         1
#define CONTENT_FIELD_FLAG_SUBSTMASK    2
#define CONTENT_FIELD_FLAG_FIELDEDIT    4
#define CONTENT_FIELD_FLAG_WIDECHAR    16

#define CONTENT_NO_FIELD               -1
#define CONTENT_FILE_NOT_SUPPORTED     -2
#define CONTENT_ERROR                  -3
#define CONTENT_DELAYED                -4

#define CONTENT_FIELD_VALUE_UNKNOWN     0
#define CONTENT_FIELD_VALUE_OK          1
#define CONTENT_FIELD_VALUE_NOT_SUPPORTED -2
#define CONTENT_FIELD_VALUE_ERROR       -3

#ifdef _WIN32
  #define DCPCALL __stdcall
  #define DCPEXPORT __declspec(dllexport)
#else
  #define DCPCALL
  #define DCPEXPORT
#endif

typedef struct {
    int size;
    DWORD PluginInterfaceVersionLow;
    DWORD PluginInterfaceVersionHi;
    char DefaultIniName[MAX_PATH];
} ContentDefaultParamStruct;

#endif // CONTENTPLUG_H
