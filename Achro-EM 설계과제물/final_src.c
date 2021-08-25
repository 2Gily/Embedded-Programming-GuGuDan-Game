
#include <stdio.h >
#include <stdlib.h >
#include <unistd.h >
#include <sys /types.h >
#include <sys /stat.h >
#include <fcntl.h >
#include <sys /ioctl.h >
#include <signal.h >
#include <string.h >
#include <pthread.h >
#include "./fpga_dot_font.h"
#define MAX_QUIZ 10
#define MAX_BUTTON 9
#define MAX_BUFF 32
#define LINE_BUFF 16
#define TEXT_LCD_DEVICE "/dev/fpga_text_lcd"
#define FND_DEVICE "/dev/fpga_fnd"
#define LED_DEVICE "/dev/fpga_led"
#define SWITCH_DEVICE "/dev/fpga_push_switch"
typedef struct question {
   int front_number, rear_number, result_number;
}RandomNumber;
void * func_lcd(void *data);
void * func_led();
void * func_switch();
void * func_fnd();
void * func_dot();
RandomNumber question[MAX_QUIZ];
int fnd_dev;
int sw_dev;
int right_anwser =-1; // 정답 초기값
unsigned char led_data =255;   //led 불빛 초기값
unsigned char quit =0;
int user_answer =0;
int is_pushed =0;
int quiz_number =0;
int flag =0;
// 쓰레드 관련 변수
pthread_mutex_t m_id;
pthread_mutex_t m_text;
void user_signal1(int sig)
{
   quit =1;
}
int main()
{
   memset(question, 0, sizeof(RandomNumber) * MAX_QUIZ);
   srand(time(NULL));
   pthread_t p_switch, p_led, p_fnd, p_lcd, pthread_t p_dot;
   sw_dev = open(SWITCH_DEVICE, O_RDWR);
   fnd_dev = open(FND_DEVICE, O_RDWR);
   for (int i =0; i <10; i ++)
   {
      question[i].front_number = (rand() % 8) +2;
      question[i].rear_number = (rand() % 8) +2;
      question[i].result_number = question[i].front_number * question[i].rear_number;
   }
   right_anwser = question[0].rear_number; // 첫 문제 정답으로 초기화
   pthread_mutex_init(&m_id, NULL);
   pthread_mutex_init(&m_text, NULL);
   pthread_create(&p_switch, NULL, func_switch, NULL);
   pthread_create(&p_fnd, NULL, func_fnd, NULL);
   pthread_create(&p_led, NULL, func_led, NULL); 
   pthread_create(&p_dot, NULL, func_dot, NULL);
      if (pthread_create(&p_lcd, NULL, func_lcd, NULL) <0)
      {
         perror("thread create error:");
         exit(0);
      }
   for (int i =0; i <10; i ++)
   {
      // 전송
      while (1)
      {
         //printf("answer: %d, user answer: %d, is_pushed: %d", answer, user_answer, is_pushed);
         if ((right_anwser == user_answer) && is_pushed ==1)
         {
            printf("correct! right answer: %d\n", right_anwser);
            printf("user_answer :%d\n\n", user_answer);
            quiz_number++; 
            flag =1;
            pthread_mutex_lock(&m_id);
            user_answer =-1;
            is_pushed =0;
            pthread_mutex_unlock(&m_id);
            break;
         }
         else if ((right_anwser != user_answer) && is_pushed ==1)
         {
            //test
            printf("right_anwser : %d\n\n", right_anwser);
            printf("user_answer :%d\n\n", user_answer);
            
            flag =0;
            led_data >>=1;
            pthread_mutex_lock(&m_id);
            is_pushed =0;
            pthread_mutex_unlock(&m_id);
            if (led_data ==0x00)
            {
               printf("Bomb!!!!!!!!!!\nYou Die...\n");
               printf("Exit 3sec later...\n");
               for (int j =0; j <3; j ++)
               {
                  usleep(10000);
               }
               exit(0);
            }
         }
         //   pthread_mutex_lock(&m_id);
         //   user_answer = -1;
         //   is_pushed = 0;
         //   pthread_mutex_unlock(&m_id);
      }
   }
   pthread_join(p_lcd, NULL);
   pthread_join(p_switch, NULL);
   pthread_join(p_fnd, NULL);
   pthread_join(p_led, NULL);
   pthread_join(p_dot, NULL);
   pthread_mutex_destroy(&m_id);
   pthread_mutex_destroy(&m_text);
}
void * func_switch() {
   int i;
   int dev, buff_size;
   unsigned char push_sw_buff[MAX_BUTTON];
   //dev = open("/dev/fpga_push_switch", O_RDWR);
   if (sw_dev <0) {
      printf("Device Open Error\n");
      close(sw_dev);
      // return -1;
   }
   (void)signal(SIGINT, user_signal1);
   buff_size =sizeof(push_sw_buff);
   memset(push_sw_buff, 0, buff_size); // 예제 Test 프로그램에서는 memset 안해서 빼봄
                              // 읽기
   while (!quit)
   {
      usleep(400000);
      memset(push_sw_buff, 0, sizeof(push_sw_buff));
      read(sw_dev, &push_sw_buff, buff_size);
      for (i =0; i < MAX_BUTTON; i ++)
      {
         if (push_sw_buff[i] ==1)
         {
            //   printf("%d button pushed!\n", i);
            pthread_mutex_lock(&m_id);
            user_answer = i +1;
            is_pushed =1;
            memset(push_sw_buff, 0, buff_size);
            pthread_mutex_unlock(&m_id);
         }
         // Button TEST
         for (int j =0; j <MAX_BUTTON; j ++)
         {
            printf("[%d] ", push_sw_buff[j]);
         }
         printf("\n");
         // Button TEST
      }
   }
   close(sw_dev);
}
void * func_lcd(void *data)
{
   unsigned char front;
   unsigned char result[2];
   unsigned char string[32];
   unsigned int ten_position, one_position;
   unsigned int str_size;
   unsigned int dev;
   int i =0;
   dev = open(TEXT_LCD_DEVICE, O_WRONLY);
   if (dev <0) {
      printf("Device open error : %s\n", TEXT_LCD_DEVICE);
      close(dev);
   }
   while (1)
   {
      i = quiz_number;
      // 정답 설정
      pthread_mutex_lock(&m_text);
      right_anwser = question[i].rear_number;
      pthread_mutex_unlock(&m_text);
      //정답 설정
      memset(result, '\0', sizeof(result));
      memset(string, '\0', sizeof(string));
      front ='0'+ question[i].front_number;
      if (question[i].result_number >=10)
      {
         ten_position = question[i].result_number /10;
         one_position = question[i].result_number % 10;
         result[0] ='0'+ ten_position;
         result[1] ='0'+ one_position;
      }
      else
      {
         result[0] ='0'+ question[i].result_number;
         result[1] =' ';
      }
      string[0] =front;
      strncat(string, " x ? = ", strlen(" x ? = "));
      strncat(string, result, sizeof(result));
      str_size = strlen((char *)string);
      memset(string + str_size, ' ', MAX_BUFF - str_size);
      write(dev, string, MAX_BUFF);
   }
   close(dev);
}
void * func_led()
{
   int dev = open(LED_DEVICE, O_RDWR);
   if (dev <0)
   {
      printf("error()\n");
      close(dev);
   }
   while (1)
   {
      if (write(dev, &led_data, 1) <0)
      {
         puts("led write error!\n");
      }
   }
   sleep(1);
}
void * func_fnd() {
   //int dev;
   unsigned char data[4];
   unsigned char retval;
   int sec, msec;
   memset(data, 0, sizeof(data));
   //dev = open(FND_DEVICE, O_RDWR);
   if (fnd_dev <0) {
      printf("Device open error : %s\n", FND_DEVICE);
      exit(1);
   }
   data[0] =3 +0x30;
   write(fnd_dev, &data, 4);
   usleep(10000);
   for (sec =29; sec >-1; sec --) {
      /*if (quit == 0) {
      break;
      }*/
      data[0] = (sec /10) +0x30;
      data[1] = (sec % 10) +0x30;
      for (msec =99; msec >-1; msec --) {
         data[2] = (msec) /10 +0x30;
         data[3] = msec % 10 +0x30;
         retval = write(fnd_dev, &data, 4);
         usleep(10000);
      }
   }
   if (retval <0) {
      printf("Write Error!\n");
      //return -1;
   }
   memset(data, 0, sizeof(data));
   sleep(1);
   close(fnd_dev);
   printf("\n===== Time over =====\n"); //
   exit(0);
}
void * func_dot()
{
   int dev = open(FPGA_DOT_DEVICE, O_WRONLY);
   if (dev <0) {
      
         printf("Device open error : %s\n", FPGA_DOT_DEVICE); 
         exit(1); 
   }
   if (flag ==1) { //answere is true...
      str_size =sizeof(fpga_number[1]);//O
      write(dev, fpga_number[1], str_size);
   }
   if (flag ==0) { //answere is false...
      str_size =sizeof(fpga_number[1]); //X
      write(dev, fpga_number[1], str_size);
   }
   
      
      close(dev); 
      
      return 0; 
}
