#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>
#include <default.h>

#define FAIL_STOP

#ifdef DEBUG
#define log_file(fmt, ...) do { \
  FILE *fp = fopen(PATH_LOG, "a"); \
  fprintf(fp, fmt "\n", ##__VA_ARGS__); \
  fclose(fp); \
} while (0)
#define log_func(fmt, ...) printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
#define log_debug(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#else
#define log_file(...) do {} while (0)
#define log_func(...) do {} while (0)
#define log_debug(...) do {} while (0)
#endif

#ifdef FAILE_STOP
#define log_err(fmt, ...) do { \
  printf("Error: " fmt  " (file %s, line %d, function %s)\n", ##__VA_ARGS__, __FILE__, __LINE__, __func__); \
  exit(-1); \
} while (0)
#else
#define log_err(fmt, ...) printf("Error: " fmt  " (file %s, line %d, function %s)\n", ##__VA_ARGS__, __FILE__, __LINE__, __func__)
#endif

#define log_eval(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#endif
