This project simulates a group of Teaching Assistants (TAs) marking exams at the same time using shared memory. It includes:

Part 2A: No synchronization

Part 2B: Same program but using POSIX semaphores to avoid race conditions

The goal is to show how concurrent processes interfere with each other when accessing shared data, and how proper locking fixes those issues.

Files
p2a_101264747_101276213.cpp   // Part 2A (unsynchronized)
p2b_101264747_101276213.cpp   // Part 2B (synchronized with semaphores)
rubric.txt                    // Rubric file used by all TAs
exams/                        // Contains exam_0001.txt ... exam_9999.txt


Each exam file contains only the last 4 digits of a student number.
exam_9999.txt is used as the termination signal.

How to Compile

Part 2A:

g++ -std=c++17 p2a_101264747_101276213.cpp -o p2a


Part 2B:

g++ -std=c++17 p2b_101264747_101276213.cpp -pthread -o p2b

How to Run

Run with the number of TA processes (must be ≥ 2):

./p2a 3
./p2b 3

What Each Version Does
Part 2A (No Semaphores)

TAs update the rubric and mark questions at the same time.

Race conditions occur:

rubric letters get overwritten unpredictably

multiple TAs may mark the same question

exams can be loaded more than once

Behaviour is chaotic on purpose.

Part 2B (With Semaphores)

Adds four semaphores:

rubric_mutex

exam_mutex

loader_mutex

print_mutex

Fixes all the issues from Part 2A:

rubric updates become consistent

each question is marked exactly once

only one TA loads the next exam

clean termination when 9999 is reached

Notes

My version uses 50 exam files instead of 20 (the program still behaves correctly).

The system stops as soon as the student number 9999 is loaded.
