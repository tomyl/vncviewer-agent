
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "Agent.h"

int running = 0;
int pipein[2]; /* input from agent */
int pipeout[2]; /* output to agent */
char inbuf[2048];
int offset = 0;

void
agent_init(const char *path)
{
    if (pipe2(pipein, O_NONBLOCK) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    if (pipe2(pipeout, O_NONBLOCK) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    switch (fork()) {
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);

        case 0: /* child */
            dup2(pipein[1], STDOUT_FILENO);
            dup2(pipeout[0], STDIN_FILENO);
            close(pipein[0]); /* close read end */
            close(pipeout[1]); /* close write end */
            execlp(path, path, NULL);
            perror("exec");
            exit(EXIT_FAILURE);

        default: /* parent */
            close(pipein[1]); /* close write end */
            close(pipeout[0]); /* close read end */
            running = 1;
    }

    agent_send("init\n");
}

void
agent_close()
{
    if (pipein[0])
        close(pipein[0]);
    if (pipeout[1])
        close(pipeout[1]);
    running = 0;
}

void
agent_add_fds(fd_set * rfds)
{
    if (!running)
        return;

    FD_SET(pipein[0], rfds);
}

void
agent_send(const char *fmt, ...)
{
    va_list ap;
    char buf[1024];

    if (!running)
        return;

    va_start(ap, fmt);
    vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);

    fprintf(stderr, "Sending to agent: %s", buf);
    
    if (write(pipeout[1], buf, strlen(buf)) < 0)
        perror("write");
}

void
agent_process(fd_set * rfds, DesktopWindow *desktop)
{
    int n;
    char *start;
    char *end;
    char *cur;
    char *next;

    char cmd[1024];
    char *arg;
    char *pos;

    if (!running)
        return;

    if (!FD_ISSET(pipein[0], rfds))
        return;

    fprintf(stderr, "READING FROM AGENT\n");
    start = inbuf + offset;
    n = read(pipein[0], start, 1023);

    if (n == -1)
        perror("read");
    else if (n == 0)
        agent_close();
    else {
        end = start+n;
        *end = '\0';
        cur = start;

        while (cur < end) {
            next = strchr(cur+1, '\n');
            if (!next)
                break;

            strncpy(cmd, cur, next-cur); 
            cmd[next-cur] = '\0';
            cur = next+1;

            arg = (char *) NULL;
            pos = strchr(cmd, ' ');

            if (pos) {
                *pos = '\0';
                arg = pos+1;
            } else {
                pos = strchr(cmd, '\n');
                if (pos) {
                    *pos = '\0';
                }
            }
            if (arg) {
                pos = strchr(arg, '\n');
                if (pos) {
                    *pos = '\0';
                }
            }

            fprintf(stderr, "From agent: cmd='%s' arg='%s'\n", cmd, arg);

            if (!strcmp(cmd, "exit")) {
                fprintf(stderr, "Exiting...\n");
                exit(0);
            } else if (!strcmp(cmd, "send")) {
                if (arg)
                    desktop->sendString(arg);
            }
        }

        offset = end-cur;

        if (offset)
            memcpy(inbuf, cur, offset);
    }
}

