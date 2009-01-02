#!/usr/bin/python

import sys
import os

def readFilelist(filename):
    try:
        f = open(filename, 'r')
    except:
        return []
    return [ele[:-1] for ele in f.readlines()]

def create_filter_func(cmd, log_file):
    if log_file == "":
        return lambda filename : 0 == os.system(cmd + " " + filename)
    else:
        return lambda filename : 0 == os.system(cmd + " " + filename + " 2>> " + log_file)

def write_filelist(files, filename):
    f = open(filename, 'w')
    for arg in files:
        f.write(arg + '\n')
    f.close()

if __name__ == "__main__":
    if len(sys.argv) < 3 or len(sys.argv) > 4:
        print "Usage: " + sys.argv[0] + " <test_executable> <file_list>"
        raise SystemExit


    if not os.path.isfile(sys.argv[1]):
        print sys.argv[1] + " not found"
        raise SystemExit

    cmd = sys.argv[1]
    base_name = cmd.split('/')[-1]

    success_file = base_name + ".pass"
    fail_file = base_name + ".fail"

    if len(sys.argv) == 4:
        log_file = sys.argv[3]
        if os.path.isfile(log_file):
            os.system("rm " + log_file)
    else:
        log_file = ""
        
    files = readFilelist(sys.argv[2])

    filterfunc = create_filter_func(cmd, log_file)
    successful = filter(filterfunc, files)
    failed = list(set(files) - set(successful))

    write_filelist(failed, fail_file)
    write_filelist(successful, success_file)

