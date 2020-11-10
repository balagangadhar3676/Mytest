#include <stdio.h>
#include <string.h>
void add_fun(void)
{
    int result;
    printf("Addition Operation\n");
    result=5+6;
    printf("Total addition value is:%d\n",result);
}

void diff_fun(void)
{
    int diff;
    printf("subtraction Operation\n");
    diff=6-5;
    printf("Total diff value is:%d\n",diff);
}
    
main()
{
    printf("Main Lizard Gateway Source Code\n");
    printf("Collect the data from uart\n");
    add_fun();
	diff_fun();
}

