################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../getch.c \
../getopt.c \
../gpssim.c \
../uhdgps.c 

OBJS += \
./getch.o \
./getopt.o \
./gpssim.o \
./uhdgps.o 

C_DEPS += \
./getch.d \
./getopt.d \
./gpssim.d \
./uhdgps.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O1 -g -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


