#include "libzran.h"
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "strnstr.h"

void usage(void)
{
    errx(EXIT_FAILURE,
	"usage: zran [ OPTIONS ] file.gz offset[+<len>|line|para|xxxx]...\n"
	"Options:\n"
	"  -c				'cat' the file to stdout while indexing\n"
	"  -i filename			create index in filename\n"
	"  -s <len>|line|para|xxxx	\n"
	"  -S <len>			output index point for every X uncompressed bytes");
}

void guess_sep(char *arg, int *len, char **sep)
{
    *len = 0;
    *sep = NULL;

    if (!strcasecmp(arg, "line"))
	*sep = "\n";
    else if (!strcasecmp(arg, "para"))
	*sep = "\n\n";
    else if (isdigit(*arg))
	*len = atoi(arg);
    else
	*sep = arg;
}

int main(int argc, char **argv)
{
    FILE *output = NULL;
    char *index_filename = NULL;
    int index_span = 1024*1024;
    char *filename = NULL;
    char *sep = NULL;
    int len = 0;

    int ch;

    while ((ch = getopt(argc, argv, "ci:s:S:")) != -1)
    {
	switch (ch) {
	    case 'c':
		output = stdout;
		break;
	    case 'i':
		index_filename = optarg;
		break;
	    case 's':
		guess_sep(optarg, &len, &sep);
		break;
	    case 'S':
		index_span = atoi(optarg);
		break;
	    case '?':
	    default:
		usage();
	}
    }

    if ((argc - optind) < 1)
	usage();

    filename = argv[optind++];

    struct zran *zran;

    if (!(zran = zran_init(filename, index_filename)))
	errx(EXIT_FAILURE, "zran: could not open %s for reading", filename);

    if (!zran_index_available(zran))
    {
	TRACE("slow path; indexing file");

	zran_build_index(zran, index_span, output);
    }

    char *buf = NULL;
    int buf_len = 0;

    for (; optind < argc; optind++)
    {
	int this_len = len;
	char *this_sep = sep;
	char *ptr;

	if ((ptr = strchr(argv[optind], '+')))
	{
	    *ptr++ = 0;

	    guess_sep(ptr, &this_len, &this_sep);
	}

	if (this_len <= 0)
	    this_len = 4096;

	if (this_len > buf_len)
	{
	    if (!(buf = realloc(buf, this_len)))
		err(EXIT_FAILURE, "realloc");

	    buf_len = this_len;
	}

	off_t offset = atoll(argv[optind]);
	int n;

	if ((n = zran_extract(zran, offset, buf, this_len)) < 0)
	    errx(EXIT_FAILURE, "zran: extraction failed: %s",
		n == Z_MEM_ERROR ? "out of memory" : "input corrupted");

	if (this_sep && (ptr = strnstr(buf, this_sep, n)) != NULL)
	    n = (ptr - buf) + strlen(this_sep);

	fwrite(buf, n, 1, stdout);
    }

    zran_cleanup(zran);

    return 0;
}
