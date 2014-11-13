#define _GNU_SOURCE // memmem(.), strchrnul needs this :-(
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "h/minicrawler.h"
#include "h/config.h"
#include "h/string.h"

/** vypise napovedu
 */
void printusage()
{
	printf("\nminicrawler, version %s\n\nUsage:   minicrawler [options] [urloptions] url [[url2options] url2]...\n\n"
	         "Where\n"
	         "   options:\n"
	         "         -d         enable debug messages (to stderr)\n"
	         "         -tSECONDS  set timeout (default is 5 seconds)\n"
	         "         -h         enable output of HTTP headers\n"
	         "         -i         enable impatient mode (minicrawler exits few seconds earlier if it doesn't make enough progress)\n"
	         "         -p         output also URLs that timed out and a reason of it\n"
	         "         -A STRING  custom user agent (max 255 bytes)\n"
	         "         -w STRING  write this custom header to all requests (max 4095 bytes)\n"
	         "         -c         convert content to text format (with UTF-8 encoding)\n"
	         "         -8         convert from page encoding to UTF-8\n"
	         "         -DMILIS    set delay time in miliseconds when downloading more pages from the same IP (default is 100 ms)\n"
#ifdef HAVE_LIBSSL
	         "         -S         disable SSL/TLS support\n"
#endif
	         "         -g         accept gzip encoding\n"
	         "         -6         resolve host to IPv6 address only\n"
	         "         -b STRING  cookies in the netscape/mozilla file format (max 20 cookies)\n"
	         "\n   urloptions:\n"
	         "         -C STRING  parameter which replaces '%%' in the custom header\n"
	         "         -P STRING  HTTP POST parameters\n"
	         "         -X STRING  custom request HTTP method, no validation performed (max 15 bytes)\n"
	         "\n", VERSION);
}

static int writehead = 0;

/** nacte url z prikazove radky do struktur
 */
void initurls(int argc, char *argv[], struct surl **url, struct ssettings *settings)
{
	struct surl *curl, *purl = NULL;
	char *p, *q;
	long options = 0;
	char customheader[4096];
	char customagent[256];
	struct cookie cookies[COOKIESTORAGESIZE];
	int ccnt = 0, i = 0;

	*url = (struct surl *)malloc(sizeof(struct surl));
	memset(*url, 0, sizeof(struct surl));
	curl = *url;

	for (int t = 1; t < argc; ++t) {

		// options
		if(!strcmp(argv[t], "-d")) {settings->debug=1; continue;}
		if(!strcmp(argv[t], "-S")) {options |= 1<<SURL_OPT_NONSSL; continue;}
		if(!strcmp(argv[t], "-h")) {writehead=1; continue;}
		if(!strcmp(argv[t], "-i")) {settings->impatient=1; continue;}
		if(!strcmp(argv[t], "-p")) {settings->partial=1; continue;}
		if(!strcmp(argv[t], "-c")) {options |= 1<<SURL_OPT_CONVERT_TO_TEXT | 1<<SURL_OPT_CONVERT_TO_UTF8; continue;}
		if(!strcmp(argv[t], "-8")) {options |= 1<<SURL_OPT_CONVERT_TO_UTF8; continue;}
		if(!strcmp(argv[t], "-g")) {options |= 1<<SURL_OPT_GZIP; continue;}
		if(!strncmp(argv[t], "-t", 2)) {settings->timeout = atoi(argv[t]+2); continue;}
		if(!strncmp(argv[t], "-D", 2)) {settings->delay = atoi(argv[t]+2); continue;}
		if(!strncmp(argv[t], "-w", 2)) {safe_cpy(customheader, argv[t+1], I_SIZEOF(customheader)); t++; continue;}
		if(!strncmp(argv[t], "-A", 2)) {str_replace(customagent, argv[t+1], "%version%", VERSION); t++; continue;}
		if(!strncmp(argv[t], "-b", 2)) {
			p = argv[t+1];
			while (p[0] != '\0' && ccnt < COOKIESTORAGESIZE) {
				q = strchrnul(p, '\n');
				cookies[ccnt].name = malloc(q-p);
				cookies[ccnt].value = malloc(q-p);
				cookies[ccnt].domain = malloc(q-p);
				cookies[ccnt].path = malloc(q-p);
				sscanf(p, "%s\t%d\t%s\t%d\t%d\t%s\t%s", cookies[ccnt].domain, &cookies[ccnt].host_only, cookies[ccnt].path, &cookies[ccnt].secure, &cookies[ccnt].expire, cookies[ccnt].name, cookies[ccnt].value);
				p = (q[0] == '\n') ? q + 1 : q;
				ccnt++;
			}
			t++;
			continue;
		}
		if(!strcmp(argv[t], "-6")) {options |= 1<<SURL_OPT_IPV6; continue;}

		// urloptions
		if(!strcmp(argv[t], "-P")) {
			curl->post = malloc(strlen(argv[t+1]) + 1);
			memcpy(curl->post, argv[t+1], strlen(argv[t+1]) + 1);
			t++;
			continue;
		}
		if(!strncmp(argv[t], "-C", 2)) {
			if (customheader[0]) {
				str_replace(curl->customheader, customheader, "%", argv[t+1]);
			}
			t++;
			continue;
		}
		if(!strcmp(argv[t], "-X")) {safe_cpy(curl->method, argv[t+1], I_SIZEOF(curl->method)); t++; continue;}

		// init url
		mcrawler_init_url(curl, argv[t]);
		curl->index = i++;
		if (!curl->method[0]) {
			strcpy(curl->method, curl->post ? "POST" : "GET");
		}
		for (int i = 0; i < ccnt; i++) {
			cp_cookie(&curl->cookies[i], &cookies[i]);
		}
		curl->cookiecnt = ccnt;
		strcpy(curl->customagent, customagent);
		if (!curl->customheader[0]) {
			strcpy(curl->customheader, customheader);
		}
		curl->options = options;

		purl = curl;
		curl = (struct surl *)malloc(sizeof(struct surl));
		purl->next = curl;
	}

	if (curl == *url) {
		*url = NULL;
	}
	free(curl);
	if (purl) {
		purl->next = NULL;
	}

	for (int t = 0; t < ccnt; t++) {
		free_cookie(&cookies[t]);
	}
}

