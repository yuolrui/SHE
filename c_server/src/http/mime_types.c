#include "mime_types.h"
#include <string.h>
#include <strings.h>

typedef struct {
    const char *extension;
    const char *mime_type;
} mime_entry_t;

static const mime_entry_t mime_table[] = {
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css; charset=utf-8"},
    {".js",   "application/javascript; charset=utf-8"},
    {".mjs",  "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".txt",  "text/plain; charset=utf-8"},
    {".c",    "text/plain; charset=utf-8"},
    {".h",    "text/plain; charset=utf-8"},
    {".md",   "text/markdown; charset=utf-8"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".svg",  "image/svg+xml"},
    {".webp", "image/webp"},
    {".ico",  "image/x-icon"},
    {".pdf",  "application/pdf"},
    {".zip",  "application/zip"},
    {".tar",  "application/x-tar"},
    {".gz",   "application/gzip"},
    {".mp3",  "audio/mpeg"},
    {".mp4",  "video/mp4"},
    {".webm", "video/webm"},
    {".woff", "font/woff"},
    {".woff2","font/woff2"},
    {".ttf",  "font/ttf"},
    {".wasm", "application/wasm"},
    {NULL, NULL}
};

const char *mime_type_get(const char *path) {
    if (!path) return "application/octet-stream";
    
    const char *dot = strrchr(path, '.');
    if (!dot) return "text/plain";
    
    for (int i = 0; mime_table[i].extension != NULL; i++) {
        if (strcasecmp(dot, mime_table[i].extension) == 0) {
            return mime_table[i].mime_type;
        }
    }
    
    return "application/octet-stream";
}
