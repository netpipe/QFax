//* http://www.asterisk.org/doxygen/1.2/utils_8c-source.html */

size_t strnlen(const char *s, size_t n)
{
    size_t len;

    for (len=0; len < n; len++)
       if (s[len] == '\0')
          break;

    return len;
}

char *strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *new = malloc(len + 1);

    if (!new)
       return NULL;

    new[len] = '\0';
    return memcpy(new, s, len);
}