/**
 * Formats timing data for output
 */
static void format_timing(char *dest, struct timing *timing) {
	int n;
	const int now = timing->done;
	if (timing->dnsstart) {
		n = sprintf(dest, "DNS Lookup=%d ms; ", (timing->dnsend ? timing->dnsend : now) - timing->dnsstart);
		if (n > 0) dest += n;
	}
	if (timing->connectionstart) {
		n = sprintf(dest, "Initial connection=%d ms; ", (timing->requeststart ? timing->requeststart : now) - timing->connectionstart);
		if (n > 0) dest += n;
	}
	if (timing->sslstart) {
		n = sprintf(dest, "SSL=%d ms; ", (timing->sslend ? timing->sslend : now) - timing->sslstart);
		if (n > 0) dest += n;
	}
	if (timing->requeststart) {
		n = sprintf(dest, "Request=%d ms; ", (timing->requestend ? timing->requestend : now) - timing->requeststart);
		if (n > 0) dest += n;
	}
	if (timing->requestend) {
		n = sprintf(dest, "Waiting=%d ms; ", (timing->firstbyte ? timing->firstbyte : now) - timing->requestend);
		if (n > 0) dest += n;
	}
	if (timing->firstbyte) {
		n = sprintf(dest, "Content download=%d ms; ", (timing->lastread ? timing->lastread : now) - timing->firstbyte);
		if (n > 0) dest += n;
	}
	if (timing->connectionstart) {
		n = sprintf(dest, "Total=%d ms; ", (timing->lastread ? timing->lastread : now) - timing->connectionstart);
		if (n > 0) dest += n;
	}
}

