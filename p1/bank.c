// Ejercicio 4
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20
#define TRUE 1
#define FALSE 0

struct bank {
    int num_accounts;        // number of accounts
    int *accounts;           // balance array
	pthread_mutex_t *mutex_cuentas; //Array de mutexes (uno por cuenta)
};

struct deposit_args {
	int		thread_num;       // application defined thread #
	int		delay;			  // delay between operations
	int		*iterations; 	  // número de iteraciones global
	pthread_mutex_t *mutex_iteraciones; //mutex de iteraciones    
	int     net_total;        // total amount deposited by this thread
	struct  bank *bank;       // pointer to the bank (shared with other threads)
};

struct transfer_args {
	int		thread_num;       // application defined thread #
	int		delay;			  // delay between operations
	int		*iterations;      // número de iteraciones global
	pthread_mutex_t *mutex_iteraciones; //mutex de iteraciones	      
	struct bank *bank;        // pointer to the bank (shared with other threads)
};

struct monitor_args {
	int		delay;			  // delay between operations
	int 	end;			  // indica que el hilo monitor debe finalizar
	struct bank *bank;        // pointer to the bank (shared with other threads)
};

struct deposit_thread_info {
	pthread_t    		id;         // id returned by pthread_create()
	struct deposit_args *args;      // pointer to the arguments
};

struct transfer_thread_info {
	pthread_t    		 id;        // id returned by pthread_create()
	struct transfer_args *args;     // pointer to the arguments
};

struct monitor_thread_info {
	pthread_t    		 id;        // id returned by pthread_create()
	struct monitor_args *args;     // pointer to the arguments
};

// Deposit Threads run on this function
void *deposit(void *ptr)
{
    struct deposit_args *args =  ptr;
    int amount, account, balance;

    while(TRUE) {
		pthread_mutex_lock(args->mutex_iteraciones);
		if(*(args->iterations) <= 0){ //Es local para que no se libere al acabar la funcion
			pthread_mutex_unlock(args->mutex_iteraciones);
			return NULL;
		}
		(*(args->iterations))--;
		pthread_mutex_unlock(args->mutex_iteraciones);
		
        account = rand() % args->bank->num_accounts;
        amount  = rand() % MAX_AMOUNT;

        printf("Thread %d depositing %d on account %d.\n",
            args->thread_num, amount, account);
           
		//Empieza la sección crítica
		pthread_mutex_lock(&(args->bank->mutex_cuentas[account])); //Bloqueamos el acceso a la cuenta
        balance = args->bank->accounts[account];
        if(args->delay) usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);
		pthread_mutex_unlock(&(args->bank->mutex_cuentas[account]));//desbloqueamos el acceso a la cuenta
		//Finaliza la sección crítica

        args->net_total += amount;
    }
    return NULL;
}

// Aquí van los threads de las transferencias
void *transfer(void *ptr){
	
	struct transfer_args *args =  ptr;
	
	int transfer_amount, origin_account, destination_account, origin_balance, destination_balance;

	while(TRUE) {
		
		pthread_mutex_lock(args->mutex_iteraciones);
		if(*(args->iterations) <= 0){
			pthread_mutex_unlock(args->mutex_iteraciones);
			return NULL;
		}
		
		(*(args->iterations))--;
		pthread_mutex_unlock(args->mutex_iteraciones);
				
		do{
			destination_account = rand() % args->bank->num_accounts;
			origin_account = rand() % args->bank->num_accounts;
		}while(origin_account==destination_account);
			
		//Bloqueamos las cuentas intervinientes en la transacción en orden (menor, mayor)
		//para evitar interbloqueos
		if(origin_account < destination_account){
			pthread_mutex_lock(&(args->bank->mutex_cuentas[origin_account]));
			pthread_mutex_lock(&(args->bank->mutex_cuentas[destination_account]));
		}else{		
			pthread_mutex_lock(&(args->bank->mutex_cuentas[destination_account]));
			pthread_mutex_lock(&(args->bank->mutex_cuentas[origin_account]));		
		}	
		
		//Comienza la sección crítica
		if(args->bank->accounts[origin_account] == 0)
			transfer_amount = 0;
		else
			transfer_amount  = rand() % args->bank->accounts[origin_account];
		
		printf("Thread %d transfering %d from account %d to account %d\n",
			args->thread_num, transfer_amount, origin_account, destination_account);
			
		destination_balance = args->bank->accounts[destination_account];
		origin_balance = args->bank->accounts[origin_account];
		if(args->delay) usleep(args->delay); // Force a context switch

		destination_balance += transfer_amount;
		origin_balance -= transfer_amount;
		if(args->delay) usleep(args->delay);

		args->bank->accounts[destination_account] = destination_balance;
		args->bank->accounts[origin_account] = origin_balance;
		if(args->delay) usleep(args->delay);
		
		pthread_mutex_unlock(&(args->bank->mutex_cuentas[destination_account]));
		pthread_mutex_unlock(&(args->bank->mutex_cuentas[origin_account]));
		//Fin de seccion crítica
		
	}
	return NULL;
}

