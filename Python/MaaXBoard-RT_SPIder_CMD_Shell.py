#/usr/bin/env python

#Python network access to DualSPIder via IP

import socket

def Main():
    host = '192.168.1.91'
    port = 8080
    
    ##mySocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ##mySocket.connect((host, port))
    
    #ATT: after accept - it runs in a thread!
    #the thread will be closed - so also the connection is closed
    #we need a different module to implement this

    message = ""

    while message != 'q':
        message = input("-> ")
        if message == 'q':
            break;
        message = 'GET /C' + message + '\n'
        mySocket = socket.create_connection((host, port))
        mySocket.setblocking(1)
        mySocket.sendall(message.encode())
        data = mySocket.recv(4096).decode()
                 
        ##print ('Received from server: ' + data)
        print(data)

    print ('Good bye')
    mySocket.close()

if __name__ == '__main__':
    Main()

