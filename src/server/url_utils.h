#ifndef URL_UTILS_H_
#define URL_UTILS_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 解析出来字符串(utf8编码), 参数str也保存解析后的字符串
 * @param str 解码
 * @param len 字符串长度
 */
int url_decode(char *str, int len);
char *url_decode2(char *str);
/**
 * url的utf8编码
 * @param s 原字符串
 * @param len 长度
 * @param new_length 编码之后的长度
 */
char *url_encode(char const *s, int len, int *new_length);

#ifdef __cplusplus
}
#endif

#endif /* URL_UTILS_H_ */
