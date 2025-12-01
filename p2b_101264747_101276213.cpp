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
    int currStudent;
    int currExamIndex;
    char rubric[NUM_QUESTIONS];
    int qStatus[NUM_QUESTIONS];
    int stopFlag;

    // Semaphores
    sem_t rubric_mut;   // rubric protection
    sem_t exam_mut;     // exam protection
    sem_t loader_mut;   // TA loader protection
    sem_t print_mut;    // Print protection
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
    if (shm->currExamIndex >= 50) {
        sprintf(filename, "exams/exam_9999.txt");
    } else {
        sprintf(filename, "exams/exam_%04d.txt", shm->currExamIndex + 1);
    }
    ifstream file(filename);
    if (!file.is_open()) {
        perror("Error: exam file");
        shm->currStudent = 9999;
        shm->stopFlag = 1;
        return;
    }
    file >> shm->currStudent;
    file.close();
    for (int i = 0; i < NUM_QUESTIONS; i++)
        shm->qStatus[i] = 0;
}
void TAprocess(int id, SharedData* shm) {
    srand(time(NULL) + id);
    while (true) {
        // Global stop flag check
        if (shm->stopFlag == 1) {
            sem_wait(&shm->print_mut);
            cout << "TA " << id << " exit" << endl;
            sem_post(&shm->print_mut);
            exit(0);
        }
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            usleep((500 + rand() % 500) * 1000);
            if (rand() % 4 == 0) { 
                sem_wait(&shm->rubric_mut);
                char oldVal = shm->rubric[i];
                // printable range in ASCII table
                if (shm->rubric[i] >= 126) {  
                    shm->rubric[i] = 'A';
                } else {
                    shm->rubric[i]++;
                }
                sem_wait(&shm->print_mut);
                cout << "TA " << id << " corrected Q" << i+1
                     << " from " << oldVal << " to " << shm->rubric[i] << endl;
                sem_post(&shm->print_mut);
                saveRubric(shm);
                sem_post(&shm->rubric_mut);
            }
        }
        //  Marking
        int qsToMark = -1;
        int copy = -1;
        // Pick question
        sem_wait(&shm->exam_mut);
        if (shm->stopFlag == 1) {
            sem_post(&shm->exam_mut);
            continue;
        }
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shm->qStatus[i] == 0) {
                shm->qStatus[i] = 1;
                qsToMark = i;
                copy = shm->currStudent;
                break;
            }
        }
        sem_post(&shm->exam_mut);
        // Mark
        if (qsToMark != -1) {
            sleep(1 + rand() % 2);
            sem_wait(&shm->exam_mut);
            shm->qStatus[qsToMark] = 2; // done
            sem_post(&shm->exam_mut);
            sem_wait(&shm->print_mut);
            cout << "TA " << id << " marked Q"
                 << (qsToMark + 1) << " for student #"
                 << copy << endl;
            sem_post(&shm->print_mut);
        }

        // Check if all questions are done
        bool done = true;
        sem_wait(&shm->exam_mut);
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shm->qStatus[i] != 2) {
                done = false;
                break;
            }
        }
        sem_post(&shm->exam_mut);
        if (done) {
            // ensure only one TA loads the next exam
            sem_wait(&shm->loader_mut);
            // check after lock
            bool doneAgain = true;
            sem_wait(&shm->exam_mut);
            for (int i = 0; i < NUM_QUESTIONS; i++) {
                if (shm->qStatus[i] != 2) {
                    doneAgain = false;
                    break;
                }
            }
            int currStudent = shm->currStudent;
            sem_post(&shm->exam_mut);
            if (doneAgain && shm->stopFlag == 0) {
                shm->currExamIndex++;
                loadExam(shm);
                if (shm->currStudent == 9999) {
                    shm->stopFlag = 1;
                    sem_wait(&shm->print_mut);
                    cout << "TA " << id << " Terminating" << endl;
                    sem_post(&shm->print_mut);
                } else {
                    sem_wait(&shm->print_mut);
                    cout << "Up next: "
                         << shm->currStudent << endl;
                    sem_post(&shm->print_mut);
}
            }
            sem_post(&shm->loader_mut);
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
    shm->currExamIndex = 0;
    shm->stopFlag = 0;
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        shm->qStatus[i] = 0;
    }
    loadRubric(shm);
    loadExam(shm);
    // Semaphores
    sem_init(&shm->rubric_mut, 1, 1);
    sem_init(&shm->exam_mut,   1, 1);
    sem_init(&shm->loader_mut, 1, 1);
    sem_init(&shm->print_mut,  1, 1);
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
    sem_destroy(&shm->rubric_mut);
    sem_destroy(&shm->exam_mut);
    sem_destroy(&shm->loader_mut);
    sem_destroy(&shm->print_mut);
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    cout << "Done" << endl;
    return 0;
}
