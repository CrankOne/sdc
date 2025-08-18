#include "sdc-db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int
sdc_read_sql_file(const char *filepath_, char **destBuffer) {
    assert(filepath_ && *filepath_);
    assert(destBuffer);

    const char * basePath = getenv(SDC_SQL_SCRIPTS_PATH_ENVVAR);
    if(!basePath) {
        fputs("Error: environment variable " SDC_SQL_SCRIPTS_PATH_ENVVAR
                " is empty or not defined.", stderr);
        return 1;
    }
    size_t pathBufLen = strlen(filepath_) + strlen(basePath) + 2;
    char * filepath = alloca(pathBufLen);
    assert(filepath);

    strcpy(filepath, basePath);
    strcat(filepath, "/");
    strcat(filepath, filepath_);

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Cannot open SQL file: %s\n", filepath);
        return 2;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char * sql = malloc(size + 1);
    if (!sql) {
        fprintf(stderr, "Out of memory reading SQL file\n");
        fclose(file);
        return 3;
    }

    fread(sql, 1, size, file);
    sql[size] = '\0';  // null-terminate the string
    fclose(file);

    *destBuffer = sql;

    return 0;
}


