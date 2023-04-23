#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>

#define MAX_STOLEN_ITEMS 10 // Максимальное количество похищенного имущества

typedef struct {
    int items; // Количество похищенного имущества
    int value; // Рыночная стоимость похищенного имущества
} StolenGoods;

int main() {
    // Удаление существующего именованного POSIX семафора, если он существует
    sem_unlink("/my_semaphore");

    // Создание именованного POSIX семафора
    sem_t *semaphore = sem_open("/my_semaphore", O_CREAT | O_EXCL, 0644, 1);

    // Проверка на корректность семафора
    if (semaphore == SEM_FAILED) {
        perror("Ошибка создания семафора");
        exit(EXIT_FAILURE);
    }

    // Создание разделяемой памяти для хранения похищенного имущества
    int shm_fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0644);
    if (shm_fd == -1) {
        perror("Ошибка создания разделяемой памяти");
        sem_unlink("/my_semaphore");
        sem_close(semaphore);
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(StolenGoods));
    StolenGoods *stolen_goods = (StolenGoods *)mmap(NULL, sizeof(StolenGoods), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (stolen_goods == MAP_FAILED) {
        perror("Ошибка отображения разделяемой памяти");
        sem_unlink("/my_semaphore");
        sem_close(semaphore);
        shm_unlink("/my_shm");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    // Создание дочерних процессов
    for (int i = 0; i < 3; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Ошибка создания дочернего процесса");
            sem_unlink("/my_semaphore");
            sem_close(semaphore);
            shm_unlink("/my_shm");
            close(shm_fd);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Код дочернего процесса
            srand(getpid());
            int stolen_items = 0;
            int total_value = 0;

            // Взаимодействие с использованием именованных семафоров и разделяемой памяти
            while (stolen_items < MAX_STOLEN_ITEMS) {
                // Взаимодействие с семафором: ожидание освобождения
                sem_wait(semaphore);

                // Доступ и модификация разделяемой памяти
                stolen_goods->items++;
                stolen_goods->value += rand() % 101; // Генерация случайной стоимости от 0 до 100
                            // Взаимодействие с семафором: освобождение
            sem_post(semaphore);
            
            printf("Дочерний процесс с PID %d похитил товар. Всего похищено: %d\n", getpid(), stolen_goods->items);
            
            stolen_items++;
            total_value += stolen_goods->value;
            
            // (задержка тут необязательна, но для красоты вывода данных написал, чтобы можно было лучше разглядеть параллельность процессов)
            // Задержка между похищениями
            usleep(500000); // 500 миллисекунд
        }
        
        printf("Дочерний процесс с PID %d завершил работу. Всего похищено: %d, Общая стоимость: %d\n", getpid(), stolen_goods->items, total_value);
        
        // Отсоединение от разделяемой памяти
        munmap(stolen_goods, sizeof(StolenGoods));
        close(shm_fd);
        
        exit(EXIT_SUCCESS);
    }
}

// Ожидание завершения работы всех дочерних процессов
for (int i = 0; i < 3; i++) {
    wait(NULL);
}

// Уничтожение именованного семафора и разделяемой памяти
sem_unlink("/my_semaphore");
sem_close(semaphore);
shm_unlink("/my_shm");
close(shm_fd);

return 0;

}