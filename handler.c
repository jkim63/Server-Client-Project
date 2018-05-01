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
 * 200 (1 i enumed HTTPSTATUS) 400 (2 in enumed HTTPSTATUS) :bad request, client error; 404: not found;  500:generic
 **/
HTTPStatus  handle_request(Request *r) {
    HTTPStatus result =0;
    
    /* Parse request */
    if (parse_request(r) == -1){
        result =2;
        handle_error(r, result);
        return result;
    }
    
    /* Determine request path */
    char* path;
    if ((path= determine_request_path(r->uri))==-1){
        result =3;
        handle_error(r, result);
        return result;
    }
    debug("HTTP REQUEST PATH: %s", r->path);

    /* Dispatch to appropriate request handler type based on file type */
    
    struct stat storeStat;
    if (stat(path, &storeStat == -1)){
        result= 2;
        handle_error(r, result);
        return result;
    }
    else if ((storeStat.st_mode & S_IFMT)   == S_IFREG){

        if (access(path, X_OK )){
            result = handle_cgi_request(r);
        } else if (access(path, R_OK)){
            result= handle_file_request(r);
        } else {
            result= handle_error(r, 2);
        }
    }
    else if ((storeStat.st_mode & S_IFMT) == S_IFDIR){
        result= handle_browse_request(r);
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
    //struct dirent **entries;
    //int n;

    /* Open a directory for reading or scanning */

    /* Write HTTP Header with OK Status and text/html Content-Type */

    /* For each entry in directory, emit HTML list item */

    /* Flush socket, return OK */
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

    /* Write HTTP Headers with OK status and determined Content-Type */
    Header *temp = r->headers;
    fprintf(r->file, "HTTP/1.1 200 OK\n");
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
    //FILE *pfs;
    //char buffer[BUFSIZ];

    /* Export CGI environment variables from request structure:
     * http://en.wikipedia.org/wiki/Common_Gateway_Interface */

    /* Export CGI environment variables from request headers */

    /* POpen CGI Script */

    /* Copy data from popen to socket */

    /* Close popen, flush socket, return OK */
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
    //const char *status_string = http_status_string(status);

    /* Write HTTP Header */
    fprintf(r->file, "HTTP/1.1 %s\n\n", status_string);
    /* Write HTML Description of Error*/
    fprintf(r->file, "<html><body> \"HTTP Status: %s\n\" </body></html>", status_string);

    /* Return specified status */
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
