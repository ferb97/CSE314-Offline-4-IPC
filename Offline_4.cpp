#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <chrono>
#include <random>
#include <semaphore.h>
using namespace std;

#define NOT_READY 0
#define READY 1
#define PRINTING 2
#define PRINT_FINISHED 3

#define UNOCCUPIED 0
#define OCCUPIED 1

pthread_mutex_t mutex, reader_writer_mutex, print_mutex;
typedef int semaphore;
vector<int> state;
vector<sem_t> s;
sem_t binding_semaphore;
sem_t writing_semaphore;
vector<int> printing_done_group_members;

int printer[5];
int N, M, w, x, y;
int reader_count = 0;
int total_submissions = 0;

random_device rd;
mt19937 generator(rd());
double lambda = 7.0;
poisson_distribution<int> distribution(lambda);

auto start_time = chrono::high_resolution_clock::now();

void prepareToPrint(){
    int prepare_time = distribution(generator);
    sleep(prepare_time);
}

void test(int std_num){

    if(state[std_num] == READY && printer[std_num % 4 + 1] == UNOCCUPIED){

       state[std_num] = PRINTING;
       printer[std_num % 4 + 1] = OCCUPIED;
       //cout<< "Student " << std_num << " starts printing at time " << time << endl;
       sem_post(&s[std_num]); 
    }
}

void occupy_printer(int std_num){
    
    auto end_time = chrono::high_resolution_clock::now();
    auto elapsed_time = chrono::duration_cast<chrono::seconds>(end_time - start_time);

    pthread_mutex_lock(&print_mutex);
    cout<<"Student "<< std_num << " has arrived at the print station at time " << elapsed_time.count()<<endl;
    pthread_mutex_unlock(&print_mutex);

    pthread_mutex_lock(&mutex);
    state[std_num] = READY;
    test(std_num);
    pthread_mutex_unlock (&mutex);

    sem_wait(&s[std_num]);
}

void use_printer(){
    sleep(w);
}

void perform_binding(){
    sleep(x);
}

void perform_writing(){
    sleep(y);
}

void reading(){
    sleep(y);
}

void leave_printer(int std_num){
    
    auto end_time = chrono::high_resolution_clock::now();
    auto elapsed_time = chrono::duration_cast<chrono::seconds>(end_time - start_time);

    pthread_mutex_lock(&print_mutex);
    cout<<"Student "<< std_num <<" has finished printing at time "<< elapsed_time.count() <<endl;
    pthread_mutex_unlock(&print_mutex);

    pthread_mutex_lock(&mutex);

    state[std_num] = PRINT_FINISHED;
    printer[std_num % 4 + 1] = UNOCCUPIED;
    printing_done_group_members[(std_num - 1) / M + 1]++;
    int group_number = (std_num - 1) / M + 1;

    //sending message to own group members
    for(int i = M * (group_number - 1) + 1; i <= M * group_number; i++){
        if(std_num % 4 + 1 == i % 4 + 1){
            test(i);
        }
    }

    //sending message to other group members
    for(int i = 1; i <= M * (group_number - 1); i++){
        if(std_num % 4 + 1 == i % 4 + 1){
            test(i);
        }
    }

    for(int i = M * group_number + 1; i <= N; i++){
        if(std_num % 4 + 1 == i % 4 + 1){
            test(i);
        }
    }

    //Group has finished printing. Wakeup the leader
    if(printing_done_group_members[group_number] == M){
    //    cout << "Group " << group_number << " has finished printing at time " << elapsed_time.count() << endl;
       sem_post(&s[M * group_number]); 
    }

    pthread_mutex_unlock(&mutex);
}

