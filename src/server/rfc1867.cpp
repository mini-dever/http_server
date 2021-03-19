/*
 +----------------------------------------------------------------------+
 | PHP Version 5                                                        |
 +----------------------------------------------------------------------+
 | Copyright (c) 1997-2011 The PHP Group                                |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
 |          Jani Taskinen <jani@php.net>                                |
 +----------------------------------------------------------------------+
 */

/* $Id: rfc1867.c 312103 2011-06-12 15:14:18Z felipe $ */

/*
 *  This product includes software developed by the Apache Group
 *  for use in the Apache HTTP server project (http://www.apache.org/).
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "rfc1867.h"
#include "zend_llist.h"

/* The longest property name we use in an uploaded file array */
#define MAX_SIZE_OF_INDEX sizeof("[tmp_name]")

/* The longest anonymous name */
#define MAX_SIZE_ANONNAME 33

#define FILLUNIT (1024 * 24)

typedef struct {
	char *key;
	char *value;
} mime_header_entry;

inline char *MultipartParser::strtolower(char *s, size_t len) {
	unsigned char *c, *e;

	c = (unsigned char *) s;
	e = c + len;

	while (c < e) {
		*c = tolower(*c);
		c++;
	}
	return s;
}

/* {{{ Following code is based on apache_multipart_buffer.c from libapreq-0.33 package. */

/*
 * Fill up the buffer with client data.
 * Returns number of bytes added to buffer.
 */
int MultipartParser::fill_buffer() {
	int bytes_to_read, total_read = 0, actual_read = 0;

	/* shift the existing data if necessary */
	if (self->bytes_in_buffer > 0 && self->buf_begin != self->buffer) {
		memmove(self->buffer, self->buf_begin, self->bytes_in_buffer);
	}

	self->buf_begin = self->buffer;

	/* calculate the free space in the buffer */
	bytes_to_read = self->bufsize - self->bytes_in_buffer;

	/* read the required number of bytes */
	while (bytes_to_read > 0) {

		char *buf = self->buffer + self->bytes_in_buffer;

		actual_read = FCGX_GetStr(buf, bytes_to_read, self->stream_in);

		/* update the buffer length */
		if (actual_read > 0) {
			self->bytes_in_buffer += actual_read;
			self->read_post_bytes += actual_read;
			total_read += actual_read;
			bytes_to_read -= actual_read;
		} else {
			break;
		}
	}

	return total_read;
}

/* eof if we are out of bytes, or if we hit the final boundary */
inline int MultipartParser::multipart_buffer_eof() {
	if ((self->bytes_in_buffer == 0 && fill_buffer() < 1)) {
		return 1;
	} else {
		return 0;
	}
}

/* create new multipart_buffer structure */
MultipartParser::multipart_buffer *MultipartParser::multipart_buffer_new(
		char *boundary, int boundary_len) {
	self = (multipart_buffer *) malloc(sizeof(multipart_buffer));
	if (!self) {
		return NULL;
	}
	memset(self, 0, sizeof(multipart_buffer));

	int minsize = boundary_len + 6;
	if (minsize < FILLUNIT)
		minsize = FILLUNIT;

	self->buffer = (char *) malloc(minsize + 1);
	self->bufsize = minsize;

	self->boundary = (char *) malloc(boundary_len + 6);
	sprintf(self->boundary, "--%s", boundary);

	self->boundary_next = (char *) malloc(boundary_len + 6);
	self->boundary_next_len = sprintf(self->boundary_next, "\n--%s", boundary);

	self->buf_begin = self->buffer;
	self->bytes_in_buffer = 0;

	self->read_post_bytes = 0;
	return self;
}

/*
 * Gets the next CRLF terminated line from the input buffer.
 * If it doesn't find a CRLF, and the buffer isn't completely full, returns
 * NULL; otherwise, returns the beginning of the null-terminated line,
 * minus the CRLF.
 *
 * Note that we really just look for LF terminated lines. This works
 * around a bug in internet explorer for the macintosh which sends mime
 * boundaries that are only LF terminated when you use an image submit
 * button in a multipart/form-data form.
 */
