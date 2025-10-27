/*
 * Simple Watchdog Daemon
 *
 * Implements:
 *   Usage: watchdog [-t N[ms]] [-T N[ms]] [-F] <device>
 *
 *   -t N    Ping (keepalive) every N seconds (default 30)
 *   -T N    Watchdog timeout in seconds (default 60)
 *   -F      Run in foreground (don't daemonize)
 *
 * On exit (SIGINT/SIGTERM), disables the watchdog (magic close) and quits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <getopt.h>
#include <sys/time.h>

static volatile sig_atomic_t stop = 0;

static void sig_handler(int signo)
{
    stop = 1;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [-t N] [-T N] [-F] <watchdog_device>\n"
        "  -t N    Ping interval in seconds (default 30)\n"
        "  -T N    Watchdog timeout in seconds (default 30,Max 85)\n"
        "  -F      Run in foreground\n",
        prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    const char *dev = NULL;
    int fd, opt;
    int ping_interval = 30;
    int timeout = 60;
    int foreground = 0;
    int ret;

    while ((opt = getopt(argc, argv, "t:T:F")) != -1) {
        switch (opt) {
        case 't':
            ping_interval = atoi(optarg);
            if (ping_interval <= 0)
                usage(argv[0]);
            break;
        case 'T':
            timeout = atoi(optarg);
            if (timeout <= 0 || timeout > 85)
                usage(argv[0]);
            break;
        case 'F':
            foreground = 1;
            break;
        default:
            usage(argv[0]);
        }
    }

    if (optind >= argc) {
        usage(argv[0]);
    }
    dev = argv[optind];

    /* Daemonize if not in foreground */
    if (!foreground) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            /* parent */
            exit(EXIT_SUCCESS);
        }
        /* child */
        if (setsid() < 0) {
            perror("setsid");
            /* continue anyway */
        }
        /* ignore SIGPIPE */
        signal(SIGPIPE, SIG_IGN);
    }

    /* Install signal handlers */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* Open the watchdog device */
    fd = open(dev, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", dev, strerror(errno));
        exit(EXIT_FAILURE);
    }
    /* --- 1) close watchdog hardware, to modify timeout --- */
    {
        int option = WDIOS_DISABLECARD;
        ret = ioctl(fd, WDIOC_SETOPTIONS, &option);
        if (ret < 0) {
            if (errno == EINVAL || errno == EOPNOTSUPP) {
                fprintf(stderr,
                    "WDIOC_SETOPTIONS(DISABLE) failed: %s\n"
                    "  maybe nowayout=1, running without stop watchdog.\n"
                    "  please add nowayout=0 to kernel command line.\n",
                    strerror(errno));
            } else {
                perror("WDIOC_SETOPTIONS(DISABLE)");
            }
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
    /* --- 2) set timeout --- */
    ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
    if (ret < 0) {
        fprintf(stderr, "WDIOC_SETTIMEOUT failed: %s\n", strerror(errno));
        /* continue anyway */
    } else {
        printf("Watchdog timeout set to %d seconds\n", timeout);
    }

    /* You can also read back the actual timeout:
     * ioctl(fd, WDIOC_GETTIMEOUT, &timeout);
     * printf("Effective timeout is %d seconds\n", timeout);
     */

    /* --- 3) enable watchdog hardware --- */
    {
        int option = WDIOS_ENABLECARD;
        ret = ioctl(fd, WDIOC_SETOPTIONS, &option);
        if (ret < 0) {
            fprintf(stderr, "WDIOC_SETOPTIONS(ENABLE) failed: %s\n", strerror(errno));
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    printf("Starting watchdog ping every %d seconds\n", ping_interval);

    /* Main ping loop */
    while (!stop) {
        ret = ioctl(fd, WDIOC_KEEPALIVE, 0);
        if (ret < 0) {
            fprintf(stderr, "WDIOC_KEEPALIVE failed: %s\n", strerror(errno));
            break;
        }
        sleep(ping_interval);
    }

    /* Disable (magic close) */
    {
        int option = WDIOS_DISABLECARD;
        if (ioctl(fd, WDIOC_SETOPTIONS, &option) < 0) {
            fprintf(stderr, "WDIOC_SETOPTIONS(DISABLE) failed: %s\n", strerror(errno));
        }
        /* Magic character 'V' */
        if (write(fd, "V", 1) != 1) {
            fprintf(stderr, "Magic close write failed: %s\n", strerror(errno));
        }
    }
    close(fd);
    printf("Watchdog stopped, exiting.\n");
    return 0;
}

