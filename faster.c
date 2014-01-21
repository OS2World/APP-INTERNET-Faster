/*	
	Faster 1.0
	Copyright (C) 1996 Ching-Chih (Oscar) Chang
	All Rights Reserved
	
	oscar.chang@rahul.net

	This program is designed to add  the WIDTH and HEIGHT arguments into
	the IMG HTML tag.

	You may use this program freely, recompile it for other platforms or
	alter it to your liking.  I do *require* you to state that I am  the
	original author of this program.
	
	As a courtesy, please send me an email about the changes you've made
	or  if you've ported this program to another platform.  The makefile
	you write for  your  platform  could  be  added  to the distribution
	source.

	This program is distributed in the hope that it will be useful,  but
	WITHOUT   ANY   WARRANTY;  without  even  the  implied  warranty  of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	In other words, use this at your own risk.  I am not responsible for
	any or all damages that may come from the use of this program.

	( Don't you just love legal  mumbo  jumbo?  Oh well it keeps my butt
	out of jail.  :) )

*/

/*
   The following define statements are OS specific settings that you can 
   change without changing the rest of the source code.
*/

#define COPYCMD	"copy "		/* the command for COPY (e.g. would be cp for unix)
							   Note: remember to have a space afterwards so it
							   will system() the command correctly
							*/

#ifdef __DOS__
#define	MAXFILENAME	128		/* the DOS 8.3 filename and the max path */
#define ALLMASK		"*.*"	/* mask for all files in DOS */
#else
#define	MAXFILENAME	1024	/* OS/2 uses long filenames and I think the max path */
#define ALLMASK		"*"		/* mask for all files in OS/2 */
#endif

/*
   End of OS specific defines
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __WATCOMC__
#include <dos.h>
#include <direct.h>
#endif

/*
   JPEG Tag names
*/
#define M_APP0   0xe0
#define M_SOI    0xd8
#define M_SOF0   0xc0
#define M_SOF1   0xc1
#define M_SOF9   0xc9

void printhelp();					/* prints the help file on how to use this thing */
void backupfile(char *srcfile);		/* backups the file before I mess with it (muhahahah ;) */
void dopath(char *mask);			/* Does all the path/file stuff (is recursive) */
int parsefile(char *filename);		/* The hard stuff, reading in the file,
									   and parsing the img tags */
unsigned char imgtype(char *filename, unsigned short *width, unsigned short *height);
		/*Opens the file and see what the graphics file type it is
		  Returns: 0 for error, 1 for GIF, 2 for JPG, 3 for PNG (added later)
		*/
unsigned short get2bytes(FILE *infile);
		/* Get's two bytes from the an open file, the way JPG wants it. */
void memerror(char *error);
		/* Displays the error and quits the program with exit(1); */
/*
   Link List functions and structures
*/
struct link
	{
	char *filename;
	struct link *next;
	};

void addlink(char *str);/* Adds a link to the end of the list (via linktail) */
int dellink();			/* Deletes the head of the link (via linkhead) */

static struct link *linkhead = NULL;	/* link list head */
static struct link *linktail;			/* link list tail */

/*
    Switches
*/
char upper = 0,
	 lower = 1,		/* default is lowercase tags */
	 recurse = 0,	/* recurse into directories */
	 backup = 0,	/* Backup file */
	 deleteWH = 0;	/* delete width and height tags */

int main(int argc, char **argv)
	{
	int i = 1;

	#ifdef __DOS__
	printf("\nFaster for DOS 1.00");
	#endif
	#ifdef __OS2__
	printf("\nFaster for OS/2 1.00");
	#else
	printf("\nFaster 1.00");
	#endif
	printf("\nCopyright (C) 1996 Ching-Chih (Oscar) Chang\nAll Rights Reserved\n");

	if (argc == 1)		/* help screen if no args */
		printhelp();
	else
		{
		while(i < argc && argv[i][0] == '-')	/* while they are options (with -) */
			{
			switch(tolower(argv[i][1]))
				{
				case 'u' : upper=1; lower=0; break;		/* use uppercase */
				case 'l' : lower=1; upper=0; break;		/* use lowercase */
				case 'b' : backup = 1; break;			/* backup files */
				case 'r' : recurse = 1; break;			/* recursive */
				case 'd' : deleteWH = 1; break;			/* delete tags */
				case '?' :
				case 'h' : printhelp(); exit(0); break;
				default  : printf("\nError: Unknown switch %s", argv[i]); 
						   return 1; exit(1); break;
				}
			i++;	/* go to next argument */
			}
		while(i < argc)		/* go through the rest of the args */
			dopath(argv[i++]);
		}
	return 0;
	}

