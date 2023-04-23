#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_STOLEN_ITEMS 10

    typedef struct
{
    int items;
    int value;
} StolenGoods;

int main()
{
    // Создание именованного POSIX семафора
    sem_t *semaphore = sem_open("/my_semaphore", O_CREAT | O_EXCL, 0644, 1);
    if (semaphore == SEM_FAILED)
    {
        perror("Ошибка создания семафора");
        exit(EXIT_FAILURE);
    }

    // Создание разделяемой памяти для хранения похищенного имущества
    int shm_fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0644);
    if (shm_fd == -1)
    {
        perror("Ошибка создания разделяемой памяти");
        sem_unlink("/my_semaphore");
        sem_close(semaphore);
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(StolenGoods));
    StolenGoods *stolen_goods = (StolenGoods *)mmap(NULL, sizeof(StolenGoods), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (stolen_goods == MAP_FAILED)
    {
        perror("Ошибка отображения разделяемой памяти");
        sem_unlink("/my_semaphore");
        sem_close(semaphore);
        shm_unlink("/my_shm");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    // Создание дочерних процессов
    for (int i = 0; i < 3; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("Ошибка создания дочернего процесса");
            sem_unlink("/my_semaphore");
            sem_close(semaphore);
            shm_unlink("/my_shm");
            close(shm_fd);
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // Код дочернего процесса
            srand(getpid());
            int stolen_items = 0;
            int total_value = 0;

            // Взаимодействие с использованием именованных семафоров и разделяемой памяти
            while (stolen_items < MAX_STOLEN_ITEMS)
            {
                // Взаимодействие с семафором: ожидание освобождения
                sem_wait(semaphore);

                // Доступ и модификация разделяемой памяти
                stolen_goods->items++;
                stolen_goods->value += rand() % 101; // Генерация случайной стоимости от 0 до 100

                // Взаимодействие с семафором: освобождение
                sem_post(semaphore);

                printf("Процесс %d: Украдено %d предметов. Общая стоимость: %d\n", getpid(), stolen_goods->items, stolen_goods->value);
                stolen_items++;
                usleep(500000); 
            }

            // Завершение работы дочернего процесса
            munmap(stolen_goods, sizeof(StolenGoods));
            exit(EXIT_SUCCESS);
        }
    }

    // Ожидание завершения всех дочерних процессов
    for (int i = 0; i < 3; i++)
    {
        wait(NULL);
    }

    // Закрытие и освобождение ресурсов
    sem_unlink("/my_semaphore");
    sem_close(semaphore);
    munmap(stolen_goods, sizeof(StolenGoods));
    shm_unlink("/my_shm");
    close(shm_fd);

    return 0;
}