char *MultipartParser::next_line() {
	/* look for LF in the data */
	char* line = self->buf_begin;
	char* ptr = (char*) memchr(self->buf_begin, '\n', self->bytes_in_buffer);

	if (ptr) { /* LF found */

		/* terminate the string, remove CRLF */
		if ((ptr - line) > 0 && *(ptr - 1) == '\r') {
			*(ptr - 1) = 0;
		} else {
			*ptr = 0;
		}

		/* bump the pointer */
		self->buf_begin = ptr + 1;
		self->bytes_in_buffer -= (self->buf_begin - line);

	} else { /* no LF found */

		/* buffer isn't completely full, fail */
		if (self->bytes_in_buffer < self->bufsize) {
			return NULL;
		}
		/* return entire buffer as a partial line */
		line[self->bufsize] = 0;
		self->buf_begin = ptr;
		self->bytes_in_buffer = 0;
	}

	return line;
}

/* Returns the next CRLF terminated line from the client */
inline char *MultipartParser::get_line() {
	char* ptr = next_line();

	if (!ptr) {
		fill_buffer();
		ptr = next_line();
	}

	return ptr;
}

/* Free header entry */
static void php_free_hdr_entry(mime_header_entry *h) {
	if (h->key) {
		free(h->key);
	}
	if (h->value) {
		free(h->value);
	}
}

/* finds a boundary */
inline int MultipartParser::find_boundary(char *boundary) {
	char *line;

	/* loop thru lines */
	while ((line = get_line())) {
		/* finished if we found the boundary */
		if (!strcmp(line, boundary)) {
			return 1;
		}
	}

	/* didn't find the boundary */
	return 0;
}

/* parse headers */
int MultipartParser::multipart_buffer_headers(zend_llist *header) {
	char *line;
	mime_header_entry prev_entry, entry;
	int prev_len, cur_len;

	memset(&prev_entry,0,sizeof(mime_header_entry));
	memset(&entry,0,sizeof(mime_header_entry));

	/* didn't find boundary, abort */
	if (!find_boundary(self->boundary)) {
		return 0;
	}

	/* get lines of text, or CRLF_CRLF */

	while ((line = get_line()) && strlen(line) > 0) {
		/* add header to table */
		char *key = line;
		char *value = NULL;

		/* space in the beginning means same header */
		if (!isspace(line[0])) {
			value = strchr(line, ':');
		}

		if (value) {
			*value = 0;
			do {
				value++;
			} while (isspace(*value));

			entry.value = strdup(value);
			entry.key = strdup(key);

		} else if (zend_llist_count(header)) { /* If no ':' on the line, add to previous line */

			prev_len = strlen(prev_entry.value);
			cur_len = strlen(line);

			entry.value = (char*) malloc(prev_len + cur_len + 1);
			memcpy(entry.value, prev_entry.value, prev_len);
			memcpy(entry.value + prev_len, line, cur_len);
			entry.value[cur_len + prev_len] = '\0';

			entry.key = strdup(prev_entry.key);

			zend_llist_remove_tail(header);
		} else {
			continue;
		}

		zend_llist_add_element(header, &entry);
		prev_entry = entry;
	}

	return 1;
}

inline char *MultipartParser::php_mime_get_hdr_value(zend_llist header,
		const char *key) {
	mime_header_entry *entry;

	if (key == NULL) {
		return NULL;
	}

	entry = (mime_header_entry*) zend_llist_get_first(&header);
	while (entry) {
		if (!strcasecmp(entry->key, key)) {
			return entry->value;
		}
		entry = (mime_header_entry*) zend_llist_get_next(&header);
	}

	return NULL;
}

char *MultipartParser::php_ap_getword(char **line, char stop) {
	char *pos = *line, quote;
	char *res;

	while (*pos && *pos != stop) {
		if ((quote = *pos) == '"' || quote == '\'') {
			++pos;
			while (*pos && *pos != quote) {
				if (*pos == '\\' && pos[1] && pos[1] == quote) {
					pos += 2;
				} else {
					++pos;
				}
			}
			if (*pos) {
				++pos;
			}
		} else
			++pos;
	}
	if (*pos == '\0') {
		res = strdup(*line);
		*line += strlen(*line);
		return res;
	}

	res = strndup(*line, pos - *line);

	while (*pos == stop) {
		++pos;
	}

	*line = pos;
	return res;
}

