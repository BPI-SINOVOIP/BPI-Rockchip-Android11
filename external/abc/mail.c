/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "config.h"
#include "misc.h"
#include <strings.h>

#define MIME    "Mime-Version: 1.0"
#define CONTENT_CODE    "Content-Transfer-Encoding: base64\r\n"

static const char mime_body[] =
    "Content-Type: multipart/mixed;"
    "  boundary=__=_Part_Boundary_001_011991.029871\r\n\r\n"
    "--__=_Part_Boundary_001_011991.029871\r\n"
    "Content-Type: multipart/alternative;"
    "  boundary=__=_Part_Boundary_001_011991.029872\r\n\r\n"
    "--__=_Part_Boundary_001_011991.029872\r\n"
    "Content-Type: text/plain;  charset=ISO-8859-1\r\n\r\n"
    "\r\n"
    "--__=_Part_Boundary_001_011991.029872\r\n"
    "Content-Type: text/html;  charset=ISO-8859-1\r\n"
    "\r\n\r\n\r\n"
    "--__=_Part_Boundary_001_011991.029872--\r\n"
    "--__=_Part_Boundary_001_011991.029871\r\n";


static int sockfd;

static int setup_socket(const char *server)
{
    struct sockaddr_in server_addr;
    struct hostent *host;

    if ((host = gethostbyname(server)) == NULL)
    {
        printf("Get host name error!\n");
        close(sockfd);
        return -1;
    }
    /* set up socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Socket setup error!\n");
        close(sockfd);
        return -1;
    }
    bzero(&server_addr, sizeof (server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)25);
    server_addr.sin_addr = *((struct in_addr *)host->h_addr);

    /* try to connect server */
    if (connect(sockfd, (struct sockaddr *)&server_addr,
                sizeof (struct sockaddr)) == -1)
    {
        printf("Connect server error!\n");
        close(sockfd);
        return -1;
    }
    return 0;
}

static int command_HELO(const char *server)
{
    char buffer[1024 * 5] = {0};

    /* send HElO to server */
    sprintf(buffer, "HELO %s\r\n", server);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send HELO error!\n");
        close(sockfd);
        return -1;
    }
    if (recv(sockfd, buffer, sizeof (buffer), 0) == -1)
    {
        printf("Receive HELO error!\n");
        close(sockfd);
        return -1;
    }
    printf("\nData received HELO: %s\n", buffer);
    return 0;
}

static int command_AUTH(void)
{
    char buffer[1024 * 5] = {0};
    /* send login message */
    if (send(sockfd, "AUTH LOGIN\r\n", strlen("AUTH LOGIN\r\n"), 0) == -1)
    {
        printf("Send AUTH LOGIN error!\n");
        close(sockfd);
        return -1;
    }

    if (recv(sockfd, buffer, sizeof (buffer), 0) == -1)
    {
        printf("Receive AUTH LOGIN error!\n");
        close(sockfd);
        return -1;
    }
    printf("\nData received AUTH LOGIN: %s", buffer);
    return 0;
}

static int command_NAME(void)
{
    char buffer[1024 * 5] = {0};

    int name_len = 0;
    int base64_name_len = 0;
    char *base64_name_buf = NULL;

    /* send user name*/
    name_len = strlen(USER_NAME);
    base64_name_len = name_len * 4 / 3 + 50;
    base64_name_buf = (char *)malloc(base64_name_len);
    if (base64_name_buf == NULL)
    {
        printf("Base64_name_buf memory allocate error!\nExit process...\n");
        exit(EXIT_FAILURE);
    }
    if (base64_encode(USER_NAME, base64_name_buf, name_len) == -1)
    {
        printf("Base64 encode user name error!\nExit process...\n");
        exit(EXIT_FAILURE);
    }

    sprintf(buffer, "%s\n", base64_name_buf);
    if (send(sockfd, buffer, strlen(buffer), 0) < 0)
    {
        printf("Send USER NAME error!\n");
        close(sockfd);
        return -1;
    }
    if (base64_name_buf != NULL)
    {
        free(base64_name_buf);
        base64_name_buf = NULL;
    }
    else
    {
        printf("Base64_name_buf is NULL!\nExit process...\n");
        exit(EXIT_FAILURE);
    }
    if (recv(sockfd, buffer, sizeof (buffer), 0) == -1)
    {
        printf("Receive USER NAME error!\n");
        close(sockfd);
        return -1;
    }
    printf("Data received NAME: %s\n", buffer);
    return 0;
}

static int command_PASSWD(void)
{
    char buffer[1024 * 5] = {0};
    int passwd_len = 0;
    int base64_passwd_len = 0;
    char *base64_passwd_buf = NULL;

    /* send user password */
    passwd_len = strlen(USER_PASSWORD);
    base64_passwd_len = passwd_len * 4 / 3 + 50;
    base64_passwd_buf = (char *)malloc(base64_passwd_len);
    if (base64_passwd_buf == NULL)
    {
        printf("Base64_passed_buf is NULL!\nExit process...\n");
        exit(EXIT_FAILURE);
    }
    if(base64_encode(USER_PASSWORD, base64_passwd_buf, passwd_len) == -1)
    {
        printf("Base64 encode user password error!\nExit process...\n");
        exit(EXIT_FAILURE);
    }

    sprintf(buffer, "%s\n", base64_passwd_buf);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send PASSWORD error!\n");
        close(sockfd);
        return -1;
    }
    if (base64_passwd_buf != NULL)
    {
        free(base64_passwd_buf);
        base64_passwd_buf = NULL;
    }
    else
    {
        printf("Base64_passed_buf is NULL!\nExit process...\n");
        exit(EXIT_FAILURE);
    }
    if (recv(sockfd, buffer, sizeof (buffer), 0) == -1)
    {
        printf("Receive PASSWORD error!\n");
        close(sockfd);
        return -1;
    }
    printf("Data received PASSWD: %s", buffer);
    return 0;
}