void dopath(char *mask)
	{
	unsigned rc;
	struct find_t fileinfo;
	char *cwd;	/* current working directory */

	if(recurse == 1)
		{
		/* get's all directories */
		rc = _dos_findfirst(ALLMASK, _A_SUBDIR, &fileinfo);
		while (rc == 0)		/* does the subdirectories */
			{
			/* Make sure it doesn't process \. or \.. and then only the dirs */
			if(fileinfo.name[0] != '.' && fileinfo.attrib == _A_SUBDIR)
				{
				chdir(fileinfo.name);
				dopath(mask);		/* recurse and do it again baby */
				chdir("..");		/* go back to starting directory */
				}
			rc = _dos_findnext(&fileinfo);
			}
		#if defined(__OS2__)
		_dos_findclose(&fileinfo);
		#endif
		}

	/* start working on the files in the direcotry */
	rc = _dos_findfirst(mask, _A_NORMAL, &fileinfo);
	while(rc == 0)
		{
		addlink(fileinfo.name);			/* add filename to the linklist */
		rc = _dos_findnext(&fileinfo);
		}
	#if defined(__OS2__)
	_dos_findclose(&fileinfo);
	#endif

	if(linkhead != NULL)	/* if not an empty directory */
		{
		cwd = getcwd(NULL, 0);	/* Show the current directory */
		printf("\n%s", cwd);
		free(cwd);
		do {
			printf("\n\t%s",linkhead->filename);	/* show the file beening processed */
			if(backup == 1)
				backupfile(linkhead->filename);
			parsefile(linkhead->filename);		/* do the hard work and process the thing */
			}while(dellink() == 0);
		}
	}

