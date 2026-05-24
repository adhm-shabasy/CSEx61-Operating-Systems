#include "headers.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./process_generator <processes.txt>\n");
        exit(1);
    }

    // TODO 1: Initialize the POSIX Message Queue (mq_open)
    struct mq_attr attr;
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = 10;
    attr.mq_msgsize = sizeof(struct process_data);
    attr.mq_curmsgs = 0;

    mqd_t mq = mq_open(QUEUE_NAME, O_WRONLY, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed in generator");
        exit(1);
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("fopen failed");
        exit(1);
    }
    char buffer[256];
    fgets(buffer, sizeof(buffer), file); // Skip header

    struct process_data p;
    while (fscanf(file, "%d %d %d %d", &p.id, &p.arrival_time, &p.run_time, &p.priority) == 4) {
        // TODO 2: Send the process data to the queue (mq_send)
        if (mq_send(mq, (char *)&p, sizeof(struct process_data), 0) == -1) {
            perror("mq_send failed");
            exit(1);
        }
        printf("Generator: Sent Process %d\n", p.id);
    }

    // TODO 3: Send a termination message (Process with ID -1)
    struct process_data terminator;
    terminator.id = -1;
    terminator.arrival_time = 0;
    terminator.run_time = 0;
    terminator.priority = 0;
    if (mq_send(mq, (char *)&terminator, sizeof(struct process_data), 0) == -1) {
        perror("mq_send terminator failed");
        exit(1);
    }
    printf("Generator: Sent termination signal.\n");

    printf("Generator: All processes sent. Exiting.\n");
    fclose(file);

    // TODO 4: Close the queue (mq_close)
    mq_close(mq);
    return 0;
}
