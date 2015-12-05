#ifdef DEBUG
#define log_debug(fmt, ...) do { \
  printf(fmt, ##__VA_ARGS__); \
  printf("\n"); \
} while (0)
#else
#define log_debug(...) do {} while (0)
#endif

#ifndef RELEASE
#define log_err(fmt, ...) do { \
  printf(fmt, ##__VA_ARGS__); \
  printf(" (file %s, line %d, function %s)\n", __FILE__, __LINE__, __func__); \
} while (0)
#else
#define log_err(...) do {} while (0)
#endif