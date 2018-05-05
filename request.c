/* request.c: HTTP Request Functions */

#include "spidey.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

int parse_request_method(Request *r);
int parse_request_headers(Request *r);

/**
 * Accept request from server socket.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Newly allocated Request structure.
 *
 * This function does the following:
 *
 *  1. Allocates a request struct initialized to 0.
 *  2. Initializes the headers list in the request struct.
 *  3. Accepts a client connection from the server socket.
 *  4. Looks up the client information and stores it in the request struct.
 *  5. Opens the client socket stream for the request struct.
 *  6. Returns the request struct.
 *
 * The returned request struct must be deallocated using free_request.
 **/
Request * accept_request(int sfd) {
    Request *r = calloc(1, sizeof(Request));
    struct sockaddr raddr;
    socklen_t rlen = sizeof(struct sockaddr);

    /* Allocate request struct (zeroed) */
   
    /* Accept a client */
    r->fd = accept(sfd, &raddr, &rlen);
    if (r->fd < 0) {
        fprintf(stderr, "Unable to accept: %s\n", strerror(errno));
        goto fail;
    }

    /* Lookup client information */
    int client_info = getnameinfo(&raddr, rlen, r->host, sizeof(r->host), r->port, sizeof(r->port), 0);
    if (client_info != 0) {
        fprintf(stderr, "Unable to lookup: %s\n", gai_strerror(client_info));
        goto fail;
    }

    /* Open socket stream */
    r->file = fdopen(r->fd, "r+");
    if (!r->file) {
        fprintf(stderr, "Unable to fdopen: %s\n", strerror(errno));
        goto fail;
    }

    log("Accepted request from %s:%s", r->host, r->port);
    return r;

fail:
    /* Deallocate request struct */
    free_request(r);
    return NULL;
}

/**
 * Deallocate request struct.
 *
 * @param   r           Request structure.
 *
 * This function does the following:
 *
 *  1. Closes the request socket stream or file descriptor.
 *  2. Frees all allocated strings in request struct.
 *  3. Frees all of the headers (including any allocated fields).
 *  4. Frees request struct.
 **/
void free_request(Request *r) {
    if (!r) {
    	return;
    }

    /* Close socket or fd */
    if (!r->fd) ;
    else fclose(r->file);

    /* Free allocated strings */
    if (!r->method) ;
    else free(r->method);

    if (!r->uri) ;
    else free(r->uri);

    if (!r->path) ;
    else free(r->path);

    if (!r->query) ;
    else free(r->query);

    /* Free headers */
    if (!r->headers) ;
    else {
        while (r->headers != NULL) {
            Header *temp = r->headers;
            r->headers = r->headers->next;
            free(temp);
        }
    }

    /* Free request */
    free(r); 
}

/**
 * Parse HTTP Request.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * This function first parses the request method, any query, and then the
 * headers, returning 0 on success, and -1 on error.
 **/
int parse_request(Request *r) {
    /* Parse HTTP Request Method */
    if (parse_request_method(r) == -1) {
        return -1;
    }

    /* Parse HTTP Requet Headers*/
    if (parse_request_headers(r) == -1) {
        return -1;
    }

    return 0;
}

/**
 * Parse HTTP Request Method and URI.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Requests come in the form
 *
 *  <METHOD> <URI>[QUERY] HTTP/<VERSION>
 *
 * Examples:
 *
 *  GET / HTTP/1.1
 *  GET /cgi.script?q=foo HTTP/1.0
 *
 * This function extracts the method, uri, and query (if it exists).
 **/
int parse_request_method(Request *r) {
    char buffer[BUFSIZ];
    char *method=NULL;
    char *uri=NULL;
    char *query=NULL;

    /* Read line from socket */
    if (fgets(buffer, BUFSIZ, r->file) == NULL) {
        goto fail;
    }

    /* Parse method and uri */
    if((method = strtok(buffer, " ")) == NULL) {
	debug("Could not parse method");
	return -1;
    }
    if((uri = strtok(NULL, " ")) == NULL) {
	debug("Could not parse uri");
    }
   

    /* Parse query from uri */
    if(uri != NULL) {
	//query = uri;
	query= strtok(uri, "?");
	if(query!=NULL){
            debug("query my check: %s", query);
            query = strtok(NULL," \n\r");
	   // *(query - 1) = '\0';
        }
    }

    /* Record method, uri, and query in request struct */
    if(method)
    	r->method= strdup(method);
    if(uri)
	r->uri= strdup(uri);
    if(query)
	r->query= strdup(query);

    debug("HTTP METHOD: %s", r->method);
    debug("HTTP URI:    %s", r->uri);
    debug("HTTP QUERY:  %s", r->query);

    return 0;

fail:
    return -1;
}

/**
 * Parse HTTP Request Headers.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Headers come in the form:
 *
 *  <NAME>: <VALUE>
 *
 * Example:
 *
 *  Host: localhost:8888
 *  User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:29.0) Gecko/20100101 Firefox/29.0
 *  Accept: text/html,application/xhtml+xml
 *  Accept-Language: en-US,en;q=0.5
 *  Accept-Encoding: gzip, deflate
 *  Connection: keep-alive
 *
 * This function parses the stream from the request socket using the following
 * pseudo-code:
 *
 *  while (buffer = read_from_socket() and buffer is not empty):
 *      name, value = buffer.split(':')
 *      header      = new Header(name, value)
 *      headers.append(header)
 **/
int parse_request_headers(Request *r) {
    struct header *curr = NULL;
    char buffer[BUFSIZ];
    char *name;
    char *value;

    /* Parse headers from socket */                    /*NEED TO FINISH READING STREAM*/
    if (fgets(buffer, BUFSIZ, r->file) == NULL) {
        goto fail;
    } else {
        chomp(buffer);
        name = skip_whitespace(buffer);
        value = strchr(name, ':');
        if (value == NULL) {
            goto fail;
        }
        *value = '\0';
        value++;
        value = skip_whitespace(value);
        
        curr = calloc(1, sizeof(struct header)); 

        curr->name = strdup(name);
        curr->value = strdup(value);
        curr->next = r->headers;

        r->headers = curr;
    }

#ifndef NDEBUG
    for (struct header *header = r->headers; header != NULL; header = header->next) {
    	debug("HTTP HEADER %s = %s", header->name, header->value);
    }
#endif
    return 0;

fail:
    return -1;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
