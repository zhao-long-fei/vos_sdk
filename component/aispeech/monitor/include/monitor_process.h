#ifndef MONITOR_PROCESS
#define MONITOR_PROCESS
typedef struct process_state_tag{
    float pcpu;
    int vmrss;
    int vmsize;
}process_state;

int monitor_process(unsigned int pid, process_state *p_state);
int get_pid(const char* process_name);
int get_cpu_usage(const char *process_name, const char *current_process);
#endif