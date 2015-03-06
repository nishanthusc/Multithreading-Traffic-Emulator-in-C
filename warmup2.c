#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include "my402list.h"
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include<sys/time.h>
#include<sys/stat.h>
#include<errno.h>


#define timedif(x,y) ( ((double)x.tv_sec*1000  + (double)x.tv_usec/1000 ) - ((double)y.tv_sec*1000 +  (double)y.tv_usec/1000 ) )

pthread_mutex_t lock;
pthread_t packet_arr, token_gen,server1,server2,controlC;
pthread_cond_t q2_is_not_empty;

int isDeterministicMode = 1;
char fname[1024];
FILE *fp;
struct stat buf;
long num_of_tokens=0;
long tokens_generated=0;
int tokens_dropped=0;
sigset_t set;
int q=0;
double ser;
int packet_count=0;
typedef struct{

struct timeval pkt_arrival_time;
struct timeval pkt_enque_q1;
struct timeval pkt_deque_q1;
struct timeval pkt_enque_q2;
struct timeval pkt_deque_q2;
struct timeval pkt_enter_S;
struct timeval pkt_exit_S;

double pkt_arr_rate;
double pkt_service_rate;
unsigned long int tokens;
int pkt_num;
int isdropped;
int isremoved;

}Packet_Elem;

/*default values*/
double token_rate = 1.5;
double mu = 0.35;
double lambda = 1.0;
unsigned long token_bucket_depth = 10;
unsigned long total_packets = 20;
int tokens = 3;
/*end of default values*/

/*for statistics*/
int dropPackets_count=0;
double total_inter_arrival_time = 0.0;
double time_in_Q1;
double total_service_time = 0.0;
int serviced_packets=0;
double total_time_in_q1=0.0;
double total_time_in_q2=0.0;
double service_time_arr[2] = {0.0, 0.0};
double total_time_in_sys=0.0;
double time_in_sys;
double sum_of_square_time_in_sys=0.0;
double arg1,arg2,arg3,var,std_dev;
/*end of statistics*/

int packet_limit =0;
Packet_Elem *packet;
My402List q1, q2;

struct timeval start = (struct timeval){0};
struct timeval end;
struct timeval pkt_arr_ms;

void *packetArr(){	
	
	int i,tokensReq;
	My402ListElem* Elem;
	Packet_Elem* pkt_data;
	struct timeval pkt_arr, prev_pkt_arr,pkt_Q1_entr,pkt_Q1_dep,pkt_Q2_entr;
	struct timeval after_read = (struct timeval){0};
	prev_pkt_arr = start;
	char *pch;
	char line[1024];
	for(i=0; i<total_packets;i++){


		if(!isDeterministicMode){
				memset(line,0,strlen(line));
		                if (  fgets(line,sizeof(line),fp) == NULL )    
		                        break;
		                packet = (Packet_Elem *)malloc(sizeof(Packet_Elem));
		                pch = strtok (line," \t");
		               // isnumber(pch);
		                packet->pkt_arr_rate = atoi(pch);
		                pch = strtok (NULL, " \t");
		              //  isnumber(pch);
		                packet->tokens = atoi(pch);
		                pch = strtok (NULL, " \t");
		                //isnumber(pch);
		                packet->pkt_service_rate = atoi(pch); 
		                packet->pkt_num = i+1;




		}
		else{
		
			packet = (Packet_Elem*)malloc(sizeof(Packet_Elem));
			packet->tokens = 3;
			packet->pkt_arr_rate = (double)1000/lambda;
			packet->pkt_service_rate = (double)1000/mu;
			packet->pkt_num = i+1;
		}
		
		gettimeofday(&after_read,NULL);
		if ( (packet->pkt_arr_rate*1000 - ( (after_read.tv_sec*1000+ (double)after_read.tv_usec/1000) - (prev_pkt_arr.tv_sec*1000 + (double)prev_pkt_arr.tv_usec/1000) ) *1000  ) > 0)
                        usleep(packet->pkt_arr_rate*1000 - ( (after_read.tv_sec*1000+ (double)after_read.tv_usec/1000) - (prev_pkt_arr.tv_sec*1000 + (double)prev_pkt_arr.tv_usec/1000) ) *1000 );   
                 else
                        usleep(packet->pkt_arr_rate*1000);
		//usleep(packet->pkt_arr_rate*1000000);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
		pthread_mutex_lock(&lock);
		
		packet_count++;
		gettimeofday(&pkt_arr,NULL);
		packet->pkt_arrival_time = pkt_arr;
		if(packet->tokens>token_bucket_depth){
			//printf("Inside dropped packets\n");
			dropPackets_count++;
			fprintf(stdout,"%012.3fms: p%d arrives, needs %ld tokens, inter-arival time = %.3fms, dropped\n",timedif(pkt_arr,start),packet->pkt_num,packet->tokens,timedif(pkt_arr,prev_pkt_arr));
			total_inter_arrival_time+= timedif(pkt_arr,prev_pkt_arr);
			prev_pkt_arr = pkt_arr;
			free(packet);
		}
		else{
			fprintf(stdout,"%012.3fms: p%d arrives, needs %ld tokens, inter-arrival time = %07.3fms\n", timedif(pkt_arr,start), packet->pkt_num,packet->tokens,timedif(pkt_arr,prev_pkt_arr) );

			//Capturing total inter arrival time of packets
			total_inter_arrival_time+= timedif(pkt_arr,prev_pkt_arr);
			prev_pkt_arr = pkt_arr;
			gettimeofday(&pkt_Q1_entr,NULL);
                        packet->pkt_enque_q1 =pkt_Q1_entr;
			if(My402ListAppend(&q1,packet) ==0){
				fprintf(stderr,"Unable to Append to List");
				exit(1);
			}
			
			
			fprintf(stdout,"%012.3fms: p%d enters q1\n",timedif(pkt_Q1_entr,start),packet->pkt_num);
			
			//printf("Added packet p%d to Q1\n",packet->pkt_num);
			if(!My402ListEmpty(&q1)){

				Elem = My402ListFirst(&q1);
				pkt_data = Elem->obj;
				tokensReq = pkt_data->tokens;
				if(tokensReq<=num_of_tokens){
					num_of_tokens-=tokensReq;
					My402ListUnlink(&q1,Elem);

					gettimeofday(&pkt_Q1_dep,NULL);
					pkt_data->pkt_deque_q1 = pkt_Q1_dep;
					time_in_Q1+= timedif(pkt_Q1_dep,pkt_data->pkt_enque_q1);
					total_time_in_q1+= timedif(pkt_Q1_dep,pkt_data->pkt_enque_q1);
					fprintf(stdout,"%012.3fms: p%d leaves q1 , time in Q1 = %05.3fms , token bucket now has %ld tokens\n",timedif(pkt_Q1_dep,start),pkt_data->pkt_num,timedif(pkt_Q1_dep,pkt_data->pkt_enque_q1),num_of_tokens);

					gettimeofday(&pkt_Q2_entr,NULL);
					pkt_data->pkt_enque_q2 = pkt_Q2_entr;
					My402ListAppend(&q2,pkt_data);
					
					fprintf(stdout,"%012.3fms: p%d enters q2\n",timedif(pkt_Q2_entr,start),pkt_data->pkt_num);
										
					//printf("Added p%d to Q2\n",packet->pkt_num);
					if(My402ListLength(&q2)==1){

						pthread_cond_signal(&q2_is_not_empty);
					}
				
				}
			

			}				

		} //bucket and packet token comparison

		pthread_mutex_unlock(&lock);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);

	} //End of For Loop
	packet_limit = 1;
	//pthread_cond_broadcast(&q2_is_not_empty);
	//printf("Exiting packet gen thread\n");
	pthread_exit(0);
} //End of Function



void* tokenGen(){

	My402ListElem* Elem2;
	Packet_Elem *pkt;
	struct timeval tkn_arr,pkt_Q1_dep,pkt_Q2_arr;
	struct timeval after_tkn_read = (struct timeval){0};
	double dif_cal;
	double tokn_arrival_rate = (double)1000/token_rate;
	struct timeval tkn_artm = start;
	while(1){

		gettimeofday(&after_tkn_read,NULL);		   
               dif_cal = (tokn_arrival_rate*1000 - ((after_tkn_read.tv_sec*1000+ (double)after_tkn_read.tv_usec/1000) - (tkn_artm.tv_sec*1000 + (double)tkn_artm.tv_usec/1000))*1000);
                if (dif_cal >0)
                        usleep(dif_cal);  
                else
                        usleep(tokn_arrival_rate*1000);		
		//usleep(tokn_arrival_rate);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
		pthread_mutex_lock(&lock);
		tokens_generated++;
		gettimeofday(&tkn_arr,NULL);
		if(num_of_tokens<token_bucket_depth){
			num_of_tokens++;
			fprintf(stdout,"%012.3fms: token t%ld arrives, token bucket now has %ld tokens\n",timedif(tkn_arr,start),tokens_generated,num_of_tokens);
			//printf("Token %ld added to Token Bucket, tokens in token bucket are %ld\n",tokens_generated, num_of_tokens);
			if(!My402ListEmpty(&q1)){
				Elem2 = My402ListFirst(&q1);
				pkt = Elem2->obj;

				if(pkt->tokens<=num_of_tokens){

					num_of_tokens-=pkt->tokens;
					My402ListUnlink(&q1,Elem2);
					gettimeofday(&pkt_Q1_dep,NULL);
					pkt->pkt_deque_q1 = pkt_Q1_dep;
					total_time_in_q1+= timedif(pkt_Q1_dep,pkt->pkt_enque_q1);
					fprintf(stdout,"%012.3fms: p%d leaves q1 , time in Q1 = %05.3fms , token bucket now has %ld tokens\n",timedif(pkt_Q1_dep,start),pkt->pkt_num,timedif(pkt_Q1_dep,pkt->pkt_enque_q1),num_of_tokens);
					
					gettimeofday(&pkt_Q2_arr,NULL);
					pkt->pkt_enque_q2 = pkt_Q2_arr;
					My402ListAppend(&q2,pkt);
					
					fprintf(stdout,"%012.3fms: p%d enters q2\n",timedif(pkt_Q2_arr,start),pkt->pkt_num);
					//printf("Packet %d added to q2, tokens remaining %ld\n",pkt->pkt_num, num_of_tokens);
					if(My402ListLength(&q2)==1){
						pthread_cond_signal(&q2_is_not_empty);
					}
				}
				

			}


		}
		else{
			
			tokens_dropped++;
			fprintf(stdout,"%012.3fms: token t%ld arrives, dropped\n",timedif(tkn_arr,start),tokens_generated);
			//printf("Token Dropped\n");
		}
		pthread_mutex_unlock(&lock);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
		
		if(packet_limit==1 && My402ListEmpty(&q1)){
			//printf("Exiting Token Gen Thread\n");
			pthread_cond_broadcast(&q2_is_not_empty);
			pthread_exit(0);
		}


	}
}


