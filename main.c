// Шараковский Юрий М8О-206Б-19
// Лабораторная работа №3
// Вариант 10

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>

#define NUM_OF_THREADS 128

#define MAX_WIN_W 8
#define MAX_WIN_H 8

struct bmpFileHeader {
    u_int16_t magic;
    u_int32_t size;
    u_int32_t reserved;
    u_int32_t offset;
} __attribute__((packed));

struct bmpInfo {
    u_int32_t size;
    u_int32_t width;
    u_int32_t height;
    u_int16_t planes;
    u_int16_t bitcount;
    u_int32_t compression;
    u_int32_t dataSize;
} __attribute__((packed));

struct pdata {
    u_int8_t v;
} __attribute__((packed));

struct args {
    struct pdata* in;
    struct pdata* out;
    u_int32_t line;
    u_int32_t increment;
    u_int32_t width;
    u_int32_t height;
};

void countingSort(int* array, int n) {
    int c[256] = { 0 };
    int k = 256;
    for (int i = 0; i < n; i++) {
        c[array[i]]++;
    }
    int b = 0;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < c[i]; j++) {
            array[b++] = i;
        }
    }
}

int compare(const void* arg1, const void* arg2)
{
    return *((int*)(arg1)) < *((int*)(arg2));
}

struct pdata median(struct pdata in[MAX_WIN_H][MAX_WIN_W]) {
    int a[MAX_WIN_H * MAX_WIN_W];
    for (int i = 0; i < MAX_WIN_H; ++i) {
        for (int j = 0; j < MAX_WIN_W; ++j) {
            a[i * MAX_WIN_W + j] = in[i][j].v;
        }
    }
    countingSort(a, MAX_WIN_H * MAX_WIN_W);
    struct pdata x;
    x.v = a[MAX_WIN_W * MAX_WIN_H / 2];
    return x;
}

void* medianFilter(void* arg) {
    struct args* a = (struct args*)arg;
    int ex = MAX_WIN_W / 2;
    int ey = MAX_WIN_H / 2;
    struct pdata colors[MAX_WIN_H][MAX_WIN_W];
    for (int y = a->line; y < a->height; y += a->increment) {
        for (int x = 0; x < a->width; ++x) {
            for (int fy = 0; fy < MAX_WIN_H; ++fy) {
                for (int fx = 0; fx < MAX_WIN_W; ++fx) {
                    if ((y + fy - ey >= 0) && (x + fx - ex >= 0)) {
                        colors[fy][fx] = a->in[(y + fy - ey) * a->width + (x + fx - ex)];
                    }
                }
            }
            a->out[y * a->width + x] = median(colors);
        }
    }
    pthread_exit(NULL);
}

void countThreads() {
    DIR* dir;
    struct dirent* entry;
    int pid = getpid();
    char dirname[256];
    sprintf(dirname, "/proc/%d/task", pid);
    int c = -3;
    if ((dir = opendir(dirname)) == NULL)
        perror("opendir() error");
    else {
        while ((entry = readdir(dir)) != NULL) c++;
        closedir(dir);
    }
    char buffer[256];
    sprintf(buffer, "Number of Threads: %d\n", c);
    write(0, buffer, strlen(buffer));
}

int t = 0;
void filterImage(struct pdata* in, struct pdata* out, u_int32_t w, u_int32_t h, u_int32_t numOfThreads) {
    struct args arg[NUM_OF_THREADS];
    pthread_t handles[NUM_OF_THREADS];

    for (int i = 0; i < numOfThreads; ++i) {
        arg[i].in = in;
        arg[i].out = out;
        arg[i].line = i;
        arg[i].increment = numOfThreads;
        arg[i].width = w;
        arg[i].height = h;
        if (pthread_create(&handles[i], NULL, medianFilter, &arg[i]) != 0) {
            exit(-20);
        }
    }
    if (!t) {
        countThreads();
        t = 1;
    }
    for (int i = 0; i < numOfThreads; ++i) {
        int res = pthread_join(handles[i], NULL);
        if (res != 0) {
            exit(-21);
        }
    }
}

int parseBMPheader(int fd, struct bmpFileHeader* hdr, struct bmpInfo* bmp) {
    u_int32_t ret;
    if ((ret = read(fd, hdr, sizeof(*hdr))) != sizeof(*hdr))
        return -1;
    if (hdr->magic != 0x4D42)
        return -2;
    if (hdr->reserved != 0x00000000)
        return -2;
    if ((ret = read(fd, bmp, sizeof(*bmp))) != sizeof(*bmp))
        return -3;
    if (bmp->bitcount != 8) {
        perror("Unsupported bitsize!");
        return -1;
    }
    return 0;
}

long long getTime() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
}

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    u_int32_t ret;
    if (argc < 4)
        exit(1);
    int fd = open(argv[1], O_RDWR);
    if (fd < 0)
        exit(fd);
    struct bmpFileHeader hdr;
    struct bmpInfo bmp;
    if (ret = parseBMPheader(fd, &hdr, &bmp))
        exit(ret);
    u_int32_t k = strtol(argv[2], NULL, 10);
    if (k > 100) {
        perror("Unsupported \'K\' value!");
        exit(-11);
    }
    u_int32_t thr = strtol(argv[3], NULL, 10);
    if (thr > NUM_OF_THREADS) {
        perror("Unsupported number of threads!");
        exit(-11);
    }

    if (lseek(fd, hdr.offset, SEEK_SET) != hdr.offset)
        exit(-3);

    struct pdata* inputData = malloc(bmp.dataSize);
    struct pdata* outputData = malloc(bmp.dataSize);
    struct pdata* tmp;
    if ((ret = read(fd, inputData, bmp.dataSize)) != bmp.dataSize) {
        perror("Bad read!");
        exit(-4);
    }

    double start, end;
    double dif;

    start = getTime();
    for (int i = 0; i < k; ++i) {
        filterImage(inputData, outputData, bmp.width, bmp.height, thr);
        tmp = inputData;
        inputData = outputData;
        outputData = tmp;
    }
    end = getTime();
    dif = end - start;

    char buffer[256];
    sprintf(buffer, "Time: %lf seconds.\n", dif / 1000.0);
    write(0, buffer, strlen(buffer));

    if (lseek(fd, hdr.offset, SEEK_SET) != hdr.offset) {
        exit(-5);
    }
    if ((ret = write(fd, outputData, bmp.dataSize)) != bmp.dataSize) {
        perror("Bad write!");
        exit(-6);
    }

    close(fd);
    exit(0);
}