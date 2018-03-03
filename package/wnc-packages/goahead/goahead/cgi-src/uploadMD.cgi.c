/*
 * cgitest.c -- CGI Test example for the GoAhead WebServer
 *
 * Copyright (c) GoAhead Software Inc., 1995-2000. All Rights Reserved.
 *
 * See the file "license.txt" for usage and redistribution license requirements
 *
 * $Id: cgitest.c,v 1.2 2001/12/06 16:28:24 bporter Exp $
 */

/******************************** Description *********************************/

/*
 *      Tests the CGI environment variables generated by the GoAhead Webserver
 */

/********************************* Includes ***********************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEBUG_MODE 0
#define RFC_ERROR "RFC1867 error"

#define MODULE_FILE_PATH "/tmp/mdm_mass_storage/mdm_fw"
#define UPFW_STATE_FILE "/tmp/MDM_UPFW_STATUS"
//#define MOUNT_SCRIPT "/usr/local/bin/mdm_mount_ums.sh mount"

#define MAXLINE 512
#define BUFFSIZE 1024*1024
//define max size of uploads to 100M
#define MAXSIZE 104857600

int chk_is_substring(char *s1, char *s2)
{
	int str_len=0, k=0, i = 0, j = 0;

	//assign length of be to blen
	str_len =strlen(s1);

	while (s2[i] != '\0') {
		if (tolower(s2[i]) == tolower(s1[j])) {
			j++;
			if (j == str_len)
				return 1;
		} else {
			j=0;
			if (tolower(s2[i]) == tolower(s1[j]))
				j++;
		}
		i++;
	}
	return 0;
}

int getFWUPState(void)
{
	FILE *out = NULL;
	char buf[8];
	if ((out = fopen(UPFW_STATE_FILE, "r")) != NULL)
	{
		fgets(buf, sizeof(buf), out);
		if(strlen(buf) > 0){
			fclose(out);
			return atoi(buf);
		}
		fclose(out);
	}
    return -1;
}

int upload_file(long size, char* fileName)
{
	int rc = 0;
	int szBoundary_len= 0;
	char *psz1;
	FILE *out = NULL;
	long i, total = 0, count;
    long int szBuff_size = 0; 
	char szBoundary[MAXLINE];
	char *szBuff = NULL;
	char cmd[MAXLINE];
	
	if (size > MAXSIZE) {
		rc=1;
		goto Error;
	}
	szBuff_size = BUFFSIZE;
	szBuff = malloc(sizeof(char) * szBuff_size );
	if (szBuff == NULL) {
		szBuff_size = 16*1024;
		szBuff = malloc(sizeof(char) * szBuff_size );
		if (szBuff == NULL){
			rc=5;
			goto Error;
		}
	}
#if DEBUG_MODE	
	sprintf(cmd,"echo \"malloc %ld\" > /tmp/MDM_UPFW_STATUS_LOG",szBuff_size);
	system(cmd);
	memset(szBuff,0,szBuff_size);
#endif

	// first line should be MIME boundary
	if (fgets(szBoundary, sizeof(szBoundary), stdin) == NULL)
	{
	  rc = 2;
	  goto Error;
	}
	szBoundary_len = strlen(szBoundary);
	size-=(szBoundary_len*2) + 2;

	// second line should contain "Content-Disposition:
	//Content-Disposition: form-data; name=txtUploadLteFile; filename=
	if (fgets(szBuff, MAXLINE, stdin) == NULL)
	{
		rc = 2;
		goto Error;
	}
	size-=strlen(szBuff);

	// throw away until we get a blank line
	while (fgets(szBuff, MAXLINE, stdin) != NULL) {
        size-=strlen(szBuff);
		if (strlen(szBuff) <= 2) {
			break;
		}
	}

	if ((out = fopen(fileName, "wb+")) == NULL)
	{
		rc = 3;
		goto Error;
	}
#if DEBUG_MODE
	sprintf(cmd,"echo \"size=%ld\" >> /tmp/MDM_UPFW_STATUS_LOG",size);
	system(cmd);
#endif
	// copy the file
	while ((count=fread(szBuff, 1, szBuff_size, stdin)) != 0) {
        if((total+count) < size){
		  if ((i=fwrite(szBuff, 1, count, out)) != count) {
			  rc = 1;
			  goto Error;
		  }
		  memset(szBuff,0,szBuff_size);
		  total+=count;

		}else{
			count = size - total;
			if ((i=fwrite(szBuff, 1, count, out)) != count) {
				rc = 1;
				goto Error;
			}
			total+=count;
			break;
		}
	}
	
	if (total == 0) {
		rc = 2;
		goto Error;
	}
	
	// truncate the file at the correct length by writing 0 bytes
	fflush(out);

	Error:
	if (szBuff != NULL) {
		free(szBuff);
	}
	if (out != NULL) {
		fclose(out);
	}
#if DEBUG_MODE
	switch (rc) {
		case 0: // success
			sprintf(cmd,"echo \"The file %d bytes was uploaded sucessfully\" >> /tmp/MDM_UPFW_STATUS_LOG",size);
			break;
		case 1: // file too big
			sprintf(cmd,"echo \"lalalalala The file is too big. Try again.\" >> /tmp/MDM_UPFW_STATUS_LOG");
			break;
		case 2: // 0 byte file
			sprintf(cmd,"echo \"The file contains no data. Please try again with a different file.\" >> /tmp/MDM_UPFW_STATUS_LOG");
			break;
		default: // all other cases
			sprintf(cmd,"echo \"Error %d uploading file Please try again.\" >> /tmp/MDM_UPFW_STATUS_LOG",rc);
			break;
	}
	system(cmd);
#endif
	return rc;
}

/*************************************************************************/
/*
 *      Test program entry point
 */