void* service(void* serverNumber){

	My402ListElem *elem;
	//double ser = (double)1000000/mu;
	
	Packet_Elem *pkt_ser;
	struct timeval pkt_q2_dep,pkt_s_arr,pkt_s_dep,instr_strt;
	while(1){
		
		pthread_mutex_lock(&lock);
		while(My402ListEmpty(&q2)){
			//printf("%d, %d\n",My402ListEmpty(&q1),My402ListEmpty(&q2));
			if((packet_limit==1 && My402ListEmpty(&q1) && My402ListEmpty(&q2)) || (q==1)){
				//printf("Server %d exiting\n", (int)serverNumber);
				pthread_mutex_unlock(&lock);
				pthread_cond_broadcast(&q2_is_not_empty);
				pthread_exit(0);
			}
			pthread_cond_wait(&q2_is_not_empty,&lock);
		}
		

		
		elem = My402ListFirst(&q2);
		pkt_ser =(Packet_Elem *)elem->obj;	
		
		//printf("Packet p%d leaves Q2, serviced by %d having tokens %d\n",pkt_ser->pkt_num,(int)serverNumber,pkt_ser->tokens);
		My402ListUnlink(&q2,elem);
		gettimeofday(&pkt_q2_dep,NULL);
		pkt_ser->pkt_deque_q2 = pkt_q2_dep; 

		gettimeofday(&instr_strt,NULL);
		fprintf(stdout,"%012.3fms: p%d leaves q2, time in q2 = %.3fms\n",timedif(pkt_q2_dep,start),pkt_ser->pkt_num,timedif(pkt_q2_dep,pkt_ser->pkt_enque_q2));		
		pthread_mutex_unlock(&lock);
		total_time_in_q2+=timedif(pkt_ser->pkt_deque_q2,pkt_ser->pkt_enque_q2);

		gettimeofday(&pkt_s_arr,NULL);
		fprintf(stdout,"%012.3fms: p%d begins service at S%d, requesting %.3fms of service\n",timedif(pkt_s_arr,start),pkt_ser->pkt_num,(int)serverNumber,pkt_ser->pkt_service_rate);
		
		ser = (double)1000/pkt_ser->pkt_service_rate;
		usleep(ser);

		gettimeofday(&pkt_s_dep,NULL);
		fprintf(stdout,"%012.3fms: p%d departs S%d,service time =%.3f ms , time in system = %.3f ms\n",timedif(pkt_s_dep,start),pkt_ser->pkt_num,(int)serverNumber,timedif(pkt_s_dep,pkt_s_arr),timedif(pkt_s_dep,pkt_ser->pkt_arrival_time));
		service_time_arr[(int)serverNumber-1]+=timedif(pkt_s_dep,pkt_s_arr);
		serviced_packets++;
		total_service_time+=timedif(pkt_s_dep,pkt_s_arr);
		//time_in_sys = timedif(pkt_s_dep,pkt_s_arr);
		total_time_in_sys+= timedif(pkt_s_dep,pkt_ser->pkt_arrival_time);
		if(q==1){
			pthread_exit(0);
		}
		time_in_sys = timedif(pkt_s_dep,pkt_ser->pkt_arrival_time);
		sum_of_square_time_in_sys+= pow(time_in_sys,2.0);
		//printf("Packet p%d is serviced\n",pkt_ser->pkt_num);
	}

}

void* handler(){

	
	int sig;
	while(1){
		sigwait(&set,&sig);
		pthread_cancel(packet_arr);
		pthread_cancel(token_gen);
		q=1;
		pthread_cond_broadcast(&q2_is_not_empty);

	}


}

int main(int argc, char* argv[]){

struct timeval exit1,exit2;
int k;

if(argc>1){
		if(argc%2==1){
			
			for (k=1;k<=argc-1;k=k+2){
				double dval=(double)0;
				long lval = 0;
				
				if(strcmp(argv[k],"-lambda")==0){
					
					if (sscanf(argv[k+1], "%lf", &dval) != 1) {
						fprintf(stderr,"Malformed command\n");//Usage ./warmup2 -<parameter> <paramter val>\n");
						fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
						//pthread_exit(0);
						exit(1);
						//return 0;
					}
					else {

						lambda = dval;
						if(lambda<=0.1){
							lambda = 0.1;
						}
						//printf("lambda : %f\n",lambda);
					}

				}

				else if(strcmp(argv[k],"-mu")==0){

					if (sscanf(argv[k+1], "%lf", &dval) != 1) {
						fprintf(stderr,"Malformed command/n");//\nUsage ./warmup2 -<parameter> <paramter val>\n");
						fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
						exit(0);
					}
					else {

						mu = dval;
						if(mu<=0.1){
							mu = 0.1;
						}
						//printf("mu : %f\n",mu);
					}
				}


				else if(strcmp(argv[k],"-r")==0){

					if (sscanf(argv[k+1], "%lf", &dval) != 1) {
						fprintf(stderr,"Malformed command\n");//Usage ./warmup2 -<parameter> <paramter val>\n");
						fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
						exit(0);
					}
					else {

						token_rate = dval;
						if(token_rate<=0.1){
							token_rate = 0.1;
						}
						
						//printf("r : %f\n",token_rate);
					}

				}

				else if(strcmp(argv[k],"-B")==0){

					if (sscanf(argv[k+1], "%ld", &lval) != 1) {
						fprintf(stderr,"Malformed command\n");//Usage ./warmup2 -<parameter> <paramter val>\n");
						fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
						exit(0);
					}
					else {

						token_bucket_depth = lval;
						//printf("B : %ld\n",token_bucket_depth);
					}
				}

				else if(strcmp(argv[k],"-P")==0){

					if (sscanf(argv[k+1], "%ld", &lval) != 1) {
						fprintf(stderr,"Malformed command\n");//Usage ./warmup2 -<parameter> <paramter val>\n");
						fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
						exit(0);
					}
					else {

						tokens = lval;
						//printf("B : %d\n",tokens);
					}
				}

				else if(strcmp(argv[k],"-n")==0){

					if (sscanf(argv[k+1], "%ld", &lval) != 1) {
						fprintf(stderr,"Malformed command\n");//Usage ./warmup2 -<parameter> <paramter val>\n");
						fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
						exit(0);
					}
					else {

						total_packets = lval;
						//printf("B : %d\n",total_packets);
					}
				}

				else if(strcmp(argv[k],"-t")==0){

					if (sscanf(argv[k+1], "%s", fname) != 1) {
						fprintf(stderr,"Malformed command\n");//Usage ./warmup2 -<parameter> <paramter val>\n");
						fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
						exit(0);
					}
					else {
						isDeterministicMode = 0;
						strcpy(fname,argv[k+1]);
						//printf("file path : %s\n",fname);
						fp = fopen(fname,"r");
						//Handling the cases which dealing with permissions of the tfile
						if(fp==NULL){

							if(ENOENT == errno){
								fprintf(stderr,"input file \" %s \" does not exist\n",fname);
							}
							if(EACCES == errno){
								fprintf(stderr,"input %s cannot be opened -- access denied\n",fname);
							}

						    return 0;
						}
						stat(fname,&buf);
						if(S_ISDIR(buf.st_mode)){
							fprintf(stderr,"input %s is a directory\n",argv[2]);
							return 0;
						}
						
					}
				}
				else{
					fprintf(stderr,"Malformed command\n");//Usage ./warmup2 -<parameter> <paramter val>\n");
					fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
					return 0;
				}


			} //End of for
			
	   	}
	       else{

		   fprintf(stderr,"Malformed command\n");//Usage ./warmup2 -<parameter> <paramter val>\n");
		   fprintf(stderr,"Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
		   return 0;
	       }
	}

pthread_mutex_init(&lock,NULL);
pthread_cond_init(&q2_is_not_empty,NULL);

memset(&q1, 0, sizeof(My402List));
memset(&q2, 0, sizeof(My402List));
My402ListInit(&q1);
My402ListInit(&q2);


sigemptyset(&set);
sigaddset(&set,SIGINT);
sigprocmask(SIG_BLOCK, &set, 0);
pthread_create(&controlC, 0, handler, 0);

if (!isDeterministicMode)
        {
                
                fp = fopen(fname,"r");
                if(fp == NULL)
                {
                        perror("Error: ");
                        exit(1);
                }
                char linew[1024]; 
                memset(linew,0,strlen(linew));
                        if (  fgets(linew,sizeof(linew),fp) == NULL )    
                        {
                                fprintf(stderr,"Error reading the lines in file");
                                exit(1);
                        }
		if (sscanf(linew, "%ld", &total_packets)!=1){
			fprintf(stderr,"input file is not in the right format\n");
			return 0;

		}
		else{
                	total_packets = atoi(linew);
		}

                fprintf(stdout, "Emulation Parameters: \n number to arrive = %ld\nr = %f\nB = %lu\ntsfile = %s\n",total_packets,token_rate,token_bucket_depth,fname);
        }
        
        else
        {
                 fprintf(stdout, "Emulation Parameters: \nlambda = %f\nmu = %f\nr = %f\nB = %ld\nP =%d\nnumber to arrive = %ld\n",lambda, mu, token_rate,token_bucket_depth,tokens,total_packets);       
        }

	if ( token_bucket_depth >2147483647  || tokens >2147483647  || total_packets > 2147483647 ) 
        {
                fprintf(stderr," Value exceeding the limit \n");
                exit(1);
        }



gettimeofday(&start, NULL);
fprintf(stdout,"%012.3fms: emulation begins \n",timedif(start,start));


if((pthread_create(&packet_arr,0,packetArr,0))<0 || (pthread_create(&token_gen,0,tokenGen,0)<0) || (pthread_create(&server1,0,service,(void*)1)<0) || (pthread_create(&server2,0,service,(void*)2)<0)){

	fprintf(stderr,"Error in creating threads");
	exit(1);
}

pthread_join(packet_arr,NULL);
pthread_join(token_gen,NULL);
pthread_join(server1,NULL);
pthread_join(server2,NULL);

while(My402ListLength(&q1) != 0) {
	My402ListElem *first = My402ListFirst(&q1);
	Packet_Elem *d = (Packet_Elem *)first->obj;
	gettimeofday(&exit1,NULL);
	printf("%012.3fms: p%d removed from Q1\n",timedif(exit1,start),d->pkt_num);
	My402ListUnlink(&q1, first);
}
while(My402ListLength(&q2) != 0) {
	My402ListElem *first = My402ListFirst(&q2);
	Packet_Elem *f = (Packet_Elem *)first->obj;
	gettimeofday(&exit2,NULL);
	printf("%012.3fms: p%d removed from Q2\n",timedif(exit2,start),f->pkt_num);
	My402ListUnlink(&q2, first);
}

gettimeofday(&end,NULL);
fprintf(stdout,"%012.3fms: Emulation ends\n",timedif(end,start));

fprintf(stdout,"Statistics\n \n");
//Printing Statistics
if(packet_count==0){

	fprintf(stdout,"\taverage packet inter-arrival time =N/A(no packets arrived)\n");
}
else{
	fprintf(stdout,"\taverage packet inter-arrival time = %.6g\n",(total_inter_arrival_time/(double)packet_count)/1000);
}

if(serviced_packets==0){
	fprintf(stdout,"\taverage packet service time =	N/A(no packets completed the service)\n\n" );                                
        fprintf(stdout,"\taverage number of packets in Q1 = N/A(no packets completed the service)\n");
        fprintf(stdout,"\taverage number of packets in Q2 = N/A(no packets completed the service)\n");
        fprintf(stdout,"\taverage number of packets in S =N/A(no packets completed the service)\n\n");
        fprintf(stdout,"\taverage time a packet spent in system = N/A(no packets completed the service)\n" );
        fprintf(stdout,"\tstandard deviation for time spent in system =  N/A(no packets completed the service)\n\n" );

}
else{

	fprintf(stdout,"\taverage service time = %.6g\n",(total_service_time/(double)serviced_packets)/1000);

	fprintf(stdout,"\taverage number of packets in Q1 = %.6g\n",(total_time_in_q1/timedif(end,start))/1000);
	fprintf(stdout,"\taverage number of packets in Q2 = %.6g\n",(total_time_in_q2/timedif(end,start))/1000);
	fprintf(stdout,"\taverage number of packets at S1 = %.6g\n",(service_time_arr[0]/timedif(end,start))/1000);
	fprintf(stdout,"\taverage number of packets at S2 = %.6g\n",(service_time_arr[1]/timedif(end,start))/1000);
	fprintf(stdout,"\taverage time a packet spent in the system = %.6f\n",(total_time_in_sys/(double)serviced_packets)/1000);

	var = ((sum_of_square_time_in_sys/serviced_packets) - pow(total_time_in_sys/serviced_packets,2.0))/1000; 
	
	//double  sd = ((total_sys2_time/serviced_pkts) - pow(total_sys_time/serviced_pkts,2.0))/1000;
	//var = arg1 - arg2;	
	std_dev = sqrt(var/1000);	
	fprintf(stdout,"\tstandard deviation for time spent in system = %.6g\n",std_dev);

}


if(tokens_generated==0){

	fprintf(stdout,"\ttoken drop probability = N/A(no tokens arrived)\n");
}
else{

	fprintf(stdout,"\ttoken drop probability = %.6g\n",(double)tokens_dropped/tokens_generated);
}

if(packet_count==0){
	fprintf(stdout,"\tpacket drop probability= N/A(no packets arrived)\n" );

}
else{
	fprintf(stdout,"\tpacket drop probability = %.6g\n",(double)dropPackets_count/packet_count);
}

return 0;
}
