#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void new_window(const char *command) {
    const char *terminals[] = {
        "gnome-terminal", "konsole", "xterm", "lxterminal", "mate-terminal",
        "terminator", "tilix", "alacritty", "urxvt", "st", "kitty", "xfce4-terminal"
    };

    for (size_t i = 0; i < 12; i++) {
        pid_t pid = fork();
        if (pid < 0) return;
        if (pid == 0) {
            execlp(terminals[i], terminals[i], "-e", "bash", "-c", command, (char*)NULL);
            execlp(terminals[i], terminals[i], "--", "bash", "-c", command, (char*)NULL);
            _exit(127);
        }
    }
}