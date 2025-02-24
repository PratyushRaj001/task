#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>

#define DRIVER_NAME "/dev/vicharak"
#define POP_DATA _IOR('a', 'c', struct data*)

struct data {
    int length;
    char *data;
};

int main(void) {
    int fd = open(DRIVER_NAME, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    struct data *d = malloc(sizeof(struct data));
    d->length = 3;
    d->data = malloc(3);

    int ret = ioctl(fd, POP_DATA, d);
    if (ret < 0) {
        perror("IOCTL failed");
        free(d->data);
        free(d);
        close(fd);
        return -1;
    }

    printf("Popped data: %s\n", d->data);

    free(d->data);
    free(d);
    close(fd);
    return 0;
}
