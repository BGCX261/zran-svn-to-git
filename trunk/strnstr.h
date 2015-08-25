#include <string.h>

static char *strnstr(const char *haystack, const char *needle, size_t haystacklen)
{
    char *p;
    ssize_t plen;
    ssize_t len = strlen(needle);

    if (*needle == '\0')
	return (char*)haystack;

    plen = haystacklen;

    for (p = (char *)haystack; p != (char *)0; p = (char *)memchr(p + 1, *needle, plen-1))
    {
	plen = haystacklen - (p - haystack);

	if (plen < len)
	    return (char *)0;

	if (strncmp(p, needle, len) == 0)
	    return (p);
    }

    return (char*) 0;
}
