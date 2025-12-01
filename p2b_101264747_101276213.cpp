#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <semaphore.h>

using namespace std;

#define NUM_QUESTIONS 5

struct SharedData {
    int currentStudent;
    int currentExamIndex;
    char rubric[NUM_QUESTIONS];
    int questionStatus[NUM_QUESTIONS];
    int stopFlag;

    // Semaphores
    sem_t rubric_mutex;   // rubric protection
    sem_t exam_mutex;     // exam protection
    sem_t loader_mutex;   // TA loader protection
    sem_t print_mutex;    // Print protection
};

void loadRubric(SharedData* shm) {
    ifstream file("rubric.txt");
    int num;
    char comma, letter;

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        file >> num >> comma >> letter;

        // Check it's a valid grade letter
        if (letter < 32 || letter > 126) {
            letter = 'A';
        }

        shm->rubric[i] = letter;
    }

    file.close();
}

void saveRubric(SharedData* shm) {
    ofstream file("rubric.txt");
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        file << (i+1) << ", " << shm->rubric[i] << endl;
    }
    file.close();
}

void loadExam(SharedData* shm) {
    char filename[50];

    // Reach exam 50: jump  to termination exam (9999)
    if (shm->currentExamIndex >= 50) {
        sprintf(filename, "exams/exam_9999.txt");
    } else {
        sprintf(filename, "exams/exam_%04d.txt", shm->currentExamIndex + 1);
    }

    ifstream file(filename);
    if (!file.is_open()) {
        perror("Error: exam file");
        shm->currentStudent = 9999;
        shm->stopFlag = 1;
        return;
    }

    file >> shm->currentStudent;
    file.close();

    for (int i = 0; i < NUM_QUESTIONS; i++)
        shm->questionStatus[i] = 0;
}

void TAprocess(int id, SharedData* shm) {
    srand(time(NULL) + id);

    while (true) {

        // Global stop flag check
        if (shm->stopFlag == 1) {
            sem_wait(&shm->print_mutex);
            cout << "TA " << id << " exit" << endl;
            sem_post(&shm->print_mutex);
            exit(0);
        }


        for (int i = 0; i < NUM_QUESTIONS; i++) {
            usleep((500 + rand() % 500) * 1000);

            if (rand() % 4 == 0) { 
                sem_wait(&shm->rubric_mutex);

                char oldVal = shm->rubric[i];

                // printable range in ASCII table
                if (shm->rubric[i] >= 126) {  
                    shm->rubric[i] = 'A';
                } else {
                    shm->rubric[i]++;
                }

                sem_wait(&shm->print_mutex);
                cout << "TA " << id << " corrected Q" << i+1
                     << " from " << oldVal << " to " << shm->rubric[i] << endl;
                sem_post(&shm->print_mutex);

                saveRubric(shm);
                sem_post(&shm->rubric_mutex);
            }
        }

        //  Marking
        int questionToMark = -1;
        int studentCopy = -1;

        // Pick question
        sem_wait(&shm->exam_mutex);
        if (shm->stopFlag == 1) {
            sem_post(&shm->exam_mutex);
            continue;
        }

        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shm->questionStatus[i] == 0) {
                shm->questionStatus[i] = 1;
                questionToMark = i;
                studentCopy = shm->currentStudent;
                break;
            }
        }
        sem_post(&shm->exam_mutex);

        // Mark
        if (questionToMark != -1) {
            sleep(1 + rand() % 2);

            sem_wait(&shm->exam_mutex);
            shm->questionStatus[questionToMark] = 2; // done
            sem_post(&shm->exam_mutex);

            sem_wait(&shm->print_mutex);
            cout << "TA " << id << " marked Q"
                 << (questionToMark + 1) << " for student #"
                 << studentCopy << endl;
            sem_post(&shm->print_mutex);
        }

        // Check if all questions are done
        bool done = true;

        sem_wait(&shm->exam_mutex);
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shm->questionStatus[i] != 2) {
                done = false;
                break;
            }
        }
        sem_post(&shm->exam_mutex);

        if (done) {
            // ensure only one TA loads the next exam
            sem_wait(&shm->loader_mutex);

            // check after lock
            bool doneAgain = true;

            sem_wait(&shm->exam_mutex);
            for (int i = 0; i < NUM_QUESTIONS; i++) {
                if (shm->questionStatus[i] != 2) {
                    doneAgain = false;
                    break;
                }
            }
            int currentStudent = shm->currentStudent;
            sem_post(&shm->exam_mutex);

            if (doneAgain && shm->stopFlag == 0) {
                shm->currentExamIndex++;
                loadExam(shm);

                if (shm->currentStudent == 9999) {
                    shm->stopFlag = 1;
                    sem_wait(&shm->print_mutex);
                    cout << "TA " << id << " Terminating" << endl;
                    sem_post(&shm->print_mutex);
                } else {
                    sem_wait(&shm->print_mutex);
                    cout << "Up next: "
                         << shm->currentStudent << endl;
                    sem_post(&shm->print_mutex);
                }
            }

            sem_post(&shm->loader_mutex);
        }

    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "./part2b [# of TA's]" << endl;
        return 1;
    }

    int numTAs = atoi(argv[1]);
    if (numTAs < 2) {
        cout << "Error: 2 TA's" << endl;
        return 1;
    }

    // Shared mem.
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        return 1;
    }

    SharedData* shm = (SharedData*) shmat(shmid, NULL, 0);
    if (shm == (void*) -1) {
        perror("shmat");
        return 1;
    }

    // Shared data
    shm->currentExamIndex = 0;
    shm->stopFlag = 0;

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        shm->questionStatus[i] = 0;
    }

    loadRubric(shm);
    loadExam(shm);

    // Semaphores
    sem_init(&shm->rubric_mutex, 1, 1);
    sem_init(&shm->exam_mutex,   1, 1);
    sem_init(&shm->loader_mutex, 1, 1);
    sem_init(&shm->print_mutex,  1, 1);

    // Fork
    for (int i = 0; i < numTAs; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            TAprocess(i + 1, shm);
        }
    }

    // Wait
    for (int i = 0; i < numTAs; i++) {
        wait(NULL);
    }

    // Cleanup
    sem_destroy(&shm->rubric_mutex);
    sem_destroy(&shm->exam_mutex);
    sem_destroy(&shm->loader_mutex);
    sem_destroy(&shm->print_mutex);
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);

    cout << "Done" << endl;
    return 0;
}
