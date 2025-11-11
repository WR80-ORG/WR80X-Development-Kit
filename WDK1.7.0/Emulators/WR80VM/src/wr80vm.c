// Compilar no Linux com gcc wr80vm.c -lX11 -lpthread -o wr80vm, ou execute o arquivo "make"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "wr80vm_private.h"
#include "../wr80emu/src/wr80emu.h"

#define WIDTH  320
#define HEIGHT 200
#define PIXELS (WIDTH * HEIGHT)

static uint8_t *pixel_buffer = NULL;
static uint32_t *image_buffer = NULL;
static volatile bool running = false;

static Display *display = NULL;
static Window window;
static GC gc;
static XImage *ximage = NULL;
static pthread_t hScan, hEmu;
static pthread_mutex_t cs = PTHREAD_MUTEX_INITIALIZER;

static uint32_t palette[256];

typedef struct {
    char filename[512];
    uint8_t *buf;
    size_t size;
    bool flag;
} EmuArgs;

void *emulate_thread(void *param);
void *scan_thread(void *param);
void build_palette(void);
void flush_pixels(void);
void draw_frame(void);

void build_palette(void) {
    int idx = 0;
    for (int r = 0; r < 6; r++)
        for (int g = 0; g < 6; g++)
            for (int b = 0; b < 6; b++)
                palette[idx++] = (r * 51 << 16) | (g * 51 << 8) | (b * 51);
    for (; idx < 256; idx++) {
        int v = (idx - 216) * (255 / (256 - 216));
        palette[idx] = (v << 16) | (v << 8) | v;
    }
}

void flush_pixels(void) {
    pthread_mutex_lock(&cs);
    for (int i = 0; i < PIXELS; i++)
        image_buffer[i] = palette[pixel_buffer[i]];
    pthread_mutex_unlock(&cs);
}

void draw_frame(void) {
    flush_pixels();
    XPutImage(display, window, gc, ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
    XFlush(display);
}

void *scan_thread(void *arg) {
    while (running) {
        draw_frame();
        usleep(16000);
    }
    return NULL;
}

void *emulate_thread(void *param) {
    EmuArgs *args = (EmuArgs*)param;
    unsigned char *memory = NULL;
    char *binary = args->filename;

    int size = load_bin(binary, &memory);
    if (size != -1) {
        load_config("config.dat");

        pthread_t ctrl;
        if (devs.controller) {
            ctrl_run = true;
            pthread_create(&ctrl, NULL, controller, NULL);
        }

        emulate_buffer(memory, size, false);
        free(memory);

        if (devs.romf[0]) {
            write_bin(devs.romf, rom, size);
            free(rom);
        }

        if (devs.controller) {
            ctrl_run = false;
            pthread_join(ctrl, NULL);
        }
    }

    running = false;
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Uso: %s <arquivo>\n", argv[0]);
        return 1;
    }

    EmuArgs args;
    memset(&args, 0, sizeof(args));
    strncpy(args.filename, argv[1], sizeof(args.filename) - 1);
    args.flag = true;

    build_palette();

    display = XOpenDisplay(NULL);
    if (!display) return 1;
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    int win_w = WIDTH * 2, win_h = HEIGHT * 2;
    window = XCreateSimpleWindow(display, root, 100, 100, win_w, win_h, 1,
                                 BlackPixel(display, screen), WhitePixel(display, screen));
    XStoreName(display, window, "WR80 Virtual Machine - Res: 320x200x256");
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    gc = XCreateGC(display, window, 0, NULL);

    pixel_buffer = malloc(PIXELS);
    memset(pixel_buffer, 0, PIXELS);
    image_buffer = malloc(PIXELS * 4);
    memset(image_buffer, 0, PIXELS * 4);

    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);
    ximage = XCreateImage(display, visual, depth, ZPixmap, 0, 
                          (char*)image_buffer, WIDTH, HEIGHT, 32, WIDTH * 4);

    XMapWindow(display, window);
    XFlush(display);

    running = true;
    pthread_create(&hScan, NULL, scan_thread, NULL);
    pthread_create(&hEmu, NULL, emulate_thread, &args);

    while (running) {
        while (XPending(display)) {
            XEvent ev;
            XNextEvent(display, &ev);
            if (ev.type == Expose) draw_frame();
            if (ev.type == KeyPress) running = false;
        }
        usleep(10000);
    }

    pthread_join(hEmu, NULL);
    pthread_join(hScan, NULL);

    if (ximage) { ximage->data = NULL; XDestroyImage(ximage); }
    free(image_buffer);
    free(pixel_buffer);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    pthread_mutex_destroy(&cs);

    return 0;
}