#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <default.h>

#define FAIL_STOP

#ifdef DEBUG
#define log_file(fmt, ...) do { \
  FILE *fp = fopen(PATH_LOG, "a"); \
  fprintf(fp, fmt "\n", ##__VA_ARGS__); \
  fclose(fp); \
} while (0)

#define log_func(fmt, ...) do { \
  if (node_id >= 0) \
    printf("%s: " fmt " @node%d\n", __func__, ##__VA_ARGS__, node_id); \
  else \
    printf("%s: " fmt "\n", __func__, ##__VA_ARGS__); \
} while (0)

#define log_debug(fmt, ...) do { \
  if (node_id >= 0) \
    printf(fmt " @node%d\n", ##__VA_ARGS__, node_id); \
  else \
    printf(fmt "\n", ##__VA_ARGS__); \
} while (0)
#else
#define log_file(...) do {} while (0)
#define log_func(...) do {} while (0)
#define log_debug(...) do {} while (0)
#endif

#ifdef FAIL_STOP
#define log_err(fmt, ...) do { \
  if (node_id >= 0) \
    printf("Error: " fmt  " (file %s, line %d, function %s @node%d)\n", ##__VA_ARGS__, __FILE__, __LINE__, __func__, node_id); \
  else \
    printf("Error: " fmt  " (file %s, line %d, function %s\n", ##__VA_ARGS__, __FILE__, __LINE__, __func__); \
  exit(-1); \
} while (0)
#else
#define log_err(fmt, ...) do { \
  if (node_id >= 0) \
    printf("Error: " fmt  " (file %s, line %d, function %s @node%d)\n", ##__VA_ARGS__, __FILE__, __LINE__, __func__, node_id); \
  else \
    printf("Error: " fmt  " (file %s, line %d, function %s)\n", ##__VA_ARGS__, __FILE__, __LINE__, __func__); \
} while (0)
#endif

#define log_eval(fmt, ...) do { \
  if (node_id >= 0) \
    printf(fmt " @node%d\n", ##__VA_ARGS__, node_id); \
  else \
    printf(fmt "\n", ##__VA_ARGS__); \
} while (0)
#endif