void output(struct surl *u) {
	unsigned char header[16384];

	sprintf(header,"URL: %s",u->rawurl);
	if(u->redirectedto != NULL) sprintf(header+strlen(header),"\nRedirected-To: %s",u->redirectedto);
	for (struct redirect_info *rinfo = u->redirect_info; rinfo; rinfo = rinfo->next) {
		sprintf(header+strlen(header), "\nRedirect-info: %s %d; ", rinfo->url, rinfo->status);
		format_timing(header+strlen(header), &rinfo->timing);
	}
	sprintf(header+strlen(header),"\nStatus: %d\nContent-length: %d\n",u->status,u->bufp-u->headlen);

	const int url_state = u->state;
	if (url_state <= SURL_S_RECVREPLY) {
		char timeouterr[50];
		switch (url_state) {
			case SURL_S_JUSTBORN:
				strcpy(timeouterr, "Process has not started yet"); break;
			case SURL_S_PARSEDURL:
				strcpy(timeouterr, "Timeout while contacting DNS servers"); break;
			case SURL_S_INDNS:
				strcpy(timeouterr, "Timeout while resolving host"); break;
			case SURL_S_GOTIP:
				if (u->timing.connectionstart) {
					strcpy(timeouterr, "Connection timed out");
				} else {
					strcpy(timeouterr, "Waiting for download slot");
				}
				break;
			case SURL_S_CONNECT:
				strcpy(timeouterr, "Connection timed out"); break;
			case SURL_S_HANDSHAKE:
				strcpy(timeouterr, "Timeout during SSL handshake"); break;
			case SURL_S_GENREQUEST:
				strcpy(timeouterr, "Timeout while generating HTTP request"); break;
			case SURL_S_SENDREQUEST:
				strcpy(timeouterr, "Timeout while sending HTTP request"); break;
			case SURL_S_RECVREPLY:
				strcpy(timeouterr, "HTTP server timed out"); break;
		}

		sprintf(header+strlen(header), "Timeout: %d (%s); %s\n", url_state, state_to_s(url_state), timeouterr);
	}
	if (*u->error_msg) {
		sprintf(header+strlen(header), "Error-msg: %s\n", u->error_msg);
	}
	if (*u->charset) {
		sprintf(header+strlen(header), "Content-type: text/html; charset=%s\n", u->charset);
	}
	if (u->cookiecnt) {
		sprintf(header+strlen(header), "Cookies: %d\n", u->cookiecnt);
		// netscape cookies.txt format
		// @see http://www.cookiecentral.com/faq/#3.5
		for (int t = 0; t < u->cookiecnt; t++) {
			sprintf(header+strlen(header), "%s\t%d\t/\t%d\t0\t%s\t%s\n", u->cookies[t].domain, u->cookies[t].host_only/*, u->cookies[t].path*/, u->cookies[t].secure/*, u->cookies[t].expiration*/, u->cookies[t].name, u->cookies[t].value);
		}
	}
	if (u->conv_errno) {
		sprintf(header+strlen(header), "Conversion error: %s\n", strerror(u->conv_errno));
	}

	// downtime
	int downtime;
	if (url_state == SURL_S_DOWNLOADED) {
		assert(u->timing.lastread >= u->timing.connectionstart);
		downtime = u->timing.lastread - u->downstart;
	} else if (u->downstart) {
		downtime = u->timing.done - u->downstart;
	} else {
		downtime = u->timing.done;
	}
	sprintf(header+strlen(header), "Downtime: %dms; %dms", downtime, u->downstart);
	if (u->addr != NULL) {
		char straddr[INET6_ADDRSTRLEN];
		inet_ntop(u->addr->type, u->addr->ip, straddr, sizeof(straddr));
		sprintf(header+strlen(header), " (ip=%s)", straddr);
	}
	sprintf(header+strlen(header), "\nTiming: ");
	format_timing(header+strlen(header), &u->timing);
	sprintf(header+strlen(header), "\nIndex: %d\n\n",u->index);

	write_all(STDOUT_FILENO, header, strlen(header));
	if (writehead) {
		write_all(STDOUT_FILENO, u->buf, u->headlen);
		if (0 == u->headlen) {
			write_all(STDOUT_FILENO, "\n", 1); // PHP library expects one empty line at the end of headers, in normal circumstances it is contained
						      // within u->buf[0 .. u->headlen] .
		}
	}

	write_all(STDOUT_FILENO, u->buf+u->headlen, u->bufp-u->headlen);
	write_all(STDOUT_FILENO, "\n", 1); // jinak se to vývojářům v php špatně parsuje
}