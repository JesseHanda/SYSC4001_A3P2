#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>

using namespace std;

#define NUM_QUESTIONS 5

struct SharedData
{
    int currStudent;
    int currExamIndex;
    char rubric[NUM_QUESTIONS];
    int qStatus[NUM_QUESTIONS];
    int stopFlag;
};

void loadRubric(SharedData *shm)
{
    ifstream file("rubric.txt");
    int num;
    char comma, letter;
    for (int i = 0; i < NUM_QUESTIONS; i++)
    {
        file >> num >> comma >> letter;
        if (letter < 32 || letter > 126)
        { // Check it's a valid grade letter
            letter = 'A';
        }
        shm->rubric[i] = letter;
    }
    file.close();
}

void saveRubric(SharedData *shm)
{
    ofstream file("rubric.txt");
    for (int i = 0; i < NUM_QUESTIONS; i++)
    {
        file << (i + 1) << ", " << shm->rubric[i] << endl;
    }
    file.close();
}

void loadExam(SharedData *shm)
{
    char filename[50];
    // Reach exam 50: jump  to termination exam (9999)
    if (shm->currExamIndex >= 50)
    {
        sprintf(filename, "exams/exam_9999.txt");
    }
    else
    {
        sprintf(filename, "exams/exam_%04d.txt", shm->currExamIndex + 1);
    }
    ifstream file(filename);
    if (!file.is_open())
    {
        perror("Error: exam file");
        return;
    }
    file >> shm->currStudent;
    file.close();
    for (int i = 0; i < NUM_QUESTIONS; i++)
        shm->qStatus[i] = 0;
}

void TAprocess(int id, SharedData *shm)
{
    srand(time(NULL) + id);
    while (true)
    {
        if (shm->stopFlag == 1)
            break;
        // Review rubric
        for (int i = 0; i < NUM_QUESTIONS; i++)
        {
            usleep((500 + rand() % 500) * 1000);
            if (rand() % 4 == 0)
            {
                char oldVal = shm->rubric[i];
                // printable range in ASCII table
                if (shm->rubric[i] >= 126)
                {
                    shm->rubric[i] = 'A';
                }
                else
                {
                    shm->rubric[i]++;
                }
                cout << "TA " << id << " corrected rubric Q" << i + 1
                     << " from " << oldVal << " to " << shm->rubric[i] << endl;
                saveRubric(shm);
            }
        }
        // Mark a question
        for (int i = 0; i < NUM_QUESTIONS; i++)
        {
            if (shm->qStatus[i] == 0)
            {
                shm->qStatus[i] = 1;
                sleep(1 + rand() % 2);
                shm->qStatus[i] = 2;
                cout << "TA " << id << " marked Q"
                     << (i + 1) << " for student #"
                     << shm->currStudent << endl;
                break;
            }
        }
        // Check if all questions are done
        bool done = true;
        for (int i = 0; i < NUM_QUESTIONS; i++)
            if (shm->qStatus[i] != 2)
                done = false;
        if (done)
        {
            shm->currExamIndex++;
            loadExam(shm);
            if (shm->currStudent == 9999)
            {
                shm->stopFlag = 1;
                cout << "TA " << id << " terminating" << endl;
                break;
            }
            else
            {
                cout << "Up next: "
                     << shm->currStudent << endl;
            }
        }
    }
    cout << "TA " << id << " exiting." << endl;
    exit(0);
}
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "./part2a [# of TA's]" << endl;
        return 1;
    }
    int numTAs = atoi(argv[1]);
    if (numTAs < 2)
    {
        cout << "Error: 2 TA's" << endl;
        return 1;
    }
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    SharedData *shm = (SharedData *)shmat(shmid, NULL, 0);
    shm->currExamIndex = 0;
    shm->stopFlag = 0;
    loadRubric(shm);
    loadExam(shm);
    for (int i = 0; i < numTAs; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            TAprocess(i + 1, shm);
        }
    }
    for (int i = 0; i < numTAs; i++)
        wait(NULL);

    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    cout << "Done" << endl;
    return 0;
}