#include <stdio.h>
#include <stdlib.h>

void Usage(void)
{
	fprintf(stderr,"Usage: wcm-client filename\n");
	exit(1);
}

int main(int argc, char** argv)
{
	char* a, *pszFile=NULL;
	FILE* f;

	++argv;
	while ((a=*(argv++)) != NULL)
	{
		if (!pszFile) pszFile=a;
		else Usage();
	}
	if (!pszFile) Usage();

	f = fopen(pszFile,"w");
	if (!f) { perror("failed to open"); exit(1); }

	fprintf(f,"foo\n");
	fclose(f);

	return 0;
}
