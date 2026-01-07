#include "Arduino.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define MAXPINS 256
int pinsValue[MAXPINS];

void pinMode(int pin, int type)
{
   char pinName[100];
   int fd;

   if (pin >= MAXPINS || pin < 0) {
      printf("Bad pin %d\n", pin);
      exit(1);
   }
   snprintf(pinName, sizeof(pinName), "/sys/class/gpio/gpio%d/direction", pin);
   fd = open(pinName, O_RDWR);
   if (fd < 0) {
      printf("Unable to open %s\n", pinName);
      exit(1);
   }
   snprintf(pinName, sizeof(pinName), "/sys/class/gpio/gpio%d/value", pin);
   pinsValue[pin] = open(pinName, O_RDWR);
   if (pinsValue[pin] < 0) {
      printf("Unable to open %s\n", pinName);
      exit(1);
   }
   if (type == OUTPUT) {
      if (write(fd, "low", 3) == -1) {
         printf("Write low failed\n");
         exit(1);
      }
   } else {
      if (write(fd, "in", 2) == -1) {
         printf("Write in failed\n");
         exit(1);
      }
   }
}

void digitalWrite(int pin, int value)
{
   if (pin >= MAXPINS || pin < 0 || pinsValue[pin] == 0) {
      printf("Bad pin %d\n", pin);
      exit(1);
   }

   if (value == HIGH) {
      if (write(pinsValue[pin], "1", 1) == -1) {
         printf("Write 1 failed\n");
         exit(1);
      }
   } else {
      if (write(pinsValue[pin], "0", 1) == -1) {
         printf("Write 1 failed\n");
         exit(1);
      }
   }
}

int digitalRead(int pin)
{
   char str[10];
   if (pin >= MAXPINS || pin < 0 || pinsValue[pin] == 0) {
      printf("Bad pin %d\n", pin);
      exit(1);
   }
   if (pread(pinsValue[pin], str, sizeof(str), 0) == -1) {
      printf("pread failed pin %d\n", pin);
      exit(1);
   }
   return str[0] == '1';
}

void delayMicroseconds(int microSec) {
   usleep(microSec);
}

void delay(int miliSec) {
   usleep(miliSec * 1000);
}

int bitRead(int value, int bit) {
   return (value >> bit) & 1;
}
