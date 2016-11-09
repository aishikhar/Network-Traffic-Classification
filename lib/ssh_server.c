#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include<libssh/libssh.h>
#include<libssh/server.h>
#define RSAPATH "/libssh/ssh_host_rsa_key"
#define DSAPATH "/libssh/ssh_host_dsa_key"

static int auth_password(const char *user, const char *password){
    if(strcmp(user,"user"))
        return 0;
    if(strcmp(password,"pass"))
        return 0;
    return 1; // authenticated
}

int main(int argc, char **argv){
    ssh_session session;
    ssh_bind sshbind;
    ssh_message message;
    ssh_channel chan=0;
    char buf[2048];
    int auth=0;
    int sftp=0;
    int i;
    int r;
    int verbosity=1;
    sshbind=ssh_bind_new();
    session=ssh_new();
    int port = 2232;
    int iError;
    iError= ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, "libssh/ssh_host_rsa_key");
    
   // iError= ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_DSAKEY, "libssh/ssh_host_dsa_key");
    
    iError=ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    iError=ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    iError=ssh_options_set(session, SSH_OPTIONS_HOST, "192.168.43.68");
    iError= ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port);
    iError=ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_LOG_VERBOSITY_STR, "1");
    puts("Binding done");
    puts(ssh_get_error(session));
    if(iError < 0 )
        printf("Error listening to socket: %s\n",ssh_get_error(sshbind));
    puts("listen...");

    if(ssh_bind_listen(sshbind)<0){
        printf("Error listening to socket: %s\n",ssh_get_error(sshbind));
        return 1;
    }
        puts("accept...");

    r=ssh_bind_accept(sshbind,session);
    if(r==SSH_ERROR){
      puts("error accepting a connection :");puts(ssh_get_error(sshbind));
      return 1;
    }
        puts("acepted,exchange key...");

    if (ssh_handle_key_exchange(session)) {
        puts("ssh_handle_key_exchange: ");puts(ssh_get_error(session));
        return 1;
    }
        puts("key done get message...");

    do {
        message=ssh_message_get(session); 
        if(!message)
            break;
        switch(ssh_message_type(message)){
            case SSH_REQUEST_AUTH:
                switch(ssh_message_subtype(message)){
                    case SSH_AUTH_METHOD_PASSWORD:
                        printf("User %s wants to auth with pass %s\n",
                               ssh_message_auth_user(message),
                               ssh_message_auth_password(message));
                        if(auth_password(ssh_message_auth_user(message),
                           ssh_message_auth_password(message))){
                               auth=1;
                               ssh_message_auth_reply_success(message,0);
                               break;
                           }
                        // not authenticated, send default message
                    case SSH_AUTH_METHOD_NONE:
                    default:
                        ssh_message_auth_set_methods(message,SSH_AUTH_METHOD_PASSWORD);
                        ssh_message_reply_default(message);
                        break;
                }
                break;
            default:
                ssh_message_reply_default(message);
        }
        ssh_message_free(message);
    } while (!auth);
    if(!auth){
        printf("auth error: %s\n",ssh_get_error(session));
        ssh_disconnect(session);
        return 1;
    }
    puts("\nAuthentication Completed\n\nSECURE CHANNEL CREATED!\n");
    do {
        message=ssh_message_get(session);
        if(message){
            switch(ssh_message_type(message)){
                case SSH_REQUEST_CHANNEL_OPEN:
                    if(ssh_message_subtype(message)==SSH_CHANNEL_SESSION){
                        chan=ssh_message_channel_request_open_reply_accept(message);
                        break;
                    }
                default:
                ssh_message_reply_default(message);
            }
            ssh_message_free(message);
        }
    } while(message && !chan);
    if(!chan){
        printf("error : %s\n",ssh_get_error(session));
        ssh_finalize();
        return 1;
    }
    do {
        message=ssh_message_get(session);
        if(message && ssh_message_type(message)==SSH_REQUEST_CHANNEL &&
           ssh_message_subtype(message)==SSH_CHANNEL_REQUEST_SHELL){
            if(!strcmp(ssh_message_channel_request_subsystem(message),"sftp")){
                sftp=1;
                ssh_message_channel_request_reply_success(message);
                break;
              }
           }
        if(!sftp){
            ssh_message_reply_default(message);
        }
        ssh_message_free(message);
    } while (message && !sftp);
    if(!sftp){
        printf("error sftp: %s\n",ssh_get_error(session));
        return 1;
    }
    puts("it works !\n");
    do{
        i=ssh_channel_read(chan,buf, 2048, 0);
        if(i>0) {
            ssh_channel_write(chan, buf, i);
            if (write(1,buf,i) < 0) {
                printf("error writing to buffer\n");
                return 1;
            }
        }
    } while (i>0);
    ssh_disconnect(session);
    ssh_bind_free(sshbind);
    ssh_finalize();
    return 0;
}
