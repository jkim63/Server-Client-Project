/* handler.c: HTTP Request Handlers */

#include "spidey.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Internal Declarations */
HTTPStatus handle_browse_request(Request *request);
HTTPStatus handle_file_request(Request *request);
HTTPStatus handle_cgi_request(Request *request);
HTTPStatus handle_error(Request *request, HTTPStatus status);

/**
 * Handle HTTP Request.
 *
 * @param   r           HTTP Request structure
 * @return  Status of the HTTP request.
 *
 * This parses a request, determines the request path, determines the request
 * type, and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
HTTPStatus  handle_request(Request *r) {
    HTTPStatus result =0;
    
    /* Parse request */
    if (parse_request(r) == -1){
        result =HTTP_STATUS_BAD_REQUEST;
        handle_error(r, result);
        return result;
    }
    
    /* Determine request path */
    if ((r->path= determine_request_path(r->uri))==NULL){
        result = HTTP_STATUS_NOT_FOUND;
        handle_error(r, result);
        return result;
    }
    debug("HTTP REQUEST PATH: %s", r->path);

    /* Dispatch to appropriate request handler type based on file type */
    
    struct stat storeStat;
    if ( (stat(r->path, &storeStat)) == -1){
        result= HTTP_STATUS_INTERNAL_SERVER_ERROR;
        handle_error(r, result);
        return result;
    }
    else if ((storeStat.st_mode & S_IFMT)   == S_IFREG){
        if (access(r->path, X_OK )){
            result = handle_cgi_request(r);
        } else if (access(r->path, R_OK)){
            result= handle_file_request(r);
        } else {
            result= handle_error(r, HTTP_STATUS_NOT_FOUND);
	    return result;
        }
    }
    else if ((storeStat.st_mode & S_IFMT) == S_IFDIR){
        result= handle_browse_request(r);
    }
    
    if( result != HTTP_STATUS_OK) {
	handle_error(r, result);
    }
    log("HTTP REQUEST STATUS: %s", http_status_string(result));
    
    return result;
    
}

/**
 * Handle browse request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP browse request.
 *
 * This lists the contents of a directory in HTML.
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
HTTPStatus  handle_browse_request(Request *r) {
    log(" handle_browse_request");
    struct dirent **entries = NULL;
    int n = 0;

    /* Open a directory for reading or scanning */
    if( (n = scandir(r->path, &entries, NULL, alphasort)) < 0) {
	debug("Could not scan (%s): %s", r->path, strerror(errno)); 
	return HTTP_STATUS_NOT_FOUND;
    }
    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->file, "HTTP/1.0 200 OK\n");
    fprintf(r->file, "Content-Type: text/html\n\n");
    /* For each entry in directory, emit HTML list item */
    fprintf(r->file, "<html><body><ul>\n");
    for(int i = 0; i<n; i++) {
	fprintf(r->file, "<li>%s</li>\n", entries[i]->d_name);
	free(entries[i]);
    }
    fprintf(r->file, "</ul></body></html>\n");
    free(entries);

    /* Flush socket, return OK */
    fflush(r->file);
    return HTTP_STATUS_OK;
}

/**
 * Handle file request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This opens and streams the contents of the specified file to the socket.
 *
 * If the path cannot be opened for reading, then handle error with
 * HTTP_STATUS_NOT_FOUND.
 **/
HTTPStatus  handle_file_request(Request *r) {
    log("handle_file_request");
    FILE *fs;
    char buffer[BUFSIZ];
    char *mimetype = NULL;
    size_t nread;

    /* Open file for reading */
    fs = fopen(r->path, "w+");
    if (!fs) {
        fprintf(stderr, "fopen failed: %s\n", strerror(errno));
        return HTTP_STATUS_NOT_FOUND;
    }

    /* Determine mimetype */
    mimetype = determine_mimetype(r->path);
    debug("Mimetype: %s", mimetype);

    /* Write HTTP Headers with OK status and determined Content-Type */
    Header *temp = r->headers;
    fprintf(r->file, "HTTP/1.0 200 OK\n");
    fprintf(r->file, "Content-Type: %s", mimetype);
    while (temp != NULL) {
        fprintf(r->file, "%s: %s\n", temp->name, temp->value);
        temp = temp->next;
    }
    fprintf(r->file, "\n");

    /* Read from file and write to socket in chunks */
    while ((nread = fread(buffer, sizeof(char), BUFSIZ, fs)) > 0) {
        if(nread <= 0) {
            debug("Could not read %s: %s", r->path, strerror(errno));
            goto fail;
        }
        size_t fwritten = fwrite(buffer, sizeof(char), nread, r->file);
        if(fwritten <= 0) {
            debug("Could not write: %s", strerror(errno));
            goto fail;
        }
        while (fwritten != nread) {
            fwritten += fwrite(buffer, sizeof(char), nread-fwritten, r->file);
        }
    }

    /* Close file, flush socket, deallocate mimetype, return OK */
    fclose(fs);
    fflush(r->file);
    free(mimetype);
    return HTTP_STATUS_OK;

fail:
    /* Close file, free mimetype, return INTERNAL_SERVER_ERROR */
    fclose(fs);
    free(mimetype);
    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
}

