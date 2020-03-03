/*----------------------------------------------------------------------------------
Base on:
        www.cnblogs.com/subo_peng/p/4848260.html
        Author: lzy

Modified and improved for half-dual SPI transfer, as of Openwrt for WidoraNEO.

Noticed that tx_buf[] and rx_buf[] in spi_io_transfer is for full-dual transfer.
for half-dual SPI transfer,we shall put send-mesg in tx_buf[0], while get recv_mesg
in rx_buf[1]. rx_buf[0] is deemed inncorrect!

LIMITS:!!!
Max. total length of all TxBuf and RxBuf in each SPI ioctl transfer = 36(bytes)

(max. SPI transfer) 1-byte command + 3-bytes address + 32-bytes data = 36 bytes

Midas Zhou
----------------------------------------------------------------------------------*/
#include "spi.h"

/* set main parameters */
const char *spi_fdev = "/dev/spidev32766.2";
static uint8_t spi_mode = 3;//3;
static uint8_t spi_bits = 8;// Not applicable!!!， MSB first
static uint32_t spi_speed = 2*1000*1000;/* min.1M 设置传输速度, 2M for touch XPT2064  */
static uint16_t spi_delay = 0;
static int g_SPI_Fd = -1; //SPI device file descriptor

/* print a system error message then abort */
void pabort(const char *s) {
	perror(s);
	abort();
}

/*-----------------------------------------------------------------------
 one time transfer only, ns=1.
 send mesg in tx_buf
 recv mesg in rx_buf shall not be used, as it's not correct !!!
 2*len MAX.36bytes
------------------------------------------------------------------------*/
int SPI_Transfer( const uint8_t *TxBuf,  uint8_t *RxBuf, int len, int ns)
{
	int ret;
	int fd = g_SPI_Fd;

	 struct spi_ioc_transfer tr={
		.tx_buf = (unsigned long) TxBuf,
		.rx_buf = (unsigned long) RxBuf,
		.len =len, // len of TxBuf and RxBuf to be the same
		.delay_usecs = spi_delay,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(ns), &tr); // repeat the message ns times

	if (ret < 1)
		printf("can't send spi message\n");
	else
	{
		#if SPI_DEBUG
		int i;
		printf("send spi message Succeed.\n");
		printf("SPI Send [Len:%d]: ", len);
		for (i = 0; i < len; i++)
		{
			if (i % 8 == 0)
			printf("0x%02X ", TxBuf[i]);
			printf("\n");
		}
		printf("SPI Receive [len:%d]: ", len);
		for (i = 0; i < len; i++)
		{
			if (i % 8 == 0)
			printf("0x%02X ", RxBuf[i]);
			printf("\n");
		}
		#endif
	}

	return ret;
}


/*--------------------------------------------------------------
SPI_Write( )
  Emulating half-duplex transfer.
  Write to SPI device with no interruption

  n_tx = MAX.36bytes

  send mesg in xfer[0].tx_buf
--------------------------------------------------------------*/
 int SPI_Write(const uint8_t *TxBuf, int n_tx)
{
 	int ret;
 	int fd = g_SPI_Fd;

	 struct spi_ioc_transfer xfer[2];
	 memset(xfer,0,sizeof(xfer));

	 xfer[0].tx_buf = (unsigned long) TxBuf;
	 xfer[0].len = n_tx;
	 xfer[0].delay_usecs = spi_delay;

	 ret = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
	 if (ret < 1)
		printf("********** SPI_Write(): Can't send message **********\n");

 return ret;
}

/*--------------------------------------------------------------
SPI_Write_Command_Data( )
  Write to SPI device command then data,with no interruption

  ncmd+ndat = MAX.36bytes

cmd: command pointer
ncmd: command length
dat: data pointer
cdat: data length

Return:
	< 1  fails
--------------------------------------------------------------*/
int SPI_Write_Command_Data(const uint8_t *cmd, int ncmd, const uint8_t *dat, int ndat)
{
 	int ret;
 	int fd = g_SPI_Fd;

	struct spi_ioc_transfer xfer[2];
	memset(xfer,0,sizeof(xfer));

	xfer[0].tx_buf = (unsigned long) cmd;
	xfer[0].len = ncmd;
	xfer[0].delay_usecs = spi_delay;
	xfer[1].tx_buf = (unsigned long) dat;
	xfer[1].len = ndat;
	xfer[1].delay_usecs = spi_delay;

	ret = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
	if (ret < 1)
		printf("********** SPI_Write(): Can't send message **********\n");

 return ret;
}


