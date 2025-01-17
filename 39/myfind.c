#include <stdio.h>     // fprintf, perror
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>    // getopt
#include <stdlib.h>    // exit
#include <string.h>    // strncmp, strlen
#include <dirent.h>    // opendir, readdir, closedir
#include <errno.h>     // EACCES
#include <limits.h>    // INT_MAX
#include <regex.h>     // regcomp, regexec

#define LOG_OPTIND
#define LOG_EACCES_SEPARATE
#define STRINGSIZE 1024
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

void find_dir(char * pathname, int currentDepth, int maxDepth, regex_t * preg) {
    DIR *dp;
    struct dirent *d;
    errno = 0;
    if (currentDepth++ > maxDepth)
        return;
    if ((dp = opendir(pathname)) == NULL) {
        #ifdef LOG_EACCES_SEPARATE
        if (errno == EACCES) {
            fprintf(stderr, "myfind: '%s': Permission denied\n", pathname);
            return;
        } else {
        #endif
            handle_error("opendir");
        #ifdef LOG_EACCES_SEPARATE
        }
        #endif
    }
    while ((d = readdir(dp)) != NULL) {
        char filePath[STRINGSIZE] = "";
        strncpy(filePath, pathname, strlen(pathname));
        if (filePath[strlen(filePath) - 1] != '/')
            strncat(filePath, "/", 1);
        strncat(filePath, d->d_name, strlen(d->d_name));
        /*
        avoid current dir search loop by "." and parent dir search by "..".
        */
        if (strncmp(d->d_name, ".", 1) != 0 && strncmp(d->d_name, "..", 2) != 0) {
            if (preg == NULL || regexec(preg, d->d_name, 0, NULL, 0) != REG_NOMATCH)
                printf("%s\n", filePath);
            if (d->d_type == DT_DIR)
                find_dir(filePath, currentDepth, maxDepth, preg);
        }
    }
    closedir(dp);
}

int main(int argc, char *argv[]) {
    char * pathname = ".";
    char * pattern = "";
    struct stat sb;
    int maxDepth = INT_MAX, opt, currentDepth = 1, enable_pattern = 0;
    regex_t preg;

    while ((opt = getopt(argc, argv, "d:n:")) != -1) {
        switch (opt) {
            case 'd':
                #ifdef LOG_OPTIND
                printf("optind:%d\n",optind);
                #endif
                maxDepth = atoi(optarg);
                if (maxDepth < 0) {
                    fprintf(stderr, "Max depth must be positive.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'n':
                pattern = optarg;
                /*
                run '*' to use the bare regex parameter.
                https://stackoverflow.com/a/29332008/21294350
                */
                #ifdef LOG_OPTIND
                printf("optind:%d pattern:%s\n",optind,pattern);
                #endif
                enable_pattern = 1;
                break;
            default:
                break;
        }
    }

    // if (argc > 3 && optind == 1) {
    if (argc != 6 ) {
        #ifdef LOG_OPTIND
        printf("argc:%d optind:%d\n",argc,optind);
        #endif
        fprintf(stderr, "Usage: %s -d [max depth] -n [pattern] [filepath]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (optind == argc - 1) {
        pathname = argv[optind];
    }

    if (enable_pattern && regcomp(&preg, pattern, 0) != 0)
        handle_error("regcomp");

    if (stat(pathname, &sb) == -1)
        handle_error("stat");
    /*
    1. "0, NULL" doesn't store the result
    2. use the "^$" wrapper for the exact match https://stackoverflow.com/questions/63421654/why-does-this-regexec-in-c-return-as-a-match-when-it-shouldnt#comment112146676_63421654.
    */
    if (!enable_pattern || (enable_pattern && regexec(&preg, pathname, 0, NULL, 0) != REG_NOMATCH))
        printf("%s\n", pathname);

    if (S_ISDIR(sb.st_mode)) {
        if (enable_pattern)
            find_dir(pathname, currentDepth, maxDepth, &preg);
        else
            find_dir(pathname, currentDepth, maxDepth, NULL);
    }

    exit(EXIT_SUCCESS);
}
