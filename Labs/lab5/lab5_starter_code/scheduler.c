#include "headers.h"
#include <stdbool.h>

// structure to track process state in the Ready Queue
struct pcb {
    struct process_data data;
    pid_t pid;
    int remaining_time;
    int waiting_time;
    bool is_started;
};

#define MAX_PROC 100

// Simple array-based ready queue (not circular, keeps things simple)
struct pcb ready_queue[MAX_PROC];
int rq_size = 0;

void rq_push(struct pcb *p) {
    ready_queue[rq_size++] = *p;
}

// Remove element at index i, shift left
struct pcb rq_remove(int i) {
    struct pcb p = ready_queue[i];
    for (int j = i; j < rq_size - 1; j++)
        ready_queue[j] = ready_queue[j+1];
    rq_size--;
    return p;
}

// FCFS: index 0
// RR: always index 0 (we rotate by removing from front and pushing to back)
// HPF: pick lowest priority integer, ties broken by arrival time
int rq_hpf_pick() {
    int best = 0;
    for (int i = 1; i < rq_size; i++) {
        if (ready_queue[i].data.priority < ready_queue[best].data.priority ||
           (ready_queue[i].data.priority == ready_queue[best].data.priority &&
            ready_queue[i].data.arrival_time < ready_queue[best].data.arrival_time)) {
            best = i;
        }
    }
    return best;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./scheduler <algo_id> [quantum]\n");
        exit(1);
    }

    int algo    = atoi(argv[1]);
    int quantum = (argc == 3) ? atoi(argv[2]) : 0;

    // TODO 1: Open the POSIX Message Queue
    struct mq_attr attr;
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = 10;
    attr.mq_msgsize = sizeof(struct process_data);
    attr.mq_curmsgs = 0;

    mqd_t mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    if (mq == (mqd_t)-1) { perror("mq_open"); exit(1); }

    // TODO 2: Read ALL processes from the MQ
    struct process_data future_processes[MAX_PROC];
    int total_processes = 0;

    while (1) {
        struct process_data p;
        if (mq_receive(mq, (char *)&p, sizeof(p), NULL) == -1) { perror("mq_receive"); exit(1); }
        if (p.id == -1) break;
        future_processes[total_processes++] = p;
    }

    printf("Scheduler: Received %d processes. Starting Virtual Clock...\n", total_processes);

    FILE *log_file = fopen("scheduler.log", "w");
    if (!log_file) { perror("fopen log"); exit(1); }

    int current_time        = 0;
    int processes_completed = 0;
    int running_idx         = -1; // index in ready_queue of running process (-1 = idle)
    int quantum_remaining   = 0;
    int busy_time           = 0;
    double total_tat = 0, total_wt = 0;

    // ==========================================================
    // VIRTUAL TIME LOOP
    // ==========================================================
    while (processes_completed < total_processes) {

        // TODO 3: Handle Arrivals
        for (int i = 0; i < total_processes; i++) {
            if (future_processes[i].arrival_time == current_time) {
                struct pcb new_pcb;
                new_pcb.data           = future_processes[i];
                new_pcb.remaining_time = future_processes[i].run_time;
                new_pcb.waiting_time   = 0;
                new_pcb.is_started     = false;

                pid_t pid = fork();
                if (pid == 0) {
                    execl("./process", "process", NULL);
                    perror("execl"); exit(1);
                }
                new_pcb.pid = pid;
                usleep(1000);
                kill(pid, SIGSTOP);
                rq_push(&new_pcb);

                // if this new arrival shifted the running index (it's always appended at end,
                // so running_idx stays valid)
            }
        }

        // TODO 4: Handle Termination
        if (running_idx != -1 && ready_queue[running_idx].remaining_time == 0) {
            struct pcb *r = &ready_queue[running_idx];
            int tat = current_time - r->data.arrival_time;
            total_tat += tat;
            total_wt  += r->waiting_time;

            fprintf(log_file,
                "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d\n",
                current_time, r->data.id, r->data.arrival_time,
                r->data.run_time, r->waiting_time, tat);

            kill(r->pid, SIGKILL);
            waitpid(r->pid, NULL, 0);
            processes_completed++;

            rq_remove(running_idx);
            running_idx = -1;
        }

        // TODO 5: Scheduling Logic
        if (algo == 1) {
            // FCFS – non-preemptive
            if (running_idx == -1 && rq_size > 0) {
                running_idx = 0;
                struct pcb *p = &ready_queue[running_idx];
                kill(p->pid, SIGCONT);
                p->is_started = true;
                fprintf(log_file,
                    "At time %d process %d started arr %d total %d remain %d wait %d\n",
                    current_time, p->data.id, p->data.arrival_time,
                    p->data.run_time, p->remaining_time, p->waiting_time);
            }

        } else if (algo == 2) {
            // Round Robin
            if (running_idx == -1 && rq_size > 0) {
                running_idx = 0;
                struct pcb *p = &ready_queue[running_idx];
                kill(p->pid, SIGCONT);
                quantum_remaining = quantum;
                if (!p->is_started) {
                    p->is_started = true;
                    fprintf(log_file,
                        "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        current_time, p->data.id, p->data.arrival_time,
                        p->data.run_time, p->remaining_time, p->waiting_time);
                } else {
                    fprintf(log_file,
                        "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        current_time, p->data.id, p->data.arrival_time,
                        p->data.run_time, p->remaining_time, p->waiting_time);
                }
            } else if (running_idx != -1 && quantum_remaining == 0 && rq_size > 1) {
                // Preempt: stop current, move to back, start next
                struct pcb *p = &ready_queue[running_idx];
                fprintf(log_file,
                    "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                    current_time, p->data.id, p->data.arrival_time,
                    p->data.run_time, p->remaining_time, p->waiting_time);
                kill(p->pid, SIGSTOP);
                struct pcb tmp = rq_remove(running_idx);
                rq_push(&tmp);
                running_idx = -1;

                running_idx = 0;
                struct pcb *next = &ready_queue[running_idx];
                kill(next->pid, SIGCONT);
                quantum_remaining = quantum;
                if (!next->is_started) {
                    next->is_started = true;
                    fprintf(log_file,
                        "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        current_time, next->data.id, next->data.arrival_time,
                        next->data.run_time, next->remaining_time, next->waiting_time);
                } else {
                    fprintf(log_file,
                        "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        current_time, next->data.id, next->data.arrival_time,
                        next->data.run_time, next->remaining_time, next->waiting_time);
                }
            }

        } else if (algo == 3) {
            // HPF – non-preemptive
            if (running_idx == -1 && rq_size > 0) {
                int best = rq_hpf_pick();
                // Move best to front (index 0) by rotating
                struct pcb chosen = rq_remove(best);
                // Shift everything right to make room at index 0
                for (int i = rq_size; i > 0; i--)
                    ready_queue[i] = ready_queue[i-1];
                ready_queue[0] = chosen;
                rq_size++;

                running_idx = 0;
                struct pcb *p = &ready_queue[running_idx];
                kill(p->pid, SIGCONT);
                p->is_started = true;
                fprintf(log_file,
                    "At time %d process %d started arr %d total %d remain %d wait %d\n",
                    current_time, p->data.id, p->data.arrival_time,
                    p->data.run_time, p->remaining_time, p->waiting_time);
            }
        }

        // TODO 6: Update Virtual Time Stats
        if (running_idx != -1) {
            ready_queue[running_idx].remaining_time--;
            busy_time++;
            if (algo == 2) quantum_remaining--;
        }
        // Increment waiting_time for queued (non-running) processes
        for (int i = 0; i < rq_size; i++) {
            if (i != running_idx)
                ready_queue[i].waiting_time++;
        }

        current_time++;
        usleep(1000);
    }
    // ==========================================================

    // TODO 7: Write metrics.txt and clean up IPC
    double cpu_util = ((double)busy_time / current_time) * 100.0;
    double avg_tat  = total_tat / total_processes;
    double avg_wt   = total_wt  / total_processes;

    FILE *metrics = fopen("metrics.txt", "w");
    fprintf(metrics, "CPU utilization = %.2f%%\n", cpu_util);
    fprintf(metrics, "Avg TAT = %.2f, Avg WT = %.2f\n", avg_tat, avg_wt);
    fclose(metrics);
    fclose(log_file);

    mq_close(mq);
    mq_unlink(QUEUE_NAME);

    printf("Simulation Finished.\n");
    return 0;
}
