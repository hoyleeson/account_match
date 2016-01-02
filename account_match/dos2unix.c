#include <stdio.h>

#define CR 		0x0D 		// <回车> '\r'
#define LF 		0x0A 		// <换行> '\n'
#define SPACE 	0x20 		// <空格>



void dos2unix(const char* file_name)
{
	FILE *fp = NULL;

	fp = fopen(file_name, "rb+");

	if (fp != NULL)
	{
		char ch;
		while (fread(&ch, sizeof(ch), 1, fp) == 1)
		{
			if (CR == ch)
			{
				ch = SPACE;
				fseek(fp, -1L, SEEK_CUR);
				fwrite(&ch, sizeof(ch), 1, fp);
				fseek(fp, 1L, SEEK_CUR);
			}
		}
	}

	if (fp != NULL)
	{
		fclose(fp);
		fp = NULL;
	}
}

