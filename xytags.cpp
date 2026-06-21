/**
 * xytags.wdx / xytags.wdx64  –  Total Commander Content Plugin
 *
 * Liest XYplorer tag.dat und stellt drei Felder bereit:
 *   XY_Label    – Label-Name  (z.B. "Tcmd")
 *   XY_Tags     – Tags        (z.B. "app, portable")
 *   XY_Comment  – Kommentar
 *
 * Vollstaendig ohne STL / CRT-Abhaengigkeiten.
 * Nur WinAPI + statische Laufzeit.
 *
 * Build:
 *   g++ -shared -O2 -o xytags.wdx64 xytags.cpp
 *       -static -static-libgcc -static-libstdc++
 *       -lkernel32 -lshlwapi -Wl,--kill-at -s
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>

// ---------------------------------------------------------------------------
// TC Content Plugin Konstanten
// ---------------------------------------------------------------------------
// Feldtypen - exakt die Werte aus dem offiziellen contentplug.h (ghisler).
// ACHTUNG: ft_string == 8, NICHT 7. Die 7 ist ft_multiplechoice!
#define CONTENT_FIELD_TYPE_STRING          8   // ft_string  (ANSI-Puffer)
#define CONTENT_FIELD_TYPE_STRINGW        11   // ft_stringw (Wide-Puffer)
#define CONTENT_NO_FIELD                  -1   // ft_nosuchfield
#define CONTENT_FILE_NOT_SUPPORTED        -2   // ft_fileerror
#define CONTENT_FIELD_VALUE_NOT_SUPPORTED -2
#define CONTENT_DELAYED                   -3
#define CONTENT_FIELD_FLAG_WIDECHAR       16

typedef struct {
    int   size;
    DWORD PluginInterfaceVersionLow;
    DWORD PluginInterfaceVersionHi;
    char  DefaultIniName[MAX_PATH];
} ContentDefaultParamStruct;

// ---------------------------------------------------------------------------
// Einfacher Heap-String (kein STL)
// ---------------------------------------------------------------------------
struct WStr {
    wchar_t* p;
    WStr()                    : p(NULL) {}
    WStr(const wchar_t* s)    : p(NULL) { Set(s); }
    ~WStr()                             { Free(); }
    void Free()  { if (p) { HeapFree(GetProcessHeap(), 0, p); p = NULL; } }
    void Set(const wchar_t* s) {
        Free();
        if (!s) return;
        SIZE_T n = (lstrlenW(s) + 1) * sizeof(wchar_t);
        p = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, n);
        if (p) CopyMemory(p, s, n);
    }
    bool Empty() const { return !p || p[0] == L'\0'; }
private:
    WStr(const WStr&);
    WStr& operator=(const WStr&);
};

// ---------------------------------------------------------------------------
// Einfache Hash-Map: Pfad (uppercase) -> Record
// Open addressing, power-of-2 Groesse
// ---------------------------------------------------------------------------
struct Record {
    wchar_t* key;      // uppercase Pfad, NULL = freier Slot
    wchar_t* label;
    wchar_t* tags;
    wchar_t* comment;
};

static wchar_t* HAlloc(const wchar_t* s, int len = -1)
{
    if (!s) return NULL;
    if (len < 0) len = lstrlenW(s);
    wchar_t* p = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(wchar_t));
    if (p) { CopyMemory(p, s, len * sizeof(wchar_t)); p[len] = L'\0'; }
    return p;
}

static void HFree(wchar_t*& p) { if (p) { HeapFree(GetProcessHeap(), 0, p); p = NULL; } }

// FNV-1a 64-bit hash
static DWORD HashW(const wchar_t* s)
{
    DWORD h = 2166136261u;
    while (*s) { h ^= (BYTE)*s++; h *= 16777619u; }
    return h;
}

struct HashMap {
    Record*  slots;
    DWORD    cap;
    DWORD    count;

    HashMap() : slots(NULL), cap(0), count(0) {}

    void Init(DWORD initialCap) {
        cap = initialCap;
        slots = (Record*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                    cap * sizeof(Record));
    }

    void Clear() {
        if (!slots) return;
        for (DWORD i = 0; i < cap; i++) {
            if (slots[i].key) {
                HFree(slots[i].key);
                HFree(slots[i].label);
                HFree(slots[i].tags);
                HFree(slots[i].comment);
            }
        }
        ZeroMemory(slots, cap * sizeof(Record));
        count = 0;
    }

    void Free() {
        Clear();
        if (slots) { HeapFree(GetProcessHeap(), 0, slots); slots = NULL; }
        cap = 0;
    }

    void Insert(const wchar_t* key, const wchar_t* label,
                const wchar_t* tags, const wchar_t* comment)
    {
        if (!slots || count >= cap * 3 / 4) return; // ignore overflow
        DWORD idx = HashW(key) & (cap - 1);
        while (slots[idx].key) {
            if (lstrcmpW(slots[idx].key, key) == 0) return; // duplicate
            idx = (idx + 1) & (cap - 1);
        }
        slots[idx].key     = HAlloc(key);
        slots[idx].label   = (label   && label[0])   ? HAlloc(label)   : NULL;
        slots[idx].tags    = (tags    && tags[0])    ? HAlloc(tags)    : NULL;
        slots[idx].comment = (comment && comment[0]) ? HAlloc(comment) : NULL;
        count++;
    }

    const Record* Find(const wchar_t* key) const {
        if (!slots || count == 0) return NULL;
        DWORD idx = HashW(key) & (cap - 1);
        for (DWORD i = 0; i < cap; i++) {
            if (!slots[idx].key) return NULL;
            if (lstrcmpW(slots[idx].key, key) == 0) return &slots[idx];
            idx = (idx + 1) & (cap - 1);
        }
        return NULL;
    }
};

// ---------------------------------------------------------------------------
// Globaler Zustand
// ---------------------------------------------------------------------------
static HashMap  g_map;
static wchar_t  g_labels[32][64];   // max 32 Labels, je 63 Zeichen
static int      g_labelCount = 0;
static char     g_iniPath[MAX_PATH] = "";
static char     g_tagPath[MAX_PATH] = "";
static bool     g_loaded = false;
static HANDLE   g_mutex  = NULL;

#define FIELD_LABEL   0
#define FIELD_TAGS    1
#define FIELD_COMMENT 2

static const char* g_fieldNames[] = { "XY_Label", "XY_Tags", "XY_Comment" };

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------
static wchar_t* StrToWide(const char* s, UINT cp = CP_UTF8)
{
    if (!s || !*s) return NULL;
    int n = MultiByteToWideChar(cp, 0, s, -1, NULL, 0);
    wchar_t* w = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, n * sizeof(wchar_t));
    if (w) MultiByteToWideChar(cp, 0, s, -1, w, n);
    return w;
}

// In-place Uppercase (Windows)
static void UpperInPlace(wchar_t* s)
{
    if (s) CharUpperBuffW(s, lstrlenW(s));
}

// Zeiger auf n-tes Pipe-Feld in einer Zeile (0-basiert)
// Gibt Laenge in *outLen zurueck, Zeiger auf Anfang des Feldes
static const wchar_t* GetField(const wchar_t* line, int fieldIdx, int* outLen)
{
    const wchar_t* p = line;
    for (int f = 0; f < fieldIdx; f++) {
        while (*p && *p != L'|') p++;
        if (*p == L'|') p++; else { *outLen = 0; return p; }
    }
    const wchar_t* start = p;
    while (*p && *p != L'|' && *p != L'\n' && *p != L'\r') p++;
    *outLen = (int)(p - start);
    return start;
}

// ---------------------------------------------------------------------------
// tag.dat parsen
// ---------------------------------------------------------------------------
static void ParseTagFile(const char* path)
{
    g_map.Clear();
    g_labelCount = 0;
    ZeroMemory(g_labels, sizeof(g_labels));

    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD sz = GetFileSize(hFile, NULL);
    if (sz < 4) { CloseHandle(hFile); return; }

    BYTE* raw = (BYTE*)HeapAlloc(GetProcessHeap(), 0, sz + 4);
    if (!raw) { CloseHandle(hFile); return; }

    DWORD rd = 0;
    ReadFile(hFile, raw, sz, &rd, NULL);
    CloseHandle(hFile);
    ZeroMemory(raw + rd, 4);

    // UTF-16LE mit BOM
    wchar_t* text = (wchar_t*)raw;
    DWORD wlen = rd / 2;
    if (wlen > 0 && text[0] == 0xFEFF) { text++; wlen--; }

    // Anzahl Eintraege grob schaetzen fuer HashMap-Groesse
    DWORD lineCount = 0;
    for (DWORD i = 0; i < wlen; i++) if (text[i] == L'\n') lineCount++;

    // HashMap mit naechster Zweierpotenz >= lineCount*2 initialisieren
    DWORD cap = 1024;
    while (cap < lineCount * 2) cap <<= 1;
    g_map.Init(cap);

    enum { SEC_NONE, SEC_LABELS, SEC_EXTRA, SEC_DATA } sec = SEC_NONE;

    wchar_t* pos = text;
    wchar_t* end = text + wlen;

    while (pos < end) {
        // Zeilenende finden
        wchar_t* lineStart = pos;
        while (pos < end && *pos != L'\n') pos++;
        wchar_t* lineEnd = pos;
        if (pos < end) pos++; // '\n' ueberspringen

        // '\r' am Ende entfernen
        if (lineEnd > lineStart && *(lineEnd-1) == L'\r') lineEnd--;
        int lineLen = (int)(lineEnd - lineStart);
        if (lineLen == 0) continue;

        // Abschnitts-Header erkennen (kurze Zeilen ohne '|')
        if (lineLen == 7  && wcsncmp(lineStart, L"Labels:", 7)     == 0) { sec = SEC_LABELS; continue; }
        if (lineLen == 11 && wcsncmp(lineStart, L"Extra Tags:",11)  == 0) { sec = SEC_EXTRA;  continue; }
        if (lineLen == 5  && wcsncmp(lineStart, L"Data:", 5)        == 0) { sec = SEC_DATA;   continue; }

        if (sec == SEC_LABELS) {
            // Format: Name|FG|BG;Name2|FG|BG;...  (1-basiert)
            g_labelCount = 1; // Index 0 = kein Label
            const wchar_t* p = lineStart;
            while (p < lineEnd && g_labelCount < 32) {
                // Name bis '|' oder ';' oder Zeilenende
                const wchar_t* nameStart = p;
                while (p < lineEnd && *p != L'|' && *p != L';') p++;
                int nameLen = (int)(p - nameStart);
                if (nameLen > 63) nameLen = 63;
                wcsncpy(g_labels[g_labelCount], nameStart, nameLen);
                g_labels[g_labelCount][nameLen] = L'\0';
                g_labelCount++;
                // Rest des Eintrags bis ';' ueberspringen
                while (p < lineEnd && *p != L';') p++;
                if (p < lineEnd) p++; // ';' ueberspringen
            }
            sec = SEC_NONE;
            continue;
        }

        if (sec == SEC_DATA) {
            // Feld 0: Pfad
            int f0len = 0;
            const wchar_t* f0 = GetField(lineStart, 0, &f0len);
            if (f0len == 0) continue;

            // Pfad als uppercase-Key
            wchar_t* key = HAlloc(f0, f0len);
            if (!key) continue;
            UpperInPlace(key);

            // Feld 1: Label-Index
            int f1len = 0;
            const wchar_t* f1 = GetField(lineStart, 1, &f1len);
            wchar_t labelBuf[64] = L"";
            if (f1len > 0) {
                int idx = 0;
                for (int i = 0; i < f1len; i++) idx = idx*10 + (f1[i]-L'0');
                if (idx > 0 && idx < g_labelCount)
                    lstrcpyW(labelBuf, g_labels[idx]);
            }

            // Feld 2: Tags
            int f2len = 0;
            const wchar_t* f2 = GetField(lineStart, 2, &f2len);

            // Feld 19: Kommentar (letztes der 20 Felder, Index 19)
            int f19len = 0;
            const wchar_t* f19 = GetField(lineStart, 19, &f19len);

            // Temporaere Puffer
            wchar_t* tags    = f2len  > 0 ? HAlloc(f2,  f2len)  : NULL;
            wchar_t* comment = f19len > 0 ? HAlloc(f19, f19len) : NULL;

            g_map.Insert(key, labelBuf, tags, comment);

            HFree(key);
            HFree(tags);
            HFree(comment);
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    g_loaded = true;
}

static void EnsureLoaded()
{
    if (g_loaded) return;
    if (g_tagPath[0]) ParseTagFile(g_tagPath);
}

// ---------------------------------------------------------------------------
// DLL Entry
// ---------------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        g_mutex = CreateMutexA(NULL, FALSE, NULL);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_map.Free();
        if (g_mutex) { CloseHandle(g_mutex); g_mutex = NULL; }
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// TC Content Plugin API
// ---------------------------------------------------------------------------
extern "C" {

BOOL __declspec(dllexport) __stdcall
ContentGetDetectString(char* DetectString, int maxlen)
{
    lstrcpynA(DetectString, "", maxlen);  // leer = immer aktiv
    return TRUE;
}

int __declspec(dllexport) __stdcall
ContentGetSupportedField(int FieldIndex, char* FieldName, char* Units, int maxlen)
{
    if (FieldIndex < 0 || FieldIndex > FIELD_COMMENT) return CONTENT_NO_FIELD;
    lstrcpynA(FieldName, g_fieldNames[FieldIndex], maxlen);
    Units[0] = '\0';
    return CONTENT_FIELD_TYPE_STRING;
}

int __declspec(dllexport) __stdcall
ContentGetSupportedFieldFlags(int FieldIndex)
{
    (void)FieldIndex;
    return 0;
}



// TC ruft ContentGetValueW auf wenn es Wide will (neuere TC-Versionen)
// Wir implementieren beide: GetValue (ANSI) und GetValueW (Wide)

// Statischer Puffer fuer ContentGetValueW Rueckgabe
static wchar_t g_retBuf[2048];

static int GetValueImpl(const wchar_t* fileNameW, int FieldIndex, void* FieldValue, int maxlen, bool wideOut)
{
    if (FieldIndex < FIELD_LABEL || FieldIndex > FIELD_COMMENT)
        return CONTENT_FIELD_VALUE_NOT_SUPPORTED;

    WaitForSingleObject(g_mutex, INFINITE);
    EnsureLoaded();
    ReleaseMutex(g_mutex);

    wchar_t keyBuf[MAX_PATH * 2];
    lstrcpynW(keyBuf, fileNameW, MAX_PATH * 2);

    UpperInPlace(keyBuf);
    const Record* rec = g_map.Find(keyBuf);
    if (!rec) return CONTENT_FILE_NOT_SUPPORTED;

    const wchar_t* val = NULL;
    switch (FieldIndex) {
        case FIELD_LABEL:   val = rec->label;   break;
        case FIELD_TAGS:    val = rec->tags;     break;
        case FIELD_COMMENT: val = rec->comment;  break;
    }
    if (!val || val[0] == L'\0') return CONTENT_FILE_NOT_SUPPORTED;

    if (wideOut) {
        // TC uebergibt einen wchar_t-Buffer der maxlen BYTES gross ist
        wchar_t* wbuf = (wchar_t*)FieldValue;
        lstrcpynW(wbuf, val, maxlen / (int)sizeof(wchar_t));
    } else {
        char* abuf = (char*)FieldValue;
        WideCharToMultiByte(CP_ACP, 0, val, -1, abuf, maxlen - 1, NULL, NULL);
        abuf[maxlen - 1] = '\0';
    }
    // WICHTIG: TC interpretiert den RUECKGABEWERT als Feldtyp.
    // Bei ContentGetValueW muss ft_stringw (Wide) zurueck, sonst ft_string.
    return wideOut ? CONTENT_FIELD_TYPE_STRINGW : CONTENT_FIELD_TYPE_STRING;
}

int __declspec(dllexport) __stdcall
ContentGetValue(char* FileName, int FieldIndex, int UnitIndex,
                void* FieldValue, int maxlen, int flags)
{
    (void)FileName; (void)FieldIndex; (void)UnitIndex;
    (void)FieldValue; (void)maxlen; (void)flags;
    // CONTENT_DELAYED: TC soll ContentGetValueW aufrufen
    return CONTENT_DELAYED;
}

// ContentGetValueW - TC 64-bit ruft diese Variante auf wenn verfuegbar
int __declspec(dllexport) __stdcall
ContentGetValueW(wchar_t* FileName, int FieldIndex, int UnitIndex,
                 void* FieldValue, int maxlen, int flags)
{
    (void)UnitIndex; (void)flags;
    // maxlen ist die Puffergroesse in BYTES (gilt auch fuer den Wide-Puffer)
    return GetValueImpl(FileName, FieldIndex, FieldValue, maxlen, true);
}

void __declspec(dllexport) __stdcall
ContentSetDefaultParams(ContentDefaultParamStruct* dps)
{
    if (!dps) return;
    lstrcpynA(g_iniPath, dps->DefaultIniName, MAX_PATH);
    GetPrivateProfileStringA("XYTags", "TagFile", "",
                             g_tagPath, MAX_PATH, g_iniPath);
    if (g_tagPath[0]) {
        WaitForSingleObject(g_mutex, INFINITE);
        ParseTagFile(g_tagPath);
        ReleaseMutex(g_mutex);
    }
}

void __declspec(dllexport) __stdcall
ContentPluginUnloading(void)
{
    g_map.Free();
    g_loaded = false;
}

} // extern "C"
