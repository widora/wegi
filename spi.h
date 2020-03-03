/*-------------------------------------------------
Based on:
        www.cnblogs.com/subo_peng/p/4848260.html
        Author: lzy

Modified by:
Midas Zhou
-------------------------------------------------*/
#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/* turn on/off SPI DEBUG */
#define SPI_DEBUG 0

extern const char *spi_fdev;

/* FUCNTION DECLARATION */
void pabort(const char *s);
int SPI_Transfer( const uint8_t *TxBuf,  uint8_t *RxBuf, int len,int ns);
int SPI_Write(const uint8_t *TxBuf, int len);
int SPI_Write_Command_Data(const uint8_t *cmd, int ncmd, const uint8_t *dat, int ndat);
int SPI_Write_then_Read(const uint8_t *TxBuf, int n_tx, uint8_t *RxBuf, int n_rx);
int SPI_Write_then_Write(const uint8_t *TxBuf1, int n_tx1, uint8_t *TxBuf2, int n_tx2);
int SPI_Read(uint8_t *RxBuf, int len);
int SPI_Open(void);
int SPI_Close(void);
int SPI_LookBackTest(void);


#endif
