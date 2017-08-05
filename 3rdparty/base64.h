#ifndef BASE64_H
#define BASE64_H

static inline size_t base64_raw_to_encoded_count(size_t len)
{
    // Base64 converts 3 bytes to 4 encoded characters
    size_t groups = (len + 2) / 3;
    return groups * 4;
}

static inline size_t base64_raw_to_unpadded_count(size_t len)
{
    // Base64 converts 3 bytes to 4 encoded characters
    size_t groups = len / 3;
    size_t normal_bytes = groups * 3;
    size_t extras = 0;
    if (normal_bytes != len)
        extras = len - normal_bytes + 1;

    return groups * 4 + extras;
}

size_t to_base64(char *dst, size_t dst_len, const void *src, size_t src_len);
const char *from_base64(void *dst, size_t *dst_len, const char *src);

#endif // BASE64_H
