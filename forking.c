/* forking.c: Forking HTTP Server */

#include "spidey.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>

/**
 * Fork incoming HTTP requests to handle the concurrently.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Exit status of server (EXIT_SUCCESS).
 *
 * The parent should accept a request and then fork off and let the child
 * handle the request.
 **/
int forking_server(int sfd) {
    /* Accept and handle HTTP request */
    while (true) {
        /* Accept request */
        Request *r = accept_request(sfd);
        /* Ignore children */
        
        /* Fork off child process to handle request */
        pid_t pid = fork();
        if(pid<0) {
            debug("Failed to fork: %s\n", strerror(pid));
        }
        else if(pid == 0) {
            close(sfd);
            HTTPStatus status = handle_request(r);
	    free_request(r);
            debug("Request Status: %s", http_status_string(status));
            _exit(EXIT_SUCCESS);
        }
        else {
            free_request(r);
        }
    }

    /* Close server socket */
    close(sfd);
    return EXIT_SUCCESS;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */

