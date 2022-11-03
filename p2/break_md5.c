//Original + E1 + E2 + E3 + E4
//El informador actualiza la barra y las comp/seg cada segundo con sleep() 

#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#define PASS_LEN 6
#define TRUE 1
#define FALSE 0
#define NUM_BREAKERS 1
#define AVANCE 10 //Tamaño del "lote" de contraseñas repartido
#define PERIODO_INFORMADOR  1000 //En microsegundos


typedef struct {
	pthread_t id;
	int  num_md5s;
	unsigned char **md5s;
	char **str_md5s;
	int  *encontradas;
	int *finalizar;
	long *contadorPass;
	pthread_mutex_t *mutexContador;
	unsigned char *pass;
} ArgsBreaker;

typedef struct {
	ArgsBreaker *argsBreaker;
	int finalizar;
	long *contadorPass;
	pthread_mutex_t *mutexContador;
} ArgsInformador;

void pinta_barra(int porcentaje){
		int j;
		for(j=0; j<porcentaje/2; j++){
			printf("*");
		}
		for(; j<50; j++){
			printf("_");
		}
		printf("[%3d%%]",porcentaje);
}

long ipow(long base, int exp){
    long res = 1;
    for (;;)
    {
        if (exp & 1)
            res *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return res;
}

long pass_to_long(char *str){
    long res = 0;

    for(int i=0; i < PASS_LEN; i++)
        res = res * 26 + str[i]-'a';

    return res;
};

void long_to_pass(long n, unsigned char *str) {  // str should have size PASS_SIZE+1
    for(int i=PASS_LEN-1; i >= 0; i--) {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

int hex_value(char c) {
    if (c>='0' && c <='9')
        return c - '0';
    else if (c>= 'A' && c <='F')
        return c-'A'+10;
    else if (c>= 'a' && c <='f')
        return c-'a'+10;
    else return 0;
}

void hex_to_num(char *str, unsigned char *hex) {
    for(int i=0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i*2]) << 4) + hex_value(str[i*2 + 1]);
}

void *break_pass(void *args) {
	
	ArgsBreaker *largs = args;
	
    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char pass[PASS_LEN + 1];
    long bound = ipow(26, PASS_LEN); //nº de contraseñas desde aaaaaa -> zzzzzz
    //long centiBound = bound/100;	//1% de Bound
    int passIni, passFin;
    
    while(!*largs->finalizar){
		//Obtenemos el bloque de contraseñas a analizar
		pthread_mutex_lock(largs->mutexContador);
		passIni = *largs->contadorPass;
		passFin = (passIni+AVANCE) > bound ? bound : passIni+AVANCE;
		*largs->contadorPass = passFin;
		pthread_mutex_unlock(largs->mutexContador);
		for(long i=passIni; i < passFin && !*largs->finalizar; i++) {
			long_to_pass(i, pass);

			MD5(pass, PASS_LEN, res);
			
			for(int j=0; j<largs->num_md5s; j++){
				//Si ya se encontró evitamos el resto del bucle
				if (largs->encontradas[j]) continue;
				//Si la encontramos ...
				if(!memcmp(res, largs->md5s[j],MD5_DIGEST_LENGTH)) {
					// ... la mostramos aquí
					printf("\n%s: %s\n", largs->str_md5s[j], pass);
					// ... anotamos que la encontramos
					largs->encontradas[j] = TRUE;
					// ... miramos si ya no quedan más por encontrar
					int encontradas = 0;
					for(int k=0; k<largs->num_md5s; k++){
						if(largs->encontradas[k]) encontradas++;
					}
					//... si se encontraron todas avisamos para que otros finalicen
					if(encontradas==largs->num_md5s) *(largs->finalizar) = TRUE;
					break;
				}
			}   
		}
	}
    return NULL;
}

//Hilo informador que se encarga de pintar la barra de progreso
void *informador(void  *args) {
	
	ArgsInformador *largs = args;
	long MAX_PASSWORDS = ipow(26, PASS_LEN);
	long contador; //Siguiente pass (inicio de bloque) a repartir == Cuantas llevamos comprobadas	
	struct timeval t0,t1;
	long useconds; //Tiempo total desde el arranque de este hilo


	//pinta_barra(0);printf("\r");
	gettimeofday(&t0,NULL); //Instante de arranque
	while(!largs->finalizar){
		pthread_mutex_lock(largs->mutexContador);
		contador = *largs->contadorPass;
		gettimeofday(&t1,NULL);
		pthread_mutex_unlock(largs->mutexContador);
		
		useconds = (t1.tv_sec - t0.tv_sec) * 1E6 + (t1.tv_usec - t0.tv_usec);			
		pinta_barra((contador/(double)MAX_PASSWORDS)*100);
		printf("%12ld comprobaciones/segundo\r",(long)(contador/(useconds/1E6)));
		fflush(stdout); //Al no finalizar el printf en "\n" hay que forzar el vaciado del buffer de STD_OUT
		usleep(PERIODO_INFORMADOR); //El informador se activa con este período (microsegundos)
	}

	return NULL;
}


int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

	long MAX_PASSWORDS = ipow(26, PASS_LEN);

	//Lista de md5s que se quieren romper, los indicadores
	//de que no/sí se encontaron y sus inicializaciones
	int num_md5s = argc - 1;
	unsigned char **md5s = malloc(num_md5s * sizeof(unsigned char*));
	int *encontradas = malloc(num_md5s * sizeof(int));
	for(int i=0; i<num_md5s; i++){
		md5s[i] = malloc(MD5_DIGEST_LENGTH * sizeof(unsigned char));
		hex_to_num(argv[i+1], md5s[i]);
		encontradas[i] = FALSE;
	}
	
	
	//printf("------ Paso por aquí -------\n");
    ArgsBreaker *listaArgsBreakers; 
    unsigned char pass[PASS_LEN + 1];
    
    
    
	//Variable "global" para el control de finalización de los hilos breakers
	int finalizar = FALSE;
	
	//Siguiente contraseña (inicio de bloque de contraseñas) a repartir
	long contadorPass = 0;	
	pthread_mutex_t mutex_contadorPass;	
	pthread_mutex_init(&mutex_contadorPass,NULL);

	//Reserva para la lista de argumentos de los breakers,
	//Inicializamos argumentos para los breakers, los creamos y arrancamos
	listaArgsBreakers = malloc(NUM_BREAKERS*sizeof(ArgsBreaker));	
	for(int i=0; i<NUM_BREAKERS;i++){
		listaArgsBreakers[i].num_md5s = num_md5s;
		listaArgsBreakers[i].md5s = md5s;
		listaArgsBreakers[i].str_md5s = &argv[1]; //Lista de md5s a buscar
		listaArgsBreakers[i].encontradas = encontradas;
		listaArgsBreakers[i].contadorPass = &contadorPass;
		listaArgsBreakers[i].mutexContador = &mutex_contadorPass;
		listaArgsBreakers[i].finalizar = &finalizar;
		listaArgsBreakers[i].pass = pass;
		pthread_create(&listaArgsBreakers[i].id,NULL,break_pass,&listaArgsBreakers[i]);
	}	

  
	//Creación y arranque del hilo informador
	ArgsInformador argsInformador;
	pthread_t idInformador;
	argsInformador.argsBreaker = listaArgsBreakers;
	argsInformador.finalizar = FALSE;
	argsInformador.contadorPass = &contadorPass;
	argsInformador.mutexContador = &mutex_contadorPass;	
	pthread_create(&idInformador,NULL,informador, &argsInformador);
   
   //Esperamos la finalización de todos los "breakers" 
   for(int i=0; i<NUM_BREAKERS;i++){
	   pthread_join(listaArgsBreakers[i].id, NULL);
   }
    
    //Finalizamos el hilo informador y esperamos a que termine
    argsInformador.finalizar = TRUE;
    pthread_join(idInformador,NULL);
    
    
	pinta_barra((contadorPass/(double)MAX_PASSWORDS)*100);
	printf("                                            \n");

	//Esta impresión no la piden. Es para facilitar la explicación/entendimiento
	printf("Comprobaciones realizadas: %ld\n", contadorPass);
	
	//Ahora esto se hace en el momento de encontrar cada pass en el hilo breaker que lo encuentra
	//es lo que nos obliga a pasarle los md5s como cadenas (str_md5s) a los breakers
    //printf("%s: %s\n", argv[1], pass); 
    
    free(listaArgsBreakers);
	pthread_mutex_destroy(&mutex_contadorPass);

    return 0;
}
