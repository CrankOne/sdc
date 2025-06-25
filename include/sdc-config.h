#ifndef H_SDC_CONFIG_H
#define H_SDC_CONFIG_H 1

#define SDC_VERSION "0.3.0"

#define SQLite3_FOUND 1
#define SDC_SQL_SCRIPTS_PATH_ENVVAR "SDC_SQL_SCRIPTS_PATH"

#ifndef SDC_NO_ROOT
#   define ROOT_FOUND 1
#   if !ROOT_FOUND
#       define SDC_NO_ROOT 1
#   endif
#endif

#endif  /* H_SDC_CONFIG_H */