inline char *MultipartParser::substring_conf(char *start, int len, char quote) {
	char *result = (char*) malloc(len + 2);
	char *resp = result;
	int i;

	for (i = 0; i < len; ++i) {
		if (start[i] == '\\' && (start[i + 1] == '\\' || (quote && start[i + 1]
				== quote))) {
			*resp++ = start[++i];
		} else {
			*resp++ = start[i];
		}
	}

	*resp = '\0';
	return result;
}

char *MultipartParser::php_ap_getword_conf(char **line) {
	char *str = *line, *strend, *res, quote;

	while (*str && isspace(*str)) {
		++str;
	}

	if (!*str) {
		*line = str;
		return strdup("");
	}

	if ((quote = *str) == '"' || quote == '\'') {
		strend = str + 1;
		look_for_quote: while (*strend && *strend != quote) {
			if (*strend == '\\' && strend[1] && strend[1] == quote) {
				strend += 2;
			} else {
				++strend;
			}
		}
		if (*strend && *strend == quote) {
			char p = *(strend + 1);
			if (p != '\r' && p != '\n' && p != '\0') {
				strend++;
				goto look_for_quote;
			}
		}

		res = substring_conf(str + 1, strend - str - 1, quote);

		if (*strend == quote) {
			++strend;
		}

	} else {

		strend = str;
		while (*strend && !isspace(*strend)) {
			++strend;
		}
		res = substring_conf(str, strend - str, 0);
	}

	while (*strend && isspace(*strend)) {
		++strend;
	}

	*line = strend;
	return res;
}

/*
 * Search for a string in a fixed-length byte string.
 * If partial is true, partial matches are allowed at the end of the buffer.
 * Returns NULL if not found, or a pointer to the start of the first match.
 */
void *MultipartParser::php_ap_memstr(char *haystack, int haystacklen,
		char *needle, int needlen, int partial) {
	int len = haystacklen;
	char *ptr = haystack;

	/* iterate through first character matches */
	while ((ptr = (char*) memchr(ptr, needle[0], len))) {

		/* calculate length after match */
		len = haystacklen - (ptr - (char *) haystack);

		/* done if matches up to capacity of buffer */
		if (memcmp(needle, ptr, needlen < len ? needlen : len) == 0 && (partial
				|| len >= needlen)) {
			break;
		}

		/* next character */
		ptr++;
		len--;
	}

	return ptr;
}

/* read until a boundary condition */
int MultipartParser::multipart_buffer_read(char *buf, int bytes, int *end) {
	int len, max;
	char *bound;

	/* fill buffer if needed */
	if (bytes > self->bytes_in_buffer) {
		fill_buffer();
	}

	/* look for a potential boundary match, only read data up to that point */
	if ((bound = (char*) php_ap_memstr(self->buf_begin, self->bytes_in_buffer,
			self->boundary_next, self->boundary_next_len, 1))) {
		max = bound - self->buf_begin;
		if (end && php_ap_memstr(self->buf_begin, self->bytes_in_buffer,
				self->boundary_next, self->boundary_next_len, 0)) {
			*end = 1;
		}
	} else {
		max = self->bytes_in_buffer;
	}

	/* maximum number of bytes we are reading */
	len = max < bytes - 1 ? max : bytes - 1;

	/* if we read any data... */
	if (len > 0) {

		/* copy the data */
		memcpy(buf, self->buf_begin, len);
		buf[len] = 0;

		if (bound && len > 0 && buf[len - 1] == '\r') {
			buf[--len] = 0;
		}

		/* update the buffer */
		self->bytes_in_buffer -= len;
		self->buf_begin += len;
	}

	return len;
}

/*
 XXX: this is horrible memory-usage-wise, but we only expect
 to do this on small pieces of form data.
 */
char *MultipartParser::multipart_buffer_read_body(unsigned int *len) {
	char buf[FILLUNIT], *out = NULL;
	int total_bytes = 0, read_bytes = 0;

	while ((read_bytes = multipart_buffer_read(buf, sizeof(buf), NULL))) {
		out = (char*) realloc(out, total_bytes + read_bytes + 1);
		memcpy(out + total_bytes, buf, read_bytes);
		total_bytes += read_bytes;
	}

	if (out) {
		out[total_bytes] = '\0';
	}
	*len = total_bytes;

	return out;
}
/* }}} */