void *startWork (void *arg) {
    int std_num = *(int *) arg;
    
    prepareToPrint();
    
    occupy_printer(std_num);
    
    use_printer();

    leave_printer(std_num);

    free(arg);

    //if the current student is not the group leader, exit thread
    if(std_num % M != 0){
       pthread_exit(0); 
    }

    //if all other members have not finished printing, the group leader will sleep until it is done
    int group_number = std_num / M;
    if(printing_done_group_members[group_number] != M){
        sem_wait(&s[std_num]);
    }

    auto end_time4 = chrono::high_resolution_clock::now();
    auto elapsed_time4 = chrono::duration_cast<chrono::seconds>(end_time4 - start_time);

    pthread_mutex_lock(&print_mutex);
    cout << "Group " << group_number << " has finished printing at time " << elapsed_time4.count() << endl;
    pthread_mutex_unlock(&print_mutex);

    //starting the binding process
    sem_wait(&binding_semaphore);

    auto end_time = chrono::high_resolution_clock::now();
    auto elapsed_time = chrono::duration_cast<chrono::seconds>(end_time - start_time);

    pthread_mutex_lock(&print_mutex);
    cout << "Group " << group_number << " has started binding at time "<< elapsed_time.count() << endl;
    pthread_mutex_unlock(&print_mutex);

    perform_binding();

    auto end_time1 = chrono::high_resolution_clock::now();
    auto elapsed_time1 = chrono::duration_cast<chrono::seconds>(end_time1 - start_time);

    pthread_mutex_lock(&print_mutex);
    cout << "Group " << group_number << " has finished binding at time "<< elapsed_time1.count() << endl;
    pthread_mutex_unlock(&print_mutex);

    sem_post(&binding_semaphore);
    //binding process finished

    //writing process started
    sem_wait(&writing_semaphore);

    perform_writing();

    auto end_time3 = chrono::high_resolution_clock::now();
    auto elapsed_time3 = chrono::duration_cast<chrono::seconds>(end_time3 - start_time);

    pthread_mutex_lock(&print_mutex);
    cout << "Group " << group_number << " has submitted the report at time "<< elapsed_time3.count() << endl;
    pthread_mutex_unlock(&print_mutex);

    total_submissions++;

    sem_post(&writing_semaphore);

}

void *readEntryBook (void *arg){

    int staff_num = *(int *) arg;
    int reader_start_time = distribution(generator);
    sleep(reader_start_time);

    //staff starts reading
    while(true){

        //mutex lock to update the reader count
        pthread_mutex_lock(&reader_writer_mutex);
        reader_count++;
        if(reader_count == 1){
            sem_wait(&writing_semaphore); 
        }
        pthread_mutex_unlock (&reader_writer_mutex);

        auto end_time = chrono::high_resolution_clock::now();
        auto elapsed_time = chrono::duration_cast<chrono::seconds>(end_time - start_time);

        pthread_mutex_lock(&print_mutex);
        cout << "Staff " << staff_num << " has started reading the entry book at time " << elapsed_time.count() << ". No. of submission = " << total_submissions << endl;
        pthread_mutex_unlock(&print_mutex);

        reading();

        //if all submissions have been done, end thread
        if(total_submissions == N / M){
           break; 
        }  

        //mutex lock to decrement the reader count
        pthread_mutex_lock(&reader_writer_mutex);
        reader_count--;
        if(reader_count == 0){
            sem_post(&writing_semaphore);
        }
        pthread_mutex_unlock (&reader_writer_mutex);

        //wait after a read
        int wait_time = distribution(generator);
        sleep(wait_time);
    }

}

int main (int argc, char *argv[]) {

    freopen("output.txt", "w", stdout);
    freopen("input.txt", "r", stdin);

    cin >> N >> M >> w >> x >> y;

    pthread_t threads[N], staff1_thread, staff2_thread;

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&reader_writer_mutex, NULL);
    pthread_mutex_init(&print_mutex, NULL);

    s.resize(N + 1);
    state.resize(N + 1);
    printing_done_group_members.resize(N / M + 1);

    for (int i = 0; i < N; i++) {
        
        sem_init(&s[i + 1], 0 , 0);
        state[i + 1] = NOT_READY;
    }

    for(int i = 0; i < N / M; i++){
        printing_done_group_members[i + 1] = 0;
    }

    sem_init(&binding_semaphore, 0 , 2);
    sem_init(&writing_semaphore, 0 , 1);

    int* staff1 = new int;
    *staff1 = 1;
    int* staff2 = new int;
    *staff2 = 2;

    for(int i = 0; i < N; i++){
       int* std_num = new int;
       *std_num = i + 1; 
       if(pthread_create(&threads[i], NULL, startWork, std_num) != 0){
         perror("Failed to create thread\n");
       } 
    }

    if(pthread_create(&staff1_thread, NULL, readEntryBook, staff1) != 0){
        perror("Failed to create staff 1 thread\n");
    }

    if(pthread_create(&staff2_thread, NULL, readEntryBook, staff2) != 0){
        perror("Failed to create staff 2 thread\n");
    }

    for(int i = 0; i < N; i++){
       if(pthread_join(threads[i], NULL) != 0){
         perror("Failed to join thread\n");
       } 
    }

    if(pthread_join(staff1_thread, NULL) != 0){
        perror("Failed to join thread\n");
    } 
    if(pthread_join(staff2_thread, NULL) != 0){
        perror("Failed to join thread\n");
    } 

    for (int i = 0; i < N; ++i) {
        sem_destroy(&s[i + 1]);
    }

    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&reader_writer_mutex);
    pthread_mutex_destroy(&print_mutex);

    sem_destroy(&binding_semaphore);
    sem_destroy(&writing_semaphore);

    cout << "All threads finished" << endl;

    fclose(stdin);
    fclose(stdout);
    return  0;
}