// arquivo: wr80vm.c (versão Linux/X11, adaptada do vga13h_window.c)
// Compile com: gcc wr80vm.c -lX11 -lpthread -o wr80vm

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "../wr80vm_private.h"
#include "../../wr80emu/src/wr80emu.h"

#define WIDTH  320
#define HEIGHT 200
#define PIXELS (WIDTH * HEIGHT)

static uint8_t *pixel_buffer = NULL;
static uint32_t *image_buffer = NULL;
static uint32_t palette[256];

static pthread_t hThreadScan = 0;
static pthread_t hThreadEmu = 0;
static pthread_t hThreadDemo = 0;
static volatile bool running = false;

static Display *display = NULL;
static Window window;
static GC gc;
static XImage *ximage = NULL;

static pthread_mutex_t cs = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t frame_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t frame_cond = PTHREAD_COND_INITIALIZER;
static volatile bool frame_ready = false;

// wrapper que chama emulate_buffer
void *emulate_thread(void *param) {
    EmuArgs *args = (EmuArgs*)param;

    unsigned char *memory = NULL;
    char *binary = args->filename;

    int size = load_bin(binary, &memory);

    if (size != -1) {
        load_config("config.dat");

        pthread_t ctrl = 0;
        if (devs.controller) {
            ctrl_run = true;
            pthread_create(&ctrl, NULL, controller, NULL);
        }

        emulate_buffer(memory, size, false);
        free(memory);
        memory = NULL;

        if (devs.romf[0]) {
            write_bin(devs.romf, rom, size);
            free(rom);
            rom = NULL;
        }

        if (devs.controller) {
            ctrl_run = false;
            if (ctrl) pthread_join(ctrl, NULL);
        }
    }

    // se o emulador terminar, sinaliza frame e encerra
    pthread_mutex_lock(&frame_mtx);
    frame_ready = true;
    pthread_cond_signal(&frame_cond);
    pthread_mutex_unlock(&frame_mtx);

    running = false;
    return NULL;
}

#pragma GCC push_options
#pragma GCC optimize ("O3")
static void flush_pixels_optimized(void) {
    uint32_t *restrict dst = image_buffer;
    uint8_t  *restrict src = pixel_buffer;
    uint32_t *restrict pal = palette;
    for (size_t i = 0; i < (size_t)PIXELS; ++i)
        dst[i] = pal[src[i]];
}
#pragma GCC pop_options

unsigned long long timespec_to_ms(const struct timespec *ts) {
    return (unsigned long long)ts->tv_sec * 1000ULL + (unsigned long long)(ts->tv_nsec / 1000000ULL);
}

void *scan_thread(void *arg) {
    while (running) {
        // espera por frame_ready ou timeout (para manter compatibilidade)
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        struct timespec timeout = now;
        // timeout curto para não travar se emulador não sinaliza
        timeout.tv_nsec += 50 * 1000000L; // 50ms
        if (timeout.tv_nsec >= 1000000000L) {
            timeout.tv_sec += 1;
            timeout.tv_nsec -= 1000000000L;
        }

        pthread_mutex_lock(&frame_mtx);
        if (!frame_ready) {
            pthread_cond_timedwait(&frame_cond, &frame_mtx, &timeout);
        }
        frame_ready = false;
        pthread_mutex_unlock(&frame_mtx);

        pthread_mutex_lock(&cs);
        flush_pixels_optimized();
        XPutImage(display, window, gc, ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
        pthread_mutex_unlock(&cs);

        // fallback sleep para evitar busy loop
        usleep(1000);
    }
    return NULL;
}

void set_pixel(int x, int y, uint8_t color_index) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    pthread_mutex_lock(&cs);
    pixel_buffer[y * WIDTH + x] = color_index;
    pthread_mutex_unlock(&cs);

    pthread_mutex_lock(&frame_mtx);
    frame_ready = true;
    pthread_cond_signal(&frame_cond);
    pthread_mutex_unlock(&frame_mtx);
}

// Thread exemplo que mexe pixels
void *demo_thread(void *arg) {
    int t = 0;
    while (running) {
        for (int x = 0; x < WIDTH; x += 5) {
            int y = (t + x/5) % HEIGHT;
            uint8_t color = (uint8_t)((x + t) & 255);
            set_pixel(x, y, color);
        }
        t++;
        usleep(50000); // Sleep(50) no Windows
    }
    return NULL;
}