//Hilo que monitoriza los saldos
void *monitor(void *ptr){
	
	struct monitor_args *args =  ptr;
	int bank_total, balance;
	
	while(!args->end){
		
		bank_total = 0;
		
		for(int i=0; i < args->bank->num_accounts; i++) {
			pthread_mutex_lock(&(args->bank->mutex_cuentas[i]));
		}
		for(int i=0; i < args->bank->num_accounts; i++) {
			balance = args->bank->accounts[i];
			bank_total += balance;
		}
		for(int i=0; i < args->bank->num_accounts; i++) {
			pthread_mutex_unlock(&(args->bank->mutex_cuentas[i]));
		}
		printf("Total: %d\n", bank_total);

		usleep(args->delay);
	}
	
	return NULL;
}

// start opt.num_threads threads running on deposit.
struct deposit_thread_info *start_deposit_threads(struct options opt, struct bank *bank)
{
    int i;
    struct deposit_thread_info *threads;

    printf("creating %d deposit_threads\n", opt.num_threads);
    threads = malloc(sizeof(struct deposit_thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }
    
    //Declaración, reserva e inicialización de iterations y su mutex
    int *iterations = malloc(sizeof(int)); //se puede declarar en main aumentando un arg en la cabecera
    *iterations = opt.iterations;
    pthread_mutex_t *mutex =  malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutex ,NULL);
	
    // Create num_thread threads running deposit()
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct deposit_args));

        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;
        threads[i].args -> iterations = iterations;
        threads[i].args -> mutex_iteraciones = mutex;

        if (0 != pthread_create(&threads[i].id, NULL, deposit, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    return threads;
}

// arrancamos opt.num_threads threads ejecutando transfer.
struct transfer_thread_info *start_transfer_threads(struct options opt, struct bank *bank)
{
	int i;
	struct transfer_thread_info *threads;

	printf("creating %d transfer_threads\n", opt.num_threads);
	threads = malloc(sizeof(struct transfer_thread_info) * opt.num_threads);

	if (threads == NULL) {
		printf("Not enough memory\n");
		exit(1);
	}
	
    //Declaración, reserva e inicialización de iterations y su mutex
    int *iterations = malloc(sizeof(int));
    *iterations = opt.iterations;
    pthread_mutex_t *mutex =  malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutex ,NULL);

	// Creamos num_thread threads running transfer()
	for (i = 0; i < opt.num_threads; i++) {
		threads[i].args = malloc(sizeof(struct transfer_args));

		threads[i].args -> thread_num = i;
		threads[i].args -> bank       = bank;
		threads[i].args -> delay      = opt.delay;
        threads[i].args -> iterations = iterations;
        threads[i].args -> mutex_iteraciones = mutex;

		if (0 != pthread_create(&threads[i].id, NULL, transfer, threads[i].args)) {
			printf("Could not create thread #%d", i);
			exit(1);
		}
	}

	return threads;
}

struct monitor_thread_info *start_monitor_thread(struct options opt, struct bank *bank){

	struct monitor_thread_info *thread;

    thread = malloc(sizeof(struct monitor_thread_info));
    printf("creating monitor_thread\n");

