/*********************************************************
         shared_memory_reader..
**********************************************************/
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>
#include <errno.h>
#include <libconfig.h>
#include <sys/types.h>
#include <stdarg.h>

#define LEN 10
#define WATCH_TAG "[WATCHDOG]"
#define GW_CONFIG_PATH "/etc/lizard/gateway.conf"

const char *__lz_app_path, *__check_lz_app;
int __log_level;

pid_t __pid;

struct _health
{
    time_t curtime;
    bool inet_status;
    bool uart_status;
    bool first_interface_status;
    bool second_interface_status;
    bool usb_status;
} __health;

void error_log(char *tag, char *format_str, ...)
{
    if ((__log_level < 2) || (__log_level == 3))
    {
        openlog(tag, LOG_PID | LOG_NDELAY, LOG_USER);
        //va_list is a type to hold information about variable arguments
        va_list args;
        // va_start must be called before accessing variable argument list
        va_start(args, format_str);
        vsyslog(LOG_ERR, format_str, args);
        //va_end should be executed before the function returns whenever va_start has been previously used in that function
        va_end(args);
        closelog();
    }
}
void info_log(char *tag, char *format_str, ...)
{
    if ((__log_level == 2) || (__log_level == 3))
    {
        openlog(tag, LOG_PID | LOG_NDELAY, LOG_USER);
        va_list args;
        va_start(args, format_str);
        vsyslog(LOG_INFO, format_str, args);
        va_end(args);
        closelog();
    }
}
int find_running_pid()
{
    char line[LEN];
    FILE *cmd = popen(__check_lz_app, "r");
    if (cmd == NULL)
    {
        error_log(WATCH_TAG, "[FIND_RUNNING_PID]:File does not exist %s", strerror(errno));
        exit(1);
    }

    info_log(WATCH_TAG, "[FIND_RUNNING_PID]:File opened successfully");
    fgets(line, LEN, cmd);
    __pid = strtoul(line, NULL, 10);
    pclose(cmd);
    return __pid;
}
void health_check(void)
{
    struct _health *hdata;
    // ftok to generate unique key
    key_t key = ftok("shmfile", 65);

    // shmget returns an identifier in shmid
    int shmid = shmget(key, (sizeof(struct _health)), 0666 | IPC_CREAT);
    // shmat to attach to shared memory
    hdata = (struct _health *)shmat(shmid, (void *)0, 0);

    if ((hdata->inet_status == 1))
    {
        time(&hdata->curtime);
        info_log(WATCH_TAG, "[HEALTH_CHECK]:Health check is OK");
        info_log(WATCH_TAG, "[HEALTH_CHECK]:Data read in memory: Time: %s inet_stat: %d  uart_stat: %d", ctime(&hdata->curtime), hdata->inet_status, hdata->uart_status);
        if (hdata->first_interface_status == true)
        {
            info_log(WATCH_TAG, "[HEALTH_CHECK]:eth0 is available");
        }
        else
        {
            info_log(WATCH_TAG, "[HEALTH_CHECK]:eth0 is not available");
        }
        if (hdata->second_interface_status == true)
        {
            info_log(WATCH_TAG, "[HEALTH_CHECK]:eth1 is available...Modem connected");
        }
        else
        {
            info_log(WATCH_TAG, "[HEALTH_CHECK]:eth1 is not available...Modem not connected");
        }
        if (hdata->usb_status == true)
        {
            info_log(WATCH_TAG, "[HEALTH_CHECK]:usb0 is available...hotspot connected");
        }
        else
        {
            info_log(WATCH_TAG, "[HEALTH_CHECK]:usb0 is not available...hotspot not connected");
        }
    }
    else
    {
        info_log(WATCH_TAG, "[HEALTH_CHECK]:Health check is NOT OK");
        info_log(WATCH_TAG, "[HEALTH_CHECK]:Kill the present running main lizard binary");
        kill(__pid, SIGKILL);
        pid_t pid_2 = fork();

        if (pid_2 == 0)
        {
            char *args[] = {(char *)__lz_app_path, NULL};
            info_log(WATCH_TAG, "[HEALTH_CHECK]:Main lizard binary is Restarted now");
            execvp(args[0], args);
        }
    }
    sleep(2);
    //detach from shared memory
    shmdt(hdata);
}

int main()
{
    FILE *fptr;
  
    config_t cfg;
    //represents a configuration setting
    config_setting_t *gateway;
    // initializes the config_t structure pointed to by config.
    config_init(&cfg);
    //open a file
    fptr = fopen(GW_CONFIG_PATH, "r");
    if (fptr == NULL)
    {
        error_log(WATCH_TAG, "There is NO Config file in gateway");
        exit(1);
    }
    info_log(WATCH_TAG, "Successfully reading the gateway Configuration file");
    //This function reads and parses a configuration from the given stream into the configuration object config.
    if (!config_read(&cfg, fptr))
    {
        info_log(WATCH_TAG, "Data format is invalid in config file");
        exit(1);
    }
    if (config_lookup_int(&cfg, "conf_log_level", &__log_level))
    {
        info_log(WATCH_TAG, "Successfully reading the log_level");
    }
    else
    {
        info_log(WATCH_TAG, "The conf_log_level is not available");
    }
    if (config_lookup_string(&cfg, "check_lz_app", &__check_lz_app))
    {

        syslog(LOG_INFO, "Successfully reading the lz_app_path");
    }
    if (config_lookup_string(&cfg, "lz_app_path", &__lz_app_path))
    {

        syslog(LOG_INFO, "Successfully reading the lz_app_path");
    }

    if (find_running_pid())
    {
        info_log(WATCH_TAG, "Main lizard binary is running now %d", __pid);
        health_check();
    }
    else
    {
        info_log(WATCH_TAG, "Main lizard binary is Not running");
        pid_t pid_1 = fork();
        if (pid_1 == 0)
        {
            char *args[] = {(char *)__lz_app_path, NULL};
            info_log(WATCH_TAG, "Main lizard binary is restart now ");
            execvp(args[0], args);
        }
    }
    fclose(fptr);
    return 0;
}
