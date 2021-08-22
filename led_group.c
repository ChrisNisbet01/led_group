// SPDX-License-Identifier: GPL-2.0

/*
 * led_group.c
 *
 * This program creates a new userspace LED class device and monitors it.
 * A timestamp and brightness value is printed each time the brightness changes.
 *
 * Usage: led_group <group_leader_led_name> <led1> <led2>
 *
 * <devicegroup_leader_led_namename> is the name of the LED class device to be
 * created. The other LEDs will track the brightness of this LED.
 *
 * <led[n]> are the LEDS that should track the brightness of the group leader.
 *
 * Pressing CTRL+C will exit.
 */

#include <linux/uleds.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if WITH_TIMESTAMP
#include <time.h>
#endif
#include <unistd.h>

#define MAX_LEDS_IN_GROUP 4
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
static int const max_brightness = 100;

struct led_group
{
    size_t count;
    int led_fds[MAX_LEDS_IN_GROUP];
};

#if HAVE_STRLCPY == 0
static char *
strlcpy(char * const dest, char const * const src, size_t const len)
{
    strncpy(dest, src, len - 1);
    dest[len - 1] = '\0';

    return dest;
}
#endif

static void
update_led(int const fd, int const brightness)
{
    char buf[20];

    snprintf(buf, sizeof buf, "%d\n", brightness);
    lseek(fd, 0, SEEK_SET);
    if (TEMP_FAILURE_RETRY(write(fd, buf, strlen(buf))) < 0)
    {
        perror(("failed to write to group LED"));
    }
}

static void
update_led_group(struct led_group const * const group, int const brightness)
{
    for (size_t i = 0; i < group->count; i++)
    {
        update_led(group->led_fds[i], brightness);
    }
}

static bool
add_led_by_fd_to_led_group(struct led_group * const led_group, int const fd)
{
    bool added;

    if (led_group->count >= ARRAY_SIZE(led_group->led_fds))
    {
        added = false;
        goto done;
    }

    led_group->led_fds[led_group->count] = fd;
    led_group->count++;

    added = true;

done:
    return added;
}

static bool
add_led_by_name_to_led_group(
    struct led_group * const led_group, char const * const led_name)
{
    bool added;
    char led_brightness_path[LED_MAX_NAME_SIZE + 30];

    snprintf(led_brightness_path,
             sizeof led_brightness_path,
             "/sys/class/leds/%s/brightness",
             led_name);

    int const fd = TEMP_FAILURE_RETRY(open(led_brightness_path, O_WRONLY));

    if (fd < 0)
    {
        added = false;
        goto done;
    }

    added = add_led_by_fd_to_led_group(led_group, fd);

done:
    if (!added)
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    return added;
}

static void
close_led_group_leds(struct led_group * const led_group)
{
    for (size_t i = 0; i < led_group->count; i++)
    {
        close(led_group->led_fds[i]);
    }
}

static void
track_led_brightness(int const fd, struct led_group const * const led_group)
{
    while (1)
    {
        int brightness;
        int const ret =
            TEMP_FAILURE_RETRY(read(fd, &brightness, sizeof(brightness)));

        if (ret == -1)
        {
            perror("Failed to read brightness from /dev/uleds");
            goto done;
        }

#if WITH_TIMESTAMP
        struct timespec ts;

        clock_gettime(CLOCK_MONOTONIC, &ts);
        printf("[%lu.%03lu] %u\n",
               ts.tv_sec,
               ts.tv_nsec / 1000000,
               brightness);
#endif

        update_led_group(led_group, brightness);
    }

done:
    return;
}

static int
create_led_group_leader(char const * const led_name)
{
    struct uleds_user_dev uleds_dev;

    strlcpy(uleds_dev.name, led_name, sizeof(uleds_dev.name));
    uleds_dev.max_brightness = max_brightness;

    int fd = TEMP_FAILURE_RETRY(open("/dev/uleds", O_RDWR));

    if (fd < 0)
    {
        perror("Failed to create LED group leader");
        goto done;
    }

    int const ret =
        TEMP_FAILURE_RETRY(write(fd, &uleds_dev, sizeof(uleds_dev)));

    if (ret == -1)
    {
        perror("Failed to create LED group leader");
        close(fd);
        fd = -1;
    }

done:
    return fd;
}

int
main(int argc, char const * argv[])
{
    int exit_code;
    int fd = -1;
    struct led_group led_group =
    {
        .count = 0
    };

    if (argc < 4)
    {
        fprintf(stderr, "format: %s <group_name> <led 1> <led 2> ...\n",
                argv[0]);
        exit_code = EXIT_FAILURE;
        goto done;
    }

    fd = create_led_group_leader(argv[1]);
    if (fd == -1)
    {
        perror("Failed to open /dev/uleds");
        exit_code = EXIT_FAILURE;
        goto done;
    }

    int const first_arg_for_led_group = 2;

    for (int arg = first_arg_for_led_group; arg < argc; arg++)
    {
        if (!add_led_by_name_to_led_group(&led_group, argv[arg]))
        {
            fprintf(stderr, "failed to add LED %s to group\n",
                    argv[arg]);
            exit_code = EXIT_FAILURE;
            goto done;
        }
    }

    track_led_brightness(fd, &led_group);

    exit_code = EXIT_SUCCESS;

done:
    if (fd >= 0)
    {
        close(fd);
    }
    close_led_group_leds(&led_group);

    return exit_code;
}