    if (thread == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    // Create num_thread threads running deposit()
        thread -> args = malloc(sizeof(struct monitor_args));
        thread -> args -> bank       = bank;
        thread -> args -> delay     = opt.delay;
		thread -> args -> end        = FALSE;
        if (0 != pthread_create(&thread->id, NULL, monitor, thread -> args)) {
            printf("Could not create monitor thread\n");
            exit(1);
        }
        
        return thread;
}

// Print the final balances of accounts and deposit_threads
void print_balances(struct bank *bank, struct deposit_thread_info *thrs, int num_threads){
    int total_deposits=0, bank_total=0;
    printf("\nNet deposits by thread\n");

    for(int i=0; i < num_threads; i++) {
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_deposits += thrs[i].args->net_total;
    }
    printf("Total: %d\n", total_deposits);

    printf("\nAccount balance\n");
    for(int i=0; i < bank->num_accounts; i++) {
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
    }
    printf("Total: %d\n", bank_total);
}

// wait for all deposit threads to finish
void wait_deposit_threads(struct options opt, struct bank *bank, struct deposit_thread_info *threads) {
	// Wait for the threads to finish
	for (int i = 0; i < opt.num_threads; i++)
		pthread_join(threads[i].id, NULL);
}

// wait for all transfer threads to finish
void wait_transfer_threads(struct options opt, struct bank *bank, struct transfer_thread_info *threads) {
	// Wait for the threads to finish
	for (int i = 0; i < opt.num_threads; i++)
		pthread_join(threads[i].id, NULL);
}

// Reservamos memoria, inicializamos las cuentas a 0 e inicializamos los mutexes de las cuentas
void init_accounts(struct bank *bank, int num_accounts) {
	bank->num_accounts = num_accounts;
	bank->accounts     = malloc(bank->num_accounts * sizeof(int)); 
	bank->mutex_cuentas     = malloc(bank->num_accounts * sizeof(pthread_mutex_t)); 

	for(int i=0; i < bank->num_accounts; i++){
		bank->accounts[i] = 0;
		pthread_mutex_init(&(bank->mutex_cuentas[i]),NULL); 
	}
}

// liberamos memoria de las cuentas y los mutexes de las cuentas
void free_accounts(struct bank *bank, int num_accounts) {

	for(int i=0; i < bank->num_accounts; i++){
		pthread_mutex_destroy(&(bank->mutex_cuentas[i])); 
	}
	
	free(bank->mutex_cuentas);
	free(bank->accounts); 
}

void free_threads_memory(struct options opt, struct deposit_thread_info *deposit_threads,
						struct transfer_thread_info *transfer_threads, struct monitor_thread_info *monitor_thread){
	
	pthread_mutex_destroy(deposit_threads[0].args->mutex_iteraciones); //Estan en 0 porque el hilo 0 es el unico que existe
	pthread_mutex_destroy(transfer_threads[0].args->mutex_iteraciones);
	free(deposit_threads[0].args->iterations);
	free(transfer_threads[0].args->iterations);
	free(deposit_threads[0].args->mutex_iteraciones);
	free(transfer_threads[0].args->mutex_iteraciones);
	
	for (int i = 0; i < opt.num_threads; i++){
		free(deposit_threads[i].args);
	}
	free(deposit_threads);
	
	for (int i = 0; i < opt.num_threads; i++){
		free(transfer_threads[i].args);
	}
	free(transfer_threads);
	
	free(monitor_thread->args);
	free(monitor_thread);
}

int main (int argc, char **argv){
    struct options      opt;
    struct bank         bank;
    struct deposit_thread_info *deposit_thrs;
    struct transfer_thread_info *transfer_thrs;
    struct monitor_thread_info *monitor_thr;
    
    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 100;
    opt.delay        = 10;

    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);

 	deposit_thrs = start_deposit_threads(opt, &bank);
    wait_deposit_threads(opt, &bank, deposit_thrs);
    
	transfer_thrs = start_transfer_threads(opt, &bank);
	monitor_thr = start_monitor_thread(opt, &bank);
	
    wait_transfer_threads(opt, &bank, transfer_thrs);
    
    monitor_thr->args->end = TRUE;
    pthread_join(monitor_thr->id,NULL);
    
    print_balances(&bank, deposit_thrs, opt.num_threads);
 
	free_threads_memory(opt, deposit_thrs, transfer_thrs, monitor_thr);
    free_accounts(&bank, opt.num_accounts);

    return 0;
}
