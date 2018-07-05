#include <unistd.h>

ssize_t write_nbytes(int fd, void *buf, size_t n);
ssize_t read_nbytes(int fd, void *buf, size_t n);
int readline(int fd, void *vptr, int maxlen);
