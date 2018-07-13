#include "basic.h"
#include "manage_io.h"
#include "parser_functions.h"
#include "timer_functions.h"
#include "Client.h"
#include "list_client.h"
#include "get_client.h"
#include "functions_communication.h"
#include "put_client.h"
#include "lock_functions.h"

int great_alarm_client = 0; //diventa 1 quando scatta il timer globale
struct params
 param_client;
char *client_dir;

void timeout_handler_client(int sig, siginfo_t *si, void *uc){ //signal handler del timer globale
    
(void)sig;
    (void)si;
    (void)uc;
    great_alarm_client=1;
    return;

}

void free_shm(struct sel_repeat *shm){
    
    for (int i = 0; i < 2 *(param_client.window); i++) {
        free(shm->win_buf_snd[i].payload);
        shm->win_buf_snd[i].payload=NULL;
        free(shm->win_buf_rcv[i].payload);
        shm->win_buf_rcv[i].payload=NULL;
    }
    
    free(shm->win_buf_rcv);
    free(shm->win_buf_snd);
    shm->win_buf_rcv=NULL;
    shm->win_buf_snd=NULL;
    free(shm);
    shm=NULL;

}

void initialize_shm(struct sel_repeat *shm, int sockfd, struct sockaddr_in serv_addr ){
 

    initialize_mtx(&(shm->mtx));
    initialize_cond(&(shm->list_not_empty));
    shm->fd=-1; //DEVE ESSERE FUORI DALLA FUNZIONE
    shm->byte_written=0;
    shm->byte_sent=0;
    shm->list=NULL;
    shm->pkt_fly=0;
    shm->byte_read
=0;
    shm->window_base_rcv=0;
    shm->window_base_snd=0;
    shm->win_buf_snd=0;
    shm->seq_to_send=0;
    shm->param.window=param_client.window;
    shm->param.loss_prob=param_client.loss_prob;
    
    if(param_client.timer_ms !=0 ) {
        shm->param.timer_ms = param_client.timer_ms;
        shm->adaptive = 0;
    }

    else{
        shm->param.timer_ms = TIMER_BASE_ADAPTIVE;
        shm->adaptive = 1;
        shm->dev_RTT_ms=0;
        shm->est_RTT_ms=TIMER_BASE_ADAPTIVE;
    }
    shm->address
.sockfd=sockfd;
    shm->address
.dest_addr=serv_addr;
    shm->dimension=-1;//DEVE ESSERE FATTO FUORI DALLA FUNZIONE
    shm->filename=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD)); //DEVE ESSERE FATTO FUORI DALLA FUNZIONE
    shm->address
.len=sizeof(serv_addr);
    shm->head=NULL;
    shm->tail=NULL;
    
}

long get_command(int sockfd, struct sockaddr_in serv_addr, char *filename,sem_t*mtx_file) {//svolgi la get con connessione già instaurata
    long byte_written=0;
    struct sel_repeat *shm=malloc(sizeof(struct sel_repeat));//alloca memoria condivisa thread
    if(filename==NULL){
        handle_error_with_exit("error in get_command\n");
    }
    if(shm==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    //inizializza memoria condivisa thread
    initialize_shm(shm, sockfd, serv_addr);
   
    shm->mtx_file=mtx_file;
    shm->fd=-1;
    shm->dimension=-1;
    shm->filename=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD)); 

    if(shm->filename==NULL){
        handle_error_with_exit("Error in malloc\n");
    }

    better_strcpy(shm->filename,filename);
    shm->win_buf_rcv=malloc(sizeof(struct rcv_w_buf)*(2*param_client.window));

    if(shm->win_buf_rcv==NULL){
        handle_error_with_exit("Error in malloc win_buf_rcv\n");
    }
    shm->win_buf_snd=malloc(sizeof(struct snd_w_buf)*(2*param_client.window));

    if(shm->win_buf_snd==NULL){
        handle_error_with_exit("Error in malloc win_buf_snd\n");
    }

    memset(shm->win_buf_rcv, 0, sizeof(struct rcv_w_buf) * (2 * param_client.window)); //inizializza a zero
    memset(shm->win_buf_snd, 0, sizeof(struct snd_w_buf) * (2 * param_client.window)); //inizializza a zero

    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].payload =malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_snd[i].payload==NULL){
            handle_error_with_exit("Error in malloc\n");
        }
        memset(shm->win_buf_snd[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
        shm->win_buf_rcv[i].payload=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_rcv[i].payload==NULL){
            handle_error_with_exit("Error in malloc\n");
        }
        memset(shm->win_buf_rcv[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);

        shm->win_buf_snd[i].lap = -1;
        shm->win_buf_snd[i].acked=2;
        shm->win_buf_rcv[i].lap = -1;
    }

    set_max_buff_rcv_size(shm->address.sockfd);
    get_client(shm);
    byte_written=shm->byte_written;
    
    //libera memoria
    free_shm(shm);

    return byte_written;

}

