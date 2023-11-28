#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdint.h>



#define DIMENSIUNE_BUFFER 1024

typedef struct {
    uint16_t tip;
    uint32_t dimensiune;
    uint16_t rezervat1;
    uint16_t rezervat2;
    uint32_t offset;
    int32_t latime;
    int32_t inaltime;
    uint16_t planuri;
    uint16_t biti_pe_pixel;
    uint32_t comprimare;
    uint32_t dimensiune_imagine;
    int32_t x_pixeli_per_metru;
    int32_t y_pixeli_per_metru;
    uint32_t culori_utilizate;
    uint32_t culori_importante;
} AntetImagineBMP;

int calculeazaOffsetDatePixeli(AntetImagineBMP *antet) {
    return antet->offset;
}

int calculeazaDimensiuneDatePixeli(AntetImagineBMP *antet) {
    int dimensiuneDate = (antet->latime * antet->inaltime) * (antet->biti_pe_pixel / 8);
    return dimensiuneDate;
}

void proceseazaImagine(const char *numeImagine, int offsetDatePixeli, int dimensiuneDatePixeli) {
    int fisierImagine = open(numeImagine, O_RDWR);
    if (fisierImagine == -1) {
        perror("Eroare la deschiderea fișierului BMP");
        exit(1);
    }
    
    lseek(fisierImagine, offsetDatePixeli, SEEK_SET);
    unsigned char pixel[3];
    while (read(fisierImagine, pixel, 3) == 3) {
        unsigned char pixelGri = (unsigned char)(0.299 * pixel[2] + 0.587 * pixel[1] + 0.114 * pixel[0]);
        pixel[0] = pixel[1] = pixel[2] = pixelGri;
        
        lseek(fisierImagine, -3, SEEK_CUR);
        write(fisierImagine, pixel, 3);
    }

    close(fisierImagine);
}

void numaraPropozitiiCorecte(int canal[2], char c) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Eroare la forking");
        return;
    } else if (pid == 0) {
        close(canal[0]);

        dup2(canal[1], STDOUT_FILENO);
        close(canal[1]);

        execl("/bin/bash", "bash", "script", &c, NULL);
        perror("Eroare la exec");
        exit(EXIT_FAILURE);
    } else {
        close(canal[1]);

        int count;
        read(canal[0], &count, sizeof(count));

        close(canal[0]);
        waitpid(pid, NULL, 0);
        printf("Numărul de propoziții corecte: %d\n", count);
    }
}

void scrieStatisticiFisierObisnuit(const char *numeFisier, const char *directorIesire) {
    char outputFile[DIMENSIUNE_BUFFER];
    snprintf(outputFile, sizeof(outputFile), "%s/%s_statistics.txt", directorIesire, numeFisier);

    int statsFile = open(outputFile, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (statsFile == -1) {
        perror("eroare scriere/citire");
        return;
    }

    struct stat elementInfo;
    char elementPath[DIMENSIUNE_BUFFER];
    snprintf(elementPath, sizeof(elementPath), "%s/%s", directorIesire, numeFisier);

    if (lstat(elementPath, &elementInfo) == -1) {
        perror("Eroare la lstat");
        close(statsFile);
        return;
    }

    char statsInfo[DIMENSIUNE_BUFFER];
    sprintf(statsInfo, "nume fisier: %s\nSize: %ld bytes\nLast Modified: %s"
                        "\nPermissions: %o\n----\n",
            numeFisier, elementInfo.st_size, ctime(&elementInfo.st_mtime), elementInfo.st_mode);

    if (write(statsFile, statsInfo, strlen(statsInfo)) == -1) {
        perror("eroare la scriere");
    }

    close(statsFile);
}

void trimiteContinutFisier(const char *numeFisier, const char *directorIesire) {
    char inputFile[DIMENSIUNE_BUFFER];
    snprintf(inputFile, sizeof(inputFile), "%s/%s", directorIesire, numeFisier);

    int inputFileDescriptor = open(inputFile, O_RDONLY);
    if (inputFileDescriptor == -1) {
        perror("Eroare la deschiderea fișierului de intrare");
        return;
    }

    off_t fileSize = lseek(inputFileDescriptor, 0, SEEK_END);
    lseek(inputFileDescriptor, 0, SEEK_SET);

    char fileContent[DIMENSIUNE_BUFFER];
    if (read(inputFileDescriptor, fileContent, fileSize) == -1) {
        perror("Eroare la citirea continutului fisierului");
        close(inputFileDescriptor);
        return;
    }

    close(inputFileDescriptor);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Eroare la crearea canalului anonim");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Eroare la forking");
        return;
    } else if (pid == 0) {
        close(pipefd[1]);

        char buffer[DIMENSIUNE_BUFFER];
        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            write(STDOUT_FILENO, buffer, bytesRead);
        }

        close(pipefd[0]);
        exit(0);
    } else {
        close(pipefd[0]);

        if (write(pipefd[1], fileContent, fileSize) == -1) {
            perror("Eroare la scrierea in canal");
            close(pipefd[1]);
            return;
        }

        close(pipefd[1]);
    }
}

void proceseazaElement(const char *numeElement, const char *directorIesire, char c) {
    char elementPath[DIMENSIUNE_BUFFER];
    snprintf(elementPath, sizeof(elementPath), "%s/%s", directorIesire, numeElement);

    struct stat elementInfo;
    if (lstat(elementPath, &elementInfo) == -1) {
        perror("Eroare");
        return;
    }

    if (S_ISREG(elementInfo.st_mode)) {
        scrieStatisticiFisierObisnuit(numeElement, directorIesire);
        trimiteContinutFisier(numeElement, directorIesire);
	int status;
        if (strstr(numeElement, ".bmp") != NULL) {
            int offsetDatePixeli = calculeazaOffsetDatePixeli((AntetImagineBMP *)&elementInfo);
            int dimensiuneDatePixeli = calculeazaDimensiuneDatePixeli((AntetImagineBMP *)&elementInfo);

            pid_t pid = fork();
            if (pid < 0) {
                perror("eroare la forking");
                return;
            } else if (pid == 0) {
                proceseazaImagine(numeElement, offsetDatePixeli, dimensiuneDatePixeli);
                exit(0);
            } else {
                int status;
                waitpid(pid, &status, 0);
                printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", pid, WEXITSTATUS(status));
            }
        } else {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("Eroare la crearea canalului anonim");
                return;
            }

            pid_t pid = fork();
            if (pid < 0) {
                perror("Eroare la forking");
                return;
            } else if (pid == 0) {
                numaraPropozitiiCorecte(pipefd, c);
                exit(0);
            } else {
                close(pipefd[1]);

                int count;
                read(pipefd[0], &count, sizeof(count));

                close(pipefd[0]);
                waitpid(pid, NULL, 0);
                printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", pid, WEXITSTATUS(status));
                printf("Au fost identificate in total %d propozitii corecte care contin caracterul %c\n", count, c);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "folosește: %s <director_intrare> <director_ieșire> <c>\n", argv[0]);
        return 1;
    }

    char c = argv[3][0]; 

    DIR *dir = opendir(argv[1]);
    if (dir == NULL) {
        perror("Eroare la deschiderea directorului de intrare");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            proceseazaElement(entry->d_name, argv[2], c);
        }
    }

    closedir(dir);

    return 0;
}
