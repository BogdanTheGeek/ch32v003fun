all : flash

TARGET:=bleh
TARGET_MCU:=CH32V208
TARGET_MCU_PACKAGE:=CH32V208WBU6

include ../../ch32fun/ch32fun.mk
CFLAGS+=-D$(TARGET_MCU_PACKAGE)

flash : cv_flash
clean : cv_clean