long list_command(int sockfd, struct sockaddr_in serv_addr) { //svolgi la list con connessione già instaurata
    
    long byte_read=0;
    struct sel_repeat *shm=malloc(sizeof(struct sel_repeat)); //alloca memoria condivisa thread
    if(shm==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    //inizializza memoria condivisa thread
    initialize_shm(shm, sockfd, serv_addr);
   
    shm->fd=-1;
    shm->dimension=-1;
    shm->filename=NULL;
    shm->win_buf_rcv=malloc(sizeof(struct rcv_w_buf)*(2*param_client.window));

    if(shm->win_buf_rcv==NULL){
        handle_error_with_exit("error in malloc win buf rcv\n");
    }

    shm->win_buf_snd=malloc(sizeof(struct snd_w_buf)*(2*param_client.window));

    if(shm->win_buf_snd==NULL){
        handle_error_with_exit("error in malloc win buf snd\n");
    }

    memset(shm->win_buf_rcv, 0, sizeof(struct rcv_w_buf) * (2 * param_client.window));//inizializza a zero
    memset(shm->win_buf_snd, 0, sizeof(struct snd_w_buf) * (2 * param_client.window));//inizializza a zero

    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].payload =malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));

        if(shm->win_buf_snd[i].payload==NULL){
            handle_error_with_exit("Error in malloc\n");
        }

        memset(shm->win_buf_snd[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
        shm->win_buf_rcv[i].payload=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));

        if(shm->win_buf_rcv[i].payload==NULL){
            handle_error_with_exit("Error in malloc\n");
        }

        memset(shm->win_buf_rcv[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);

        shm->win_buf_snd[i].lap = -1;
        shm->win_buf_snd[i].acked=2;
        shm->win_buf_rcv[i].lap = -1;

    }

    set_max_buff_rcv_size(shm->address
.sockfd);
    list_client(shm);
    byte_read
=shm->byte_read
;
    
    //libera memoria   
    free_shm(shm);

    return byte_read
;

}

long put_command(int sockfd, struct sockaddr_in serv_addr, char *filename,long dimension,int fd) { //svolgi la put con connessione già instaurata
    long byte_read
=0;
    char* path;
    if(filename==NULL){
        handle_error_with_exit("Error in 'put' command\n");
    }
    path=generate_full_pathname(filename,client_dir);
    if(path==NULL){
        handle_error_with_exit("Error in generate_full_pathname\n");
    }
    struct sel_repeat *shm=malloc(sizeof(struct sel_repeat)); //alloca memoria condivisa thread
    if(shm==NULL){
        handle_error_with_exit("Error in malloc\n");
    }

    //inizializza memoria condivisa thread
    initialize_shm(shm, sockfd, serv_addr);
    
    shm->fd=fd;
    shm->dimension=dimension;
    shm->filename=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD));

    if(shm->filename==NULL){
        handle_error_with_exit("Error in malloc\n");
    }

    better_strcpy(shm->filename,filename);

    if(!calc_file_MD5(path,shm->md5_sent, shm->dimension)){
        handle_error_with_exit("Error while calculating MD5\n");
    }

    free(path);
    path=NULL;

    shm->win_buf_rcv=malloc(sizeof(struct rcv_w_buf)*(2*param_client.window));

    if(shm->win_buf_rcv==NULL){
        handle_error_with_exit("Error in malloc win_buf_rcv\n");
    }
    shm->win_buf_snd=malloc(sizeof(struct snd_w_buf)*(2*param_client.window));

    if(shm->win_buf_snd==NULL){
        handle_error_with_exit("Error in malloc win_buf_snd\n");
    }

    memset(shm->win_buf_rcv, 0, sizeof(struct rcv_w_buf) * (2 * param_client.window)); //inizializza a zero
    memset(shm->win_buf_snd, 0, sizeof(struct snd_w_buf) * (2 * param_client.window)); //inizializza a zero

    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].payload =malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_snd[i].payload==NULL){
            handle_error_with_exit("Error in malloc\n");
        }
        memset(shm->win_buf_snd[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
        shm->win_buf_rcv[i].payload=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_rcv[i].payload==NULL){
            handle_error_with_exit("Error in malloc\n");
        }
        memset(shm->win_buf_rcv[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);

        shm->win_buf_snd[i].lap = -1;
        shm->win_buf_snd[i].acked = 2;
        shm->win_buf_rcv[i].lap = -1;

    }

    put_client(shm);
    byte_read
=shm->byte_read
;
    
	//libera memoria
    
    free_shm(shm);
    return byte_read
;

}

