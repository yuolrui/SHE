#ifndef SHE_MIME_H
#define SHE_MIME_H

const char *she_mime_type_get(const char *filename);

/* Compatibility macro */
#define mime_type_get she_mime_type_get

#endif /* SHE_MIME_H */
