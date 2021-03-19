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
 | Author:                                                              |
 +----------------------------------------------------------------------+
 */

/* $Id: rfc1867.h 306939 2011-01-01 02:19:59Z felipe $ */

#ifndef RFC1867_H
#define RFC1867_H

#include "fcgiapi.h"

#include "zend_llist.h"

struct FCGI_FILE;
/**
 * 解析rfc1867 规定的multipart MIME。
 * 主要解析代码来自PHP5，去掉了不许要的部分如PHP变量维护，并修改了内存分配代码。
 * 并把代码修改成C++方式，以方便C++代码调用。
 */
class MultipartParser {
public:
	enum EventType{
		MULTIPART_EVENT_START 		= 0,
		MULTIPART_EVENT_FORMDATA 	= 1,
		MULTIPART_EVENT_FILE_START	= 2,
		MULTIPART_EVENT_FILE_DATA	= 3,
		MULTIPART_EVENT_FILE_END		= 4,
		MULTIPART_EVENT_END 			= 5
	};
	enum UploadError{
		/* Errors */
		UPLOAD_ERROR_OK   =0,  /* File upload succesful */
		UPLOAD_ERROR_A    =1,  /* Uploaded file exceeded upload_max_filesize */
		UPLOAD_ERROR_B    =2,  /* Uploaded file exceeded MAX_FILE_SIZE */
		UPLOAD_ERROR_C    =3,  /* Partially uploaded */
		UPLOAD_ERROR_D    =4,  /* No file uploaded */
		UPLOAD_ERROR_E    =6,  /* Missing /tmp or similar directory */
		UPLOAD_ERROR_F    =7,  /* Failed to write file to disk */
		UPLOAD_ERROR_X    =8   /* File upload stopped by extension */
	};
	/**
	 * 解析函数。
	 * @param fp 要解析的文件流
	 * @param content_type HTTP报文的ContentType, 里面包含了rfc1867规定的boundary
	 * @param upload_max_filesize 允许上传的最大文件大小,默认300k。允许OnEvent函数中途改变对应的大小
	 * @param file_uploads 最大文件数
	 * @return 返回0表示解析成功，
	 *             否则： -1，content_type错误，找不到boundary
	 *                    -2，分配内存失败
	 *
	 */
	int Parse(FCGX_Stream *in, const char *content_type,
			int upload_max_filesize = 1024*300, int file_uploads = 10);

	void SetUploadMaxFileSize(int max_size) { upload_max_filesize_ = max_size; }

	/**
	 * 回调函数。
	 * 解析过程会生成对应事件，和事件类型对应的数据。
	 * 返回false,会终结整个解析过程。
	 */
	virtual bool OnEvent(EventType event, void *event_data)=0;

	typedef struct _multipart_event_start {
		size_t content_length;
	} multipart_event_start;

	typedef struct _multipart_event_formdata {
		size_t post_bytes_processed;
		char *name;
		char **value;
		size_t length;
		size_t *newlength;
	} multipart_event_formdata;

	typedef struct _multipart_event_file_start {
		size_t post_bytes_processed;
		char *name;
		char **filename;
	} multipart_event_file_start;

	typedef struct _multipart_event_file_data {
		size_t post_bytes_processed;
		off_t offset;
		char *data;
		size_t length;
		size_t *newlength;
	} multipart_event_file_data;

	typedef struct _multipart_event_file_end {
		size_t post_bytes_processed;
		UploadError cancel_upload;
	} multipart_event_file_end;

	typedef struct _multipart_event_end {
		size_t post_bytes_processed;
	} multipart_event_end;
protected:
	int upload_max_filesize_;
private:
	typedef struct {
		/* read buffer */
		char *buffer;
		char *buf_begin;
		int bufsize;
		int bytes_in_buffer;

		/* boundary info */
		char *boundary;
		char *boundary_next;
		int boundary_next_len;

		FCGX_Stream* stream_in;
		int read_post_bytes;
	} multipart_buffer;

	char *strtolower(char *s, size_t len);
	int fill_buffer();
	int multipart_buffer_eof();
	multipart_buffer *multipart_buffer_new(char *boundary, int boundary_len);
	char *next_line();
	char *get_line();
	int find_boundary(char *boundary);
	int multipart_buffer_headers(zend_llist *header);
	char *php_mime_get_hdr_value(zend_llist header, const char *key);
	char *php_ap_getword(char **line, char stop);
	inline char *substring_conf(char *start, int len, char quote);
	char *php_ap_getword_conf(char **line);
	void *php_ap_memstr(char *haystack, int haystacklen, char *needle,
			int needlen, int partial);
	int multipart_buffer_read(char *buf, int bytes, int *end);
	char *multipart_buffer_read_body(unsigned int *len);

	multipart_buffer *self;
};

#endif /* RFC1867_H */