int parsefile(char *filename)
	{
	FILE *htmlfile;
	char *html, *buffer, *src, imgfile[MAXFILENAME];
	char wtext[] = "width=", htext[] = "height=", ltoabuf[4];
	unsigned short width, height, i;
	unsigned long len, bufindex, htmlindex;

	imgfile[0] = '\0';
	
	htmlfile = fopen(filename, "r+b");	/* Open the html file */

	if (htmlfile == NULL)	/* This doesn't seem to print anything in watcom, weird */
		{
		printf("\nUnable to open file %s", filename);
		return 1;
		}
	
	fseek(htmlfile, 0L, SEEK_END);	/* go to the end of the file to... */
	len = ftell(htmlfile);			/* get the filesize */
	fseek(htmlfile, 0L, SEEK_SET);	/* and then jump back */

	html = (char *)malloc(len);		/* allocate memory for file */
	if (html == NULL)
		memerror("Not enough memory for HTML file.");
	buffer = (char *)malloc(len*2);	/* make the work buffer twice as big
									   I know, not very efficent, but I don't
									   know how else to calculate how big my 
									   buffer should be */
	if (html == NULL)
		memerror("Not enough memory for HTML buffer.");
	fread(html, len, 1, htmlfile);	/* read in html file */
	fclose(htmlfile);				/* close file, finished reading it */

	unlink(filename);	/* erase the file 
						   Note: instead of just overwriting the file, erasing
						   it first may help some undelete programs 
						*/
	htmlfile = fopen(filename, "w+b");	/* create the file */
	bufindex = htmlindex = 0;			/* reset the buffer indexs */

	while(htmlindex < len)
		{
		if(html[htmlindex] == '<')		/* if it's a tag */
			{
			/* copy the '<' over then...
			   skip any white space between the '<' and the tag name if any
			*/
			do	{
				buffer[bufindex++] = html[htmlindex++];
				}while(html[htmlindex] == ' ');

			if (strnicmp(html+htmlindex, "img", 3) == 0)	/* if img tag */
				{
/*
  				printf("\nFound IMG tag");
*/
				while(html[htmlindex] != '>')	/* until end of tag */
					{
					/* get other tags that I don't touch */
					do	{
						buffer[bufindex++] = html[htmlindex++];
						}while (html[htmlindex] != ' ' && html[htmlindex]!= '>');
					
					/* found source tag (yes there's a space there) */
					if (strnicmp ( html+htmlindex, " src", 4) == 0)
						{
/*
  						printf(" and SRC tag");
*/
						/* Find the = so we can get to the filename */
						do	{
							buffer[bufindex++] = html[htmlindex++];
							}while(html[htmlindex] != '=');
						/* skip any white space */
						do {
							buffer[bufindex++] = html[htmlindex++];
							}while(html[htmlindex] == ' ');
						/* If the filename is inbetween quotes */
						if(html[htmlindex] == '"')
							{
							/* Get the quote */
							buffer[bufindex++] = html[htmlindex++];
							i = 0;
							do {
								/* Copy the filename to imgfile and main buffer */
								imgfile[i++] = html[htmlindex];
								buffer[bufindex++] = html[htmlindex++];
								}while(html[htmlindex] != '"');
							imgfile[i] = '\0';	/* Null terminate */
							/* get rid of the closing quote */
							buffer[bufindex++] = html[htmlindex++];
							}
						 else	/* no quotes */
						 	{
							i = 0;
							do {
								imgfile[i++] = html[htmlindex];
								buffer[bufindex++] = html[htmlindex++];
								}while(html[htmlindex] != ' ' && html[htmlindex] != '>');
							imgfile[i] = '\0';	/* Null terminate */
							}
						if(imgfile[0] != '\0')	/* if filename was retrieved */
							{
/*
  							printf(" %s", imgfile);
*/
							/* see what type of file it
							   and if it returns a non-0 then filetype found
							*/
							if (imgtype(imgfile, &width, &height) != 0 && deleteWH != 1)
								{
								if(upper == 1)		/* if they want uppercase */
									{				/* Lowercase by default   */
									strupr(wtext);
									strupr(htext);
									}
	
								/* copy WIDTH to buffer with space before and after */
								buffer[bufindex++] = ' ';
								buffer[bufindex] = '\0';	/* make sure it's end of string */

								strcat(buffer, wtext);		/* copy text over */

								/* insert WIDTH value here */
								ltoa(width, ltoabuf, 10);	/* convert number to text */
								strcat(buffer, ltoabuf);

								/* add letter length */
								bufindex += strlen(wtext) + strlen(ltoabuf);

								/* copy HEIGHT to buffer with space before and after */
								buffer[bufindex++] = ' ';
								buffer[bufindex] = '\0';	/* make sure it's end of string */

								strcat(buffer, htext);		/* copy text over */

								/* insert HEIGHT value here */
								ltoa(height, ltoabuf, 10);
								strcat(buffer, ltoabuf);
								bufindex += strlen(htext) + strlen(ltoabuf);
								}
							}
						}
					/* Skips the old width and height tags, if any
					   Also used if deleteWH is set which just erases any W/H tags
					   */
					if (strnicmp(html+htmlindex, " width", 6) == 0)
						{
						do	{
							htmlindex++;
							}while(html[htmlindex] != ' ' && html[htmlindex] != '>' );
						/* one more to get rid of an extra space */
						}						   
					if (strnicmp(html+htmlindex, " height", 7) == 0)
						{
						do	{
							htmlindex++;
							}while(html[htmlindex] != ' ' && html[htmlindex] != '>' );
						}
					}
				}
			}
		buffer[bufindex++] = html[htmlindex++];		/* copy it over */
		}

	len = fwrite(buffer, 1, bufindex, htmlfile);
	
	if(len != bufindex)		/* check to make sure all the bytes were written */
		{
		do	{
			printf("\nWrite Error: %d bytes of %d were written!\nEnter alternate path to save file: ");
			/* instead of making a new buffer, I thought I'd just reuse html 
  			   since it's not being used anyway. */
			scanf("%s", html);

			fclose(htmlfile);

			htmlfile = fopen(html, "w+b");
			if(htmlfile != NULL)
				{
				len = fwrite(buffer, bufindex, 1, htmlfile);
				}
			}while(len != bufindex);
		}

	/* Clean up	*/
	fclose(htmlfile);
	free(html);
	free(buffer);
	return 0;
	}