/*
 * The combined READER/HANDLER
 */
int MultipartParser::Parse(FCGX_Stream* in, const char *content_type,
		int upload_max_filesize, int file_uploads) /* {{{ */
{
    upload_max_filesize_ = upload_max_filesize;
	char *content_type_dup = strdup(content_type);
	char *boundary, *boundary_end = NULL;

	int boundary_len = 0, total_bytes = 0;
	UploadError cancel_upload = UPLOAD_ERROR_OK;
	int max_file_size = 0, skip_upload = 0, anonindex = 0;
	//, is_anonymous;

	zend_llist header;
	int upload_cnt = file_uploads;

	/* Get the boundary */
	boundary = strstr(content_type_dup, "boundary");
	if (!boundary) {
		int content_type_len = strlen(content_type_dup);
		char *content_type_lcase = strndup(content_type_dup, content_type_len);

		strtolower(content_type_lcase, content_type_len);
		boundary = strstr(content_type_lcase, "boundary");
		if (boundary) {
			boundary = content_type_dup + (boundary - content_type_lcase);
		}
		free(content_type_lcase);
	}

	if (!boundary || !(boundary = strchr(boundary, '='))) {
		//printf("Missing boundary in multipart/form-data POST data");
		return -1;
	}

	boundary++;
	boundary_len = strlen(boundary);

	if (boundary[0] == '"') {
		boundary++;
		boundary_end = strchr(boundary, '"');
		if (!boundary_end) {
			//printf("Invalid boundary in multipart/form-data POST data");
			return -1;
		}
	} else {
		/* search for the end of the boundary */
		boundary_end = strchr(boundary, ',');
	}
	if (boundary_end) {
		boundary_end[0] = '\0';
		boundary_len = boundary_end - boundary;
	}

	/* Initialize the buffer */
	if (!(multipart_buffer_new(boundary, boundary_len))) {
		//printf("Unable to initialize the input buffer");
		return -2;
	}
	free(content_type_dup);
	self->stream_in = in;

	zend_llist_init(&header, sizeof(mime_header_entry),
			(llist_dtor_func_t) php_free_hdr_entry, 0);

	multipart_event_start event_start;

	if (OnEvent(MULTIPART_EVENT_START, &event_start) == false) {
		goto fileupload_done;
	}

	while (!multipart_buffer_eof()) {
		char buff[FILLUNIT];
		char *cd = NULL, *param = NULL, *filename = NULL, *tmp = NULL;
		size_t blen = 0;
		off_t offset;

		zend_llist_clean(&header);

		if (!multipart_buffer_headers(&header)) {
			goto fileupload_done;
		}

		if ((cd = php_mime_get_hdr_value(header, "Content-Disposition"))) {
			char *pair = NULL;
			int end = 0;

			while (isspace(*cd)) {
				++cd;
			}

			while (*cd && (pair = php_ap_getword(&cd, ';'))) {
				char *key = NULL, *word = pair;

				while (isspace(*cd)) {
					++cd;
				}

				if (strchr(pair, '=')) {
					key = php_ap_getword(&pair, '=');

					if (!strcasecmp(key, "name")) {
						if (param) {
							free(param);
						}
						param = php_ap_getword_conf(&pair);
					} else if (!strcasecmp(key, "filename")) {
						if (filename) {
							free(filename);
						}
						filename = php_ap_getword_conf(&pair);
					}
				}
				if (key) {
					free(key);
				}
				free(word);
			}

			/* Normal form variable, safe to read all data into memory */
			if (!filename && param) {
				unsigned int value_len;
				char *value = multipart_buffer_read_body(&value_len);
				//unsigned int new_val_len; /* Dummy variable */

				if (!value) {
					value = strdup("");
				}

				multipart_event_formdata event_formdata;

				event_formdata.post_bytes_processed = self->read_post_bytes;
				event_formdata.name = param;
				event_formdata.value = &value;
				event_formdata.length = value_len;
				event_formdata.newlength = NULL;
				OnEvent(MULTIPART_EVENT_FORMDATA, &event_formdata);

				if (!strcasecmp(param, "MAX_FILE_SIZE")) {
					max_file_size = atol(value);
				}

				free(param);
				free(value);
				continue;
			}

			/* If file_uploads=off, skip the file part */
			if (!file_uploads) {
				skip_upload = 1;
			} else if (upload_cnt <= 0) {
				skip_upload = 1;
				printf(
						"Maximum number of allowable file uploads has been exceeded");
			}

			/* Return with an error if the posted data is garbled */
			if (!param && !filename) {
				printf("File Upload Mime headers garbled");
				goto fileupload_done;
			}

			if (!param) {
				//is_anonymous = 1;
				param = (char*) malloc(MAX_SIZE_ANONNAME);
				snprintf(param, MAX_SIZE_ANONNAME, "%u", anonindex++);
			} else {
				//is_anonymous = 0;
			}

			/* New Rule: never repair potential malicious user input */
			if (!skip_upload) {
				long c = 0;
				tmp = param;

				while (*tmp) {
					if (*tmp == '[') {
						c++;
					} else if (*tmp == ']') {
						c--;
						if (tmp[1] && tmp[1] != '[') {
							skip_upload = 1;
							break;
						}
					}
					if (c < 0) {
						skip_upload = 1;
						break;
					}
					tmp++;
				}
			}

			total_bytes = 0;
			cancel_upload = UPLOAD_ERROR_OK;

			if (!skip_upload) {
				multipart_event_file_start event_file_start;

				event_file_start.post_bytes_processed = self->read_post_bytes;
				event_file_start.name = param;
				event_file_start.filename = &filename;
				if (OnEvent(MULTIPART_EVENT_FILE_START, &event_file_start)
						== false) {
					free(param);
					free(filename);
					continue;
				}
			}

			if (skip_upload) {
				free(param);
				free(filename);
				continue;
			}

			if (strlen(filename) == 0) {
				cancel_upload = UPLOAD_ERROR_D;
			}

			offset = 0;
			end = 0;

			if (!cancel_upload) {
				/* only bother to open temp file if we have data */
				blen = multipart_buffer_read(buff, sizeof(buff), &end);
				/* in non-debug mode we have no problem with 0-length files */
			}

			while (!cancel_upload && (blen > 0)) {
				multipart_event_file_data event_file_data;

				event_file_data.post_bytes_processed = self->read_post_bytes;
				event_file_data.offset = offset;
				event_file_data.data = buff;
				event_file_data.length = blen;
				event_file_data.newlength = &blen;
				if (OnEvent(MULTIPART_EVENT_FILE_DATA, &event_file_data)
						== false) {
					cancel_upload = UPLOAD_ERROR_X;
					continue;
				}

				if (upload_max_filesize_ > 0 && (total_bytes + blen)
						> (size_t)upload_max_filesize_) {
					cancel_upload = UPLOAD_ERROR_A;
				} else if (max_file_size && ((total_bytes + blen)
						> (size_t)max_file_size)) {
					cancel_upload = UPLOAD_ERROR_B;
				} else if (blen > 0) {
					offset += blen;
				}

				/* read data for next iteration */
				blen = multipart_buffer_read(buff, sizeof(buff), &end);
			}

			if (!cancel_upload && !end) {
				cancel_upload = UPLOAD_ERROR_C;
			}
			multipart_event_file_end event_file_end;

			event_file_end.post_bytes_processed = self->read_post_bytes;
			event_file_end.cancel_upload = cancel_upload;
			if (OnEvent(MULTIPART_EVENT_FILE_END, &event_file_end) == false) {
				cancel_upload = UPLOAD_ERROR_X;
			}

			free(param);
			if (filename) {
				free(filename);
			}
		}
	}

	fileupload_done: multipart_event_end event_end;

	event_end.post_bytes_processed = self->read_post_bytes;
	OnEvent(MULTIPART_EVENT_END, &event_end);

	zend_llist_destroy(&header);
	if (self->boundary_next)
		free(self->boundary_next);
	if (self->boundary)
		free(self->boundary);
	if (self->buffer)
		free(self->buffer);
	if (self)
		free(self);
	return 0;
}
/* }}} */

