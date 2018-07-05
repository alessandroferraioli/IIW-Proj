#include "basic.h"
#include "manage_io.h"
#include "parser_functions.h"
#include "timer_functions.h"
#include "Server.h"
#include "list_server.h"
#include "get_server.h"
#include "functions_communication.h"
#include "put_server.h"


int main_sockfd,msgid,queue_mtx_id,mtx_prefork_id,mtx_file_id,great_alarm_serv=0;//dopo le fork tutti i figli-->sono globali
// sanno quali sono gli id
struct select_param param_serv;
char*dir_server;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void timeout_handler_serv(int sig, siginfo_t *si, void *uc){//gestione del segnale alarm
    (void)sig;
    (void)si;
    (void)uc;
    great_alarm_serv=1;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void setup_mtx_prefork(struct mtx_prefork*mtx_prefork){//inizializza memoria condivisa contentente semaforo
    //impostando numero di processi liberi a 0
    if(mtx_prefork==NULL){
        handle_error_with_exit("error in setup_mtx_prefork\n");
    }
    if(sem_init(&(mtx_prefork->sem),1,1)==-1){
        handle_error_with_exit("error in sem_init\n");
    }
    mtx_prefork->free_process=0;
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void setup_timer(struct shm_sel_repeat **shm_temp){
     if(param_serv.timer_ms !=0 ) {
        (*shm_temp)->param.timer_ms = param_serv.timer_ms;
        (*shm_temp)->adaptive = 0;
    }
    else{
        (*shm_temp)->param.timer_ms = TIMER_BASE_ADAPTIVE;
        (*shm_temp)->adaptive = 1;
        (*shm_temp)->dev_RTT_ms=0;
        (*shm_temp)->est_RTT_ms=TIMER_BASE_ADAPTIVE;
    }



}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void allocate_memory_payload(struct shm_sel_repeat **shm){
    for (int i = 0; i < 2 *(param_serv.window); i++) {//alloco memoria per i payload di ogni buffer
        
        (*shm)->win_buf_snd[i].payload =malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if((*shm)->win_buf_snd[i].payload==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        
        memset((*shm)->win_buf_snd[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);//setto a zero
        (*shm)->win_buf_rcv[i].payload=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if((*shm)->win_buf_rcv[i].payload==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        
        memset((*shm)->win_buf_rcv[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
        (*shm)->win_buf_snd[i].lap = -1;
        (*shm)->win_buf_snd[i].acked=2;//li metto a 2-->sono pkt vuoti
        (*shm)->win_buf_rcv[i].lap = -1;
    }





}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void allocate_memory_shm(struct shm_sel_repeat **shm){
  (*shm)->win_buf_rcv=malloc(sizeof(struct window_rcv_buf)*(2*(param_serv.window)));
    if((*shm)->win_buf_rcv==NULL){
        handle_error_with_exit("error in malloc win buf rcv\n");
    }

    //alloco memoria buffer di invio
    (*shm)->win_buf_snd=malloc(sizeof(struct window_snd_buf)*(2*(param_serv.window)));
    if((*shm)->win_buf_snd==NULL){
        handle_error_with_exit("error in malloc win buf snd\n");
    }
    
    memset((*shm)->win_buf_rcv,0,sizeof(struct window_rcv_buf)*(2*(param_serv.window)));//inizializza a zero
    memset((*shm)->win_buf_snd,0,sizeof(struct window_snd_buf)*(2*(param_serv.window)));//inizializza a zero

    allocate_memory_payload(shm);
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void fillUp_shm(struct shm_sel_repeat **shm_temp,struct msgbuf request,sem_t *mtx_file){

    (*shm_temp)->fd=-1;
    (*shm_temp)->dimension=-1;
    (*shm_temp)->filename=NULL;
    (*shm_temp)->list=NULL;
    (*shm_temp)->byte_readed=0;
    (*shm_temp)->byte_written=0;
    (*shm_temp)->byte_sent=0;
    (*shm_temp)->addr.dest_addr=request.addr;
    (*shm_temp)->pkt_fly=0;
    (*shm_temp)->mtx_file=mtx_file;
    (*shm_temp)->window_base_rcv=0;
    (*shm_temp)->window_base_snd=0;
    (*shm_temp)->win_buf_snd=0;
    (*shm_temp)->seq_to_send=0;
    (*shm_temp)->addr.len=sizeof(request.addr);
    (*shm_temp)->param.window=param_serv.window;//primo pacchetto della finestra->primo non riscontrato

    setup_timer(shm_temp);

    (*shm_temp)->param.loss_prob=param_serv.loss_prob;
    (*shm_temp)->head=NULL;
    (*shm_temp)->tail=NULL;
    
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void reply_syn_exe_cmd(struct msgbuf request,sem_t*mtx_file){//Ho preso il msg dalla coda di richieste e soddisfo il cmd
    
    struct sockaddr_in serv_addr;
    struct temp_buffer temp_buff;//pacchetto da inviare
   
    struct shm_sel_repeat *shm=malloc(sizeof(struct shm_sel_repeat));

    if(shm==NULL){
        handle_error_with_exit("error in malloc\n");
    }

    //Inizializzo la struttura per la gestione del sel_repeat
    initialize_mtx(&(shm->mtx));
    initialize_cond(&(shm->list_not_empty));

    fillUp_shm(&shm,request,mtx_file);

    allocate_memory_shm(&shm);
    
    memset((void *)&serv_addr, 0, sizeof(serv_addr));//inizializzo socket del processo ad ogni nuova richiesta-->riduco la prob di avere problemi di socket(le assegna il SO)
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(0);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);

    if ((shm->addr.sockfd= socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket create\n");
    }
    if (bind(shm->addr.sockfd, (struct sockaddr *)&(serv_addr), sizeof(serv_addr)) < 0) {//bind con una porta scelta automataticam. dal SO
        handle_error_with_exit("error in bind\n");
    }

    //manda syn ack dopo aver ricevuto il syn(richiesta ricevuta) e aspetta il comando del client
    send_syn_ack(shm->addr.sockfd, &request.addr, sizeof(request.addr),param_serv.loss_prob );
    alarm(TIMEOUT);     //La funzione alarm() invia al processo corrente il segnale SIGALRM dopo che siano trascorsi seconds secondi.
                        //Per non farlo bloccare sulla recvfrom se non ricevuo nulla 
    if(recvfrom(shm->addr.sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr *)&(shm->addr.dest_addr),&(shm->addr.len))!=-1){  //ricevi il comando del client in finestra
                                                                                                                            //bloccati finquando non ricevi il comando dal client(almeno che non scada il TIMEOUT)
        alarm(0);//ricevuto il cmd-->metto a 0 il timeout non mi serve piu
        print_rcv_message(temp_buff);
        printf(GREEN"connection established\n"RESET);
        great_alarm_serv=0; //Lo disattivo ( attivato ad 1 nell handler del timeout)
        
        //in base al comando ricevuto il processo figlio server esegue uno dei 3 comandi
       
        if(temp_buff.command==LIST){
            exe_list(temp_buff,shm);
        }
        else if(temp_buff.command==PUT){
            set_max_buff_rcv_size(shm->addr.sockfd);//lo metto al massimo possibile(in basic.h) senza privilegi root
            exe_put(temp_buff,shm);
            if(close(shm->addr.sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
        }
        else if(temp_buff.command==GET){
            exe_get(temp_buff,shm);
            if(close(shm->addr.sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
        }
        else if(temp_buff.command==SYN_ACK || temp_buff.command==SYN){
            printf("pacchetto di connessione ricevuto e ignorato\n");
        }
        else{
            printf("invalid_command\n");
            if(close(shm->addr.sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
        }
    }
    else if(errno!=EINTR && errno!=0){
        handle_error_with_exit("error in send_syn_ack recvfrom\n");
    }

    if(great_alarm_serv==1){
        great_alarm_serv=0;
        printf(RED"Client not available\n"RESET);
        return ;
    }

    //libera la memoria della shared memory a fine lavoro
    for (int i = 0; i < 2 *(param_serv.window); i++) {
        free(shm->win_buf_snd[i].payload);
        shm->win_buf_snd[i].payload=NULL;
        free(shm->win_buf_rcv[i].payload);
        shm->win_buf_rcv[i].payload=NULL;
    }


    free(shm->win_buf_rcv);
    free(shm->win_buf_snd);
    shm->win_buf_snd=NULL;
    shm->win_buf_rcv=NULL;
    free(shm);
    shm=NULL;
    return;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void child_job() {//lavoro che svolge il processo.
    //for(;;){
        //prende la richiesta dalla coda;
        //risponde al client;
        //soddisfala richiesta;
    //}
    struct msgbuf request;//contiene indirizzo del client da servire[vedi in basic.h la struct msgbuf]
    int value;
    char done_jobs=0;//contatore richieste eseguite. Se > MAX_PROC_JOB uccido 
    struct sigaction sa_timeout;//gestione timeout
    struct mtx_prefork*mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id); //ottieni puntatore alla regione di memoria condivisa dei processi
                                                                                    //Usato per accedere alla variabile free process in modo da rigenereare quando stanno per finire
                                                                                    
    memset(&sa_timeout,0,sizeof(struct sigaction));
    unlock_signal(SIGALRM);//il processo puo accettare il segnale SIGALARM

    sem_t *mtx_file=(sem_t*)attach_shm(mtx_file_id);//puntatore al semaforo
    sem_t *mtx_queue_child=(sem_t*)attach_shm(queue_mtx_id);//ottieni puntatore al semaforo che gestitsce la coda di richieste


    if(close(main_sockfd)==-1){//chiudi il socket del padre
        handle_error_with_exit("error in close socket fd\n");
    }

    sa_timeout.sa_sigaction = timeout_handler_serv;//disposizione per segnale alarm,quando arriva l'alarm metto great_alarm =1
    if (sigemptyset(&sa_timeout.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGALRM, &sa_timeout, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }

    for(;;){
        lock_sem(&(mtx_prefork->sem));//semaforo numero processi liberi cosi da poterlo modificare
        
        if(mtx_prefork->free_process>=NUM_FREE_PROCESS){//esco in questo caso, ho creato piu processi
            unlock_sem(&(mtx_prefork->sem));
            exit(EXIT_SUCCESS);
        }

        mtx_prefork->free_process+=1;//abbiamo un processo in piu [io stesso] che puo gestire le richieste
        unlock_sem(&(mtx_prefork->sem));

        //Gestisco eventuali richieste
        lock_sem(mtx_queue_child);//prendo lock sulla coda di msg contentente le richieste
        value=(int)msgrcv(msgid,&request,sizeof(struct msgbuf)-sizeof(long),0,0);   //Dopo aver preso il lock sulla coda, leggo dalla coda (metto il msg letto in request)
                                                                                    //request é di tipo msg_buf
        unlock_sem(mtx_queue_child);//non è un problema prendere il mutex e bloccarsi in coda
        

        if(value==-1){//errore msgrcv
            lock_sem(&(mtx_prefork->sem));
            mtx_prefork->free_process-=1;//Ho preso la richiesta(con errore), non sono piu libero
            unlock_sem(&(mtx_prefork->sem));
            handle_error_with_exit("errore in msgrcv\n");
        }

        lock_sem(&(mtx_prefork->sem));//Prendo il lock sulla struttura del prefork e decremento il numero di processi liberi-->il processo figlio soddisfa la richiesta del client
        mtx_prefork->free_process-=1;
        unlock_sem(&(mtx_prefork->sem));

        reply_syn_exe_cmd(request,mtx_file);//soddisfa la richiesta
      
        done_jobs++;//incrementa numero di lavori svolti
        if(done_jobs>MAX_PROC_JOB){
            exit(EXIT_SUCCESS);//uccido processo
        }
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void make_pool_process(int num_child){//crea il pool di processi.
// Ogni processo ha il compito di gestire le richieste
    int pid;
    if(num_child<0){
        handle_error_with_exit("num_child must be greater than 0\n");
    }
    for(int i=0;i<num_child;i++) {
        if ((pid = fork()) == -1) {
            handle_error_with_exit("error in fork\n");
        }
        if (pid == 0) {//sono il figlio
            child_job();//i figli non ritorna mai
        }
    }
    return;//il padre ritorna dopo aver creato i processi
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void *pool_handler_job(void*arg){//thread che gestisce il pool dei processi del server
    struct mtx_prefork*mtx_prefork=arg;//la uso per accedere al campo free process che verra modificato dai processi figli per gestire la rigenerazione
    int left_process;
    block_signal(SIGALRM);
    /*Ciclicamente controllo : 
        -numero processi liberi,se sono sotto una soglia 
            -Prendo il lock sul semaforo
            -Creo il numero di processi
    */

    for(;;){
        //Stuttura ad accesso esclusivo controlalta dal mtx_prefork
        lock_sem(&(mtx_prefork->sem));
        if(mtx_prefork->free_process<NUM_FREE_PROCESS){
            left_process=NUM_FREE_PROCESS-mtx_prefork->free_process;
            unlock_sem(&(mtx_prefork->sem));
            make_pool_process(left_process);//crea i processi rimanenti per arrivare a NUM_FREE_PROCESS
        }
        else{
            unlock_sem(&(mtx_prefork->sem));
        }
        while((waitpid(-1,NULL,WNOHANG))>0);//waitpid non blccante 
    }
    return NULL;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

void make_pool_handler(struct mtx_prefork*mtx_prefork){
//crea il gestore(thread) della riserva di processi
    if(mtx_prefork==NULL){
        handle_error_with_exit("error in create thread_pool_handler\n");
    }
    pthread_t tid;
    if(pthread_create(&tid,NULL,pool_handler_job,mtx_prefork)!=0){
        handle_error_with_exit("error in create_pool_handler\n");
    }
    block_signal(SIGALRM);
    return;
}



/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(int argc,char*argv[]) {//funzione principale processo server
    
    int fd,readed;
    socklen_t len;
    
    char commandBuffer[MAXCOMMANDLINE+1],*line,*command,localname[80];
    
    struct sockaddr_in addr,cliaddr;
    struct msgbuf msgbuf;//struttura del messaggio della coda

    struct mtx_prefork*mtx_prefork;//mutex tra processi e thread pool handler
    sem_t*mtx_queue;//semaforo tra i processi che provano ad accedere alla coda di messaggi
    sem_t*mtx_file;

    if(argc!=2){
        handle_error_with_exit("usage <directory>\n");
    }

    srand((unsigned int)time(NULL));
    check_if_dir_exist(argv[1]);//verifica che directory passata come parametro esiste
    dir_server=add_slash_to_dir(argv[1]);

    //verifica che il file parameter.txt
    better_strcpy(localname,"./parameter.txt");
    fd=open(localname,O_RDONLY);
    if(fd==-1){
        handle_error_with_exit("parameter.txt in ./ not found\n");
    }

    line=malloc(sizeof(char)*MAXLINE);
    if(line==NULL){
        handle_error_with_exit("error in malloc\n");
    }

    command=line;
    memset(line,'\0',MAXLINE);
    readed=readline(fd,line,MAXLINE);//leggi parametri di esecuzione dal file parameter
    
    if(count_word_in_buf(line)!=3){
        handle_error_with_exit("parameter.txt must contains 3 parameters <window> space <loss_prob> space <timer>\n");
    }
    if(readed<=0){//ho letto ma e' tornato errore
        handle_error_with_exit("error in read line\n");
    }

    //Il primo valore che leggo e' la grandezza della dim della finestra
    param_serv.window=parse_integer_and_move(&line);//inizializza parametri di esecuzione

    if(param_serv.window<1){
        handle_error_with_exit("window must be greater than 0\n");
    }
    skip_space(&line);

    //prendo la prob di perdita 
    param_serv.loss_prob=parse_double_and_move(&line);
    if(param_serv.loss_prob<0 || param_serv.loss_prob>100){
        handle_error_with_exit("invalid loss prob\n");
    }
    skip_space(&line);

    //Prendo il time out(sia adattivo che fissato)
    param_serv.timer_ms=parse_integer_and_move(&line);
    if(param_serv.timer_ms<0){
        handle_error_with_exit("timer must be positive or 0\n");
    }
    if(close(fd)==-1){
        handle_error_with_exit("error in close file\n");
    }

    free(command);//liberazione memoria della linea retta dal file
    line=NULL;

    //inizializza memorie condivise contenenti semafori e inizializza coda
    mtx_prefork_id=get_id_shared_mem(sizeof(struct mtx_prefork));//la struct mtx_prefork viene usata per tenere traccia dei processi liberi[ogni processo andrá ad aggiornare il valore di free process]
    queue_mtx_id=get_id_shared_mem(sizeof(sem_t));//memoria condivisa contenente il semaforo per la coda 
    msgid=get_id_msg_queue();//crea coda di messaggi id globale
    mtx_file_id=get_id_shared_mem(sizeof(sem_t));//mtx per file

    //Prendo puntatori shm 
    mtx_file=(sem_t*)attach_shm(mtx_file_id);//Ritorna puntatore alla shared memory indicata dall'id passato alla funzione
    mtx_queue=(sem_t*)attach_shm(queue_mtx_id);//mutex per accedere alla coda
    mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);//mutex tra processi e pool handler
    

    initialize_sem(mtx_queue);//inizializza memoria condivisa
    initialize_sem(mtx_file);
    setup_mtx_prefork(mtx_prefork);//inizializza memoria condivisa[uguale a sopra solo che metto gia a 0 la variabile free_process]

    //inizializza socket processo principale
    memset((void *)&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(SERVER_PORT);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if ((main_sockfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
        handle_error_with_exit("error in socket create\n");
    }
    if (bind(main_sockfd,(struct sockaddr*)&addr,sizeof(addr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }

    make_pool_process(NUM_FREE_PROCESS);//crea il pool di NUM_FREE_PROCESS(definito in basic.h) processi

    //Da qui continua solo il processo padre-->i figli non ritornano dalla child_job()
    make_pool_handler(mtx_prefork);//crea il thread che gestisce la riserva di processi
    printf(GREEN"Bootstrap completed\n"RESET);

    //Ciclicamente vedo se arrivano msg dal client e li metto in coda
    while(1) {
        len=sizeof(cliaddr);
        if ((recvfrom(main_sockfd, commandBuffer, MAXCOMMANDLINE, 0, (struct sockaddr *) &cliaddr, &len)) < 0) {
            handle_error_with_exit("error in recvcommand");//memorizza  l'indirizzo del client e lo scrive in coda
        }
        msgbuf.addr=cliaddr;//inizializza la struct con addr
        msgbuf.mtype=1;
        if(msgsnd(msgid,&msgbuf,sizeof(struct msgbuf)-sizeof(long),0)==-1){//inserisce nella coda l'indirizzo del client
            handle_error_with_exit("error in msgsnd\n");
        }
    }
    return EXIT_SUCCESS;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/