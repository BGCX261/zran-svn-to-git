#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>

#include "libzran.h"
#include "strnstr.h"

#define MAX_LENGTH (1024*1024)
#define BUF_LENGTH (4*1024)

my_bool ze_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void ze_deinit(UDF_INIT *initid);
char *ze(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);

struct zran_ptr
{
    struct zran *zran;
    char *buf;
    int buf_len;
    long long last_offset;
    int last_buflen;
};

my_bool ze_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count < 2 || args->arg_count > 3
	|| args->arg_type[0] != STRING_RESULT
	|| args->arg_type[1] != INT_RESULT
	|| (args->arg_count == 3 && (args->arg_type[2] != STRING_RESULT && args->arg_type[2] != INT_RESULT)))
    {
	strcpy(message, "Wrong arguments to zran(filename, offset [, record_type/length ])");
	return 1;
    }

    struct zran_ptr *zp;

    if (!(zp = (struct zran_ptr *)malloc(sizeof(struct zran_ptr))))
	goto malloc_err;

    memset(zp, 0, sizeof(struct zran_ptr));

    zp->buf_len = BUF_LENGTH;
    if (!(zp->buf = (char *)malloc(zp->buf_len)))
	goto malloc_err;

    initid->ptr = (char *)zp;
    initid->max_length = MAX_LENGTH;
    return 0;

malloc_err:
    strcpy(message, "Unable to malloc");
    return 1;
}

void ze_deinit(UDF_INIT *initid)
{
    if (initid->ptr)
    {
	struct zran_ptr *zp = (struct zran_ptr *)initid->ptr;

	if (zp->zran)
	    zran_cleanup(zp->zran);

	if (zp->buf)
	    free(zp->buf);

	free(zp);
	initid->ptr = NULL;
    }
}

char *ze(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error)
{
    struct zran_ptr *zp = (struct zran_ptr *)initid->ptr;
    char *filename = NULL;
    long long req_offset = 0;
    long long req_len = 0;
    char *req_sep = NULL;

    switch (args->arg_count)
    {
	case 3:
	    if (args->arg_type[2] == INT_RESULT)
	    {
		req_len = *((long long *)args->args[2]);

		if (req_len < 0 || req_len > initid->max_length)
		    goto err;
	    }
	    else if (args->arg_type[2] == STRING_RESULT)
	    {
		if (!strcasecmp(args->args[2], "line"))
		    req_sep = "\n";
		else if (!strcasecmp(args->args[2], "para"))
		    req_sep = "\n\n";
		else
		    req_sep = args->args[2];
	    }
	case 2:
	    req_offset = *((long long *)args->args[1]);

	    if (!args->lengths[0] || !*args->args[0])
		goto err;

	    if (!(filename = strndup(args->args[0], args->lengths[0])))
		goto err;

	    break;
	default:
	    goto err;
    }

    if (zp->zran && strcmp(zp->zran->data.filename, filename) != 0)
    {
	zran_cleanup(zp->zran);
	zp->zran = NULL;
	zp->last_offset = 0;
	zp->last_buflen = 0;
    }

    if (!zp->zran && !(zp->zran = zran_init(filename, NULL)))
	goto err;

    if (!zran_index_available(zp->zran))
	goto err;

    if (req_len > zp->buf_len)
    {
	char *ptr;
	if (!(ptr = realloc(zp->buf, req_len)))
	    goto err;
	zp->buf = ptr;
	zp->buf_len = req_len;
    }

    long long offset = req_offset;
    char *ptr;

    /* check if request can be satisfied using previous (uncompressed) buffer */
    if (zp->last_buflen
	&& offset >= zp->last_offset
	&& offset < zp->last_offset + zp->last_buflen)
    {
	char *buf = zp->buf + (offset - zp->last_offset);
	long long len = zp->last_buflen - (offset - zp->last_offset);

	if (req_sep && (ptr = strnstr(buf, req_sep, len)) != NULL)
	{
	    *length = (ptr - buf);
	    return buf;
	}

	if (req_len && req_len <= len)
	{
	    *length = req_len;
	    return buf;
	}

	memmove(zp->buf, buf, len);
	zp->last_offset = offset;
	zp->last_buflen = len;

	offset += len;
    }
    else
    {
	zp->last_offset = offset;
	zp->last_buflen = 0;
    }

    int added;
    if ((added = zran_extract(zp->zran, offset, zp->buf + zp->last_buflen, zp->buf_len - zp->last_buflen)) < 0)
	goto err;

    zp->last_offset = req_offset;
    zp->last_buflen += added;

    int retlen = zp->last_buflen;

    if (req_sep && (ptr = strnstr(zp->buf, req_sep, zp->last_buflen)) != NULL)
	retlen = ptr - zp->buf;
    else if (req_len < zp->last_buflen)
	retlen = req_len;

    *length = retlen;
    return zp->buf;

err:
    *is_null = 1;
    return 0;
}