/*--------------------------------------------------------------------------------
SPI_Write_then_Read( )
  Emulating half-duplex transfer.
  Write then Read SPI device with no interruption

  n_tx+n_rx = MAX.36 bytes

  send mesg in xfer[0].tx_buf
  recv mesg in xfer[1].rx_buf
---------------------------------------------------------------------------------*/
 int SPI_Write_then_Read(const uint8_t *TxBuf, int n_tx, uint8_t *RxBuf, int n_rx)
{
 	int ret;
 	int fd = g_SPI_Fd;

	 struct spi_ioc_transfer xfer[2];
	 memset(xfer,0,sizeof(xfer));

	 xfer[0].tx_buf = (unsigned long) TxBuf;
	 xfer[0].len = n_tx;
	 xfer[0].delay_usecs = spi_delay;

	 xfer[1].rx_buf = (unsigned long) RxBuf;
	 xfer[1].len = n_rx;
	 xfer[1].delay_usecs = spi_delay;

	 ret = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
	 if (ret < 1)
		printf("********** SPI_Write_then_Read(): Can't send message **********\n");

 return ret;
}


/*---------------------------------------------------------
 SPI_Write_then_Write( )
 Write 2 times to SPI device with no interruption.
 n_tx1+n_tx2 = MAX.36 bytes
---------------------------------------------------------*/
int SPI_Write_then_Write(const uint8_t *TxBuf1, int n_tx1, uint8_t *TxBuf2, int n_tx2)
{
	int ret;
	int fd = g_SPI_Fd;

	struct spi_ioc_transfer xfer[2];
	memset(xfer,0,sizeof(xfer));

	xfer[0].tx_buf = (unsigned long) TxBuf1;
	xfer[0].len = n_tx1;
	xfer[0].delay_usecs = spi_delay;
	xfer[1].tx_buf = (unsigned long) TxBuf2;
	xfer[1].len = n_tx2;
	xfer[1].delay_usecs = spi_delay;

	ret = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
	if (ret < 1)
		printf("********** SPI_Write_then_Write(): Can't send message **********\n");


	return ret;
}


/*----------------------------------------------
        Open SPI device and initialize it
Return:
	0	OK
	<0	Fails
-----------------------------------------------*/
int SPI_Open(void)
{
	int fd;
	int ret = 0;

	if (g_SPI_Fd != -1) /* 设备已打开 */
		return 0xF1;

	fd = open(spi_fdev, O_RDWR|O_CLOEXEC);
	if (fd < 0){
		perror(" Can't open device");
		return -1;
	}
	else
		printf("SPI Open succeed. Start init SPI...\n");

	g_SPI_Fd = fd;

	/* set spi mode */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &spi_mode);
	if (ret == -1)
		perror("ioctl can't set spi SPI_IOC_WR_MODE mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &spi_mode);
	if (ret == -1)
		perror("ioctl can't set spi SPI_IOC_RD_MODE mode");


	/* set bits per word */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits);
	if (ret == -1)
		perror("ioctl can't set bits per word SPI_IOC_WR_BITS_PER_WORD");
	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &spi_bits);
	if (ret == -1)
		perror("ioctl can't set bits per word SPI_IOC_RD_BITS_PER_WORD");


	/* set spi speed in hz */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
	if (ret == -1)
		perror("can't set max speed hz");


	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &spi_speed);
	if (ret == -1)
		perror("can't get max speed hz");

	printf("	spi mode: %d\n", spi_mode);
	printf("	bits per word: %d\n", spi_bits);
	printf("	max speed: %d KHz (%d MHz)\n", spi_speed / 1000, spi_speed / 1000 / 1000);

	return 0;
}


/*--------------------------------------
            close spi fd
---------------------------------------*/
int SPI_Close(void)
{
	if (g_SPI_Fd == -1) //fd closed already
		return -1;

	close(g_SPI_Fd);
	g_SPI_Fd = -1;

	return 0;
}

/*-------------------------------------------------------
  Look Back test

  !!!!!  Not applicable  !!!!!!

  Dulplex transfer is not fully supported for widora now!
---------------------------------------------------------*/
int SPI_LookBackTest(void)
{
	int ret, i;
	const int BufSize = 16;
	uint8_t tx[BufSize], rx[BufSize];

	bzero(rx, sizeof(rx));

	for (i = 0; i < BufSize; i++)
		tx[i] = i;

	printf("start LookBack Mode test...\n");

	ret = SPI_Transfer(tx, rx, BufSize,1);

	if (ret > 1)
	{
		//compare tx and rx data
		ret = memcmp(tx, rx, BufSize);
		if (ret != 0)
		{
			printf("LookBack Mode test error!!\n");
		}
	else
		printf("SPI LookBack Mode test passed successfully!\n");
	}

	return ret;
}




