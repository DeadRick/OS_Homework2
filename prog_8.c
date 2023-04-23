#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>

// Ключи для разделяемой памяти и семафора
#define SHM_KEY 1234
#define SEM_KEY 5678

// Структура для хранения похищенного имущества
typedef struct
{
    int items;
    int value;
} StolenGoods;

// Функция для ожидания освобождения семафора
void sem_wait(int sem_id)
{
    struct sembuf sem_op = {0, -1, 0};
    semop(sem_id, &sem_op, 1);
}

// Функция для освобождения семафора
void sem_post(int sem_id)
{
    struct sembuf sem_op = {0, 1, 0};
    semop(sem_id, &sem_op, 1);
}

int main()
{
    // Создание и инициализация семафора
    int sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1)
    {
        perror("Ошибка создания семафора");
        exit(EXIT_FAILURE);
    }
    union semun
    {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
        struct seminfo *__buf;
    } sem_arg;
    sem_arg.val = 1; // Устанавливаем начальное значение семафора
    semctl(sem_id, 0, SETVAL, sem_arg);

    // Создание и инициализация разделяемой памяти
    int shm_id = shmget(SHM_KEY, sizeof(StolenGoods), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("Ошибка создания разделяемой памяти");
        semctl(sem_id, 0, IPC_RMID); // Удаляем семафор
        exit(EXIT_FAILURE);
    }
    StolenGoods *stolen_goods = (StolenGoods *)shmat(shm_id, NULL, 0);
    if (stolen_goods == (void *)-1)
    {
        perror("Ошибка отображения разделяемой памяти");
        semctl(sem_id, 0, IPC_RMID);    // Удаляем семафор
        shmctl(shm_id, IPC_RMID, NULL); // Удаляем разделяемую память
        exit(EXIT_FAILURE);
    }

    // Создание дочерних процессов
    for (int i = 0; i < 3; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("Ошибка создания дочернего процесса");
            semctl(sem_id, 0, IPC_RMID);    // Удаляем семафор
            shmctl(shm_id, IPC_RMID, NULL); // Удаляем разделяемую память
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // Код дочернего процесса
            int id = getpid();
            int total_price = 0;
            printf("Дочерний процесс %d запущен\n", id);
            for (int j = 0; j < 5; j++)
            {
                sem_wait(sem_id); // Ожидание освобождения семафора

                int sum = rand() % 101;
                // Взаимодействие с разделяемой памятью
                stolen_goods->items++;
                stolen_goods->value += sum;
                total_price += sum;

                printf("Дочерний процесс %d украл %d предметов на сумму %d\n", id, stolen_goods->items, stolen_goods->value);

                sem_post(sem_id); // Освобождение семафора
                sleep(1);         // Пауза между кражами
            }
            printf("Дочерний процесс %d завершен. Общая стоимость: %d\n", id, total_price);
            exit(EXIT_SUCCESS);
        }
    }

    // Ожидание завершения всех дочерних процессов
    for (int i = 0; i < 3; i++)
    {
        wait(NULL);
    }

    // Удаление разделяемой памяти и семафора
    shmdt(stolen_goods);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    return 0;
}