/**
 * Handle CGI request
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This popens and streams the results of the specified executables to the
 * socket.
 *
 * If the path cannot be popened, then handle error with
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
HTTPStatus handle_cgi_request(Request *r) {
    log(" handle_cgi_request");

    /* Export CGI environment variables from request structure:
     * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
    Header *head = r->headers;
    
    if (setenv("DOCUMENT_ROOT", RootPath, 1) == -1) {
        fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
    }
    
    if (setenv("QUERY_STRING", r->query, 1) == -1) {
        fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
    }

    if (setenv("REMOTE_ADDR", r->host, 1) == -1) {
        fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
    }

    if (setenv("REMOTE_PORT", r->port, 1) == -1) {
        fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
    }

    if (setenv("REQUEST_METHOD", r->method, 1) == -1) {
        fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
    }

    if (setenv("REQUEST_URI", r->uri, 1) == -1) {
        fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
    }

    if (setenv("SCRIPT_FILENAME", r->path, 1) == -1) {
        fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
    }

    if (setenv("SERVER_PORT", Port, 1) == -1) {
        fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
    }

    while (head != NULL) {
        if (streq(head->name, "HTTP_HOST")) {
            if (setenv("HTTP_HOST", head->value, 1) == -1) {
                fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
            }
        }

        if (streq(head->name, "HTTP_ACCEPT")) {
            if (setenv("HTTP_ACCEPT", head->value, 1) == -1) {
                fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
            }
        }

        if (streq(head->name, "HTTP_ACCEPT_LANGUAGE")) {
            if (setenv("HTTP_ACCEPT_LANGUAGE", head->value, 1) == -1) {
                fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
            }
        }

        if (streq(head->name, "HTTP_ACCEPT_ENCODING")) {
            if (setenv("HTTP_ACCEPT_ENCODING", head->value, 1) == -1) {
                fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
            }
        }

        if (streq(head->name, "HTTP_CONNECTION")) {
            if (setenv("HTTP_CONNECTION", head->value, 1) == -1) {
                fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
            }
        }

        if (streq(head->name, "HTTP_USER_AGENT")) {
            if (setenv("HTTP_USER_AGENT", head->value, 1) == -1) {
                fprintf(stderr, "Unable to setenv: %s\n", strerror(errno));
            }
        }

        head = head->next;
    }  

    /* Export CGI environment variables from request headers */
    Header *temp = r->headers;
    while(temp != NULL) {
	setenv(temp->name, temp->value, 1);
	temp = temp->next;
    }

    /* POpen CGI Script */
    FILE *ps = popen(r->path, "r");

    /* Copy data from popen to socket */
    char buffer[BUFSIZ];
    while(fgets(buffer, BUFSIZ, ps)) {
	fputs(buffer, r->file);
    }

    /* Close popen, flush socket, return OK */
    pclose(ps);
    fflush(r->file);
    return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP error request.
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
HTTPStatus  handle_error(Request *r, HTTPStatus status) {
    log(" handle_error");
    const char *status_string = http_status_string(status);

    /* Write HTTP Header */
    fprintf(r->file, "HTTP/1.0 %s\n", status_string);
    fprintf(r->file, "Content-type: text/html\n\n");
    /* Write HTML Description of Error*/
    fprintf(r->file, "<html><body> \"HTTP Status: %s\n\" </body></html>", status_string);

    /* Return specified status */
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
