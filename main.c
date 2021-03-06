#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <malloc.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "driver.h"
#include "log.h"
#include "process.h"

char log_buf[LOG_BUF_SIZE] = { 0 };

void usage(void)
{
	log_info("Usage: %s -r <resolution> -f <framerate>", program_invocation_short_name);
	log_info("Example:\n\t%s -r 1366x768 -f 10", program_invocation_short_name);
	return;
}

int main(int argc, char *argv[])
{
	Camera *c = NULL;
	int r;

	/* bump RLIMIT_MEMLOCK before using this */
/*
	r = mlockall(MCL_CURRENT|MCL_FUTURE);
	if (r < 0) {
		log_warn("Failed to lock pages in memory, ignoring: %m");
	}
*/
	r = setvbuf(stderr, log_buf, _IOFBF, LOG_BUF_SIZE);
	if (r) {
		log_warn("Failed to set full buffering, ignoring: %m");
	}

	mallopt(M_TRIM_THRESHOLD, -1);
	/* mmap needs to be disabled as on free, mappings are unmapped
	   regardless of the trim setting */
	mallopt(M_MMAP_MAX, 0);

#define SIZE 2*1024*1024 /* 2MB should be ok */
	char *buf = malloc(SIZE);
	if (!buf) {
		log_oom();
		return EXIT_FAILURE;
	}
	int pagesize = sysconf(_SC_PAGESIZE);
	/* touch each page to generate a page fault */
	for (int i = 0; i < SIZE; i += pagesize) {
		buf[i] = 0;
	}
	/* does not release any memory back to OS, but the same buffer
	   will be utilized for serving future malloc requests */
	free(buf);

	int opt;
	char *res = NULL, *framerate = NULL;

	while ((opt = getopt(argc, argv, "r:f:")) != -1) {
		switch (opt) {
		case 'r':
			res = strdup(optarg);
			break;
		case 'f':
			framerate = strdup(optarg);
			break;
		default:
			usage();
			goto end;
		}
	}

	c = calloc(1, sizeof(*c));
	if (!c) {
		log_oom();
		goto end;
	}
	r = init_cam(c);
	if (r != IS_SUCCESS) {
		log_error("Error: Failed to initialize camera");
		goto end;
	}

	r = capture_img(c);
	if (r != IS_SUCCESS) {
		log_error("Error: Failed to capture image");
		goto end;
	}

	/* TODO: sanity check for res and framerate */
	r = stream_loop(c, res ? res : "1366x768", framerate ? framerate : "10");
	if (r < 0) {
		log_error("Failure in transmission of frames to worker, exiting");
		goto end;
	}

end:
	unref_cam(c);
	free(res);
	free(framerate);
	return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
