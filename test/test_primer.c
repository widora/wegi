#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/time.h>

void print_tmdiff(struct timeval t_start, struct timeval t_end);

int main(void)
{
	uint64_t num;
	uint64_t div;
	bool	IsPrimer=true;
	struct timeval tm_start, tm_end;

	for( ; printf("Please enter a number:"), scanf("%"PRIu64"", &num)==1; )
	{
		gettimeofday(&tm_start, NULL);
//		for( div=2;  div*div < num;  div++) {  // div*div  will overflow ! //
		for( div=2;  div < num/div;  div++) {
			if( num%div == 0) {
				IsPrimer=false;
				if(div*div==num)
					printf("%"PRIu64" is divisible by %"PRIu64"\n", num, div);
				else {
					printf("%"PRIu64" divisible by %"PRIu64" and %"PRIu64" \n",
										  num, div, num/div);
				}
			}
		}
		if(IsPrimer)
			printf("%"PRIu64" is a primer\n", num);

		gettimeofday(&tm_end, NULL);

		print_tmdiff(tm_start, tm_end);

	}

	return 0;
}



void print_tmdiff(struct timeval t_start, struct timeval t_end)
{
        int ds=t_end.tv_sec-t_start.tv_sec;
        int dus=t_end.tv_usec-t_start.tv_usec;

	if(dus<0 && ds>0) {
		ds--;
		dus += 1000000;
	}

	printf("Time cost: %ds %dms\n", ds, dus/1000);

}

