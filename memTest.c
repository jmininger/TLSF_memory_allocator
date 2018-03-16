#include "tlsf.h"
#include <stdlib.h>
#include <stdio.h>
typedef struct student{
	char name[100];
	int age;
	char grade;
	int gpa;
} student;
int main()
{
	void* tlsfStart = malloc(2048*100);
	
	TLSF_t* tlsf = tlsf_init_struct(tlsfStart, 2048*100);
	void *jac1 = tlsf_malloc(sizeof(student), tlsf);
	void *jac2 = tlsf_malloc(150, tlsf);
	void *jac3 = tlsf_malloc(534, tlsf);
	void *jac4 = tlsf_malloc(1200, tlsf);
	void *jac5 = tlsf_malloc(1200, tlsf);
	tlsf_free(jac1, tlsf);
	void *jac6 = tlsf_malloc(sizeof(student), tlsf);
	if(jac1==NULL || jac2==NULL || jac3==NULL || jac4==NULL || jac5==NULL || jac6==NULL)
		printf("DID NOT ALLOCATE PROPERLY\n");

	tlsf_free(jac2, tlsf);
	tlsf_free(jac3, tlsf);
	tlsf_free(jac4, tlsf);
	tlsf_free(jac5, tlsf);
	tlsf_free(jac6, tlsf);

	free(tlsfStart);

}