struct sockaddr_in send_syn_recv_ack(int sockfd, struct sockaddr_in main_servaddr) {
	//client manda messaggio syn
	// e processo server figlio risponde con ack cosi il client sa chi contattare per eseguire i comandi put,list e get

    char rtx = 0;
    struct sigaction sa;
    socklen_t len = sizeof(main_servaddr);
    struct temp_buf temp_buff;
    sa.sa_flags =SA_SIGINFO;
    sa.sa_sigaction = timeout_handler_client;

    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("Error in sig_empty_set\n");
    }

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        handle_error_with_exit("Error in sigaction\n");
    }

    errno=0;

    while (rtx < 10 ) { //parametro da cambiare
        send_syn(sockfd, &main_servaddr, sizeof(main_servaddr),param_client.loss_prob);  
//mando syn al processo server principale  //da cambiare loss prob
        alarm(1);

        if (recvfrom(sockfd,&temp_buff,MAXPKTSIZE, 0, (struct sockaddr *) &main_servaddr, &len) !=-1) {
	//ricevo il syn_ack del server, solo qui sovrascrivo la struct
            if (temp_buff.command == SYN_ACK) {
                alarm(0);
                printf(GREEN"Connection established\n"RESET);
                great_alarm_client = 0;
                return main_servaddr;	//ritorna l'indirizzo del processo figlio del server
            }
        }

        if(errno!=EINTR && errno!=0){
            handle_error_with_exit("Error in recvfrom send_syn\n");
        }

        rtx++;
    }

    great_alarm_client = 0;
    handle_error_with_exit("Server unreachable\n");

    return main_servaddr;

}

