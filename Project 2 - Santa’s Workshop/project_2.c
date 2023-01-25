#include "queue.c"
#include <string.h>
#include <stdbool.h>


#define QUEUE_LIMIT 1000
#define PACKAGING 0
#define PAINTING 1
#define ASSEMBLY 2
#define QA 3

int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 30; // frequency of emergency gift requests from New Zealand
//My code
time_t simulation_started;
int myID = 0;
int taskID = 0;
int n = 0; //Default n value
//End of my code
void* ElfA(void *arg); // the one that can paint
void* ElfB(void *arg); // the one that can assemble
void* Santa(void *arg); 
void* ControlThread(Task *task); // handles printing and queues (up to you)
char* print_queue(Queue* pQueue);
//My-code
struct Queue *assemly_queue;
struct Queue *quality_assurance_queue;
struct Queue *packaging_queue;
struct Queue *painting_queue;
struct Queue *delivery_queue;
struct Queue *controller_queue;
struct Queue *assembly_added;
struct Queue *painting_added;
struct Queue *qa_added;
struct Queue *assembly_done;
struct Queue *painting_done;
struct Queue *qa_done;

pthread_mutex_t assembly_mutex;
pthread_mutex_t q_a_mutex;
pthread_mutex_t packaging_mutex;
pthread_mutex_t delivery_mutex;
pthread_mutex_t painting_mutex;
pthread_mutex_t controller_mutex;
pthread_mutex_t elf_a_mutex;
pthread_mutex_t elf_b_mutex;
pthread_mutex_t santa_mutex;
pthread_mutex_t task_ID_mutex;
pthread_mutex_t assembly_added_mutex;
pthread_mutex_t painting_added_mutex;
pthread_mutex_t qa_added_mutex;
pthread_mutex_t assembly_done_mutex;
pthread_mutex_t painting_done_mutex;
pthread_mutex_t qa_done_mutex;

//End of my code

// pthread sleeper function
int pthread_sleep (int seconds)
{
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;
    if(pthread_mutex_init(&mutex,NULL))
    {
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL))
    {
        return -1;
    }
    struct timeval tp;
    //When to expire is an absolute time, so get the current time and add it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;
    
    pthread_mutex_lock(&mutex);
    int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);
    
    //Upon successful completion, a value of zero shall be returned
    return res;
}


