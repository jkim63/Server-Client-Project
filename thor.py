#!/usr/bin/env python3

import multiprocessing
import os
import requests
import sys
import time

# Globals

PROCESSES = 1
REQUESTS  = 1
VERBOSE   = False
URL       = None

# Functions

def usage(status=0):
    print('''Usage: {} [-p PROCESSES -r REQUESTS -v] URL
    -h              Display help message
    -v              Display verbose output

    -p  PROCESSES   Number of processes to utilize (1)
    -r  REQUESTS    Number of requests per process (1)
    '''.format(os.path.basename(sys.argv[0])))
    sys.exit(status)

def do_request(pid):
    ''' Perform REQUESTS HTTP requests and return the average elapsed time. '''
    totalTime = 0
    for count in range(REQUESTS):
        start = time.time()
        r = requests.get(URL)

        end = time.time()
        if r.status_code == 200 and VERBOSE: #Success in theory
            print(r.text)
        print('Process: {}, Request: {}, Elapsed Time: {:2f}'.format(pid, count, (end-start)))

        totalTime = totalTime + (end - start)

    print('Process: {}, AVERAGE , Elapsed Time: {:2f}'.format(pid, (totalTime / REQUESTS))) 
    return (totalTime / REQUESTS)

# Main execution

if __name__ == '__main__':
    # Parse command line arguments
    args = sys.argv[1:]
    while len(args) and args[0].startswith('-') and len(args[0])>1:
        arg = args.pop(0)
        if arg == '-h':
            usage(0)
        elif arg == '-v':
            VERBOSE = True
        elif arg == '-p':
            PROCESSES = int(args.pop(0))
        elif arg == '-r':
            REQUESTS = int(args.pop(0))
        else:
            usage(1)
    if len(args) != 1:
        usage(1)
    URL = args.pop(0)


    # Create pool of workers and perform requests

    pool = multiprocessing.Pool(PROCESSES)
    average = pool.map(do_request,range(PROCESSES))

    total = 0
    for a in average:
        total = total + a
    average = total / PROCESSES

    print('TOTAL AVERAGE ELAPSED TIME: {:2f}'.format(average)) #Plus some stuff





# vim: set sts=4 sw=4 ts=8 expandtab ft=python:
