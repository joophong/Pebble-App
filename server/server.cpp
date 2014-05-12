/* 
This code primarily comes from 
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
 */

/* author: John Lewis, Steven Parad, and Joopyo Hong */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <pthread.h>
#include <termios.h>
#include <limits.h>
#include <pthread.h>
#include <deque>
#include <ctime>
#include <math.h>
#include <limits.h>
using namespace std;

const char FAHRENHEIT = '0';
const char CELSIUS = '1';
const char PAUSE = '2';
const char RESUME = '3';
const char TIME = '4';
const char GRAPH = '5';
const char DOT = '6';
const char DASH = '7';

void *console(void *);
void *read_temp(void *);
void *display_time(void*);

int reply_msg(int);
int reply_msg(int, char*);
void print_time();
void sendGraph(int);

int arduino;
char *ARDUINO_DIRECTORY;

double high_temp = INT_MIN;
deque<int> highQ;
double low_temp = INT_MAX;
deque<int> lowQ;
double last_temp;

double temp_avg = 0;
deque<int> averageQ;
double temp_total = 0;
int temp_count = 0;

int running = 1;
bool isFar = false;
int isFirst = 0;
bool connected = true;
bool standby = false;
bool inTimeMode = false;
bool inMorsecodeMode = false;

int last_request = CELSIUS;

time_t rawtime;
struct tm *timeinfo;

pthread_mutex_t lock;
pthread_t time_id;

/**
 * Thread for the server.
 */
int start_server(int PORT_NUMBER)
{

      // structs to represent the server and client
      struct sockaddr_in server_addr,client_addr;    
      
      int sock; // socket descriptor

      // 1. socket: creates a socket descriptor that you later use to make other system calls
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket");
        exit(1);
      }
      int temp;
      if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
        perror("Setsockopt");
        exit(1);
      }

      // configure the server
      server_addr.sin_port = htons(PORT_NUMBER); // specify port number
      server_addr.sin_family = AF_INET;         
      server_addr.sin_addr.s_addr = INADDR_ANY; 
      bzero(&(server_addr.sin_zero),8); 
      
      // 2. bind: use the socket and associate it with the port number
      if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("Unable to bind");
        exit(1);
      }

      // 3. listen: indicates that we want to listn to the port to which we bound; second arg is number of allowed connections
      if (listen(sock, 5) == -1) {
        perror("Listen");
        exit(1);
      }
          
      // once you get here, the server is set up and about to start listening
      printf("\nServer configured to listen on port %d\n", PORT_NUMBER);
      fflush(stdout);
      

      int fd = 0;

      last_temp = 0;
      high_temp = INT_MIN;
      low_temp = INT_MAX;
      temp_avg = 0;
      time_t begin;

      while (running) {
        // 4. accept: wait here until we get a connection on that port
        int sin_size = sizeof(struct sockaddr_in);
        fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
        printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));

        // buffer to read data into
        char request[2048];
        
        // // 5. recv: read incoming message into buffer
        int bytes_received = recv(fd,request,2048,0);
        // // null-terminate the string
        request[bytes_received] = '\0';

        printf("Here comes the message:\n");
        printf("%s\n", request);

        
        // this is the message that we'll send back
        /* it actually looks like this:
          {
             "msg": "cit595"
          }
        */
        // char *reply = "{\n\"msg": \"cit595\"\n}\n";

        char key[8];
        strcpy(key,"");


        key[0] = request[5];
        key[1] = '\0';


            if (key[0] == FAHRENHEIT) {
                last_request = FAHRENHEIT;
                inTimeMode = false;
                inMorsecodeMode = false;
                write(arduino, "F", 1);
                isFar = true;
                reply_msg(fd);

            } else if (key[0] == CELSIUS) {
                last_request = CELSIUS;
                inTimeMode = false;
                inMorsecodeMode = false;
                write(arduino, "C", 1);
                isFar = false;
                reply_msg(fd);

            } else if (key[0] == PAUSE) {
                standby = true;
                sleep(1);
                printf("The arduino is now in standby");
                write(arduino, "S", 1);
                reply_msg(fd, "In Standby Mode.");

            } else if (key[0] == RESUME) {
                write(arduino, "R", 1);
                sleep(1);
                printf("The arduino is now running.\n");
                standby = false;
                if (last_request == TIME) {
                    reply_msg(fd, "In Time Mode.");
                } else if (last_request == DOT || last_request == DASH) {
                    reply_msg(fd, "In Morse Code Mode.");
                } else if (last_request == GRAPH) {
                    sendGraph(fd);
                } else {
                    reply_msg(fd);
                }

            } else if (key[0] == TIME) {
                if (!inTimeMode) {
                    last_request = TIME;
                    inMorsecodeMode = false;
                    inTimeMode = true;
                    begin = time(NULL);
                    write(arduino, "T", 1);
                    pthread_create(&time_id, NULL, display_time, NULL);
                    reply_msg(fd, "In Time Mode.");
                }

            } else if (key[0] == GRAPH) {
                last_request = GRAPH;
                inTimeMode = false;
                inMorsecodeMode = false;
                sendGraph(fd);
                write(arduino, "C", 1);

            } else if (key[0] == DOT) {
                last_request = DOT;
                inTimeMode = false;
                inMorsecodeMode = true;
                write(arduino, ".", 1);
                reply_msg(fd, "In Morse Code Mode.");

            } else if (key[0] == DASH) {
                last_request = DASH;
                inTimeMode = false;
                inMorsecodeMode = true;
                write(arduino, "-", 1);
                reply_msg(fd, "In Morse Code Mode.");
            }



        // 6. send: send the message over the socket
        // note that the second argument is a char*, and the third is the number of chars

      
      // 7. close: close the socket connection
      close(fd);
      printf("Server closed connection\n");
    }
      close(sock);
      
  
      return 0;
}

/*
 * Sends the time to the arduino to display.
 */
void print_time() {
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    string hour;
    if (timeinfo->tm_hour < 10) {
        char buffer[2];
        sprintf(buffer, "%d", timeinfo->tm_hour);
        hour = "0";
        hour = hour + buffer;
    }
    else {
        char buffer[3];
        sprintf(buffer, "%d", timeinfo->tm_hour);
        hour = "";
        hour = hour + buffer;
    }

    string min;
    if (timeinfo->tm_min < 10) {
        char buffer[2];
        sprintf(buffer, "%d", timeinfo->tm_min);
        min = "0";
        min = min + buffer;
    }
    else {
        char buffer[3];
        sprintf(buffer, "%d", timeinfo->tm_min);
        min = "";
        min = min + buffer;
    }

    write(arduino, ("$$" + hour + min + "^").c_str(), 7);
}

/*
 * Sends a graph of the hourly averages to the arduino.
 */
void sendGraph(int fd) {
    int y = 4;
    double queue[] = {5,5,5,5};
    for (int i = averageQ.size() - 1; i > averageQ.size() - y - 1; i--) {
        queue[averageQ.size() - i -1] = averageQ[i];
    }

    int i,j = 0;
    //number of hours we are going to display
    int hours = y+1;

    pthread_mutex_lock(&lock);
    double current_total_avg = temp_avg;
    double current_total_count = 1;
    for (int i = 0; i < y; i++) {
        current_total_avg += queue[i];
        current_total_count++;
    }
    double total_avg = current_total_avg / current_total_count;

    pthread_mutex_unlock(&lock);

    //the maximum number of character on the pebble
    int max_pebble_size = 10;
    //max temp for the period
    double max = INT_MIN;
    //array of the difference between the queue values and the average temp
    double diff_queue[hours];
    //holds the number of stars to print
    int print_stars = 0;
    //holds the bars for each hours -- creates the graph
    char** strings = (char**) malloc(sizeof(char*)*(hours + 1));
    //construct the differences
    diff_queue[hours-1] = temp_avg - total_avg;
    for (i = 0; i < hours - 1; i++){
        if (i < (hours-1))
            diff_queue[i] = queue[i] - total_avg;
    }
    //determines max distance from average
    for (i=0; i < hours; i++){
        if (max < fabs(diff_queue[i]))
            max = fabs(diff_queue[i]);
    }

    //create the final return value and the strings for each bar
    char* total_string = (char*) malloc(sizeof(char)*((max_pebble_size+1)*(hours + 1)));
    strings[0] = (char*) malloc(sizeof(char)*max_pebble_size);
    if (total_string == NULL || strings[0] == NULL) {
        printf("Out of memory");
        return;
    }

    //create the bars
    sprintf(strings[0], "%2.1f  %2.1f  %2.1f",(total_avg - max), total_avg,(total_avg + max));
    char* temper;
    for (i = 1; i <= hours; i++){
        //malloc for the strings
        strings[i] = (char*) malloc(sizeof(char)*max_pebble_size);
        if (strings[i] == NULL) {
            printf("Out of memory");
            return;
        }
        //determine the number of stars for this hours (size of hour)
        if (max < 0.0001) {
            print_stars = 0;
        } else {
            print_stars = (fabs(diff_queue[i - 1]) / max) * 4.0;
        }
        strcpy(strings[i],"");
        //if negative, then stars below the mean, if positive then stars above the mean
        if (diff_queue[i - 1] >= 0)
            print_stars = 5 + print_stars;
        else
            print_stars = 5 - print_stars;
        //create the star bar

        for(j = 0; j < print_stars; j++)
           strcat(strings[i], "*");


        //make sure it is null terminated
        temper = strings[i];
        temper[print_stars] = '\0';
    }
    strcpy(total_string,"");
    for (i = 0; i <= hours; i++){
        strcat(strcat(total_string, strings[i]), "$");
        free(strings[i]);
    }
    free(strings);

    reply_msg(fd, total_string);
}

/*
 * Sends the specified message to the arduino.
 */
int reply_msg(int fd, char* outmsg) {
    char pre_key[20], post_key[20], reply[1000];
    strcpy(pre_key, "{\n\"msg\": \"");
    strcpy(post_key, "\"\n}\n");

    strcpy(reply, pre_key);
    strcat(reply, outmsg);
    strcat(reply, post_key);

    send(fd, reply, strlen(reply), 0);
    printf("Server sent message: %s\n", reply);
    return 0;
}

/*
 * Sends the temperature to the arduino.
 */
int reply_msg(int fd)
{
   char pre_key[20], post_key[20], reply[200];
   strcpy(pre_key, "{\n\"msg\": \"");
   strcpy(post_key, "\"\n}\n");

   char outmsg[100];
   strcpy(outmsg,"");
   if (!connected) {
       sprintf(outmsg, "Lost connection to Arduino");
       strcpy(reply, pre_key);
       strcat(reply, outmsg);
       strcat(reply, post_key);
       send(fd, reply, strlen(reply), 0);
       running = 0;
       return 0;
   }


   pthread_mutex_lock(&lock);
   double current_low = low_temp;
   double current_high = high_temp;
   double current_total_avg = temp_avg;
   double current_total_count = 1;

   for (int i = 0; i < lowQ.size(); i++) {
       if (lowQ[i] < current_low) current_low = lowQ[i];
       if (highQ[i] > current_high) current_high = highQ[i];
       current_total_avg += averageQ[i];
       current_total_count++;
   }

   double current_temp_avg = current_total_avg / current_total_count;

   if (isFar)
       sprintf(outmsg, "Temp %.2fF$Average %.2fF$High %.2fF$Low %.2fF", 9.0 * last_temp/5.0 + 32, 9.0 * current_temp_avg/5.0 +
            32,9.0 * current_high/5.0 + 32, 9.0 * current_low/5.0 + 32);
   else {
       printf("temp avg: %f, high temp: %f, low temp: %f\n", temp_avg, high_temp, low_temp);
       sprintf(outmsg, "Temp %.2fc$Average %.2fc$High %.2fc$Low %.2fc" , last_temp,current_temp_avg, current_high, current_low);
   }
   pthread_mutex_unlock(&lock);
   strcpy(reply, pre_key);
   strcat(reply, outmsg);
   strcat(reply, post_key);

   send(fd, reply, strlen(reply), 0);
   printf("Server sent message: %s\n", reply);
   return 0;
}

/*
 * Quits the server.
 */
void *console(void *)
{
  while (1) {
    char input[100];
    scanf("%s", input);
    if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0) {
      running = 0;
      break;
    }
  }
  pthread_exit(NULL);
}

/* Reads in the temperatures from the arduino and calculates the low, high, and average. */
void *read_temp(void *)
{
  arduino = open(ARDUINO_DIRECTORY, O_RDWR);
  if (arduino == -1) {
    printf("Error: Could not open Arduino file.");
    exit(1);
  }

  struct termios options;
  tcgetattr(arduino, &options);
  cfsetispeed(&options, 9600);
  cfsetospeed(&options, 9600);
  tcsetattr(arduino, TCSANOW, &options);

  int j = 0; //buffer counter
  int i = 0; //string counter
  char updatedString[11];
  strcpy(updatedString,"");
  char buf[15];
  strcpy(buf,"");
  char metric_type = 'C';

  time_t hrStart = time(NULL);
  time_t begin = time(NULL);
  while (running) {
      if(!standby && !inTimeMode){

        int bytes_read = read(arduino, buf, 1);
        if (bytes_read > 0) {
              begin = time(NULL);
              updatedString[j] = buf[0];
              j = j + 1;
              if (buf[0] == '\n') {
                updatedString[j-1] = '\0';
                printf("The temperature is: %s\n",updatedString);
                metric_type = updatedString[strlen(updatedString)-2];
                updatedString[j-2] = '\0';
                j = 0;

                pthread_mutex_lock(&lock);
                    last_temp = atof(updatedString);
                    if (metric_type == 'F'){
                        printf("\nArduino sent fahrenheit. We convert %f to %f\n",last_temp, 5.0*(last_temp - 32)/9.0);
                        last_temp = 5.0*(last_temp - 32)/9.0;
                    }
                    if (last_temp < 120 && last_temp > -120) {
                        temp_count += 1;
                        temp_total += last_temp;
                        temp_avg = temp_total / temp_count;
                        if (high_temp < last_temp)
                            high_temp = last_temp;
                        if (low_temp > last_temp)
                            low_temp = last_temp;
                    }

                pthread_mutex_unlock(&lock);
                strcpy(updatedString, "");

             }
        } else {
            time_t end = time(NULL);
            if (end - begin > 5) {
                connected = false;
            }
        }
      strcpy(buf,"");
     } else {
          begin = time(NULL);
     }
     time_t hrEnd = time(NULL);
     if (hrEnd - hrStart > 3600) {
         hrStart = time(NULL);
         averageQ.push_back(temp_avg);
         lowQ.push_back(low_temp);
         highQ.push_back(high_temp);
         temp_avg = 0;
         temp_count = 0;
         temp_total = 0;
         low_temp = INT_MAX;
         high_temp = INT_MIN;
         if (averageQ.size() > 23) averageQ.pop_front();
         if (lowQ.size() > 23) lowQ.pop_front();
         if (highQ.size() > 23) highQ.pop_front();
     }
  }
  pthread_exit(NULL);
}

/* Sends the time to the pebble and arduino every minute as long
 * it is in time mode.
 */
void* display_time(void*) {
    while (inTimeMode && running) {
        print_time();
        sleep(10);
    }
}

int main(int argc, char *argv[])
{
  // check the number of arguments
  if (argc != 3)
    {
      printf("\nUsage: server [PORT_NUMBER] [ARDUINO_DIRECTORY]\n");
      exit(0);
    }

  int PORT_NUMBER = atoi(argv[1]);
  ARDUINO_DIRECTORY = argv[2];

  pthread_mutex_init(&lock, NULL);

  pthread_t thread_id2;
  pthread_create(&thread_id2, NULL, read_temp, NULL);

  sleep(5);

  pthread_t thread_id1;
  pthread_create(&thread_id1, NULL, console, NULL);


  start_server(PORT_NUMBER);
}