int main(int argc,char **argv){
    // -t (int) => simulation time in seconds
    // -s (int) => change the random seed
    for(int i=1; i<argc; i++){
        if(!strcmp(argv[i], "-t")) 
        {simulationTime = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-s"))  
        {seed = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-n"))  
        {n = atoi(argv[++i]);}
    }
    
    srand(seed); // feed the seed
    
    /* Queue usage example
        Queue *myQ = ConstructQueue(1000);
        Task t;
        t.ID = myID;
        t.type = 2;
        Enqueue(myQ, t);
        Task ret = Dequeue(myQ);
        DestructQueue(myQ);
    */

    // your code goes here
    // you can simulate gift request creation in here, 
    // but make sure to launch the threads first
    FILE *fp = fopen("events.log", "wb");
    fprintf(fp,"TaskID GiftID GiftType TaskType Emergency RequestTime TaskArrival TT Responsible\n");
    fprintf(fp, "____________________________________________________________________________________________\n");
    fclose(fp);

    //We created different queues for all type of jobs.
    assemly_queue = ConstructQueue(QUEUE_LIMIT);
    quality_assurance_queue = ConstructQueue(QUEUE_LIMIT);
    packaging_queue = ConstructQueue(QUEUE_LIMIT);
    painting_queue = ConstructQueue(QUEUE_LIMIT);
    delivery_queue = ConstructQueue(QUEUE_LIMIT);
    controller_queue = ConstructQueue(QUEUE_LIMIT);
    assembly_added = ConstructQueue(QUEUE_LIMIT);
    painting_added = ConstructQueue(QUEUE_LIMIT);
    qa_added = ConstructQueue(QUEUE_LIMIT);
    assembly_done = ConstructQueue(QUEUE_LIMIT);
    painting_done = ConstructQueue(QUEUE_LIMIT);
    qa_done = ConstructQueue(QUEUE_LIMIT);
    //mutex initialization.
    pthread_mutex_init(&assembly_mutex, NULL);
    pthread_mutex_init(&q_a_mutex, NULL);
    pthread_mutex_init(&packaging_mutex, NULL);
    pthread_mutex_init(&delivery_mutex, NULL);
    pthread_mutex_init(&painting_mutex, NULL);
    pthread_mutex_init(&controller_mutex, NULL);
    pthread_mutex_init(&elf_a_mutex, NULL);
    pthread_mutex_init(&elf_b_mutex, NULL);
    pthread_mutex_init(&santa_mutex, NULL);
    pthread_mutex_init(&task_ID_mutex, NULL);
    pthread_mutex_init(&painting_added_mutex, NULL);
    pthread_mutex_init(&assembly_added_mutex, NULL);
    pthread_mutex_init(&qa_added_mutex, NULL);
    pthread_mutex_init(&painting_done_mutex, NULL);
    pthread_mutex_init(&assembly_done_mutex, NULL);
    pthread_mutex_init(&qa_done_mutex, NULL);
    //thread initialization
    pthread_t santa_thread;
    pthread_t elf_a_thread;
    pthread_t elf_b_thread;
    pthread_t controller_thread;

    simulation_started = time(NULL);

    pthread_create(&santa_thread, NULL, Santa, NULL); //The control santa thread.
    pthread_create(&elf_a_thread, NULL, ElfA, NULL); //The control elf a thread.
    pthread_create(&elf_b_thread, NULL, ElfB, NULL); //The control elf b thread.
    pthread_create(&controller_thread, NULL, ControlThread, NULL); //The thread of controller.

    while(1){

        if((time(NULL)-simulation_started) >= simulationTime){ //It stops generating new jobs when a specific real-time comes.
    		break;
    	}

        Task t;
        t.ID = myID;
        myID++;

        float random_probability = (float) rand() / (float) RAND_MAX;

        if(random_probability <= 0.4){
            //It creates a type 1 gift, probabilities will be included later
            t.type = 1;
            t.arrival_time = (int) time(NULL) - simulation_started;
            t.demands[PACKAGING] = 1;
            t.demands[PAINTING] = 0;
            t.demands[ASSEMBLY] = 0;
            t.demands[QA] = 0;
            
            //Add new task to controller's queue
            pthread_mutex_lock(&controller_mutex);
            Enqueue(controller_queue,t);
            pthread_mutex_unlock(&controller_mutex);
            pthread_sleep(1);

        }else if(random_probability > 0.4 && random_probability <= 0.6){
            //It creates a type 2 gift
            t.type = 2;
            t.arrival_time = (int) time(NULL) - simulation_started;
            t.demands[PAINTING] = 1;
            t.demands[PACKAGING] = 1;
            t.demands[ASSEMBLY] = 0;
            t.demands[QA] = 0;
            
            //Add new task to controller's queue
            pthread_mutex_lock(&controller_mutex);
            Enqueue(controller_queue,t);
            pthread_mutex_unlock(&controller_mutex);
            pthread_sleep(1);

        }else if(random_probability > 0.6 && random_probability <= 0.8){
            //It creates a type 3 gift
            t.type = 3;
            t.arrival_time = (int) time(NULL) - simulation_started;
            t.demands[ASSEMBLY] = 1;
            t.demands[PACKAGING] = 1;
            t.demands[PAINTING] = 0;
            t.demands[QA] = 0;
            
            //Add new task to controller's queue
            pthread_mutex_lock(&controller_mutex);
            Enqueue(controller_queue,t);
            pthread_mutex_unlock(&controller_mutex);
            pthread_sleep(1);

        }else if (random_probability > 0.8 && random_probability <= 0.85){
            //It creates a type 4 gift
            t.type = 4;
            t.arrival_time = (int) time(NULL) - simulation_started;
            t.demands[ASSEMBLY] = 0;
            t.demands[PACKAGING] = 1;
            t.demands[PAINTING] = 1;
            t.demands[QA] = 1;
            
            //Add new task to controller's queue
            pthread_mutex_lock(&controller_mutex);
            Enqueue(controller_queue,t);
            pthread_mutex_unlock(&controller_mutex);
            pthread_sleep(1);

        }else if (random_probability > 0.85 && random_probability <= 0.90){
             //It creates a type 5 gift
            t.type = 5;
            t.arrival_time = (int) time(NULL) - simulation_started;
            t.demands[ASSEMBLY] = 1;
            t.demands[PACKAGING] = 1;
            t.demands[PAINTING] = 0;
            t.demands[QA] = 1;
            
            //Add new task to controller's queue
            pthread_mutex_lock(&controller_mutex);
            Enqueue(controller_queue,t);
            pthread_mutex_unlock(&controller_mutex);
            pthread_sleep(1);
        }else{
            pthread_sleep(1);
        }
        if((time(NULL)-simulation_started) >= n){            
            char* print_str = (char *) malloc(1024*sizeof(char));
            char* painting_print = print_queue(painting_queue);
            char* assembling_print = print_queue(assemly_queue);
            char* packaging_print = print_queue(packaging_queue);
            char* delivery_print = print_queue(delivery_queue);
            char* qa_print = print_queue(quality_assurance_queue);
            char* current_time_str = (char *) malloc(1024*sizeof(char));
            long long current_time = (long long) (time(NULL) - simulation_started);
            sprintf(current_time_str, "%llu", current_time);
            strcpy(print_str, "At ");
            strcat(print_str, current_time_str);
            strcat(print_str, " sec painting\t: ");
            strcat(print_str, painting_print);
            strcat(print_str, " \n");
            strcat(print_str, "At ");
            strcat(print_str, current_time_str);
            strcat(print_str, " sec assembly\t: ");
            strcat(print_str, assembling_print);
            strcat(print_str, " \n");
            strcat(print_str, "At ");
            strcat(print_str, current_time_str);
            strcat(print_str, " sec packaging\t: ");
            strcat(print_str, packaging_print);
            strcat(print_str, " \n");
            strcat(print_str, "At ");
            strcat(print_str, current_time_str);
            strcat(print_str, " sec delivery\t: ");
            strcat(print_str, delivery_print);
            strcat(print_str, " \n");
            strcat(print_str, "At ");
            strcat(print_str, current_time_str);
            strcat(print_str, " sec QA\t\t: ");
            strcat(print_str, qa_print);
            strcat(print_str, " \n");
            printf("%s\n",print_str);
            free(print_str);
            free(current_time_str);
        }
    }
    return 0;
}

void* ElfA(void *arg){
    while(1){
        if((time(NULL)-simulation_started) >= simulationTime){ //It stops generating new jobs when a specific real-time comes.
    		break;
    	}

        int packageing_flag = 0;
        Task ret;
        ret.ID = -1;
        
        pthread_mutex_lock(&packaging_mutex);
        if(isEmpty(packaging_queue) == FALSE){
            ret = Dequeue(packaging_queue);
            packageing_flag = 1;
        }
        pthread_mutex_unlock(&packaging_mutex);


        if(ret.ID == -1){
            pthread_mutex_lock(&painting_mutex);
            if(isEmpty(painting_queue) == FALSE){
                ret = Dequeue(painting_queue);
            }
            pthread_mutex_unlock(&painting_mutex);
        }
        

        if(ret.ID != -1){
            pthread_mutex_lock(&task_ID_mutex);
            ret.taskID = taskID;
            taskID++;
            pthread_mutex_unlock(&task_ID_mutex);

            if(packageing_flag == 1){
                //Do the packaging
                pthread_sleep(1);
                ret.demands[PACKAGING] = 0;
                FILE *fp = fopen("events.log", "a"); 
                fprintf(fp, "%d\t\t%d\t\t%d\t\tpackaging\t\t%d\t\t%d\t\t%d\t\t\t%d\tA\n"
                ,ret.taskID,ret.ID, ret.type, ret.is_emergency, ret.request_time, ret.arrival_time,(int)(time(NULL)-simulation_started)-ret.request_time);
                fclose(fp);
            }else{
                //Do the painting
                pthread_sleep(3);
                ret.demands[PAINTING] = 0;
                FILE *fp = fopen("events.log", "a"); 
                fprintf(fp, "%d\t\t%d\t\t%d\t\tpainting\t\t%d\t\t%d\t\t%d\t\t\t%d\tA\n"
                ,ret.taskID,ret.ID, ret.type, ret.is_emergency, ret.request_time, ret.arrival_time,(int)(time(NULL)-simulation_started)-ret.request_time);
                fclose(fp);
                //Add the painting done list
                pthread_mutex_lock(&painting_done_mutex);
                Enqueue(painting_done,ret);
                pthread_mutex_unlock(&painting_done_mutex);
            }

            //Add the task controller queue back
            pthread_mutex_lock(&controller_mutex);
            Enqueue(controller_queue,ret);
            pthread_mutex_unlock(&controller_mutex);
        }
        
        

        /* pthread_mutex_lock(&delivery_mutex);
        //Add the packaged gift into delivery queue
        Enqueue(delivery_queue,ret);
        pthread_mutex_unlock(&delivery_mutex);  */
    }

}

void* ElfB(void *arg){
    while(1){
        if((time(NULL)-simulation_started) >= simulationTime){ //It stops generating new jobs when a specific real-time comes.
    		break;
    	}

        int packageing_flag = 0;
        Task ret;
        ret.ID = -1;
        
        pthread_mutex_lock(&packaging_mutex);
        if(isEmpty(packaging_queue) == FALSE){
            ret = Dequeue(packaging_queue);
            packageing_flag = 1;
        }
        pthread_mutex_unlock(&packaging_mutex);


        if(ret.ID == -1){
            pthread_mutex_lock(&assembly_mutex);
            if(isEmpty(assemly_queue) == FALSE){
                ret = Dequeue(assemly_queue);
            }
            pthread_mutex_unlock(&assembly_mutex);
        }
        

        if(ret.ID != -1){
            pthread_mutex_lock(&task_ID_mutex);
            ret.taskID = taskID;
            taskID++;
            pthread_mutex_unlock(&task_ID_mutex);
            
            if(packageing_flag == 1){
                //Do the packaging
                pthread_sleep(1);
                ret.demands[PACKAGING] = 0;
                FILE *fp = fopen("events.log", "a"); 
                fprintf(fp, "%d\t\t%d\t\t%d\t\tpackaging\t\t%d\t\t%d\t\t%d\t\t\t%d\tB\n"
                ,ret.taskID,ret.ID, ret.type, ret.is_emergency, ret.request_time, ret.arrival_time,(int)(time(NULL)-simulation_started)-ret.request_time);
                fclose(fp);
            }else{
                //Do the assemble
                pthread_sleep(2);
                ret.demands[ASSEMBLY] = 0;
                FILE *fp = fopen("events.log", "a"); 
                fprintf(fp, "%d\t\t%d\t\t%d\t\tassembling\t\t%d\t\t%d\t\t%d\t\t\t%d\tB\n"
                ,ret.taskID,ret.ID, ret.type, ret.is_emergency, ret.request_time, ret.arrival_time,(int)(time(NULL)-simulation_started)-ret.request_time);
                fclose(fp);
                //Add the assembly done list
                pthread_mutex_lock(&assembly_done_mutex);
                Enqueue(assembly_done,ret);
                pthread_mutex_unlock(&assembly_done_mutex);
            }

            //Add the task controller queue back
            pthread_mutex_lock(&controller_mutex);
            Enqueue(controller_queue,ret);
            pthread_mutex_unlock(&controller_mutex);
        }

        
        

        /* pthread_mutex_lock(&delivery_mutex);
        //Add the packaged gift into delivery queue
        Enqueue(delivery_queue,ret);
        pthread_mutex_unlock(&delivery_mutex);  */
    }
}

// manages Santa's tasks
void* Santa(void *arg){
    while(1){
        if((time(NULL)-simulation_started) >= simulationTime){ //It stops generating new jobs when a specific real-time comes.
    		break;
    	}

        Task ret;
        ret.ID = -1;
        int delivery_flag = 0;

        //Delivery
        pthread_mutex_lock(&delivery_mutex);
        if(isEmpty(delivery_queue) == FALSE){
            ret = Dequeue(delivery_queue);
            delivery_flag = 1;
        }
        pthread_mutex_unlock(&delivery_mutex);

        //Quality assurence
        if(ret.ID == -1){
            pthread_mutex_lock(&q_a_mutex);
            if(isEmpty(quality_assurance_queue) == FALSE){
                ret = Dequeue(quality_assurance_queue);
            }
            pthread_mutex_unlock(&q_a_mutex);
        }

        
        //Goes to delivery
        if(ret.ID != -1){ 
            pthread_mutex_lock(&task_ID_mutex);
            ret.taskID = taskID;
            taskID++;
            pthread_mutex_unlock(&task_ID_mutex);

            if(delivery_flag == 1){
                //Do the delivery
                pthread_sleep(1);
                FILE *fp = fopen("events.log", "a"); 
                fprintf(fp, "%d\t\t%d\t\t%d\t\tdelivery\t\t%d\t\t%d\t\t%d\t\t\t%d\tSanta\n"
                ,ret.taskID,ret.ID, ret.type, ret.is_emergency, ret.request_time, ret.arrival_time,(int)(time(NULL)-simulation_started)-ret.request_time);
                fclose(fp);
            }else{
                //Do the qa
                pthread_sleep(1);
                ret.demands[QA] = 0;
                FILE *fp = fopen("events.log", "a"); 
                fprintf(fp, "%d\t\t%d\t\t%d\t\tQA\t\t\t\t%d\t\t%d\t\t%d\t\t\t%d\tSanta\n"
                ,ret.taskID,ret.ID, ret.type, ret.is_emergency, ret.request_time, ret.arrival_time,(int)(time(NULL)-simulation_started)-ret.request_time);
                fclose(fp);

                //Add the qa done list
                pthread_mutex_lock(&qa_done_mutex);
                Enqueue(qa_done,ret);
                pthread_mutex_unlock(&qa_done_mutex);

                //Add the task controller queue back
                pthread_mutex_lock(&controller_mutex);
                Enqueue(controller_queue,ret);
                pthread_mutex_unlock(&controller_mutex);
            }
        }
        

    }
}

// the function that controls queues and output
void* ControlThread(Task *task){
    while(1){

        if((time(NULL)-simulation_started) >= simulationTime){ //It stops generating new jobs when a specific real-time comes.
            break;
        }
        
        Task ret;
        ret.ID = -1;

        pthread_mutex_lock(&controller_mutex);
        if(isEmpty(controller_queue) == FALSE){
            ret = Dequeue(controller_queue);
        }
        pthread_mutex_unlock(&controller_mutex);
        
        if(ret.ID != -1){
            //Check type 4 for simultane work
            //If the task came as completed both work make its demands zero
            if(ret.type == 4){
                if(contains(painting_done,ret.ID) && contains(qa_done,ret.ID)){
                    ret.demands[QA] = 0;
                    ret.demands[PAINTING] = 0;
                }
            }
            //Check type 5 for simultane work
            //If the task came as completed both work make its demands zero
            if(ret.type == 5){
                if(contains(assembly_done,ret.ID) && contains(qa_done,ret.ID)){
                    ret.demands[ASSEMBLY] = 0;
                    ret.demands[QA] = 0;
                }
            }

            ret.request_time = (int) time(NULL) - simulation_started;

            if(ret.demands[PACKAGING] == 1 && ret.demands[PAINTING] == 0 && ret.demands[ASSEMBLY] == 0 && ret.demands[QA] == 0){
                pthread_mutex_lock(&packaging_mutex);
                Enqueue(packaging_queue,ret);
                pthread_mutex_unlock(&packaging_mutex);
            }else if(ret.demands[PAINTING] == 1){
                //Prevent resend to painting queue for type 4 and type 5
                pthread_mutex_lock(&painting_added_mutex);
                if(!contains(painting_added,ret.ID)){
                    pthread_mutex_lock(&painting_mutex);
                    Enqueue(painting_queue,ret);
                    pthread_mutex_unlock(&painting_mutex);
                    Enqueue(painting_added,ret);
                }
                pthread_mutex_unlock(&painting_added_mutex);
            }else if(ret.demands[ASSEMBLY] == 1){
                //Prevent resend to assembly queue for type 4 and type 5
                pthread_mutex_lock(&assembly_added_mutex);
                if(!contains(assembly_added,ret.ID)){
                    pthread_mutex_lock(&assembly_mutex);
                    Enqueue(assemly_queue,ret);
                    pthread_mutex_unlock(&assembly_mutex);
                    Enqueue(assembly_added,ret);
                }
                pthread_mutex_unlock(&assembly_added_mutex);
            }else if(ret.demands[PACKAGING] == 0 && ret.demands[PAINTING] == 0 && ret.demands[ASSEMBLY] == 0 && ret.demands[QA] == 0){ 
                pthread_mutex_lock(&delivery_mutex);
                Enqueue(delivery_queue,ret);
                pthread_mutex_unlock(&delivery_mutex);
            }

            if(ret.demands[QA] == 1){
                //Prevent resend to qa queue for type 4 and type 5
                pthread_mutex_lock(&qa_added_mutex);
                if(!contains(qa_added,ret.ID)){
                    pthread_mutex_lock(&q_a_mutex);
                    Enqueue(quality_assurance_queue,ret);
                    pthread_mutex_unlock(&q_a_mutex);
                    Enqueue(qa_added,ret);
                }
                pthread_mutex_unlock(&qa_added_mutex);
            }
        }
    }

}
//This function prints only one queue.
char* print_queue(Queue* pQueue){
	int i = 0;
	struct Node_t *current_node = (struct Node_t *)malloc(sizeof(struct Node_t *));
	char* queue_print = (char *) malloc(1024*sizeof(char));
	strcpy(queue_print, " ");
	char* current_queue_print = (char *) malloc(1024*sizeof(char));		
	current_node = pQueue->head; 
	while(current_node != NULL){ 
		sprintf(current_queue_print, "%d", current_node->data.ID); 
		strcat(queue_print, current_queue_print); 
		if(i != pQueue->size -1){
			strcat(queue_print, ",");
		}
		i++;
		current_node = current_node->prev; 
	}
	strcat(queue_print, "\n");
	free(current_node);
	free(current_queue_print);
	return queue_print; 
}