void enqueue(Job job) {
    // pthread_mutex_lock(&SHM->mutex);
    // while (SHM->count == QUEUE_SIZE - 1) {
    //     pthread_mutex_unlock(&SHM->mutex);
    //     usleep(1);
    //     pthread_mutex_lock(&SHM->mutex);
    // }
    // SHM->job_queue[SHM->in] = job;
    // SHM->in = (SHM->in + 1) % QUEUE_SIZE;
    // SHM->count++;
    // SHM->job_created++;
    // pthread_mutex_unlock(&SHM->mutex);
}

void dequeue() {
    // pthread_mutex_lock(&SHM->mutex);
    // while (SHM->count == 0) {
    //     pthread_mutex_unlock(&SHM->mutex);
    //     usleep(1);
    //     pthread_mutex_lock(&SHM->mutex);
    // }
    // SHM->out = (SHM->out + 1) % QUEUE_SIZE;
    // SHM->count--;
    // pthread_mutex_unlock(&SHM->mutex);
}