unsigned char imgtype(char *filename, unsigned short *width, unsigned short *height)
	{
	FILE *imgfile;
	unsigned char buffer[16];
	unsigned char rtype = 0;
	unsigned short offset;

	imgfile = fopen(filename, "r+b");
	if (imgfile == NULL)
		{
		printf("\n\t\tGraphic file %s not found", filename);
		return 0;
		}
	fread(buffer, 16, 1, imgfile);		/* read a chunk of the header */
	
	if(strnicmp(buffer, "GIF", 3) == 0)	/* if it finds a GIF */
		{
/*
  		printf("\nFile %s is a GIF", filename);
*/
		rtype = 1;
		offset = 6;		/* offset where GIF w/h is */

		fseek(imgfile, offset, SEEK_SET);	/* the 6th byte is where w/h start */
		fread(width, 2, 1, imgfile);	/* read the width */
		fread(height, 2, 1, imgfile);	/* read the height */
		}
	else if(strnicmp(buffer+6,"JFIF",4) == 0)	/* +6 to get the type */
		{
/*
  		printf("\nFile %s is a JPG", filename);
*/
		offset = buffer[4];		/* Get's the length of the header tag */
		offset <<= 8;			/* width/height isn't in the header so we have */
		offset += buffer[5];	/* to jump around a bit with offset */

		fseek(imgfile, 6, SEEK_SET);	/* Get rid of the FF, SOI, FF and APP0 */
		/* Find the right tag (SOF0 or SOF1 or SOF9) */
		do	{
			/* offset should -2 since it includes the length (2 bytes) that 
			   we already read in but we need to skip the 0xff that's in 
			   front of every tag so we do -2+1 = -1 */
			fseek(imgfile, offset-1, SEEK_CUR);
			rtype = fgetc(imgfile);		
/*			printf("\n%x", rtype);
*/	
			offset = get2bytes(imgfile);	/* Get the length of this tag */
/* 			printf("\nlen = %d", offset);
*/
			}while(rtype != M_SOF0 && rtype != M_SOF1 && rtype != M_SOF9);
/*		printf("\nTag found!");
*/
		fseek(imgfile, 1, SEEK_CUR);	/* Skip the data precition, don't need it */
		*height = get2bytes(imgfile);
		*width = get2bytes(imgfile);

		rtype = 2;
		}
	else
		{
		rtype = 0;

		*width = 0;
		*height = 0;
		}

	fclose(imgfile);
	return rtype;
	}

unsigned short get2bytes(FILE *infile)
	{
	unsigned long a;
	a = fgetc(infile);
	a <<= 8;
	a += fgetc(infile);
	return a;
	}

void backupfile(char *srcfile)
	{
	int ret;
	#ifdef __DOS__
	char *where;
	char dest[13];
	char cp[256];	
	strcpy(dest, srcfile);
	where = strchr(dest, '.');	/* find where the extention is */
	if (where != NULL)			/* Chop it off there */
		*where = '\0';
	#else
	int len;
	char dest[256];
	char cp[1024];	/* I think this is the longest command length
					   anyway it's penty of space for the copy command
					   and two filenames */

	strcpy(dest, srcfile);
	len = strlen(dest);

	/* If it's gonna exceed the 255 file limit, turnicate it and 
	   force the .BAK at the end. */
	if (len > 255-4)
		dest[len - 4] = '\0';
	#endif

	strcat(dest,".bak");
	printf("\n\tBacking up file %s to %s\n",srcfile, dest);
	#ifdef __WATCOMC__
	flushall();				/* Needs to flush stdio in watcom or it reads wrong */
	#endif
	strcpy(cp, COPYCMD);	/* copy */
	strcat(cp, srcfile);	/*  src	 */
	strcat(cp, " ");		/*  <space> */
	strcat(cp, dest);		/*  dest */
	ret = system(cp);		/* run "copy src dest" */
	if (ret != 0)	/* Handles errors if copy doesn't complete for some reason */
		{
		printf("An error has accord in the copy command of your OS...\nDo you wish to contine with Faster? (y/n)");
		#ifdef __WATCOMC__
		flushall();
		#endif
		getch(ret);
		tolower(ret);
		if (ret == 'n')
			{
			printf("\nQuiting Faster");
			exit(1);
			}
		}
	}

void memerror(char *error)
	{
	printf("\nError: %s");
	exit(1);
	}

void printhelp()
	{
	printf("\nUsage:\n\tfaster <options> <wildcards> ...\nOptions:\
\n\t-h or -?\tThis help screen\
\n\t-Uppercase\tUppercase Letters (e.g. WIDTH=100 HEIGHT=100)\
\n\t-Lowercase\tLowercase Letters *Default*\
\n\t-Backup\t\tBacks up the orignal files\
\n\t-Recurse\tRecurse into subdirectories\
\n\t-Delete\t\tDelete WIDTH and HEIGHT tags");
	}
/*
   Link list functions
*/

void addlink(char *str)
	{
	struct link *newlink;

	newlink = (struct link *)malloc(sizeof(struct link));
	if(newlink == NULL)
		memerror("Not enough memory for linklist");

	newlink->filename = (char *)malloc(strlen(str) + 1);
	if(newlink == NULL)
		memerror("Not enough memory for linklist to filename");

	strcpy(newlink->filename, str);
	newlink->next = NULL;

	if(linkhead == NULL)		/* speical case for first link */
		{
		linkhead = newlink;
		linktail = linkhead;
		}
	else
		{
		linktail->next = newlink;
		linktail = linktail->next;
		}
	}

int dellink()
	{
	struct link *temp;

	temp = linkhead->next;	
	
	free(linkhead->filename);		
	free(linkhead);			
	
	linkhead = temp;
	
	if(linkhead != NULL)
		return 0;
	else 
		return 1;
	}