void client_list_job() { //inizializza socket ricevi indirizzo del processo server figlio e inizia comando list

    struct sockaddr_in serv_addr, client_addr;
    int sockfd;

    memset((void *) &serv_addr, 0, sizeof(serv_addr)); //inizializza struct per contattare il server principale
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET,IP, &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("Error in inet_pton\n");
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { //inizializza socket del client
        handle_error_with_exit("Error in socket\n");
    }

    memset((void *) &client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0);

    if (bind(sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
        handle_error_with_exit("Error in bind\n");
    }

    set_max_buff_rcv_size(sockfd);
    serv_addr = send_syn_recv_ack(sockfd, serv_addr); //ottieni l'indirizzo per contattare un child_process_server

    list_command(sockfd, serv_addr);

    close(sockfd);
    exit(EXIT_SUCCESS);

}

void client_get_job(char *filename, sem_t* mtx_file) { 

//inizializza socket ricevi indirizzo del processo server figlio e inizia comando get
    
    struct sockaddr_in serv_addr, client_addr;
    int sockfd;

    if(filename==NULL){
        handle_error_with_exit("Error in client_get_job\n");
    }

    memset((void *) &serv_addr, 0, sizeof(serv_addr)); //inizializza struct per contattare il server principale
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET,IP, &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("Error in inet_pton\n");
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { //inizializza socket del client
        handle_error_with_exit("Error in socket\n");
    }

    memset((void *) &client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0);
    if (bind(sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
        handle_error_with_exit("Error in bind\n");
    }
    set_max_buff_rcv_size(sockfd);
    serv_addr = send_syn_recv_ack(sockfd, serv_addr); //ottieni l'indirizzo per contattare un child_process_server
    get_command(sockfd, serv_addr, filename, mtx_file);
    close(sockfd);
    exit(EXIT_SUCCESS);
}

void client_put_job(char *filename, long dimension) { 

//upload e filename già verificato,inizializza socket, ricevi indirizzo del processo server figlio e inizia comando put
    struct sockaddr_in serv_addr, client_addr;
    char*path;
    int sockfd,fd,file_try_lock;
    if(filename==NULL){
        handle_error_with_exit("Error in client_put_job\n");
    }
    path = generate_full_pathname(filename, client_dir);
    if(path==NULL){
        handle_error_with_exit("Error in generate_full_pathname\n");
    }
    fd= open(path, O_RDONLY);
    if (fd == -1) {
        handle_error_with_exit("Error in open\n");
    }
    free(path);
    file_try_lock=file_try_lock_write(fd);
    if(file_try_lock==-1){
        if(errno==EWOULDBLOCK){
            errno=0;
            printf(RED"File %s is not available at the moment\n"RESET,filename);
            exit(EXIT_FAILURE);
        }
        else {
            handle_error_with_exit("Error in try_lock\n");
        }
    }
    file_unlock(fd);
    memset((void *) &serv_addr, 0, sizeof(serv_addr)); //inizializza struct per contattare il server principale
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET,IP, &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("Error in inet_pton\n");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { //inizializza socket del client
        handle_error_with_exit("Error in socket\n");
    }
    memset((void *) &client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0);
    if (bind(sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
        handle_error_with_exit("Error in bind\n");
    }

    serv_addr = send_syn_recv_ack(sockfd, serv_addr); //ottieni l'indirizzo per contattare un child_process_server
    put_command(sockfd, serv_addr, filename,dimension,fd);
    close(sockfd);
    exit(EXIT_SUCCESS);
}

void *thread_job(void *arg) { //thread che esegue waitpid dei processi del client
    (void) arg;

    block_signal(SIGALRM);
    while (1) {
        while ((waitpid(-1, NULL,WNOHANG)) > 0);
        pause();
    }
    return NULL;
}

void create_thread_waitpid() { //crea il thread che esegue waitpid
    pthread_t tid;
    pthread_create(&tid, 0, thread_job, NULL);
    return;
}


int main(int argc, char *argv[]) { //funzione principale client concorrente

    char *filename, *command, conf_upload[4], *line, localname[80],*my_list;
    //non fare mai la free su filename e command(servono ad ogni richiesta)
    int path_len, fd,mtx_file_id;
    sem_t* mtx_file;
    pid_t pid;
    key_t key;
    
    if (argc != 2) {
        handle_error_with_exit("Usage: ./client <directory>\n");
    }

    key=get_key('a');
    mtx_file_id=get_id_shared_mem_with_key(sizeof(sem_t),key); //id semaforo condiviso
    mtx_file=(sem_t*)attach_shm(mtx_file_id); //puntatore a memoria condivisa contentente semaforo
    initialize_sem(mtx_file); //inizializza semaforo
    srand(time(NULL));
    //verifica che il file parameter.txt esista
    check_if_dir_exist(argv[1]);
    client_dir=add_slash_to_dir(argv[1]);
    better_strcpy(localname,"./parameter.txt");
    fd = open(localname, O_RDONLY); //apre il file parameter per leggere i parametri di input
    if (fd == -1) {
        handle_error_with_exit("parameter.txt in ./ not found\n");
    }
    line = malloc(sizeof(char)*MAXLINE); //alloca buffer per leggere dal file parameter.txt
    if(line==NULL){
        handle_error_with_exit("Error in malloc\n");
    }
    command = line;
    memset(line, '\0', MAXLINE);
    if (readline(fd, line, MAXLINE) <= 0) {
        handle_error_with_exit("Error in read line\n");
    }
    //inizializza i parametri di esecuzione
    if(count_word_in_buf(line)!=3){
        handle_error_with_exit("'parameter.txt' must have 3 parameters <W> <loss_prob> <timer>\n");
    }
    param_client.window = parse_integer_and_move(&line);
    if (param_client.window < 1) {
        handle_error_with_exit("Window must be greater than 0\n");
    }
    skip_space(&line);
    param_client.loss_prob = parse_double_and_move(&line);
    if (param_client.loss_prob < 0 || param_client.loss_prob > 100) {
        handle_error_with_exit("Invalid loss probability\n");
    }
    skip_space(&line);
    param_client.timer_ms = parse_integer_and_move(&line);
    if (param_client.timer_ms < 0) {
        handle_error_with_exit("Timer must be positive or zero (adaptative)\n");
    }
    path_len = (int)strlen(client_dir);
    if (close(fd) == -1) {
        handle_error_with_exit("Error in close\n");
    }
    free(command);
    line=NULL;
    create_thread_waitpid();
    block_signal(SIGCHLD);

    if ((command = malloc(sizeof(char) * 8)) == NULL) { //contiene il comando digitato,8==lunghezza massima:my+" "+list\0,list\0,get\0 o put\0
        handle_error_with_exit("Error in malloc buffercommand\n");
    }
    if ((filename = malloc(sizeof(char) * (MAXFILENAME))) == NULL) { //contiene il filename del comando digitato
        handle_error_with_exit("Error in malloc filename\n");
    }
    printf("Choose one command:\n-list\n-get <filename>\n-put <filename>\n-local list\n-exit(interrupts all pending requests)\n");
    for (;;) { //ciclo infinito; ad ogni comando dell'utente associa un processo che lo svolge

        check_and_parse_command(command, filename); //inizializza command,filename e size

        if (!is_blank(filename) && (strncmp(command, "put",3) == 0)) {
            char *path = alloca(sizeof(char) * (strlen(filename) + path_len + 1));
            better_strcpy(path, client_dir);
            move_pointer(&path, path_len);
            better_strcpy(path, filename);
            path = path - path_len;
            if (!file_exist(path)) {
                printf(RED"File %s doesn't exist\n"RESET,path);
                continue;
            } else {
                printf("File %s has size %ld bytes,confirm upload? [y/n]\n",filename, get_file_size(path));
                while (1) {
                    if (fgets(conf_upload, MAXLINE, stdin) == NULL) {
                        handle_error_with_exit("Error in fgets\n");
                    }
                    if (strlen(conf_upload) != 2) {
                        printf("Type y or n\n");
                        continue;
                    } else if (strncmp(conf_upload, "y", 1) == 0) {
                        if ((pid = fork()) == -1) {
                            handle_error_with_exit("Error in fork\n");
                        }
                        if (pid == 0) {
                            client_put_job(filename,get_file_size(path)); //i figli non ritornano mai
                        }
                        break;
                    } else if (strncmp(conf_upload, "n", 1) == 0) {
                        break; //esci e riscansiona l'input
                    }
                }
            }
        } else if (!is_blank(filename) && strncmp(command,"get",3) == 0) {
            //fai richiesta al server del file
            if ((pid = fork()) == -1) {
                handle_error_with_exit("Error in fork\n");
            }
            if (pid == 0) {
                client_get_job(filename,mtx_file); //i figli non ritornano mai
            }
        } else if (strncmp(command,"list",4) == 0) { //list
            if ((pid = fork()) == -1) {
                handle_error_with_exit("Error in fork\n");
            }
            if (pid == 0) {
                client_list_job(); //i figli non ritornano mai
            }
        }
        else if(strncmp(command,"local list",10) == 0){ //comando local_list
            my_list=make_list(client_dir);
            printf(GREEN "Local list:\n%s" RESET,my_list);
            free(my_list);
        }
        else if(strncmp(command,"exit",4) == 0){
            if(kill(0,SIGKILL)!=0){
                printf("Errore in kill, segnale SIGKILL non inviato\n");
            }
        }
        else{
            handle_error_with_exit("Wrong command\n");
        }
    }
    return EXIT_SUCCESS;
}