static int command_FROM(void)
{
    char buffer[1024 * 5] = {0};

    /* send sender mail */
    sprintf(buffer, "MAIL FROM: <%s>\n", MAIL_SENDER);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send FROM error!\n");
        close(sockfd);
        return -1;
    }
    if (recv(sockfd, buffer, sizeof (buffer), 0) == -1)
    {
        printf("Receive FROM error!\n");
        close(sockfd);
        return -1;
    }
    printf("Data received FROM: %s\n", buffer);
    return 0;
}

static int command_TO(void)
{
    char buffer[1024 * 5] = {0};

    /* send recipient mail*/
    sprintf(buffer, "RCPT TO: <%s>\n", MAIL_RECIPIENT);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send TO error!\n");
        close(sockfd);
        return -1;
    }
    if (recv(sockfd, buffer, sizeof (buffer), 0) == -1)
    {
        printf("Receive TO error!\n");
        close(sockfd);
        return -1;
    }
    printf("\nData received TO: %s", buffer);
    return 0;
}

static int command_DATA(const char * pmsg, const char* file_name)
{
    char buffer[1024 * 5] = {0};

    /* send data command */
    if (send(sockfd, "DATA\r\n", strlen("DATA\r\n"), 0) == -1)
    {
        printf("Send DATA error!\n");
        close(sockfd);
        return -1;
    }
    /* send subject */
    sprintf(buffer, "From: %s\r\n", FROM);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send SUBJECT FROM error!\n");
        close(sockfd);
        return -1;
    }
    sprintf(buffer, "To: %s\r\n", TO);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send SUBJECT TO error!\n");
        close(sockfd);
        return -1;
    }
    sprintf(buffer, "%s\r\n", MIME);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send MIME HEAD error!\n");
        close(sockfd);
        return -1;
    }
    sprintf(buffer, "Subject: %s\r\n", SUBJECT);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send SUBJECT error!\n");
        close(sockfd);
        return -1;
    }
    if (send(sockfd, mime_body, strlen(mime_body), 0) == -1)
    {
        printf("Send MIME BODY eror\n");
        close(sockfd);
        return -1;
    }
    sprintf(buffer, "Content-Type: application/x-bzip2;"
            "  name=\"%s.tar.bz2\"\r\n", file_name);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send Conten-Type error!\n");
        close(sockfd);
        return -1;
    }
    sprintf(buffer, "Content-Disposition: attachment;"
            "  filename=\"%s.tar.bz2\"\r\n", file_name);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send Content-Disposition error!\n");
        close(sockfd);
        return -1;
    }
    sprintf(buffer, "%s\r\n", CONTENT_CODE);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        printf("Send ENCODE TYPE error!\n");
        close(sockfd);
        return -1;
    }
    /* send mail content*/
    if (send(sockfd, pmsg, strlen(pmsg), 0) == -1)
    {
        printf("Send MESSAGE error!\n");
        close(sockfd);
        return -1;
    }
    /*send finish message */
    if (send(sockfd, "\r\n.\r\n", strlen("\r\n.\r\n"), 0) == -1)
    {
        printf("Send FINISH error!\n");
        close(sockfd);
        return -1;
    }
    /* receive reply from server */
    if (recv(sockfd, buffer, sizeof (buffer), 0) == -1)
    {
        printf("Receive FINISH error!\n");
        close(sockfd);
        return -1;
    }
    printf("Data recevied FINISH: %s", buffer);
    return 0;

}

static int command_QUIT(void)
{
    char buffer[1024 * 5];

    /* send quit message */
    if (send(sockfd, "QUIT\r\n", strlen("QUIT\r\n"), 0) == -1)
    {
        printf("Send QUIT erroe!\n");
        close(sockfd);
        return -1;
    }
    if (recv(sockfd, buffer, sizeof (buffer), 0) == -1)
    {
        printf("Receive QUIT error!\n");
        close(sockfd);
        return -1;
    }
    printf("Data received QUIT: %s\n", buffer);
    close(sockfd);
    return 0;
}

int mail(const char *server, const char *pmsg, const char* file_name)
{
    if (setup_socket(server) == -1)
        return -1;
    if (command_HELO(server) == -1)
        return -1;
    if (command_AUTH() == -1)
        return -1;
    if (command_NAME() == -1)
        return -1;
    if (command_PASSWD() == -1)
        return -1;
    if (command_FROM() == -1)
        return -1;
    if (command_TO() == -1)
        return -1;
    if (command_DATA(pmsg, file_name) == -1)
        return -1;
    if (command_QUIT() == -1)
        return -1;

    return 0;
}