int main (int argc, char *argv[])
{
	int ret;
	char *req_method;
	char *lenStr;
	long int content_len = 0;
    char cmd[MAXLINE];
	
    req_method = getenv("REQUEST_METHOD");
	
    printf(
"\
Server: %s\n\
Pragma: no-cache\n\
Content-type: text/html\n",
getenv("SERVER_SOFTWARE"));
#if defined(__P02003_Dora)
	printf("\n\
<html>\n\
<head>\n\
<TITLE>D-Link DWR-966</TITLE>\n\
<meta http-equiv=\"content-type\" content=\"text/html; charset=iso-8859-1\">\n\
</head>\n\
<body>");
#else
    printf("\n\
<html>\n\
<head>\n\
<TITLE>Upload Firmware</TITLE>\n\
<link rel=stylesheet href=/style/normal_ws.css type=text/css>\n\
<meta http-equiv=\"content-type\" content=\"text/html; charset=iso-8859-1\">\n\
</head>\n\
<body><h1>Upload Firmware</h1>");
#endif

    if (chk_is_substring("post",req_method)){
		lenStr = getenv("CONTENT_LENGTH");
		content_len = atol(lenStr); 
		if(content_len > 10485760 && content_len < 104857600){//10~100M
			//mount module ums
			//system(MOUNT_SCRIPT);
			ret = getFWUPState();
			if( ret == 1)
			{
			    //upload file
				ret = upload_file(content_len,MODULE_FILE_PATH);
				if(ret == 0){
					//Start to upgrade in next screen
					//system("mdm_fw_upgrade.sh &");
					
					//show new screen
					printf("<script language=\"javascript\">\n");
		//#if defined(__P02003_Dora)
					printf("window.open(\'%s\',\'_parent\');\n", "../system/md_upgrade_progress.asp");
		//#else
		//			printf("window.open(\'%s\',\'_parent\');\n", "../system/md_progressing_reboot.asp");
		//#endif
					printf("</script>\n");
					printf("</body>\n");
					printf("</html>\n");
					return 0;
				}else{
					if(ret == 3)
				  		printf("Error: write file failed.\n");
					else
			  			printf("%s %d", RFC_ERROR, ret);
				}
			}else
				  printf("Error: Mount failed. Service is not ready %d.\n", ret);
		}else{
			printf("Error: The update file is wrong.\n");
			printf("Please go back module firmware upgrade page then choose again!\n");
		}
    }
	printf("</body>\n");
	printf("</html>\n");
    return 0;
}
/*************************************************************************/

