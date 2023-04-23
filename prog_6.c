#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>

#define MAX_STOLEN_ITEMS 10 // Максимальное количество похищенных товаров

typedef struct
{
    int items; // Количество похищенных товаров
    int value; // Общая стоимость похищенных товаров
} StolenGoods;

int main()
{
    // Создание неименованного POSIX семафора
    sem_t semaphore;
    if (sem_init(&semaphore, 1, 1) == -1)
    {
        perror("Ошибка создания семафора");
        exit(EXIT_FAILURE);
    }

    // Создание разделяемой памяти для хранения похищенного имущества
    int shm_fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0644);
    if (shm_fd == -1)
    {
        perror("Ошибка создания разделяемой памяти");
        sem_destroy(&semaphore);
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(StolenGoods));
    StolenGoods *stolen_goods = (StolenGoods *)mmap(NULL, sizeof(StolenGoods), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (stolen_goods == MAP_FAILED)
    {
        perror("Ошибка отображения разделяемой памяти");
        sem_destroy(&semaphore);
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
            sem_destroy(&semaphore);
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

            // Взаимодействие с использованием неименованных семафоров и разделяемой памяти
            while (stolen_items < MAX_STOLEN_ITEMS)
            {
                // Взаимодействие с семафором: ожидание освобождения
                sem_wait(&semaphore);

                // Доступ и модификация разделяемой памяти
                stolen_goods->items++;
                stolen_goods->value += rand() % 101; // Генерация случайной стоимости от 0 до 100

                // Взаимодействие с семафором: освобождение
                sem_post(&semaphore);

                printf("Дочерний процесс с PID %d похитил товар. Всего похищено: %d\n", getpid(), stolen_goods->items);

                stolen_items++;
                total_value += stolen_goods->value;

                // Задержка между похищениями
                usleep(500000); // 500 миллисекунд
            }

            printf("Дочерний процесс с PID %d завершился. Всего похищено: %d, Общая стоимость: %d\n", getpid(), stolen_goods->items, total_value);

            // Освобождение ресурсов
            munmap(stolen_goods, sizeof(StolenGoods));
            sem_destroy(&semaphore);
            close(shm_fd);
            exit(EXIT_SUCCESS);
        }
    }

    // Ожидание завершения всех дочерних процессов
    for (int i = 0; i < 3; i++)
    {
        wait(NULL);
    }

    // Вывод результатов
    printf("Программа завершила работу. Всего похищено: %d, Общая стоимость: %d\n", stolen_goods->items, stolen_goods->value);

    // Освобождение ресурсов
    munmap(stolen_goods, sizeof(StolenGoods));
    sem_destroy(&semaphore);
    shm_unlink("/my_shm");
    close(shm_fd);
    exit(EXIT_SUCCESS);
}