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
    HTTPStatus result = 0;

    /* Parse request */
    if (parse_request(r) < 0) {
        result = HTTP_STATUS_BAD_REQUEST;
        return handle_error(r, result);
    }

    /* Determine request path */
    if ((r->path = determine_request_path(r->uri)) == NULL) {
        result = HTTP_STATUS_NOT_FOUND;
        return handle_error(r, result);
    }
    debug("HTTP REQUEST PATH: %s", r->path);

    /* Dispatch to appropriate request handler type based on file type */
    struct stat storeStat;
    if ((stat(r->path, &storeStat)) < 0) {
        //debug
        result = handle_error(r, HTTP_STATUS_NOT_FOUND);
    } else if ((storeStat.st_mode & S_IFMT) == S_IFREG) {
        if (access(r->path, R_OK|X_OK) == 0) {
            result = handle_cgi_request(r);
        } else if (access(r->path, R_OK) == 0) {
            result = handle_file_request(r);
        }
    } else if ((storeStat.st_mode & S_IFMT) == S_IFDIR) {
        result = handle_browse_request(r);
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
    log("handle_browse_request");
    struct dirent **entries = NULL;
    int n = 0;

    /* Open a directory for reading or scanning */
    n = scandir(r->path, &entries, NULL, alphasort);
    if(n < 0) {
        debug("Could not scan (%s): %s", r->path, strerror(errno)); 
        return HTTP_STATUS_NOT_FOUND;
    }
    
    /*entries += 1;
    n -= 1;*/

    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->file, "HTTP/1.0 200 OK\r\n");
    fprintf(r->file, "Content-Type: text/html\r\n");
    fprintf(r->file, "\r\n");
    
    /* For each entry in directory, emit HTML list item */
    fprintf(r->file, "<ul>\n");
    for(int i = 0; i<n; i++) {
        if (!streq(entries[i]->d_name, ".")) {
            if (!streq(r->uri, "/")) {
                fprintf(r->file, "<li><a href=\"%s/%s\">%s</a></li>\r\n", r->uri, entries[i]->d_name, entries[i]->d_name);
            } else {
                fprintf(r->file, "<li><a href=\"%s%s\">%s</a></li>\r\n", r->uri, entries[i]->d_name, entries[i]->d_name);
            }
        }
        free(entries[i]);
    }
    fprintf(r->file, "</ul>\n");
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
    fs = fopen(r->path, "r");
    if (!fs) {
        fprintf(stderr, "fopen failed: %s\n", strerror(errno));
        return HTTP_STATUS_NOT_FOUND;
    }

    /* Determine mimetype */
    mimetype = determine_mimetype(r->path);
    debug("Mimetype: %s", mimetype);

    /* Write HTTP Headers with OK status and determined Content-Type */
    fprintf(r->file, "HTTP/1.0 200 OK\r\n");
    fprintf(r->file, "Content-Type: %s\r\n", mimetype);
    fprintf(r->file, "\r\n");

    /* Read from file and write to socket in chunks */
    while ((nread = fread(buffer, sizeof(char), BUFSIZ, fs)) > 0) {
        if (fwrite(buffer, sizeof(char), nread, r->file) != nread) {
            debug("Could not write: %s", strerror(errno));
            goto fail;  
        }
    }

    /* Close file, flush socket, deallocate mimetype, return OK */
    fclose(fs);
    fflush(r->file);
    free(mimetype);
    return HTTP_STATUS_OK;

fail:
    /* Close file, free mimetype, return INTERNAL_SERVER_ERROR */
    if (fs) {
        fclose(fs);
    }
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
    FILE *pfs;
    char buffer[BUFSIZ];

    /* Export CGI environment variables from request structure:
     * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
    Header *head = r->headers;

    setenv("DOCUMENT_ROOT", RootPath, 1);
        
    setenv("QUERY_STRING", r->query, 1);

    setenv("REMOTE_ADDR", r->host, 1);

    setenv("REMOTE_PORT", r->port, 1);

    setenv("REQUEST_METHOD", r->method, 1);

    setenv("REQUEST_URI", r->uri, 1);

    setenv("SCRIPT_FILENAME", r->path, 1);

    setenv("SERVER_PORT", Port, 1);

    /* Export CGI environment variables from request headers */
    while (head != NULL) {
        if (streq(head->name, "Host")) {
            setenv("HTTP_HOST", head->value, 1);       
        }

        if (streq(head->name, "Accept")) {
            setenv("HTTP_ACCEPT", head->value, 1);
        }

        if (streq(head->name, "Accept-Language")) {
            setenv("HTTP_ACCEPT_LANGUAGE", head->value, 1); 
        }

        if (streq(head->name, "Accept-Encoding")) {
            setenv("HTTP_ACCEPT_ENCODING", head->value, 1);
        }

        if (streq(head->name, "Connection")) {
            setenv("HTTP_CONNECTION", head->value, 1);
        }

        if (streq(head->name, "User-Agent")) {
            setenv("HTTP_USER_AGENT", head->value, 1);
        }
        head = head->next;
    }   


    /* POpen CGI Script */
    pfs = popen(r->path, "r");
    if (!pfs) {
        fprintf(stderr, "Unable to popen: %s\n", strerror(errno));
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    /* Copy data from popen to socket */
    while (fgets(buffer, BUFSIZ, pfs)) {
        fputs(buffer, r->file);
    }

    /* Close popen, flush socket, return OK */
    pclose(pfs);
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
    log("handle_error");
    const char *status_string = http_status_string(status);

    /* Write HTTP Header */
    fprintf(r->file, "HTTP/1.0 %si\r\n", status_string);
    fprintf(r->file, "Content-type: text/html\r\n");
    fprintf(r->file, "\r\n");

    /* Write HTML Description of Error*/
    fprintf(r->file, "<html><body> \"HTTP Status: %s\n\" </body></html>\r\n", status_string);

    /* Return specified status */
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */

