class Task {
public:
    int id;
    Task *next;
    Task(int i, Task *n):
        id(i),
        next(n)
    {}
};


int main (int argc, char const *argv[])
{
    Task *task_head = 0;
    Task *task1 = new Task(1, 0);
    Task *task2 = new Task(2, 0);
    Task *task3 = new Task(3, 0); // Orphaned.
    Task *task4 = new Task(4, 0);
    Task *task5 = new Task(5, 0);

    task_head = task1;
    task1->next = task2;
    task2->next = task4;
    task4->next = task5;

    int total = 0;
    Task *t = task_head;
    while (t != 0) {
        if (t->id >= 0)
            ++total;
        t = t->next;
    }
    __builtin_printf("We have a total number of %d tasks\n", total);

    // This corresponds to an empty task list.
    Task *empty_task_head = 0;

    Task *task_evil = new Task(1, 0);
    Task *task_2 = new Task(2, 0);
    Task *task_3 = new Task(3, 0);
    task_evil->next = task_2;
    task_2->next = task_3;
    task_3->next = task_evil; // In order to cause inifinite loop. :-)

    return 0; // Break at this line
}