void build_palette(void) {
    int idx = 0;
    for (int r = 0; r < 6; r++)
        for (int g = 0; g < 6; g++)
            for (int b = 0; b < 6; b++)
                palette[idx++] = ( (r * 51) << 16 ) | ( (g * 51) << 8 ) | (b * 51);
    for (; idx < 256; idx++) {
        int v = (idx - 216) * (255 / (256 - 216));
        palette[idx] = (v << 16) | (v << 8) | v;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Uso: %s <arquivo>\n", argv[0]);
        return 1;
    }

    // copia filename para args (structure definida em wr80vm_private.h)
    memset(&args, 0, sizeof(args));
    strncpy(args.filename, argv[1], sizeof(args.filename) - 1);
    args.filename[sizeof(args.filename)-1] = '\0';

    // Cria pixel_buffer
    pixel_buffer = (uint8_t*)malloc(PIXELS);
    if (!pixel_buffer) {
        fprintf(stderr, "Erro ao alocar pixel_buffer\n");
        return 1;
    }
    memset(pixel_buffer, 0, PIXELS);

    pthread_mutex_init(&cs, NULL);
    pthread_mutex_init(&frame_mtx, NULL);
    pthread_cond_init(&frame_cond, NULL);

    build_palette();

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Erro: não foi possível abrir o display X11\n");
        free(pixel_buffer);
        return 1;
    }
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    int win_w = WIDTH * 2, win_h = HEIGHT * 2;
    window = XCreateSimpleWindow(display, root, 100, 100, win_w, win_h, 1,
                                 BlackPixel(display, screen), WhitePixel(display, screen));
    XStoreName(display, window, "WR80 Virtual Machine - Res: 320x200x256");

    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete, 1);

    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    gc = XCreateGC(display, window, 0, NULL);

    image_buffer = (uint32_t*)malloc(PIXELS * 4);
    if (!image_buffer) {
        fprintf(stderr, "Erro ao alocar image_buffer\n");
        XFreeGC(display, gc);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        free(pixel_buffer);
        return 1;
    }
    memset(image_buffer, 0, PIXELS * 4);

    ximage = XCreateImage(display, DefaultVisual(display, screen), DefaultDepth(display, screen),
                          ZPixmap, 0, (char*)image_buffer, WIDTH, HEIGHT, 32, WIDTH * 4);
    if (!ximage) {
        fprintf(stderr, "Erro ao criar XImage\n");
        free(image_buffer);
        XFreeGC(display, gc);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        free(pixel_buffer);
        return 1;
    }

    XMapWindow(display, window);
    XFlush(display);

    running = true;

    // antes do loop de mensagens
    args.buf = pixel_buffer;
    args.size = PIXELS;        // 320*200
    args.flag = true;

    if (pthread_create(&hThreadScan, NULL, scan_thread, NULL) != 0) {
        fprintf(stderr, "Erro ao criar scan_thread\n");
        running = false;
    }
    if (pthread_create(&hThreadEmu, NULL, emulate_thread, &args) != 0) {
        fprintf(stderr, "Erro ao criar emulate_thread\n");
        running = false;
    }

    // demo_thread ativada (cimentada)
    if (pthread_create(&hThreadDemo, NULL, demo_thread, NULL) != 0) {
        // não crítico, continua sem demo
        hThreadDemo = 0;
    }

    XEvent ev;
    while (running) {
        while (XPending(display)) {
            XNextEvent(display, &ev);
            if (ev.type == Expose) {
                pthread_mutex_lock(&cs);
                XPutImage(display, window, gc, ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                pthread_mutex_unlock(&cs);
            } else if (ev.type == KeyPress) {
                running = false;
            } else if (ev.type == ClientMessage) {
                if ((Atom)ev.xclient.data.l[0] == wm_delete) {
                    running = false;
                }
            } else if (ev.type == DestroyNotify) {
                running = false;
            }
        }
        usleep(10000);
    }

    if (hThreadEmu) pthread_join(hThreadEmu, NULL);
    if (hThreadScan) pthread_join(hThreadScan, NULL);
    if (hThreadDemo) pthread_join(hThreadDemo, NULL);

    pthread_mutex_destroy(&cs);
    pthread_mutex_destroy(&frame_mtx);
    pthread_cond_destroy(&frame_cond);

    if (ximage) {
        ximage->data = NULL;
        XDestroyImage(ximage);
        ximage = NULL;
    }
    if (image_buffer) { free(image_buffer); image_buffer = NULL; }

    if (gc) XFreeGC(display, gc);
    if (window) XDestroyWindow(display, window);
    if (display) XCloseDisplay(display);

    if (pixel_buffer) free(pixel_buffer);

    return 0;
}